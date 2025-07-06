//-------------------------
// Copyright 2020-2025 (c) Quanta Sciences, Rama Hoetzlein
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//--------------------------

#include "motioncycles.h"
#include "string_helper.h"
#include "camera.h"
#include "main.h"
#include "imagex.h"

#include <stack>
#include <vector>

#define BVH_START		0
#define BVH_HIER		1
#define BVH_MOTION		2
#define BVH_TIMEDATA	3
#define BVH_END		4

#define CHAN_XYZ_POS		0
#define CHAN_ZYX_ROT		1

#define JUNDEF		0xFFFF

MotionCycles::MotionCycles () : Object()
{
	m_TPose = 0x0;
}

MotionCycles::~MotionCycles()
{
	Clear();
}

bool MotionCycles::RunCommand(std::string cmd, vecStrs args) 
{
	if (cmd.compare("tpose")==0)	{ LoadTPose ( args[0] );	return true; }
	if (cmd.compare("load") == 0)	{ LoadBVH ( args[0] );		return true; }
	if (cmd.compare("cycle")==0)	{ SplitCycle ( args[0], (int) m_Cycles.size() - 1, strToI(args[1]), strToI(args[2]) );	return true; }
	
	if (cmd.compare("finish")==0)	{ 
		RemoveAllBVH(); 
		ListCycles();	
		return true;
	}
	return false;
}

// Start loading of motion cycles
void MotionCycles::Define (int x, int y)
{
	AddInput ( "shader",	'Ashd' );	// default shader 
}

void MotionCycles::LoadTPose ( std::string fname )
{
	m_TPose = new Character;
	m_TPose->Define(0, 0);
	m_TPose->CreateOutput('Ashp');
	m_TPose->LoadBVH (fname) ;				// load TPose bvh here
	m_TPose->CopyJoints(JNTS_A, JNTS_B);
}

void MotionCycles::Clear()
{
	Cycle* cy;
	for (int n = 0; n < m_Cycles.size(); n++) {
		cy = &m_Cycles[n];
		if (cy->pos_table != 0x0)		{ free(cy->pos_table); cy->pos_table = 0x0; }
		if (cy->ang_table != 0x0)		{ free(cy->ang_table); cy->ang_table = 0x0; }
		if (cy->orient_table != 0x0)	{ free(cy->orient_table); cy->orient_table = 0x0; }
	}
	m_Cycles.clear();
}

bool sort_func ( Vec3F a, Vec3F b )
{
	return a.z < b.z;
}

int MotionCycles::AddCycle ( std::string name, int nframes, int njoints, int fskip )
{
	// *NOTE*:
	// There are ways to improve memory in future:
	// 1. only store cycle data for output channels, not all joints
	// 2. only store orientation table, not ang & orient
	// 3. compact pos & orient in single table. then index using channel ID

	Cycle cy;
	
	strncpy(cy.name, name.c_str(), 64 );
	strcpy ( cy.group, m_CurrGroup.c_str() );
	cy.frames = nframes / fskip;	
	cy.joints = njoints;
	cy.frameskip = fskip;
	cy.pos_table = (Vec3F*) malloc ( nframes * njoints * sizeof(Vec3F) );
	cy.ang_table = (Vec3F*) malloc ( nframes * njoints * sizeof(Vec3F) );
	cy.orient_table = (Quaternion*)  malloc ( nframes * njoints * sizeof(Quaternion) );	

	// clear to identity as some channels may not be used
	memset(cy.pos_table, 0, nframes * njoints * sizeof(Vec3F) );
	memset(cy.ang_table, 0, nframes * njoints * sizeof(Vec3F) );
	
	cy.vis = 0x0;

	float memuse = nframes * njoints * (sizeof(Vec3F)*2 + sizeof(Matrix4F)) / (1024.f*1024.f);		// NOT CORRECT
	//logf ( "MotionCycle mem used: %fMB\n", memuse );

	m_Cycles.push_back ( cy );
	return (int) m_Cycles.size()-1;
}


int MotionCycles::SplitCycle ( std::string name, int src, int fin, int fout )
{
	int fmax = getCycle(src)->frames;
	
	if ( fout > fmax ) fout = fmax;

	int nframes = fout - fin;
	if ( nframes <= 0 ) {
		dbgprintf ( "WARNING: SplitCycle no frames found. from: %d, to: %d, max: %d\n", fin, fout, fmax );
		return -1;
	}
	int njoints = getCycle(src)->joints;
	int fskip = getCycle(src)->frameskip;
	fin /= fskip;
	fout /= fskip;

	int cyid = AddCycle ( name, nframes, njoints, fskip );
	Cycle* cdest = getCycle(cyid);
	Cycle* csrc = getCycle(src);

	// extract sub-matrix from source cycle
	memcpy ( cdest->pos_table, &csrc->pos_table[fin*njoints], nframes*njoints*sizeof(Vec3F) );
	memcpy ( cdest->ang_table, &csrc->ang_table[fin*njoints], nframes*njoints*sizeof(Vec3F) );
	memcpy ( cdest->orient_table, &csrc->orient_table[fin*njoints], nframes*njoints*sizeof(Quaternion) );

	// generation visualization
	GenerateVis ( cyid );

	return cyid;
}

void MotionCycles::GenerateVis ( int cyid )
{
	Vec3F a, b;
	Vec3F* pnt;	
	Cycle* cy = getCycle(cyid);
	int p;

	if ( cy->vis != 0x0 ) 
		free (cy->vis);
	
	cy->vis = (Vec3F*) malloc ( cy->frames * cy->joints * sizeof(Vec3F)*2 );
	
	for (int f=0; f < cy->frames; f++) {
		
		// put cycle data into a character & evaluate
		AssignCycleToJoints ( cyid, m_TPose->getJoints(JNTS_A), f );		
		m_TPose->EvaluateJoints( JNTS_A, EVAL_CYCLE );					

		// get back the evaluated joint positions
		pnt = cy->vis + (f * cy->joints * 2);

		for (int n=1; n < cy->joints; n++) {
			a = m_TPose->getJoint(JNTS_A, n)->pos;
			*pnt++ = a;
			p = m_TPose->getJoint(JNTS_A, n)->parent;
			b = m_TPose->getJoint(JNTS_A, p)->pos;
			*pnt++ = b;
		}
	}
	// ResetTPose();
}

void MotionCycles::DeleteCycle ( int i )
{
	free ( m_Cycles[i].pos_table );
	free ( m_Cycles[i].ang_table );
	free ( m_Cycles[i].orient_table );	
	m_Cycles.erase ( m_Cycles.begin() + i );
}
void MotionCycles::RemoveAllBVH ()
{
	int i=0;
	while ( i < m_Cycles.size() ) {
		std::string name = m_Cycles[i].name;
		if ( name.find(".bvh") != std::string::npos ) {
			//dbgprintf ( "MotionCycle Removed: %s\n", name.c_str() );
			DeleteCycle ( i );
		} else {
			i++;
		}
	}
}

void MotionCycles::ListCycles ()
{
	for (int n=0; n < m_Cycles.size(); n++ ) {
		dbgprintf ( "    MotionCycle %d: %s (%d)\n", n, m_Cycles[n].name, m_Cycles[n].frames  );
	}
}
int MotionCycles::RandomCycleInGroup ( std::string name )
{
	std::vector<int>	group_list;

	// list group
	for (int n=0; n < getNumCycles(); n++ ) {
		if ( name.compare(m_Cycles[n].group)==0 ) 
			group_list.push_back ( n );
	}
	if (group_list.size()==0) {
		// no motions found
		return 0;
	}
	// random item
	int j = (int) (rand()*(group_list.size()-1)) / RAND_MAX;
	if ( j < 0 ) j = 0;
	if ( j > group_list.size()-1) j = (int) group_list.size()-1;
	
	return group_list[j];
}

int MotionCycles::LoadBVH ( std::string fname )
{
	FILE* fp;
	char buf[16384];
	char filepath[1024];

	int frame_skip = 1;
	float scaling = 0.25;

	std::string word, word1, word2, word3, name;
	int mode = BVH_MOTION;
	std::stack<int> jtree;
	int jnt = 0, jlev = 0, jparent = 0;
	int bytes = 0, cnt = 0, num_chan = 0;
	int frame = 0, used;
	//char args[10][1024];
	Vec3F vec;

	if ( !getFileLocation ( fname.c_str(), filepath ) ) {
		dbgprintf( "ERROR: Cannot find file. %s\n", fname );		
		return OBJ_NULL;
	}
	// Make sure we have a T-Pose character
	// T-Pose defines the hierarchy & channels of the output cycles.
	if ( m_TPose==0x0 ) {
		LoadTPose ( fname );
	}

	// Temporary character for incoming BVH
	// TempChar defines the hierarchy & channels of the input joints and motion.
	Character* m_TmpChar = new Character;
	m_TmpChar->Define(0, 0);
	m_TmpChar->CreateOutput('Ashp');
	m_TmpChar->LoadBVH ( filepath );		// load TmpChar bvh here
	m_TmpChar->CopyJoints(JNTS_A, JNTS_B);

	// Create mapping between incoming and outgoing hierarchy & channels
	// Method: for each incoming channel, identify best outgoing channel
	// More complex methods are possible. e.g. joint concatenation, pose matching, ML, etc.
	//
	int j;
	Vec3I src_chan, dst_chan;
	Joint* src_jnt, *dst_jnt;
	std::vector< Vec3I >	m_ChannelMap;

	for (int i = 0; i < m_TmpChar->getNumChannels(); i++) {		// number of channels in source
		src_chan = m_TmpChar->getChannel(i);					// source channel	
		src_jnt = m_TmpChar->getJoint( JNTS_A, src_chan.x );	// source joint (for channel)
		dst_jnt = m_TPose->FindJoint( JNTS_A, src_jnt->name, j );			// find joint by name
		dst_chan = Vec3I(-1, -1, -1);
		if (dst_jnt != 0x0) {
			dst_chan = m_TPose->FindChannel( j, src_chan.y );		// find matching channel			
		}
		m_ChannelMap.push_back( dst_chan );
	}

	// Open the BVH again to get motion data
	fp = fopen ( filepath, "rt" );	

	jnt = 0;
	jparent = 0;
	int cyid = OBJ_NULL;
	Cycle* cy;
	int njoints, nframes;
	
	while ( feof( fp ) == 0 ) {
		fgets ( buf, 16384, fp );	

		if ( buf[0] == '#' )				// Comments
			continue;

		switch ( mode ) {
		case BVH_MOTION:
			word = readword ( buf, ' ' );
			if ( word.compare ( "MOTION" ) == 0 ) {	
				// Start motion section
				fgets ( buf, 1024, fp );						// Frames data
				word = readword ( buf, ' ' );
				if ( word.compare ( "Frames:" ) !=0 ) {
					dbgprintf ( "ERROR: BVH file does not contain Frames entry\n", fname );
				}
				word = readword ( buf, ' ' );				
				
				nframes = strToI ( word );				// # of frames in cycle				
							
				fgets ( buf, 1024, fp );					// frame time data
				word = readword ( buf, ' ' );
				if ( word.compare ( "Frame" ) !=0 ) {
					dbgprintf ( "ERROR: BVH file does not contain Frame Time entry\n", fname );
				}
				word = readword ( buf, ' ' );
				word = readword ( buf, ' ' );
				
				njoints = m_TPose->getNumJoints(JNTS_A);	// number of joints in output character 
				
				float frametime = strToF ( word );			// frame time				
								
				// Create cycle
				cyid = AddCycle ( fname, nframes, njoints, frame_skip );
				cy = getCycle ( cyid );

				mode = BVH_TIMEDATA;
			}
			break;
		case BVH_TIMEDATA: {
			bytes = 0;
			//printf ( "  MOTION. Frame: %d of %d\n", frame, m_Cycle.frames );			
						
			if ( frame % frame_skip == 0 ) {

				// loop over each channel modification (joint pos/ang) for single frame
				int recframe = frame / frame_skip;
				
				assert( m_TmpChar->getNumChannels() == m_ChannelMap.size() );		// all channels must be accounted for
				
				for (int c=0; c < m_ChannelMap.size(); c++ ) {
					src_chan = m_TmpChar->getChannel (c);							// source channel
					dst_chan = m_ChannelMap[c];										// dest channel
					switch (src_chan.y) {
					case CHAN_XYZ_POS:
						sscanf(buf + bytes, "%f %f %f%n", &vec.x, &vec.y, &vec.z, &used);		// always read frame data
						if (dst_chan.x != -1) cy->pos_table[recframe * njoints + dst_chan.x] = vec;		
						break;
					case CHAN_ZYX_ROT:
						sscanf(buf + bytes, "%f %f %f%n", &vec.z, &vec.y, &vec.x, &used);		// always read frame data
						if (dst_chan.x != -1) cy->ang_table[recframe * njoints + dst_chan.x] = vec;
						break;
					} 
					bytes += used;			// advance to next joint in same line
				}
			}
			frame++;
			if ( frame >= cy->frames*frame_skip ) mode = BVH_END;
			} break;
		case BVH_END:
			break;
		}; 
	}
	
	// Refactor the BVH format into desired Ri' orientations		
	//   See the paper: Meredit & Maddock, Motion Capture File Formats Explained 
	//   on refactoring of BVH. (see eqn 4.2 and 4.9, p.16
	//     Mv = T Ri v = T Ri Rj0 v'' = T Ri' v''   where Ri' = orient angs, v = bone vec, v'' = bone length
	//	
	Quaternion parent;
	parent.Identity();
	
	for (int f=0; f < cy->frames; f++) {									// Process all frames
		RefactorBVH ( m_TPose->getJoints(JNTS_A), cy, 0, f, parent );		// TPose gives us the bone pivots to be refactored
	}

	return cyid;
}

Quaternion MotionCycles::ComputeOrientFromBoneVec( Vec3F v )
{
	// what rotation places the bone along the bonevec v?
	Quaternion c;
	c.fromRotationFromTo(v, Vec3F(0, 1, 0));
	c = c.inverse();
	return c;
}

void MotionCycles::RefactorBVH ( std::vector<Joint>& joints, Cycle* cy, int curr_jnt, int frame, Quaternion Oparent )
{
	int t_elem = frame * (int) joints.size() + curr_jnt;
	Vec3F angs, new_angs;
	Vec3F eo, ei, ej;
	Quaternion Ri, Rj0, Ni;
	
	// compute the orientation angles, Ri'
	angs = cy->ang_table[ t_elem ];					// get the BVH angles for current frame	
	Ri.set ( angs );
	/*Matrix4F Ri_mtx;
	Ri_mtx.RotateZYX ( angs );
	Ri.set ( Ri_mtx );
	Ri.toEuler ( ei );*/

	Rj0 = ComputeOrientFromBoneVec ( joints[curr_jnt].bonepiv );

	Ni = Oparent.negative() * Ri * Rj0;				// O^[n-1] Ri Oi

	cy->orient_table[ t_elem ] = Ni;				// write result	in Euler angles
	
	// recurse	
	int child_jnt = joints[curr_jnt].child;
	while ( child_jnt != JUNDEF ) {
		RefactorBVH ( joints, cy, child_jnt, frame, Rj0 );
		child_jnt = joints[child_jnt].next;
	}
}

float MotionCycles::getHipAngle ( Matrix4F& mtx )
{
	Vec3F pnt ( 0, 0, 1 );
	pnt *= mtx;
	return atan2(pnt.x, pnt.z)/DEGtoRAD;		
}


void MotionCycles::getInterpolatedJoint ( float f, Cycle* cy, Vec3F& p, Quaternion& q )
{
	Vec3F p1, p2;
	Quaternion q1, q2;	
	if ( f < 0 ) f = 0;
	float u = f - int(f);

	// root joint only
	p1 = cy->pos_table[ int(f) * cy->joints ];	
	q1 = cy->orient_table[ int(f) * cy->joints ];	
	
	if ( u==0.0 ) { p = p1; q = q1;	return; }
	p2 = cy->pos_table[ (int(f)+1) * cy->joints ];	
	q2 = cy->orient_table[ (int(f)+1) * cy->joints ];

	q.slerp ( q1, q2, u );		// interpolate rotation
	p = p1 + (p2-p1)*u;			// interpolate pos
}

// Assign given cycle & frame to the specified joint set (on a character)
void MotionCycles::AssignCycleToJoints ( int cyid, JointSet& joints, float f, float fprev, Vec3F& cp, Vec3F& cpl, Quaternion& co, Quaternion& col )
{
	Cycle* cy = getCycle(cyid);

	// retrieve joint poses for current frame
	AssignCycleToJoints ( cyid, joints, f );		
	
	// get root motion directly from motioncycle table (does not require prev joints)
	getInterpolatedJoint ( fprev, cy, cpl, col );
	getInterpolatedJoint ( f, cy, cp, co );
}

void MotionCycles::AssignCycleToJoints ( int cyid, JointSet& joints, float t )
{
	// locate animation data for the selected frame
	Cycle* cy = getCycle(cyid);
	int frame = int(t);
	Vec3F* cpos = &cy->pos_table[ frame*cy->joints ];		
	Vec3F* cang = &cy->ang_table[ frame*cy->joints ];			
	Quaternion* corient = &cy->orient_table[ frame*cy->joints ];			

	// write all joints
	for (int j=0; j < cy->joints; j++ ) {
		joints[j].pos = *cpos++;
		joints[j].angs = *cang++;		
		joints[j].orient = *corient++;
	}
}


int MotionCycles::FindCycle ( std::string name )
{
	std::string cn;
	for (int n=0; n < m_Cycles.size(); n++ ) {
		cn = m_Cycles[n].name;
		if ( cn.compare(name)==0 ) return n;
	}
	return -1;
}

void MotionCycles::SetGroup ( std::string name )
{
	m_CurrGroup = name;
}

Quaternion MotionCycles::getCycleFrameOrientation ( int cid, int f )
{
	Cycle* cy = getCycle(cid);
	return cy->orient_table[ f*cy->joints ];		// return orientation of base joint 0
}

Vec3F MotionCycles::getCycleFramePos ( int cid, int f )
{
	Cycle* cy = getCycle(cid);
	return cy->pos_table[ f*cy->joints ];			// return pos of base joint 0
}



//---------------- Advanced character funcs

/* 
//--- Temporary characters are used for things like pose-to-pose comparison
void MotionCycles::CreateTempChars( char* bvh1, char* bvh2 )
{
	// Load template character and joints
	// *NOTE* Should modify so it is created by asset mgr
	m_Char1 = new Character;
	m_Char1->Define (0,0);
	m_Char1->CreateOutput( 'Ashp' );
	m_Char1->LoadBVH ( bvh1 );
	m_Char1->CopyJoints ( JNTS_A, JNTS_B );

	m_Char2 = new Character;
	m_Char2->Define (0,0);
	m_Char2->CreateOutput('Ashp');
	m_Char2->LoadBVH ( bvh2 );}
	m_Char2->CopyJoints ( JNTS_A, JNTS_B );
}

// Finish loading of motion cycles
void MotionCycles::FinishTempChar ()
{
	delete m_Char1; m_Char1 = 0x0;
	delete m_Char2; m_Char2 = 0x0;
}

void MotionCycles::CompareCycles (int ci, int cj, std::string fname)
{
	ImageX img;
	float v;

	int iframes = m_Cycles[ci].frames;
	int jframes = m_Cycles[cj].frames;

	img.ResizeImage ( iframes, jframes, ImageOp::RGB24 );

	for (int i=1; i < iframes; i++)
		for (int j=1; j < jframes; j++) {
			v = ComparePoses ( ci, i, cj, j );
			img.SetPixel ( i, jframes - j, XBYTE(v*255.0f) );
		}

	IdentifyTransitions ( ci, cj, img );

	char buf[512];
	strcpy ( buf, fname.c_str() );
	img.Save ( buf );
}

void MotionCycles::CompareCycles (std::string iname, std::string jname)
{
	int ci = FindCycle(iname.c_str());
	int cj = FindCycle(jname.c_str());

	if ( ci==-1 || cj==-1 ) return;

	printf ( "Comparing: %s to %s\n", iname.c_str(), jname.c_str() );
	CompareCycles ( ci, cj, "cyc_"+iname+"_to_"+jname+".png" );
}

float MotionCycles::ComparePoses (int ic, int iframe, int jc, int jframe )
{
	assert ( m_Cycles[ic].joints == m_Cycles[jc].joints );

	Cycle* icycle = getCycle(ic);
	Cycle* jcycle = getCycle(jc);
	Quaternion *io, *jo;
	Quaternion d;
	io = &icycle->orient_table[ iframe * icycle->joints ];
	jo = &jcycle->orient_table[ jframe * jcycle->joints ];

	float sum = 0.0;
	io++; jo++;											// skip joint 0
	
	#ifdef COMPARE_JOINT_ORIENT
		//--- compare using joint orientations
		for (int n=1; n < m_Cycles[ic].joints; n++) {
				d = *io * (*jo).inverse();						// difference in orientation between poses for this joint
			sum += d.getAngle ();							// get angle
			io++;
			jo++;
		}
		sum *= 10.0 / (m_Cycles[ic].joints * 180.0f);		// normalize 	
		
		o1 =  icycle->orient_table[ iframe * icycle->joints + n];
		o1l = icycle->orient_table[ (iframe-1) * icycle->joints + n ];
		o2 =  jcycle->orient_table[ jframe * jcycle->joints + n ];
		o2l = jcycle->orient_table[ (jframe-1) * jcycle->joints + n];
		o1 = o1l.inverse() * o1; o1.normalize();
		o2 = o2l.inverse() * o2; o2.normalize();
		d = o1 * o2.inverse();				// rotational velocity
		d.normalize();
		deriv = d.getAngle();
	#endif

	// evaluate i cycle pose
	AssignCycleToJoints(ic, m_Char1->getJoints(JNTS_A), iframe);
	AssignCycleToJoints(ic, m_Char1->getJoints(JNTS_B), iframe - 1);
	m_Char1->EvaluateJoints(JNTS_A, EVAL_POSE);
	m_Char1->EvaluateJoints(JNTS_B, EVAL_POSE);

	// evaluate j cycle pose
	AssignCycleToJoints(jc, m_Char2->getJoints(JNTS_A), jframe);
	AssignCycleToJoints(jc, m_Char2->getJoints(JNTS_B), jframe - 1);	// prev frame for derivative
	m_Char2->EvaluateJoints(JNTS_A, EVAL_POSE);
	m_Char2->EvaluateJoints(JNTS_B, EVAL_POSE);

	Vec3F p1, p2;
	Vec3F o1, o1l, o2, o2l;
	float dist, deriv;
	//--- compare joint positions
	for (int n = 1; n < m_Cycles[ic].joints; n++) {
		p1.Set(0, 0, 0); p1 *= m_Char1->getJoint(JNTS_A, n)->Mworld;
		p2.Set(0, 0, 0); p2 *= m_Char2->getJoint(JNTS_A, n)->Mworld;
		dist = (p2 - p1).Length();

		o1.Set(0, 0, 0); o1 *= m_Char1->getJoint(JNTS_B, n)->Mworld;
		o2.Set(0, 0, 0); o2 *= m_Char2->getJoint(JNTS_B, n)->Mworld;
		o1 = p1 - o1;
		o2 = p2 - o2;
		deriv = (o2 - o1).Length();

		sum += dist * 2.f + deriv * 20.f;
	}
	sum *= 1.0 / (m_Cycles[ic].joints * 60.0f);

	return sum;
}

void MotionCycles::IdentifyTransitions ( int ci, int cj, ImageX& img )
{
	int iframes = m_Cycles[ci].frames;
	int jframes = m_Cycles[cj].frames;
	int interp = 20;
	float sum;
	int x, y;

	int threshold = jframes / 2;

	std::vector<Vec3F>	tlist;

	float* cost_func  = (float*) malloc ( iframes*sizeof(float) );
	float* cost_smooth = (float*) malloc ( iframes*sizeof(float) );

	// search for transitions from i-cycle
	for (int i=0; i < iframes; i++) {
		sum = 0.0;
		x = i; y = 0;
		for (int s=0; s < interp; s++) {
			sum += img.GetPixel ( x % iframes, jframes - (y % jframes) );
			x++; y++;
		}
		cost_func[i] = sum / interp;			// cost function result (float)
	}

	// get cost func min & max
	float cmin, cmax;
	cmin = 10000; cmax = 0;
	for (int i=0; i < iframes; i++) {
		if ( cost_func[i] < cmin) cmin = cost_func[i];
		if ( cost_func[i] > cmax) cmax = cost_func[i];
	}
	cost_func[0] = 100;
	cost_func[iframes-1] = 100;

	// smooth cost func to eliminate noise
	int jmax = jframes-5;
	for (int iter=0; iter < 5; iter++) {

		for (int i=1; i < iframes-1; i++)
			cost_smooth[i] = cost_func[i-1]*0.25f + cost_func[i]*0.5f + cost_func[i+1]*0.25f;

		for (int i=1; i < iframes-1; i++) {
			cost_func[i] = cost_smooth[i];
			img.SetPixel ( i, jmax - ((cost_func[i]-cmin)*jmax*0.5/(cmax-cmin)), 255 );		// debug result
		}
	}

	// identify minima
	float s1, s2;
	int j, k;
	for (int i=1; i < iframes-20; i++) {
		s1 = cost_smooth[i-1];
		sum = cost_smooth[i];
		s2 = cost_smooth[i+1];

		if ( sum < s1 && sum < threshold ) {
			j = 0;
			for (j=0; j < 20 && sum == s2 ; j++ ) {			// search for end of plateau
				s2 = cost_smooth[i+j];
			}
			k = i+(j/2);									// use center as minima
			if ( sum < s2 && k > 40 ) {						// plateau found, away from start
				tlist.push_back ( Vec3F( float(cj), float(k), sum) );	// record in transition list
			}
		}
	}

	// record transitions (append to current cycle)
	std::sort ( tlist.begin(), tlist.end(), sort_func );

	Cycle* cyclei = getCycle(ci);
	for (int n=0; n < fmin(2, tlist.size()); n++) {
		cyclei->transitions.push_back( Vec3I(tlist[n]) );	// cast to ints here
		img.SetPixel ( tlist[n].y, jframes-1, 255 );			// mark it in debug image
	}

	free ( cost_func );
	free ( cost_smooth );
}

Vec3I MotionCycles::FindTransition ( int ci, int cj, int frame )
{
	Cycle* cyclei = getCycle(ci);						// input cycle
	TransitionList& tlist = cyclei->transitions;

	if ( tlist.size()==0 ) return Vec3I(-1,-1,-1);	// no transition list

	int best = -1;
	int bestf = 100000;
	for (int n=0; n < tlist.size(); n++) {
		if ( tlist[n].x == cj && tlist[n].y >= frame && tlist[n].y < bestf ) {	// find best candidate transition
			best = n;
			bestf = tlist[n].y;
		}
	}
	if ( best == -1 ) return Vec3I(-1,-1,-1);		// no candidate found
	return tlist[ best ];
}

*/