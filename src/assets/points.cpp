//-----------------------------------------------------------------------------
// FLUIDS v5.0 - SPH Fluid Simulator for CPU and GPU
// Copyright (C) 2021. Rama Hoetzlein, http://fluids3.com
//-----------------------------------------------------------------------------

#include <GL/glew.h>
#include <assert.h>
#include <stdio.h>
#include "timex.h"
#include "main.h"
#include "image.h"
#include "mersenne.h"

#include "gxlib.h"
using namespace glib;


#ifdef USE_CUDA
	#include "common_cuda.h"
#endif

#include "points.h"


#define EPSILON			0.00001f			// for collision detection

#define SCAN_BLOCKSIZE		512				// must match value in fluid_system_cuda.cu


// Accessor functions
//
int iDivUp (int a, int b) {
    return (b==0) ? 1 : ((a % b != 0) ? (a / b + 1) : (a / b));
}
void computeNumBlocks (int numPnts, int maxThreads, int &numBlocks, int &numThreads)
{
    numThreads = std::min( maxThreads, numPnts );
    numBlocks = iDivUp ( numPnts, numThreads );
}
void computeNumBlocks (int x, int y, int z, int maxThreads, Vec3I &numBlocks, Vec3I &numThreads)
{
    numThreads = Vec3I( std::min( maxThreads, x), std::min( maxThreads, y), std::min( maxThreads, z) );
    numBlocks = Vec3I( iDivUp( x, numThreads.x), iDivUp( y, numThreads.y), iDivUp( z, numThreads.z) );
}
int Points::getGridCell ( int p, Vec3I& gc )
{
	return getGridCell ( m_Points.bufF3(FPOS)[p], gc );
}
int Points::getGridCell ( Vec3F& pos, Vec3I& gc )
{
	gc.x = (int)( (pos.x - m_Params.gridMin.x) * m_Params.gridDelta.x);			// Cell in which particle is located
	gc.y = (int)( (pos.y - m_Params.gridMin.y) * m_Params.gridDelta.y);
	gc.z = (int)( (pos.z - m_Params.gridMin.z) * m_Params.gridDelta.z);		
	return (int)( (gc.y*m_Params.gridRes.z + gc.z)*m_Params.gridRes.x + gc.x);		
}
Vec3I Points::getCell ( int c )
{
	Vec3I gc;
	int xz = m_Params.gridRes.x*m_Params.gridRes.z;
	gc.y = c / xz;				c -= gc.y*xz;
	gc.z = c / m_Params.gridRes.x;		c -= gc.z*m_Params.gridRes.x;
	gc.x = c;
	return gc;
}

// Construction
//
Points::Points ()
{
	#ifdef USE_CUDA	
		m_bGPU = true;		// use GPU pathway
	#else
		m_bGPU = false;
	#endif

	mNumPoints = 0;
	mMaxPoints = 0;	
	m_Frame = 0;	
	m_Terrain = 0;
	m_ACCL = false;
	m_SPH = false;
	m_DEM = false;
	m_FIRE = false;

	m_lastfire = 0;

	m_rand.seed ( 247 );

	#ifdef USE_CUDA
		m_Module = 0;
		for (int n=0; n < FUNC_MAX; n++ ) m_Func[n] = (CUfunction) -1;	
	#endif
}

Points::~Points()
{
	Clear ();

	#ifdef USE_CUDA
	if (m_Module != 0) cuCheck(cuModuleUnload(m_Module), "~FluidSystem()", "cuModuleUnload", "m_Module", m_bDebug);
	#endif
}
void Points::Clear ()
{
	// Free fluid buffers
	/*for (int n=0; n < MAX_BUF; n++ ) {
		if ( m_Points.bufC(n) != 0x0 )
			free ( m_Points.bufC(n) );
	}*/
}

void Points::LoadKernel ( int fid, std::string func )
{
	char cfn[512];		strcpy ( cfn, func.c_str() );
	
	#ifdef USE_CUDA
	if ( m_Func[fid] == (CUfunction) -1 )
		cuCheck ( cuModuleGetFunction ( &m_Func[fid], m_Module, cfn ), "LoadKernel", "cuModuleGetFunction", cfn, m_bDebug );	
	#endif
}

void Points::Setup ( Vec3F bmin, Vec3F bmax, float dt, float sim_scale, float grid_space, float smoothradius )
{
	// Simulation setup:
	//   ideal grid cell size (gs) = 2 * smoothing radius = 0.02*2 = 0.04
	//   ideal domain size = k*gs/d = k*0.02*2/0.005 = k*8 = {8, 16, 24, 32, 40, 48, ..}
	//    (k = number of cells, gs = cell size, d = simulation scale)

	m_Params.dt = dt;
	m_Params.sim_scale = sim_scale;
	m_Params.grid_spacing = grid_space;
	m_Params.psmoothradius = smoothradius;    // smooth radius. we can use a very small radius, if the grid_spacing is magnified accordingly. psmoothradius/sim_scale = real world radius

	m_Params.r2 = m_Params.psmoothradius * m_Params.psmoothradius;		// utility vars
	m_Params.d2 = m_Params.sim_scale * m_Params.sim_scale;
	m_Params.rd2 = m_Params.r2 / m_Params.d2;

	// domain size
	m_Params.bound_min = bmin; 
	m_Params.bound_max = bmax;

	// Static points initalization (not assuming advection)
	#ifdef USE_CUDA
		cuCheck ( cuModuleLoad ( &m_Module, "points.ptx" ), "LoadKernel", "cuModuleLoad", "points.ptx", m_bDebug);

		// Assign DataX buffers to GPU
		m_Points.AssignToGPU ( "FPnts", m_Module );
	
		// Access the sim parameters (note: should be made DataX later)
		size_t len;
		cuCheck ( cuModuleGetGlobal ( &m_cuParams, &len, m_Module, "FParams" ), "Initialize", "cuModuleGetGlobal", "cuParams", true );	
	#endif
}


void Points::CommitParams()
{
	if ( m_bGPU ) {
		#ifdef USE_CUDA			
		cuCheck ( cuMemcpyHtoD ( m_cuParams, &m_Params,	sizeof(FParams_t) ), "FluidParamCUDA", "cuMemcpyHtoD", "cuFParams", m_bDebug);		// Transfer sim params to device
		#endif
	}
}

void Points::Restart (bool sym)
{
	// Compute particle thread blocks
	m_Params.pnum = mNumPoints;
	int threadsPerBlock = 512;	
	computeNumBlocks ( m_Params.pnum, threadsPerBlock, m_Params.numBlocks, m_Params.numThreads);	// Modifies params: numBlocks, numThreads
    m_Params.szPnts = (m_Params.numBlocks  * m_Params.numThreads);									// Modifies params: szPnts
	//dbgprintf ( "  Particles: %d, t:%dx%d=%d, Size:%d\n", m_Params.pnum, m_Params.numBlocks, m_Params.numThreads, m_Params.numBlocks*m_Params.numThreads, m_Params.szPnts);
	
	// Update GPU access pointers
	UpdateGPUAccess (sym);	

	// Update acceleration
	char memflags = m_bGPU ? DT_CUMEM : DT_CPU;	
	if (m_ACCL) {
		m_PointsTemp.MatchAllBuffers ( &m_Points, memflags );				// Update temporary buffers
		
		Accel_RebuildGrid();												// Rebuild acceleration grid.		// Modifies params: gridRes, gridMin, gridDelta
	}

	// Update all params to GPU
	CommitParams();
	
	// Recommit all buffers
	CommitAll ();			
}


// Memory access functions
//
void Points::AllocatePoints ( int maxpnt )
{
	mMaxPoints = maxpnt;
	m_Params.pnum = maxpnt;	

	// required variables
	m_Points.DeleteAllBuffers ();
	m_Points.AddBuffer ( FPOS, "pos",		sizeof(Vec3F),	maxpnt, DT_CPU | DT_GLVBO | DT_CUINTEROP );		// cuda-opengl interop
	m_Points.AddBuffer ( FCLR, "clr",		sizeof(uint),	maxpnt, DT_CPU | DT_GLVBO | DT_CUINTEROP );		// cuda-opengl interop
	m_Points.AddBuffer ( FVEL, "vel",		sizeof(Vec3F),	maxpnt, DT_CPU | DT_GLVBO | DT_CUINTEROP );		// cuda-opengl interop

	m_Points.SetBufferUsage ( FPOS, DT_FLOAT3 );
	m_Points.SetBufferUsage ( FCLR, DT_UINT );
	m_Points.SetBufferUsage ( FVEL, DT_FLOAT3 );
}

int Points::AddPoint ( Vec3F pos, uint clr )
{
	if ( mNumPoints >= mMaxPoints ) return -1;

	int n = mNumPoints;
	*m_Points.bufF3(FPOS,n) = pos;	
	*m_Points.bufUI(FCLR,n) = clr;
	*m_Points.bufF3(FVEL,n) = Vec3F(0,0,0);	

	mNumPoints++;
	return n;
}

int Points::AddPointRandom ( Vec3F pos, uint clr )
{
	if ( mNumPoints < mMaxPoints ) {
		return AddPoint ( pos, clr );		
	}
	int i = m_rand.randI ( 0, mNumPoints );		// steal
	*m_Points.bufF3(FPOS,i) = pos;	
	*m_Points.bufUI(FCLR,i) = clr;
	*m_Points.bufF3(FVEL,i) = Vec3F(0,0,0);	
	return i;
}

int Points::AddPoint ()
{
	if ( mNumPoints >= mMaxPoints ) return -1;
	int n = mNumPoints;
	*m_Points.bufF3(FPOS,n) = Vec3F(0,0,0);
	*m_Points.bufF3(FVEL,n) = Vec3F(0,0,0);
	*m_Points.bufI (FCLR,n) = 0;
	mNumPoints++;
	return n;
}

void Points::CreateFrom ( Points* src )
{
	m_Points.MatchAllBuffers ( &src->m_Points );
	m_Points.CopyAllBuffers ( &src->m_Points );	
	m_Points.SetNum ( src->mNumPoints );

	mNumPoints = src->mNumPoints;
	mMaxPoints = src->mMaxPoints;
}

void Points::CreateSubset ( Points* src, int pcnt )
{
	int skip = src->mNumPoints / pcnt;
	int pnum = src->mNumPoints / skip;

	// new points
	int first = mNumPoints;
	mNumPoints += pnum;
	m_Points.SetNum ( mNumPoints );	

	Vec3F* pos =	m_Points.bufF3(FPOS) + first;
	Vec3F* vel =	m_Points.bufF3(FVEL) + first;
	uint* clr =			m_Points.bufUI(FCLR) + first;
	Vec3F* srcpos =	src->m_Points.bufF3(FPOS) + first;
	Vec3F* srcvel =	src->m_Points.bufF3(FVEL) + first;
	uint* srcclr =			src->m_Points.bufUI(FCLR) + first;

	for (int n=0; n < pnum; n++) {
		*pos++ = *srcpos;
		*vel++ = *srcvel;
		*clr++ = *srcclr;

		srcpos += skip;
		srcvel += skip;
		srcclr += skip;
	}
}


void Points::UpdateGPUAccess(bool sym)
{
	// Update GPU access 
	m_Points.UpdateGPUAccess ();
	if (m_ACCL) {
		m_PointsTemp.UpdateGPUAccess ();
		m_Accel.UpdateGPUAccess ();
	}
}

void Points::CopyAllToTemp ()
{	
	m_Points.CopyAllBuffers ( &m_PointsTemp, DT_CUMEM );	// gpu-to-gpu copy all buffers into temp
	m_PointsTemp.UpdateGPUAccess();
}

void Points::CommitAll ()
{	
	m_Points.CommitAll (); 									// send particle buffers to GPU
}

void Points::Commit (int b)
{
	m_Points.Commit ( b );
}

void Points::Retrieve ( int buf )
{	
	m_Points.Retrieve ( buf );								// return particle buffers to GPU
}


#define clamp(x,a,b)		((x<a) ? a : (x<b) ? x : b)

void Points::CreatePointsFromBuf ( int srcnum, Vec3F* srcpos, uint64_t* srcextra, Vec3F reorig, Vec3F rescal, Vec3F repos )
{
	float inc = 0;
	float skip = float (srcnum) / float( mMaxPoints );

	// new points
	int pnew = srcnum / skip;
	int first = mNumPoints;
	mNumPoints += pnew;
	m_Points.SetNum ( mNumPoints );
	
	// transform from old bounding box to new	
	Vec4F vclr;

	Vec3F* pos =	m_Points.bufF3(FPOS) + first;
	Vec3F* vel =	m_Points.bufF3(FVEL) + first;
	uint* clr =		m_Points.bufUI(FCLR) + first;
	uint64_t* srcex = srcextra;

	for (int n=0; n < pnew; n++) {
		//float v = srcpos->w / 4.f;		// intensity

		*pos = (*srcpos + reorig) * rescal + repos;		
		*vel = Vec3F(0,0,0);
		
		// compute lidar point color
		vclr = Vec4F(0,0,0,1);		
		float h = fmax(0, fmin(1, (pos->y / 80.0f)));					// height
		if ( srcextra != 0x0 ) {	
			float a = float(uint(*srcex) >> 16u) / 255.0;				// albiedo
			float b = float(uint(*srcex) & 3u) / 255.0;					// return #
			vclr += Vec4F(1, 0, 0, 0) * a + Vec4F(0, 0, 1, 0) * b;
		}
		vclr += Vec4F(0.2, 0.2, 0, 0) * (1 - h) + Vec4F(0, 1, 0, 0) * h;
		*clr = VECCLR(vclr);

		inc += skip;
		if (inc > 1.0) { 
			srcpos += int(inc); 
			srcex += int(inc);
			inc -= int(inc); 
		}
		pos++;
		vel++;
		clr++;
	}

	m_Points.Commit ( FPOS );
	m_Points.Commit ( FVEL );
	m_Points.Commit ( FCLR );

	dbgprintf("  Points::CreatePointsFromBuf.. OK\n");
}

void Points::CreatePointsRandomInVolume(Vec3F min, Vec3F max)
{
	int p;	
	Vec3F pos, clr;

	Vec3F del = Vec3F(1,1,1)/(max-min);

	for (int n=0; n < m_Params.pnum; n++) {

		p = AddPoint();							// increments mNumPoints (will not exceed mMaxPoints)

		pos.Random(min.x, max.x, min.y, max.y, min.z, max.z );
		*m_Points.bufF3(FPOS, p) = pos;

		clr = (pos - min) * del;		
		*m_Points.bufUI(FCLR, p) = COLORA(clr.x, clr.y, clr.z, 1);

	}
}


void Points::CreatePointsUniformInVolume ( Vec3F min, Vec3F max )
{
	// Spacing to add points 
	m_Params.pdist = pow ( (float) m_Params.pmass / m_Params.prest_dens, 1/3.0f );	
	m_Params.pspacing = m_Params.pdist * 0.87f / m_Params.sim_scale;		// not a critical sim variable
	
	dbgprintf ( "  CreatePointsInVolume. Density: %f, Spacing: %f, PDist: %f\n", m_Params.prest_dens, m_Params.pspacing, m_Params.pdist );

	// Distribute points at rest spacing
	float spacing = m_Params.pspacing;
	float offs = 0;
	Vec3F pos;
	int p, cntx, cntz;
	float dx, dy, dz;	
	cntx = (int) ceil( (max.x-min.x-offs) / spacing );
	cntz = (int) ceil( (max.z-min.z-offs) / spacing );
	int cnt = cntx * cntz;
	int c2;	
	
	min += offs;
	max -= offs;
	dx = max.x-min.x;
	dy = max.y-min.y;
	dz = max.z-min.z;

	Vec3F rnd;
		
	c2 = cnt/2;
	for (pos.y = min.y; pos.y <= max.y; pos.y += spacing ) {	
		for (int xz=0; xz < cnt; xz++ ) {
			
			pos.x = min.x + (xz % int(cntx))*spacing;
			pos.z = min.z + (xz / int(cntx))*spacing;
			p = AddPoint ();							// increments mNumPoints (will not exceed mMaxPoints)

			if ( p != -1 ) {
				rnd.Random ( 0, spacing, 0, spacing, 0, spacing );					
				*m_Points.bufF3(FPOS,p) = pos + rnd;
				
				Vec3F clr ( (pos.x-min.x)/dx, 0.f, (pos.z-min.z)/dz );				
				clr *= 0.8f; 
				clr += 0.2f;				
				clr.Clamp (0, 1.0);								
				*m_Points.bufUI(FCLR,p) = COLORA( clr.x, clr.y, clr.z, 1); 

				// = COLORA( 0.25, +0.25 + (y-min.y)*.75/dy, 0.25 + (z-min.z)*.75/dz, 1);  // (x-min.x)/dx
			}
		}
	}		
	// Set number in use
	m_Points.SetNum ( mNumPoints );	
	m_PointsTemp.SetNum ( mNumPoints );
}

int Points::CreateSphere ( int cnt, float radius )
{
	dbgprintf("  CreateSphere. \n" );

	int p;
	Vec3F pos;
	float r, theta, phi;
	phi = 3.1415928 * (sqrt(5)-1);		// golden angle in radians
	int first = mNumPoints;

	// Fibonnaci
	for (int i=0; i < cnt; i++) {
		p = AddPoint();
		pos.y = 1 - (float(i) / float(cnt - 1)) * 2;
		r = sqrt ( 1 - pos.y * pos.y );
		theta = phi * i;
		pos.x = cos(theta) * r;
		pos.z = sin(theta) * r;
		*m_Points.bufF3(FPOS, p) = pos * radius;
		*m_Points.bufF3(FVEL, p) = Vec3F(0, 0, 0);
		*m_Points.bufUI(FCLR, p) = COLORA(1, 1, 1, 1);
	}

	// randomly placed
	/* for (int i=0; i < cnt; i++) {		
		p = AddPoint ();		
		pos.Random (-1, 1, -1, 1, -1, 1 );
		pos.NormalizeFast();											// point on sphere
		*m_Points.bufF3(FPOS,p) = pos * radius;
		*m_Points.bufF3(FVEL,p) = Vec3F(0,0,0);
		*m_Points.bufUI(FCLR,p) = COLORA(1, 1, 1, 1);
	} */

	// Set number in use
	m_Points.SetNum(mNumPoints);
	m_PointsTemp.SetNum(mNumPoints);

	return first;
}

void Points::Randomize ( Vec3F vmin, Vec3F vmax )
{
	Mersenne rnd;
	rnd.seed ( 162 );

	Vec3F* pos =	m_Points.bufF3(FPOS);
	for (int n=0; n < mNumPoints; n++) {
		*pos++ = rnd.randV3( vmin, vmax);
	}
	m_Points.Commit(FPOS);
}

void Points::Colorize ( Vec4F clr0, Vec4F clr1 )
{
	uint* clr =	m_Points.bufUI(FCLR);
	Vec4F newclr;
	
	for (int n=0; n < mNumPoints; n++) {		
		float v = Vec3F(CLRVEC(*clr)).Length();
		newclr = clr0 + (clr1 - clr0) * v;
		*clr++ = VECCLR(newclr);
	}
	m_Points.Commit ( FCLR );
}

void Points::SetColor ( Vec3F clr )
{
	uint* pclr =	m_Points.bufUI(FCLR);

	for (int n=0; n < mNumPoints; n++) {
		*pclr++ = COLORA(clr.x, clr.y, clr.z, 1);
	}
	m_Points.Commit ( FCLR );	
}




// Debug functions
//
void Points::DebugPrint ( int i, int start, int disp)
{
	char buf[256];
 
	DataX* dat = &m_Points;		// could be m_Accel
	
	dat->Retrieve ( i );

	#ifdef USE_CUDA
		dbgprintf ("---- cpu: %012llx, gpu: %012llx\n", dat->cpu(i), dat->gpu(i) );	
	#else
		dbgprintf ("---- cpu: %012llx\n", dat->cpu(i) );	
	#endif
	for (int n=start; n < start+disp; n++ ) {
		dbgprintf ( "%d: %s\n", n, dat->printElem  (i, n, buf) );
	}
}

void Points::DebugPrintMemory ()
{
	int psize = 4*sizeof(Vec3F) + sizeof(uint) + sizeof(unsigned short) + 2*sizeof(float) + sizeof(int) + sizeof(int)+sizeof(int);
	int gsize = 2*sizeof(int);
	int nsize = sizeof(int) + sizeof(float);
		
	dbgprintf ( "MEMORY:\n");	
	dbgprintf ( "  Particles:              %d, %f MB (%f)\n", mNumPoints, (psize*mNumPoints)/1048576.0, (psize*mMaxPoints)/1048576.0);
	dbgprintf ( "  Acceleration Grid:      %d, %f MB\n",	  m_Params.gridTotal, (gsize * m_Params.gridTotal)/1048576.0 );
}


void Points::Sketch (int w, int h, Camera3D* cam)
{
	// sketch points themselves
	PERF_PUSH ("sketch");
	selfStartDraw3D ( cam );
		//setPreciseEye ( SPNT, cam );
		//setPntParams ( Vec4F(0,1,0,0), Vec4F(0,0,0,0), Vec4F(1,1,1,0), Vec4F(0,0,0,0) );	// enable x=extra, y=clr
		checkGL ( "start pnt shader" );
		glBindBuffer ( GL_ARRAY_BUFFER, m_Points.glid(FPOS) );	
		glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3F), 0 );			
		checkGL ( "bind pos" ); 
		glBindBuffer ( GL_ARRAY_BUFFER, m_Points.glid(FCLR) );
		glVertexAttribPointer ( 1, 1, GL_FLOAT, GL_FALSE, sizeof(uint), 0 );
		checkGL ( "bind clr" );		
		glDrawArrays ( GL_POINTS, 0, getNumPoints() );
		checkGL ( "draw pnts" );		
	selfEndDraw3D ();
	checkGL ( "end pnt shader" );
	PERF_POP();

	// sketch debug points as boxes
	Vec3F a,b;
	Vec4F clr;
	start3D(cam);
	for (int i=0; i < m_DebugPnts.size(); i++ ) {
		a = m_DebugPnts[i].pos - Vec3F(.2,.2,.2);
		b = m_DebugPnts[i].pos + Vec3F(.2,.2,.2);
		clr = CLRVEC( m_DebugPnts[i].clr );
		drawBox3D ( a, b, clr );
	}		
}

// Draw
// OpenGL draw using shaders and functions found in nv_gui.cpp
// VBO buffers were created in RebuildParticles using CUDA-GL interop 
//

void Points::Draw ( int frame, Camera3D* cam, float rad )
{
	
	// Render points	
	PERF_PUSH ("draw");

	selfStartDraw3D ( cam );
	//setPreciseEye ( SPNT, cam );
	//setPntParams ( Vec4F(0,1,0,0), Vec4F(0,0,0,0), Vec4F(1,1,1,0), Vec4F(0,0,0,0) );	// enable x=extra, y=clr
	checkGL ( "start pnt shader" );
		
	glEnableVertexAttribArray ( 0 );
	glEnableVertexAttribArray ( 1 );
	glDisableVertexAttribArray ( 2 );
	//glEnableVertexAttribArray ( 2 );

	glBindBuffer ( GL_ARRAY_BUFFER, m_Points.glid(FPOS) );	
	glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3F), 0 );			
	checkGL ( "bind pos" ); 
		
	glBindBuffer ( GL_ARRAY_BUFFER, m_Points.glid(FCLR) );
	glVertexAttribPointer ( 1, 1, GL_FLOAT, GL_FALSE, sizeof(uint), 0 );
	checkGL ( "bind clr" );

	//glBindBuffer ( GL_ARRAY_BUFFER, m_Points.glid(FVEL) );
	//glVertexAttribPointer ( 2, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3F), 0 );
		
	glDrawArrays ( GL_POINTS, 0, getNumPoints() );
	checkGL ( "draw pnts" );
		
	selfEndDraw3D ();
	checkGL ( "end pnt shader" );

	PERF_POP();
}


//---------------------------------------------------------------- Advection 

void Points::Advect_Init()
{
	#ifdef USE_CUDA
		LoadKernel ( FUNC_ADVANCE,			"advanceParticles" );
		LoadKernel ( FUNC_GRAVITY,			"forceGravity" );
	#endif

	// Advection variables
	int maxpnt = mMaxPoints;
	char memflags = m_bGPU ? DT_CUMEM : DT_CPU;	
	m_Points.AddBuffer ( FVEVAL, "veval",	sizeof(Vec3F),	maxpnt, memflags );
	m_Points.AddBuffer ( FFORCE, "force",	sizeof(Vec3F),	maxpnt, memflags );
	m_Points.SetBufferUsage ( FVEVAL, DT_FLOAT3 );
	m_Points.SetBufferUsage ( FFORCE, DT_FLOAT3 );
	m_Points.FillBuffer ( FVEVAL, 0 );
	m_Points.FillBuffer ( FFORCE, 0 );
	m_Points.SetNum ( mNumPoints );

	// Advection params
	m_Params.AL =				40.0f;			// accel limit, m / s^2
	m_Params.VL =				40.0f;			// vel limit, m / s		
	m_Params.AL2 = m_Params.AL * m_Params.AL;
	m_Params.VL2 = m_Params.VL * m_Params.VL;
	m_Params.grav_amt =	1.0f;	
	m_Params.grav_pos.Set ( 0, 0, 0 );
	m_Params.grav_dir.Set ( 0, -9.8f, 0 );
	m_Params.gravity = m_Params.grav_dir * m_Params.grav_amt;
	
	// domain params
	m_Params.bound_friction =	.01f;			// ground friction	
	m_Params.bound_wall_force = 0.0f;			
	m_Params.bound_wall_freq = 0.0f;
	m_Params.bound_slope = 0.0f; 
	m_Params.bound_stiff =		2.0; // 10.0	// higher stiff causes faster flow but more bouncing 
	m_Params.bound_damp =		100.0; 
}

void Points::Run_GravityForce (float factor)
{
	if ( m_bGPU ) {	
		#ifdef USE_CUDA	
		void* args[2] = { &m_Params.pnum, &factor };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_GRAVITY],  m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL), "Points::Run_GravityForce", "cuLaunch", "FUNC_GRAVITY", m_bDebug);
		#endif
	}
}

void Points::Map()
{
	if (m_Points.Map(FPOS)) {
		m_Points.Map(FVEL);
		m_Points.Map(FCLR);
	}
}
void Points::Unmap()
{
	if (m_Points.Unmap(FPOS)) {
		m_Points.Unmap(FVEL);
		m_Points.Unmap(FCLR);
	}
}


void Points::Run_Advect ()
{	
	if (mNumPoints==0) return;

	PERF_PUSH ("advance"); 

	if ( m_bGPU ) {	

		#ifdef USE_CUDA	

		if (m_Points.Map(FPOS)) {			// cuda interop if needed
			m_Points.Map(FVEL);
			m_Points.UpdateGPUAccess();
		}

		void* args[4] = { &m_Params.time, &m_Params.dt, &m_Params.sim_scale, &m_Params.pnum };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_ADVANCE],  m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL), "Points::Run_Advect", "cuLaunch", "FUNC_ADVANCE", m_bDebug);
		#endif

		if (m_Points.Unmap(FPOS)) {
			m_Points.Unmap(FVEL);
		}

		//---- debugging
		/* m_Points.RetrieveAll();
		Vec3F p;
		dbgprintf("-----\n");
		for (int n = 0; n < 10; n++) {
			p = m_Points.bufF3(FPOS)[n];
			dbgprintf("%d, %f %f %f\n", n, p.x, p.y, p.z);
		} */

	} else {						

		Vec3F norm, z;
		Vec3F dir, accel;
		Vec3F vnext;
		Vec3F bmin, bmax;
		Vec4F clr;
		float adj;
		float AL, AL2, SL, SL2, ss, radius;
		float stiff, damp, speed, diff; 
	
		AL = m_Params.AL;	AL2 = AL*AL;
		SL = m_Params.VL;	SL2 = SL*SL;
	
		stiff = m_Params.bound_stiff;
		damp = m_Params.bound_damp;
		radius = m_Params.pradius;
		bmin = m_Params.gridMin;
		bmax = m_Params.gridMax;
		ss = m_Params.sim_scale;

		// Get particle buffers
		Vec3F*	ppos =		m_Points.bufF3(FPOS);
		Vec3F*	pvel =		m_Points.bufF3(FVEL);
		Vec3F*	pveleval =	m_Points.bufF3(FVEVAL);
		Vec3F*	pforce =	m_Points.bufF3(FFORCE);
		uint*		pclr =		m_Points.bufUI(FCLR);
		float*		ppress =	m_Points.bufF(FPRESS);	

		// Advance each particle
		for ( int n=0; n < getNumPoints(); n++ ) {

			if ( *m_Points.bufI(FGCELL,n) == GRID_UNDEF) continue;

			// Compute Acceleration		
			accel = *pforce;
			accel *= m_Params.pmass;
	
			// Boundary Conditions
			// Y-axis walls
			diff = radius - ( ppos->y - (bmin.y+ (ppos->x-bmin.x)*m_Params.bound_slope) )*ss;
			if (diff > EPSILON ) {			
				norm.Set ( -m_Params.bound_slope, 1.0f - m_Params.bound_slope, 0 );
				adj = stiff * diff - damp * (float) norm.Dot ( *pveleval );
				accel.x += adj * norm.x; accel.y += adj * norm.y; accel.z += adj * norm.z;
			}		
			diff = radius - ( bmax.y - ppos->y )*ss;
			if (diff > EPSILON) {
				norm.Set ( 0, -1, 0 );
				adj = stiff * diff - damp * (float) norm.Dot ( *pveleval );
				accel.x += adj * norm.x; accel.y += adj * norm.y; accel.z += adj * norm.z;
			}		
		
			// X-axis walls
			diff = radius - ( ppos->x - (bmin.x + (sin(m_Params.time * m_Params.bound_wall_freq)+1)*0.5f * m_Params.bound_wall_force) )*ss;	
			//diff = 2 * radius - ( p->pos.x - min.x + (sin(m_Time*10.0)-1) * m_Param[FORCE_XMIN_SIN] )*ss;	
			if (diff > EPSILON ) {
				norm.Set ( 1.0, 0, 0 );
				adj = stiff * diff - damp * (float) norm.Dot ( *pveleval ) ;
				accel.x += adj * norm.x; accel.y += adj * norm.y; accel.z += adj * norm.z;					
			}

			diff = radius - ( (bmax.x - (sin(m_Params.time *m_Params.bound_wall_freq)+1)*0.5f* m_Params.bound_wall_force) - ppos->x )*ss;	
			if (diff > EPSILON) {
				norm.Set ( -1, 0, 0 );
				adj = stiff * diff - damp * (float) norm.Dot ( *pveleval );
				accel.x += adj * norm.x; accel.y += adj * norm.y; accel.z += adj * norm.z;
			}

			// Z-axis walls
			diff = radius - ( ppos->z - bmin.z )*ss;			
			if (diff > EPSILON) {
				norm.Set ( 0, 0, 1 );
				adj = stiff * diff - damp * (float) norm.Dot ( *pveleval );
				accel.x += adj * norm.x; accel.y += adj * norm.y; accel.z += adj * norm.z;
			}
			diff = radius - ( bmax.z - ppos->z )*ss;
			if (diff > EPSILON) {
				norm.Set ( 0, 0, -1 );
				adj = stiff * diff - damp * (float) norm.Dot ( *pveleval );
				accel.x += adj * norm.x; accel.y += adj * norm.y; accel.z += adj * norm.z;
			}

			// Plane gravity
			accel += m_Params.gravity;

			// Point gravity
			if ( m_Params.grav_pos.x > 0 && m_Params.grav_amt > 0 ) {
				norm.x = ( ppos->x - m_Params.grav_pos.x );
				norm.y = ( ppos->y - m_Params.grav_pos.y );
				norm.z = ( ppos->z - m_Params.grav_pos.z );
				norm.Normalize ();
				norm *= m_Params.grav_amt;
				accel -= norm;
			}

			// Acceleration limiting 
			speed = accel.x*accel.x + accel.y*accel.y + accel.z*accel.z;
			if ( speed > AL2 ) {
				accel *= AL / sqrt(speed);
			}		

			// Velocity limiting 
			speed = pvel->x*pvel->x + pvel->y*pvel->y + pvel->z*pvel->z;
			if ( speed > SL2 ) {
				speed = SL2;
				(*pvel) *= SL / sqrt(speed);
			}		

			// Vertlet integration
			Vec3F vhalf = *pvel + accel * m_Params.dt * 0.5f;	// v(t+1/2) = v(t) + 1/2*a(t)*dt				
			*ppos += vhalf * (m_Params.dt/ss);								// p(t+1) = p(t) + v(t+1/2) dt		
			*pvel = vhalf;
			*pveleval = (*pvel + vhalf) * 0.5f;								// v(t+1) = [v(t-1/2) + v(t+1/2)] * 0.5			
		
			ppos++;
			pvel++;
			pveleval++;
			pforce++;			
			ppress++;
			pclr++;
		}
	}

	PERF_POP();

	AdvanceTime ();

}

void Points::AdvanceTime ()
{
	m_Params.time += m_Params.dt;
	m_Frame++;
}


//-------------------------------------------------------------------- Acceleration Structures

// AccelInitialize
// - Caller must have already created a CUDA context 
void Points::Accel_Init ()
{
	m_ACCL = true;

	m_Params.example =		2;	

	#ifdef USE_CUDA
		// Acceleration kernels
		LoadKernel ( FUNC_INSERT,			"insertParticles" );
		LoadKernel ( FUNC_COUNTING_SORT,	"countingSortFull" );
		LoadKernel ( FUNC_FPREFIXSUM,		"prefixSum" );
		LoadKernel ( FUNC_FPREFIXFIXUP,		"prefixFixup" );
	
		m_PointsTemp.AssignToGPU ( "FPntTmp", m_Module );
		m_Accel.AssignToGPU ( "FAccel", m_Module );	
	#endif

	char memflags = m_bGPU ? DT_CUMEM : DT_CPU;	

	if ( !m_Points.hasBuf(FGCELL) ) {
		int maxpnt = mMaxPoints;
		m_Points.AddBuffer ( FGCELL, "gcell",	sizeof(int),		maxpnt, memflags );
		m_Points.AddBuffer ( FGNDX,  "gndx",	sizeof(int),		maxpnt, memflags );
		m_Points.SetBufferUsage ( FGCELL, DT_INT );
		m_Points.SetBufferUsage ( FGNDX, DT_INT );
	}

	if (m_ACCL) m_PointsTemp.MatchAllBuffers ( &m_Points, memflags );
}

// Ideal grid cell size (gs) = 2 * smoothing radius = 0.02*2 = 0.04
// Ideal domain size = k * gs / d = k*0.02*2/0.005 = k*8 = {8, 16, 24, 32, 40, 48, ..}
//    (k = number of cells, gs = cell size, d = simulation scale)
//
void Points::Accel_RebuildGrid ()
{
	// Required params: grid_spacing, psmoothradius, sim_scale

	// Grid size - cell spacing in SPH units
	m_Params.grid_size = m_Params.grid_spacing * m_Params.psmoothradius;	
	
	float grid_size = m_Params.grid_size;
	float world_cellsize = grid_size / m_Params.sim_scale;		// cell spacing in world units
	float sim_scale = m_Params.sim_scale;

	// Grid bounds - one cell beyond fluid domain
	m_Params.gridMin = m_Params.bound_min;		m_Params.gridMin -= float(4.0 * world_cellsize);
	m_Params.gridMax = m_Params.bound_max;		m_Params.gridMax += float(4.0 * world_cellsize);
	m_Params.gridSize = m_Params.gridMax - m_Params.gridMin;		

	// Grid res - grid volume uniformly sub-divided by grid size
	m_Params.gridRes.x = (int) ceil ( m_Params.gridSize.x / world_cellsize );		// Determine grid resolution
	m_Params.gridRes.y = (int) ceil ( m_Params.gridSize.y / world_cellsize );
	m_Params.gridRes.z = (int) ceil ( m_Params.gridSize.z / world_cellsize );
	m_Params.gridSize.x = m_Params.gridRes.x * world_cellsize;						// Adjust grid size to multiple of cell size
	m_Params.gridSize.y = m_Params.gridRes.y * world_cellsize;
	m_Params.gridSize.z = m_Params.gridRes.z * world_cellsize;	
	m_Params.gridDelta = Vec3F(m_Params.gridRes) / m_Params.gridSize;		// delta = translate from world space to cell #	
	
	// Grid total - total number of grid cells
	m_Params.gridTotal = (int) (m_Params.gridRes.x * m_Params.gridRes.y * m_Params.gridRes.z);

	// Number of cells to search:
	// n = (2r / w) +1,  where n = 1D cell search count, r = search radius, w = world cell width
	//
	m_Params.gridSrch = (int) (floor(2.0f * m_Params.psmoothradius / grid_size) + 1.0f);
	if ( m_Params.gridSrch < 2 ) m_Params.gridSrch = 2;
	dbgprintf ( "  Grid srch: %d\n", m_Params.gridSrch );
	
	m_Params.gridAdjCnt = m_Params.gridSrch * m_Params.gridSrch * m_Params.gridSrch;
	m_Params.gridScanMax = m_Params.gridRes - Vec3I( m_Params.gridSrch, m_Params.gridSrch, m_Params.gridSrch );

	if ( m_Params.gridSrch > 6 ) {
		dbgprintf ( "ERROR: Neighbor search is n > 6. \n " );
		exit(-1);
	}

	// Grid thread blocks
	int threadsPerBlock = 512;
	int cnt = m_Params.gridTotal;
	m_Params.szGrid = (m_Params.gridBlocks * m_Params.gridThreads);
	computeNumBlocks ( m_Params.gridTotal, threadsPerBlock, m_Params.gridBlocks, m_Params.gridThreads);			// grid blocks

	// Auxiliary buffers - prefix sums sizes
	int blockSize = SCAN_BLOCKSIZE << 1;
	int numElem1 = m_Params.gridTotal;
	int numElem2 = int ( numElem1 / blockSize ) + 1;
	int numElem3 = int ( numElem2 / blockSize ) + 1;	

	char memflags = m_bGPU ? DT_CUMEM : DT_CPU;	
	
	// Allocate acceleration
	m_Accel.DeleteAllBuffers ();
	m_Accel.AddBuffer ( AGRID,		"grid",		sizeof(uint), mMaxPoints,			memflags );
	m_Accel.AddBuffer ( AGRIDCNT,	"gridcnt",	sizeof(uint), m_Params.gridTotal,	memflags );
	m_Accel.AddBuffer ( AGRIDOFF,	"gridoff",	sizeof(uint), m_Params.gridTotal,	memflags );
	m_Accel.AddBuffer ( AAUXARRAY1, "aux1",		sizeof(uint), numElem2,				memflags );
	m_Accel.AddBuffer ( AAUXSCAN1,  "scan1",	sizeof(uint), numElem2,				memflags );
	m_Accel.AddBuffer ( AAUXARRAY2, "aux2",		sizeof(uint), numElem3,				memflags );
	m_Accel.AddBuffer ( AAUXSCAN2,  "scan2",	sizeof(uint), numElem3,				memflags );

	for (int b=0; b <= AAUXSCAN2; b++)
		m_Accel.SetBufferUsage ( b, DT_UINT );		// for debugging

	// Grid adjacency lookup - stride to access neighboring cells in all 6 directions
	int cell = 0;
	for (int y=0; y < m_Params.gridSrch; y++ ) 
		for (int z=0; z < m_Params.gridSrch; z++ ) 
			for (int x=0; x < m_Params.gridSrch; x++ ) 
				m_Params.gridAdj [ cell++]  = ( y * m_Params.gridRes.z+ z )*m_Params.gridRes.x +  x ;			

	// Update gpu access
	UpdateGPUAccess();	
	if (m_SPH) SPH_UpdateParams ();

	// Done
	dbgprintf ( "  Accel Grid: %d, t:%dx%d=%d, bufGrid:%d, Res: %dx%dx%d\n", m_Params.gridTotal, m_Params.gridBlocks, m_Params.gridThreads, m_Params.gridBlocks*m_Params.gridThreads, m_Params.szGrid, (int) m_Params.gridRes.x, (int) m_Params.gridRes.y, (int) m_Params.gridRes.z );		
}


void Points::Accel_InsertParticles ()
{
	if ( m_bGPU ) {
		#ifdef USE_CUDA
		// Reset all grid cells to empty	
		cuCheck ( cuMemsetD8 ( m_Accel.gpu(AGRIDCNT), 0,	m_Params.gridTotal*sizeof(uint) ), "InsertParticlesCUDA", "cuMemsetD8", "AGRIDCNT", m_bDebug );
		cuCheck ( cuMemsetD8 ( m_Accel.gpu(AGRIDOFF), 0,	m_Params.gridTotal*sizeof(uint) ), "InsertParticlesCUDA", "cuMemsetD8", "AGRIDOFF", m_bDebug );
		cuCheck ( cuMemsetD8 ( m_Points.gpu(FGCELL), 0,		mNumPoints*sizeof(int) ), "InsertParticlesCUDA", "cuMemsetD8", "FGCELL", m_bDebug );

		Map();
		void* args[1] = { &mNumPoints };
		cuCheck(cuLaunchKernel(m_Func[FUNC_INSERT], m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL),
			"InsertParticlesCUDA", "cuLaunch", "FUNC_INSERT", m_bDebug);
		Unmap();

		#endif

	} else {
		// Reset all grid cells to empty	
		memset( m_Accel.bufUI(AGRIDCNT),	0,	m_Params.gridTotal*sizeof(uint));
		memset( m_Accel.bufUI(AGRIDOFF),	0,	m_Params.gridTotal*sizeof(uint));
		memset( m_Points.bufUI(FGCELL),		0,	mNumPoints*sizeof(int));
		memset( m_Points.bufUI(FGNDX),		0,	mNumPoints*sizeof(int));

		float poff = m_Params.psmoothradius / m_Params.sim_scale;

		// Insert each particle into spatial grid
		Vec3F gcf;
		Vec3I gc;
		int gs; 
		Vec3F*	ppos =		m_Points.bufF3(FPOS);		
		uint*		pgcell =	m_Points.bufUI(FGCELL);
		uint*		pgndx =		m_Points.bufUI(FGNDX);		

		for ( int n=0; n < getNumPoints(); n++ ) {		
		
			gcf = (*ppos - m_Params.gridMin) * m_Params.gridDelta; 
			gc = Vec3I( int(gcf.x), int(gcf.y), int(gcf.z) );
			gs = (gc.y * m_Params.gridRes.z + gc.z)*m_Params.gridRes.x + gc.x;
	
			if ( gc.x >= 1 && gc.x <= m_Params.gridScanMax.x && gc.y >= 1 && gc.y <= m_Params.gridScanMax.y && gc.z >= 1 && gc.z <= m_Params.gridScanMax.z ) {
				*pgcell = gs;
				(*m_Accel.bufUI(AGRIDCNT, gs))++;
				*pgndx = *m_Accel.bufUI(AGRIDCNT, gs);	
			} else {
				*pgcell = GRID_UNDEF;				
			}			
			ppos++;			
			pgcell++;
			pgndx++;
		}

		// debugging
		/* pgcell =	m_Points.bufUI(FGCELL);
		for (int n=0; n < 10; n++) 
			dbgprintf ( "%d: %d\n", n, *pgcell++ ); */
	}
}


void Points::Accel_PrefixScanParticles ()
{
	if ( m_bGPU ) {

		#ifdef USE_CUDA
		// Prefix Sum - determine grid offsets
		int blockSize = SCAN_BLOCKSIZE << 1;
		int numElem1 = m_Params.gridTotal;		
		int numElem2 = int ( numElem1 / blockSize ) + 1;
		int numElem3 = int ( numElem2 / blockSize ) + 1;
		int threads = SCAN_BLOCKSIZE;
		int zero_offsets = 1;
		int zon = 1;

		CUdeviceptr array1  = m_Accel.gpu(AGRIDCNT);		// input
		CUdeviceptr scan1   = m_Accel.gpu(AGRIDOFF);		// output
		CUdeviceptr array2  = m_Accel.gpu(AAUXARRAY1);
		CUdeviceptr scan2   = m_Accel.gpu(AAUXSCAN1);
		CUdeviceptr array3  = m_Accel.gpu(AAUXARRAY2);
		CUdeviceptr scan3   = m_Accel.gpu(AAUXSCAN2);

		if ( numElem1 > SCAN_BLOCKSIZE*xlong(SCAN_BLOCKSIZE)*SCAN_BLOCKSIZE) {
			dbgprintf ( "ERROR: Number of elements exceeds prefix sum max. Adjust SCAN_BLOCKSIZE.\n" );
		}

		void* argsA[5] = {&array1, &scan1, &array2, &numElem1, &zero_offsets }; // sum array1. output -> scan1, array2
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_FPREFIXSUM], numElem2, 1, 1, threads, 1, 1, 0, NULL, argsA, NULL ), "PrefixSumCellsCUDA", "cuLaunch", "FUNC_PREFIXSUM:A", m_bDebug);

		void* argsB[5] = { &array2, &scan2, &array3, &numElem2, &zon }; // sum array2. output -> scan2, array3
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_FPREFIXSUM], numElem3, 1, 1, threads, 1, 1, 0, NULL, argsB, NULL ), "PrefixSumCellsCUDA", "cuLaunch", "FUNC_PREFIXSUM:B", m_bDebug);

		if ( numElem3 > 1 ) {
			CUdeviceptr nptr = {0};
			void* argsC[5] = { &array3, &scan3, &nptr, &numElem3, &zon };	// sum array3. output -> scan3
			cuCheck ( cuLaunchKernel ( m_Func[FUNC_FPREFIXSUM], 1, 1, 1, threads, 1, 1, 0, NULL, argsC, NULL ), "PrefixSumCellsCUDA", "cuLaunch", "FUNC_PREFIXFIXUP:C", m_bDebug);

			void* argsD[3] = { &scan2, &scan3, &numElem2 };	// merge scan3 into scan2. output -> scan2
			cuCheck ( cuLaunchKernel ( m_Func[FUNC_FPREFIXFIXUP], numElem3, 1, 1, threads, 1, 1, 0, NULL, argsD, NULL ), "PrefixSumCellsCUDA", "cuLaunch", "FUNC_PREFIXFIXUP:D", m_bDebug);
		}

		void* argsE[3] = { &scan1, &scan2, &numElem1 };		// merge scan2 into scan1. output -> scan1
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_FPREFIXFIXUP], numElem2, 1, 1, threads, 1, 1, 0, NULL, argsE, NULL ), "PrefixSumCellsCUDA", "cuLaunch", "FUNC_PREFIXFIXUP:E", m_bDebug);
		#endif
	} else {
		
		// CPU prefix scan

		int numCells = m_Params.gridTotal;		
		uint* mgcnt = m_Accel.bufUI(AGRIDCNT);
		uint* mgoff = m_Accel.bufUI(AGRIDOFF);
		int sum = 0;
		for (int n=0; n < numCells; n++) {
			mgoff[n] = sum;
			sum += mgcnt[n];
		}		
	}

}

void Points::Accel_CountingSort ()
{
	if ( m_bGPU ) {
		#ifdef USE_CUDA

		// Transfer particle data to temp buffers 
		//  (required by algorithm, gpu-to-gpu copy, no sync needed)	
		CopyAllToTemp ();

		Map();
		
		void* args[1] = { &mNumPoints };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_COUNTING_SORT], m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL),
					"CountingSortFullCUDA", "cuLaunch", "FUNC_COUNTING_SORT", m_bDebug );
		Unmap();

		#endif
	} else {



	}
}

void Points::Run_Accel ()
{
	if ( mNumPoints==0 ) return;

	PERF_PUSH ("accel_insert");		Accel_InsertParticles ();		PERF_POP();
	PERF_PUSH ("accel_prefix");		Accel_PrefixScanParticles();	PERF_POP();
	PERF_PUSH ("accel_count");		Accel_CountingSort ();			PERF_POP();					
}

//--------------------------------------------------- Fire Sim

void Points::Fire_Init ( Points* pntsB )
{
	m_FIRE = true;

	#ifdef USE_CUDA
		LoadKernel ( FUNC_SPREAD_FIRE,			"spreadFire" );
		LoadKernel ( FUNC_ADVECT_FIRE,			"advectFire" );		
	#endif

	// Fire variables
	int maxpnt = mMaxPoints;	
	m_Points.AddBuffer ( FTYPE, "type",	sizeof(uchar),	maxpnt, DT_CPU | DT_CUMEM );		// E = ember, A = ash, F = flame
	m_Points.AddBuffer ( FFUEL, "fuel",	sizeof(float),	maxpnt, DT_CPU | DT_CUMEM );
	m_Points.SetBufferUsage ( FTYPE, DT_UCHAR );
	m_Points.SetBufferUsage ( FFUEL, DT_FLOAT );
	m_Points.SetNum ( mNumPoints );
	
	m_Points.FillBuffer ( FTYPE, 'I' );		// initialize as embers
	float* fuel = m_Points.bufF(FFUEL);
	for (int n=0; n < mNumPoints; n++) {
		*fuel++ = 1.0;						// initialize fuel to 1.0
	}
	m_Points.Commit ( FFUEL );

}

void Points::Run_Fire ( Points* pntsB, float time )
{
	float fire_force = -0.0001;

	if ( m_bGPU ) {	
		#ifdef USE_CUDA			

		float radius = 0.6;		

		// copy acceleration info from accelB
		// *NOTE* THIS IS A BAD WORKAROUND. IN FUTURE, MUST PUT FParams INTO A DATAX BUFFER
		m_Params.gridMin = pntsB->m_Params.gridMin;
		m_Params.gridMax = pntsB->m_Params.gridMax;
		m_Params.gridRes = pntsB->m_Params.gridRes;
		m_Params.gridSize = pntsB->m_Params.gridSize;
		m_Params.gridDelta = pntsB->m_Params.gridDelta;
		m_Params.gridScanMax = pntsB->m_Params.gridScanMax;
		m_Params.d2 = pntsB->m_Params.d2;
		for (int n=0; n <64; n++) m_Params.gridAdj[n] = pntsB->m_Params.gridAdj[n];
		m_Params.gridAdjCnt = pntsB->m_Params.gridAdjCnt;
		CommitParams();

		// Sort source particles
		pntsB->Accel_InsertParticles ();
		pntsB->Accel_PrefixScanParticles();
		pntsB->Accel_CountingSort ();		

		// Advect fire		
		cuDataX pnts_a = m_Points.getGPUData();			
		void* argsA[6] = { &pnts_a, &time, &m_Params.dt, &m_Params.sim_scale, &m_Params.pnum, &fire_force };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_ADVECT_FIRE],  m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, argsA, NULL), "AdvectFire", "cuLaunch", "FUNC_ADVECT_FIRE", m_bDebug);

		// Spread fire				
		cuDataX pnts_b = pntsB->m_Points.getGPUData();
		cuDataX accel_b = pntsB->m_Accel.getGPUData();
		float RD2 = radius*radius; // m_Params.d2;
		void* argsS[5] = { &pnts_a, &pnts_b, &accel_b, &m_Params.pnum, &RD2 };		
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_SPREAD_FIRE], m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, argsS, NULL), "SpreadFire", "cuLaunch", "FUNC_SPREAD_FIRE", m_bDebug);		

		// fire spread rate		
		if ( time - m_lastfire > 0.1 ) {
			m_lastfire = time;

			// Create new embers
			m_Points.RetrieveAll ();								// bring back fire points as were about to make more on CPU
			pntsB->m_Points.Retrieve(FPOS);							// bring back source points so we can query them
			pntsB->m_Points.Retrieve(FCLR);
			Vec3F* pos = pntsB->m_Points.bufF3(FPOS);
			uint* clr = pntsB->m_Points.bufUI(FCLR);
			uchar tag = 0xFE;
			uchar complete = 0xFD;
			int n, i;
			float v;				
			for (n=0; n < pntsB->getNumPoints(); n++) {					// scan thru source points
				if ( (*clr >> 24) == tag ) {							// identify those tagged by spread func
					i = AddPointRandom ( *pos, COLORA(1,1,0,1) );		// add new ember at that location
					m_Points.SetElemChar(FTYPE, i, 'I');
					m_Points.SetElemFloat(FFUEL, i, 1.0);
					v = 255.0*m_rand.randF(0, 0.5);
					*clr = CLRA(v, v, v, complete );					// disable source point	
				}
				pos++;
				clr++;
			}					
			pntsB->m_Points.Commit (FCLR); 
			m_Points.SetNum ( mNumPoints );							// update number of active points
			m_Points.CommitAll ();									// send new fire points to GPU
		
			// Create new flames
			pos = m_Points.bufF3(FPOS);
			clr = m_Points.bufUI(FCLR);		
			char* typ = m_Points.bufC(FTYPE);			
			for (n=0; n < getNumPoints(); n++) {
				if ( *typ=='E' ) {
					i = AddPointRandom ( *pos, COLORA(1,1,0,1) );	// add new ember at that location
					m_Points.SetElemChar(FTYPE, i, 'F');
					m_Points.SetElemFloat(FFUEL, i, 1.0);				
				}
				pos++;
				typ++;
				clr++;
			}

			// restart fire to update embers & flames
			Restart (false);							// dont update static device symbols			
		}

			


		// debug to check everything		
		/*m_DebugPnts.clear ();
		pos = m_Points.bufF3(FPOS);
		clr = m_Points.bufI(FCLR);		
		char* typ = m_Points.bufC(FTYPE);
		float* fuel = m_Points.bufF(FFUEL );
		Vec4F cnt(0,0,0,0);
		int c = 0;
		for (int n=0; n < getNumPoints(); n++) {
			switch (*typ) {
			case 'I': cnt.x++; c=COLORA(1,1,1,1);		break;
			case 'E': cnt.y++; c=COLORA(1,*fuel,0,1);	break;
			case 'A': cnt.z++; c=COLORA(0.5,0.5,0.5,1); break;
			case 'F': cnt.w++; c=COLORA(1,0,0,1);		break;
			};
 			m_DebugPnts.push_back ( Shape(*pos, c) );
			pos++; clr++; typ++; fuel++;
		}
		dbgprintf ( " Fire: total=%d, max=%d, ignite=%d, ember=%d, ash=%d, flame=%d\n", getNumPoints(), mMaxPoints, int(cnt.x), int(cnt.y), int(cnt.z), int(cnt.w) );	 */
		
		#endif
	}	
}

//--------------------------------------------------- Radii & Rigid

void Points::AddChannel ( int id, std::string name, char dt )
{
	int maxpnt = mMaxPoints;
	char memflags = m_bGPU ? (DT_CPU | DT_CUMEM) : DT_CPU;	
	int sz = getTypeSize( dt );
	m_Points.AddBuffer( id, name, sz, maxpnt, memflags);	
	m_Points.SetBufferUsage( id, dt);
	if (m_ACCL) m_PointsTemp.MatchAllBuffers(&m_Points, memflags);
}

int Points::FindChannel(std::string name)
{
	return m_Points.FindBuffer ( name );
}

void Points::SetChannelData (int id, int first, int last, float r)
{
	uchar dt = m_Points.getUsage(id);
	//if (m_Points.getUsage( ))
	float* pr = m_Points.bufF( id ) + first;

	for (int i = first; i <= last; i++) {
		*pr++ = r;
	}
}


//--------------------------------------------------- Smoothed Particle Hydrodynamics (SPH)

void Points::SPH_Init ()
{
	char memflags = m_bGPU ? DT_CUMEM : DT_CPU;	
	int maxpnt = mMaxPoints;		

	m_SPH = true;

	if ( !m_Points.hasBuf(FPRESS) ) {
		// sph kernels
		LoadKernel ( FUNC_COMPUTE_PRESS,	"computePressure" );
		LoadKernel ( FUNC_COMPUTE_FORCE,	"computeForce" );	
		

		m_Points.AddBuffer ( FPRESS, "press",	sizeof(float),		maxpnt, memflags );		
		m_Points.SetBufferUsage ( FPRESS, DT_FLOAT );		
		m_Points.FillBuffer ( FPRESS, 0 );		
	}	

	SPH_SetupParams ();

	SPH_UpdateParams ();
}



void Points::SPH_SetupParams ()
{
	//  Range = +/- 10.0 * 0.006 (r) =	   0.12			m (= 120 mm = 4.7 inch)
	//  Container Volume (Vc) =			   0.001728		m^3
	//  Rest Density (D) =				1000.0			kg / m^3
	//  Particle Mass (Pm) =			   0.00020543	kg						(mass = vol * density)
	//  Number of Particles (N) =		4000.0
	//  Water Mass (M) =				   0.821		kg (= 821 grams)
	//  Water Volume (V) =				   0.000821     m^3 (= 3.4 cups, .21 gals)
	//  Smoothing Radius (R) =             0.02			m (= 20 mm = ~3/4 inch)
	//  Particle Radius (Pr) =			   0.00366		m (= 4 mm  = ~1/8 inch)
	//  Particle Volume (Pv) =			   2.054e-7		m^3	(= .268 milliliters)
	//  Rest Distance (Pd) =			   0.0059		m
	//
	//  Given: D, Pm, N
	//    Pv = Pm / D			0.00020543 kg / 1000 kg/m^3 = 2.054e-7 m^3	
	//    Pv = 4/3*pi*Pr^3    cuberoot( 2.054e-7 m^3 * 3/(4pi) ) = 0.00366 m
	//     M = Pm * N			0.00020543 kg * 4000.0 = 0.821 kg		
	//     V =  M / D              0.821 kg / 1000 kg/m^3 = 0.000821 m^3
	//     V = Pv * N			 2.054e-7 m^3 * 4000 = 0.000821 m^3
	//    Pd = cuberoot(Pm/D)    cuberoot(0.00020543/1000) = 0.0059 m 
	//

	// "The viscosity coefficient is the dynamic viscosity, visc > 0 (units Pa.s), 
	// and to include a reasonable damping contribution, it should be chosen 
	// to be approximately a factor larger than any physical correct viscosity 
	// coefficient that can be looked up in the literature. However, care should 
	// be taken not to exaggerate the viscosity coefficient for fluid materials.
	// If the contribution of the viscosity force density is too large, the net effect 
	// of the viscosity term will introduce energy into the system, rather than 
	// draining the system from energy as intended."
	//    Actual visocity of water = 0.001 Pa.s    // viscosity of water at 20 deg C.

	m_Params.time =				0;	
	m_Params.prest_dens =		600.0f;		    // kg / m^3		
	m_Params.pmass =			0.00020543;		// kg
	m_Params.pdist =			0.0059f;		// m			
	m_Params.pvisc =			0.005f;			// viscosity term. pascal-second (Pa.s) = 1 kg m^-1 s^-1  (see wikipedia page on viscosity)
	m_Params.iterm =			0.01f;			// internal force term. usually 1.0. together iterm and pvisc weight the total SPH forces	
	m_Params.pradius =			0.01f;			// collision radius
	m_Params.pintstiff =		0.5f;			// increases inter-particle forcing (if increased, lower iterm)						

	m_Params.init_min.Set (   0,  0,   0 );
	m_Params.init_max.Set ( 500, 100, 500 );	
}

void Points::SPH_UpdateParams ()
{
	float sr = m_Params.psmoothradius;
	
	m_Params.pdist = pow ( m_Params.pmass / m_Params.prest_dens, 1/3.0f );
	m_Params.poly6kern = 315.0f / (64.0f * 3.141592f * pow( sr, 9.0f) );
	m_Params.spikykern = -45.0f / (3.141592f * pow( sr, 6.0f) );
	m_Params.lapkern = 45.0f / (3.141592f * pow( sr, 6.0f) );	
	m_Params.gausskern = 1.0f / pow(3.141592f * 2.0f*sr*sr, 3.0f/2.0f);
	m_Params.vterm = m_Params.lapkern * m_Params.pvisc;
}


// Compute SPH Pressures - Using spatial grid, and also create neighbor table
void Points::SPH_ComputePressure ()
{
	if ( m_bGPU ) {
		#ifdef USE_CUDA
		void* args[1] = { &mNumPoints };

		Map();
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_COMPUTE_PRESS],  m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL), "ComputePressureCUDA", "cuLaunch", "FUNC_COMPUTE_PRESS", m_bDebug );
		Unmap();
		#endif
	} else {

		int i, j, cnt = 0;	
		float sum, dsq, c;
		float d = m_Params.sim_scale;
		float d2 = d*d;
		float radius = m_Params.psmoothradius / m_Params.psmoothradius;
	
		// Get particle buffers
		Vec3F*	ipos =		m_Points.bufF3(FPOS);		
		float*	ipress =	m_Points.bufF(FPRESS);			

		Vec3F	dst;
		int			nadj = (m_Params.gridRes.z + 1)*m_Params.gridRes.x + 1;
		uint*		m_Grid =	m_Points.bufUI(AGRID);
		uint*		m_GridCnt = m_Points.bufUI(AGRIDCNT);
	
		int nbrcnt = 0;
		int srch = 0;

		for ( i=0; i < getNumPoints(); i++ ) {

			sum = 0.0;

			if ( m_Points.bufI(FGCELL)[i] != GRID_UNDEF ) {
				for (int cell=0; cell < m_Params.gridAdjCnt; cell++) {
					j = m_Grid [   m_Points.bufI(FGCELL)[i] - nadj + m_Params.gridAdj[cell] ] ;
					while ( j != GRID_UNDEF ) {
						//if ( i==j ) { j = *m_Points.bufUI(FGNEXT,j); continue; }
						dst = m_Points.bufF3(FPOS)[j];
						dst -= *ipos;
						dsq = d2*(dst.x*dst.x + dst.y*dst.y + dst.z*dst.z);
						if ( dsq <= m_Params.r2 ) {
							c =  m_Params.r2 - dsq;
							sum += c * c * c;
							nbrcnt++;
							/*nbr = AddNeighbor();			// get memory for new neighbor						
							*(m_NeighborTable + nbr) = j;
							*(m_NeighborDist + nbr) = sqrt(dsq);
							inbr->num++;*/
						}
						srch++;
						//j = m_Points.bufI(FGNEXT)[j];
					}
				}
			}		
			*ipress = sum * m_Params.pmass * m_Params.poly6kern;

			ipos++;		
			ipress++;
		}
	}
}

// Compute SPH Forces
void Points::SPH_ComputeForce ()
{
	if ( m_bGPU ) {
		#ifdef USE_CUDA
		void* args[1] = { &mNumPoints };

		Map();
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_COMPUTE_FORCE],  m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL), "ComputeForceCUDA", "cuLaunch", "FUNC_COMPUTE_FORCE", m_bDebug);
		Unmap();
		#endif
	} else {

		Vec3F force;
		register float pterm, vterm, dterm;
		int i, j;
		float c, d;
		float dx, dy, dz;
		float mR, visc;	

		d = m_Params.sim_scale;
		mR = m_Params.psmoothradius;
		visc = m_Params.pvisc;
	
		// Get particle buffers
		Vec3F*	ipos =		m_Points.bufF3(FPOS);		
		Vec3F*	iveleval =	m_Points.bufF3(FVEVAL);		
		Vec3F*	iforce =	m_Points.bufF3(FFORCE);		
		float*		ipress =	m_Points.bufF(FPRESS);	
	
		Vec3F	jpos;
		float		jdist;		
		Vec3F	jveleval;
		float		dsq;
		float		d2 = d*d;
		int			nadj = (m_Params.gridRes.z + 1)*m_Params.gridRes.x + 1;
		uint* m_Grid =		m_Points.bufUI(AGRID);
		uint* m_GridCnt =	m_Points.bufUI(AGRIDCNT);
	
		float pi, pj;

		for ( i=0; i < getNumPoints(); i++ ) {

			iforce->Set ( 0, 0, 0 );

			if ( *m_Points.bufI(FGCELL,i) != GRID_UNDEF ) {
				for (int cell=0; cell < m_Params.gridAdjCnt; cell++) {
					j = m_Grid [  m_Points.bufI(FGCELL)[i] - nadj + m_Params.gridAdj[cell] ];
					pi = m_Points.bufF(FPRESS)[i];

					while ( j != GRID_UNDEF ) {
						//if ( i==j ) { j = *m_Points.bufUI(FGNEXT,j); continue; }
						jpos = m_Points.bufF3(FPOS)[j];
						dx = ( ipos->x - jpos.x);		// dist in cm
						dy = ( ipos->y - jpos.y);
						dz = ( ipos->z - jpos.z);
						dsq = d2*(dx*dx + dy*dy + dz*dz);
						if ( dsq <= m_Params.r2 ) {

							jdist = sqrt(dsq);
						
							pj = m_Points.bufF(FPRESS)[j];												
							jveleval = m_Points.bufF3(FVEVAL)[j];
							dx = ( ipos->x - jpos.x);		// dist in cm
							dy = ( ipos->y - jpos.y);
							dz = ( ipos->z - jpos.z);
							c = (mR-jdist);
							pterm = d * -0.5f * c * m_Params.spikykern * ( pi + pj ) / jdist;
							dterm = c / ( m_Points.bufF(FPRESS)[i] *  m_Points.bufF(FPRESS)[j] );
							vterm = m_Params.lapkern * visc;
							iforce->x += ( pterm * dx + vterm * ( jveleval.x - iveleval->x) ) * dterm;
							iforce->y += ( pterm * dy + vterm * ( jveleval.y - iveleval->y) ) * dterm;
							iforce->z += ( pterm * dz + vterm * ( jveleval.z - iveleval->z) ) * dterm;
						}
						//j = m_Points.bufI(FGNEXT,j);
					}
				}
			}
			ipos++;
			iveleval++;
			iforce++;
			ipress++;		
		}
	}
}

void Points::Run_SPH ()
{	
	if ( mNumPoints==0 || !m_SPH ) return;
		
	PERF_PUSH ("sph_press");	SPH_ComputePressure();			PERF_POP();		
	PERF_PUSH ("sph_force");	SPH_ComputeForce ();			PERF_POP(); 
}


//-------------------------------------------------------------- Digital Elevation Models (DEM)

void Points::DEM_Init (Image* img)
{
	m_DEM = true;
	m_Terrain = img;

	#ifdef USE_CUDA
		// DEM
		LoadKernel ( FUNC_FORCE_TERRAIN,	"forceTerrain" );
		LoadKernel ( FUNC_POINTS_TO_DEM,	"pointsToDEM" ); 
		LoadKernel ( FUNC_SMOOTH_DEM,		"smoothDEM" ); 
	#endif	
}

void Points::Run_DEMTerrainForce ()
{	
	if ( m_Terrain==0x0 || mNumPoints==0 || !m_DEM) return;

	if ( m_bGPU ) {	
		#ifdef USE_CUDA	
		int tw = m_Terrain->GetWidth();
		int th = m_Terrain->GetHeight();
		
		m_Terrain->Map();
		m_Points.Map(FPOS);		

		CUtexObject imgR; 
		CUsurfObject imgW;
		CUarray img_gpu = m_Terrain->getArray ( imgR, imgW );

		if ( img_gpu==0 ) {
			dbgprintf ( "ERROR: PointsToDEM, image is not a CUarray on gpu.\n" );
			exit(-2);
		}

		void* args[4] = { &m_Params.pnum, &tw, &th, &imgR };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_FORCE_TERRAIN],  m_Params.numBlocks, 1, 1, m_Params.numThreads, 1, 1, 0, NULL, args, NULL), "ForceTerrain", "cuLaunch", "FUNC_FORCE_TERRAIN", m_bDebug);

		m_Terrain->Unmap();
		m_Points.Unmap(FPOS);
		#endif
	}
}

void Points::DEM_Smooth ( Image* img )
{
	if ( !m_DEM) {dbgprintf("ERROR: DEM not started.\n"); exit(-2); }

	if ( m_bGPU ) {
		#ifdef USE_CUDA	
		int tw = img->GetWidth();
		int th = img->GetHeight();
		CUtexObject imgR; CUsurfObject imgW;
		CUarray img_gpu = img->getArray ( imgR, imgW );
		if ( img_gpu==0 ) {
			dbgprintf ( "ERROR: PointsToDEM, image is not a CUarray on gpu.\n" );
			exit(-2);
		}
		Vec3I numBlocks;
		Vec3I numThreads;
		computeNumBlocks (tw, th, 1, 32, numBlocks, numThreads);

		void* args[5] = { &mNumPoints, &tw, &th, &imgR, &imgW };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_SMOOTH_DEM], numBlocks.x, numBlocks.y, numBlocks.z, numThreads.x, numThreads.y, numThreads.z, 0, NULL, args, NULL), "ParticlesToDEM", "cuLaunch", "FUNC_POINT_TO_DEM", m_bDebug);
		#endif
	}
}

void Points::DEM_PointsToDEM ( Image* img, bool over)
{
	if ( !m_DEM) {dbgprintf("ERROR: DEM not started.\n"); exit(-2); }

	if ( m_bGPU ) {
		#ifdef USE_CUDA	
		PERF_PUSH ("insert");		Accel_InsertParticles ();		PERF_POP();
		PERF_PUSH ("prefix");		Accel_PrefixScanParticles();	PERF_POP();
		PERF_PUSH ("count");		Accel_CountingSort ();			PERF_POP();					
		
		PERF_PUSH ("pnt2dem");

		m_Terrain = img;

		m_Points.Map(FPOS);
		img->Map();					// cuda interop

		int tw = img->GetWidth();
		int th = img->GetHeight();
		CUtexObject imgR; CUsurfObject imgW;
		CUarray img_gpu = img->getArray ( imgR, imgW );
		if ( img_gpu==0 ) {
			dbgprintf ( "ERROR: PointsToDEM, image is not a CUarray on gpu.\n" );
			exit(-2);
		}

		Vec3I numBlocks;
		Vec3I numThreads;
		computeNumBlocks (tw, th, 1, 32, numBlocks, numThreads);

		void* args[6] = { &mNumPoints, &over, &tw, &th, &imgR, &imgW };
		cuCheck ( cuLaunchKernel ( m_Func[FUNC_POINTS_TO_DEM], numBlocks.x, numBlocks.y, numBlocks.z, numThreads.x, numThreads.y, numThreads.z, 0, NULL, args, NULL), "ParticlesToDEM", "cuLaunch", "FUNC_POINT_TO_DEM", m_bDebug);

		img->Unmap();
		m_Points.Unmap(FPOS);

		PERF_POP ();
		#endif
	}

}


