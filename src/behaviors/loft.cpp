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

#include "loft.h"
#include "shapes.h"
#include "camera.h"
#include "object_list.h"
#include "mesh.h"
#include "quaternion.h"
#include "scene.h"

#include <stack>
#include <vector>

#define L_RES		0
#define L_LOD		1
#define L_NOISE		2
#define L_SEED		3
#define L_NAME		4
#define L_OFFSET	5
#define L_SCALE		6

#define PI		(3.141592)

Loft::Loft() : Object()
{
	m_Section = 0x0;
	m_SectionNorm = 0x0;	
}

void Loft::Define (int x, int y)
{
	AddInput ( "time",		'Atim' );		// time must come first - indicates animating object. Marked Dirty by scene at start of every frame.

	AddInput ( "material",  'Amtl' );
	AddInput ( "shapes",	'Ashp' );		// input shapes to loft		
	AddInput ( "mesh",		'Amsh' );	

	AddParam(L_RES,		"res",	 "I");	SetParamI3( L_RES, 0, Vec3F(16, 40, 40));			// ures, vres, wres
	AddParam(L_LOD,		"lod",	 "i");	SetParamI ( L_LOD, 0, 0 );
	AddParam(L_NOISE,	"noise", "f");	SetParamF( L_NOISE, 0, 0.002);	
	AddParam(L_SEED,	"seed",	 "i");	SetParamI (L_SEED, 0, 172 ) ;
	AddParam(L_NAME,	"name",	 "s");	SetParamStr (L_NAME, 0, "Vsingle" );
	AddParam(L_OFFSET,  "offset", "3");	SetParamV3(L_OFFSET, 0, Vec3F(0,0,0) );
	AddParam(L_SCALE,	"scale", "3");	SetParamV3(L_SCALE, 0, Vec3F(1, 1, 1));

	mTimeRange.Set(0,10000,0);
}

void Loft::ConstructSection (int ures, Vec3F norm)
{
	Quaternion q;
	q.fromRotationFromTo(Vec3F(1, 0, 0), norm);

	if (m_Section != 0x0) {
		free(m_Section);
		free(m_SectionNorm);
	}
	m_Section = (Vec3F*) malloc( ures * sizeof(Vec3F));
	m_SectionNorm = (Vec3F*)malloc( ures * sizeof(Vec3F));
		
	for (int u = 0; u < ures; u++) {												// circle
		// shape range [-1,1]
		m_Section[u] = Vec3F(0.0f, cos(u * 2.0f * PI / (ures-1) ), sin(u * 2.0f * PI / (ures-1) ));
		m_Section[u] *= q;															// rotate to point along desired norm

		// norm range [-1,1]
		m_SectionNorm[u] = Vec3F(0.0f, cos(u * 2.0f * PI / (ures-1) ), sin(u * 2.0f * PI / (ures-1) ));	
		m_SectionNorm[u] *= q;															
	}
}

void Loft::ConstructMeshes (Vec3I res, std::string srcname, std::string vname )
{
	// create new meshes
	// based on name of input shapes

	Mesh* mesh;
	std::string mesh_name;

	m_meshes.clear ();

	for (int i=0; i < res.z; i++) {
		
		mesh_name = "Loft_"+ srcname + vname + "M"+ iToStr(i);
		
		//dbgprintf ( "%d: %s\n", i, mesh_name.c_str() );

		if ( i >= m_meshes.size() ) {
			mesh = (Mesh*) gAssets.AddObject('Amsh', mesh_name);			// cache pointer for efficient regeneration
			m_meshes.push_back ( mesh );
			m_meshes[i]->CreateFV();
		} else {
			mesh = m_meshes[i];
		}

		mesh->MeshX::Clear ();
	
		// SetInput( "mesh", mesh_name );			// connect mesh to this object

		// build loft mesh topology
		int ures = res.x; int vres = res.y;	int wres = 1;		//res.z;
		Shape* xfm = mesh->getLocalXform();
		xfm->pos.Set(0, 0, 0);
		// vertices
		//for (int w = 0; w < wres; w++) {
			for (int v = 0; v < vres; v++) {
				for (int u = 0; u < ures; u++) {
					mesh->AddVert(0, 0, 0);
					mesh->AddVertNorm(Vec3F(0, 1, 0));
					//mesh->AddVertClr ( Vec3F(float(u)/ures, 0, float(v)/vres) );		// debugging - color vertices by u,v
					mesh->AddVertClr(Vec3F(1, 1, 1));
					mesh->AddVertTex(Vec2F(float(u) / (ures - 1), float(v) / (vres - 1)));
				}
			}
		//}
		// faces - total faces per shape: ures * (vres-1)
		int faces;
		int p = 0;
		//for (int w = 0; w < wres; w++) {
		//	p = w * ures * vres;							// vertex index.. this is *not* the face index.		
			for (int v = 0; v < vres - 1; v++) {			// there is one less row of faces than there are vertices along v
				for (int u = 0; u < ures - 1; u++) {
					mesh->AddFaceFast3FV(p, p + 1 , p + ures );
					mesh->AddFaceFast3FV(p + ures, p + 1, p + ures + 1 );
					p++;
				}
				mesh->AddFaceFast3FV(p, p - ures + 1, p + ures  );		// wrap around.
				mesh->AddFaceFast3FV(p + ures, p - ures + 1, p + 1 );
				p++;
			}
			faces = mesh->GetNumFace3();
		//}

		//mesh->setElemCount ( ures * vres);
		mesh->MarkDirty();		// geometry changed for rendering
	}
}


void Loft::Generate (int x, int y)
{
	CreateOutput ( 'Ashp' );

	// resolution
	Vec3I res = getMeshRes();
	std::string name = getParamStr(L_NAME);			// variant name

	// construct mesh
	m_shapes = getInputShapes("shapes");			// input shapes
	if ( m_shapes != 0x0 ) {
		
		ConstructMeshes ( res, m_shapes->getName(), name );		// build meshes

	} else {
		return;		// no input shapes
	}
	
	// section profile 
	ConstructSection ( res.x, Vec3F(1,0,0) );

	ClearShapes ();

}

void Loft::GenerateIndividual ()
{
	// ------------- SHAPE LOFTING. Each shape is individually loft.
	Vec3F q[4];
	Vec3F scal, sz, p;
	float vt;
	Shape* s;
	Quaternion c;
	Vec3I res = getMeshRes();
	int uvres = res.x * res.y;
	int wmax = imin( res.z, m_shapes->getNumShapes());

	Mesh* mesh = m_meshes[0];
	Vec3F* vpos = (Vec3F*) mesh->GetBufData(BVERTPOS);
	Vec2F* vtex = (Vec2F*) mesh->GetBufData(BVERTTEX);
	
	for (int w = 0; w < wmax; w++) {
		s = m_shapes->getShape(w);

		q[0] = s->pos;														// start point
		q[1] = Vec3F(0, s->scale.y, 0); q[1] = s->rot.rotateVec(q[1]);	// end point
		scal = Vec3F(s->scale.x, 0, s->scale.z);

		vpos = mesh->GetElemVec3(BVERTPOS, w * uvres + 0 * res.x);
		for (int v = 0; v < res.y; v++) {
			vt = (float(v) / float(res.y - 1));
			q[2] = q[0] + q[1] * vt;										// linear path along shape Y-axis

			//q[3].Set(0, 0, s->vel.y * sin(vt * 3.141592));					// bending, section offset in Z-axis --- need to fix.. Muscle bending amount

			sz = scal * float(cos((vt - 0.5) * 2.0 * 3.141592) * 0.5 + 0.5);		// muscle bulge

			for (int u = 0; u < res.x; u++) {
				p = m_Section[u] * sz + q[3]; p *= s->rot; p += q[2];
				*vpos++ = p;
			}
		}
	}

	mesh->ComputeNormals(false);		// false = smooth normals (not flat)
	mesh->MarkDirty();					// geometry changed for rendering
}

void Loft::CountChainsAndLengths()
{
	Vec3I res = getMeshRes();
	LoftGroup g;

	// count chains & chain lengths			
	Shape *s, *sc;
	uchar* slev = (uchar*)		m_shapes->getData (BLEV);			// hierarchy data
	uxlong* schild = (uxlong*)	m_shapes->getData (BCHILD);
	uxlong* snext = (uxlong*)	m_shapes->getData (BNEXT);
	Vec3I* svari = (Vec3I*)m_shapes->getData (BVARI);		// OPTIONAL

	m_lofts.clear ();

	for (int si = 0; si < m_shapes->getNumShapes(); si++) {
		s = m_shapes->getShape(si);
		
		if ( !s->isSegment() ) {	
			// new LoftGroup
			g.parent = si;			
			int id = schild[si];								// POINT child is head of segment chain	
			g.links.clear ();			
			if ( id != S_NULL ) {
				
				for (; id != S_NULL; ) { 
					sc = m_shapes->getShape(id);			// debugging - check first child pos = parent pos	
					g.links.push_back ( id );					// record each link in chain
					id = snext[id];
				}
				if (svari != 0x0 ) g.variant = svari[si];
				m_lofts.push_back ( g );			

				// dbgprintf ( "loft: %d, si: %d, cnt: %d\n", m_lofts.size()-1, si, g.links.size() );		// debugging
			}			
		}
	}
}



void Loft::GenerateGrowth(float time)
{
	// -------- DYNAMIC SEGMENT LOFTING. Shapes create a chain with S_SEGMENTS as roots. 
	// seed with level 0 shapes
	// must recreate each frame as input changes
	// loft: 
	// given: ures = section resolution, vres = chain resolution
	// x = particle id chain
	// y = incremented v
	// z = total in chain
	// w = dy/du = change in chain with respect to vres 
	//
	Mesh* mesh = m_meshes[0];
	Object* tex = getInputResult("tex");					// set surface texture of mesh
	if (tex != 0x0) mesh->SetTexture(tex->getID());
	
	Vec3F* vpos = (Vec3F*) mesh->GetBufData(BVERTPOS);
	Vec2F* vtex = (Vec2F*) mesh->GetBufData(BVERTTEX);
	
	Vec3I res = getMeshRes();
	int uvres = res.x * res.y;

	// Find the chains, start shapes, and their lengths
	CountChainsAndLengths ();
	
	// build lofts	
	int active = 1;
	int terminal = 0;	

	float step = 0.0001;
	Object* src = getInput("shapes");

	// construct loft
	Vec3F p;
	Matrix4F xform;

	while (active > 0) {
		active = 0;
		for (int w = 0; w < m_lofts.size(); w++) {

			/*if (m_lofts[w].x != S_NULL) {
				s = shapes->getShape(m_lofts[w].x);			// get shape
				xform = s->getXform();					// get complete transform

				// loft section
				terminal = 0;

				//------- tree growth
				//float st = (time - mTimeRange.x) / (mTimeRange.y - mTimeRange.x);		// time scaling
				//st = sin(st * 3.141592 / 2);
				//sv = (m_lofts[w].z - m_lofts[w].y) / m_lofts[w].z;
				//sz = sv * st; if (sz < 0) sz = 0;

				//------- tube (snake) growth
				if (m_lofts[w].w > 0) {
					sv = (m_lofts[w].z - m_lofts[w].y + step) / (m_lofts[w].z + step);		// correct! fractional animated radial growth
					sz = sv; if (sz < 0) sz = 0;
				}
				else {
					sv = (m_lofts[w].z - m_lofts[w].y) / (m_lofts[w].z);					// loft complete, stop animating
					sz = sv;
				}
				float st = (time - mTimeRange.x) / (mTimeRange.y - mTimeRange.x);			// time scaling
				st = sin(st * 3.141592 / 2);
				sz *= st;

				//-- texture coordinates
				sv = m_lofts[w].y / res.y;				// correct texture growth (surface-fixed)
				su = 1.0f / res.x;

				//sz = imin(1, m_lofts[w].z / m_vres) + (float(step  - m_lofts[w].y) / m_lofts[w].z);		//-- awesome! (streamers)

				vpos = m_mesh->GetElemVec3(BVERTPOS, w * uvres + int(m_lofts[w].y) * res.x);
				vtex = m_mesh->GetElemVec2(BVERTTEX, w * uvres + int(m_lofts[w].y) * res.x);
				if (int(m_lofts[w].y) >= m_lofts[w].z - 1) sz = 0.0;				// last ring (at chain length), reduce to a point
				for (int u = 0; u < res.x; u++) {
					p = m_Section[u] * sz; p *= xform;				  		// point on profile (cross-section)
					*vpos++ = p; *vtex++ = Vec2F(fabs(u * su - 0.5) * 0.5, sv);	// transformed to shape space. ensure wrap around tex coord: tex(u=0) == tex(u=ures-1)
				}

				// get next shape					
				m_lofts[w].x = s->next;										// follow links, may terminate here (when next=S_NULL)					
				m_lofts[w].y += fabs(m_lofts[w].w);
				if (int(m_lofts[w].y) >= res.y) m_lofts[w].x = S_NULL;		// also terminate if vres full 	
				active++;
			} */
		}
	}

	mesh->ComputeNormals(false);		// false = smooth normals (not flat)
	mesh->MarkDirty();					// geometry changed for rendering
}


void Loft::getLoftSection(float v, int w, Matrix4F& xform, Quaternion& xrot, float& sz, uint& clr)
{
	// sample loft group to get xform & size at this 'v' coordinate

	int   vc = v;
	float f = v - vc;									// linear interpolation fraction
	int vi1 = m_lofts[w].links[ vc ];
	Shape* s1 = m_shapes->getShape(vi1);
	clr = s1->clr;
	
	if ( int(vc) < m_lofts[w].links.size()-1) { 
				
		int vi2 = m_lofts[w].links[ vc+1 ];
		Shape* s2 = m_shapes->getShape(vi2);
	
		xrot.lerp ( s1->rot, s2->rot, f );							// spherical linear interpolate orientation
		Vec3F pos = s1->pos + (s2->pos - s1->pos) * f;			// linear interpolate position	
		Vec3F scal = s1->scale + (s2->scale - s1->scale) * f;
		xform.TRST(pos, xrot, scal, s1->pivot );					// fast shape transform

	} else {																	
		xrot = s1->rot;												// endpoint is the *end* of the last shape
		xform.TRST( s1->pos, s1->rot, s1->scale, s1->pivot);		// fast shape transform
	}
	
	sz = 1.0f;

}

Vec3I Loft::getMeshRes ()
{
	int lod = getParamI(L_LOD);
	Vec3I res = getParamI3(L_RES) * (1.0f / (lod + 1.0f));			// lod adjustment of mesh resolution
	if (res.x < 6) res.x = 6;
	if (res.y < 3) res.y = 3;
	if (res.z < 3) res.z = 3;
	return res;
}

// stats: x=num obj, y=num shapes, z=num tris
Vec3F Loft::getStats()
{
	Vec3I res = getMeshRes();
	int wmax = std::min( res.z, (int) m_lofts.size());		// x = total number of lofts
	int uvres = res.x * res.y;
	int vmax = std::max( res.y-1, 2 );
	int faces = 2 * res.x * (vmax-1);			// z = final face count
	return Vec3F( wmax, wmax, wmax * faces );
}

int Loft::GenerateComplete (int w)
{
	Shape* s;
	Matrix4F xform;
	Quaternion xrot;
	Vec3F start_pos, p, scale, ctr;
	Vec3F uvz (0,0,0);
	Vec3F offset = getParamV3(L_OFFSET);
	Vec3F gscale = getParamV3(L_SCALE);
	uint clr;
	float du, sz;
	int vmax;

	Mesh* mesh = m_meshes[w];

	Object* tex = getInputResult("tex");					// set surface texture of mesh
	if (tex != 0x0) mesh->SetTexture(tex->getID());

	Vec3F* vpos = (Vec3F*) mesh->GetBufData(BVERTPOS);
	Vec3F* vnorm = (Vec3F*) mesh->GetBufData(BVERTNORM);
	Vec2F* vtex = (Vec2F*) mesh->GetBufData(BVERTTEX);
	uint* vclr = (uint*) mesh->GetBufData(BVERTCLR);	
	Vec3I res = getMeshRes();
	int uvres = res.x * res.y;
	float noise = getParamF(L_NOISE);
	int seed = getParamI(L_SEED);
	
	// Clear mesh buffer data
	memset ( vpos, 0, sizeof(Vec3F) * uvres );

	// Loft each chain

	// get mesh shape
	s = getShape(w);

	vmax = std::max( res.y-1, 2 );
	//vmax = max(min( res.y-1, m_lofts[w].links.size() ), 2);
		
	float texscale_u = 1.0;												// <--- texture scaling facators
	float texscale_v = 16.0;
	float u, v;

	uvz.x = (1.0 / texscale_u) * (1.0f / res.x);
	//uvz.y = (m_lofts[w].links.size() / texscale_v) * (1.0f / res.y);
	uvz.y = (m_lofts[w].links.size() / texscale_v) * (1.0f / res.y);				
	 
	m_rand.seed(w);

	du = 0;

	sz = 1.0;

	//dbgprintf ( "%d %d\n", m_lofts.size(), wmax, res.x, res.y, res.z );		// debugging

	for (int vi=0; vi < res.y; vi++) {

		sz = (vi==0 || vi >= vmax-1 ) ? 0 : 1.0;		

		getLoftSection ( float(vi) * (m_lofts[w].links.size()-1) / vmax, w, xform, xrot, uvz.z, clr );		// sample loft group to get xform & size at this 'v' coordinate

		for (int ui = 0; ui < res.x; ui++) {

			p = (m_Section[ui] * sz) * xform;				// oriented point on profile (cross-section), translated to start
					
			*vpos++ = p * gscale + offset;				
			*vnorm++ = (m_SectionNorm[ui] * xrot).Normalize();			// section normal transformed into loft orientation			
			u = (ui+du) * uvz.x; 
			v = vi * uvz.y; 
			*vtex++ = Vec2F( u, v );		// transformed to shape space. ensure wrap around tex coord: tex(u=0) == tex(u=ures-1)
			*vclr++ = clr;
		}

		//du += 0.3;		// twisting		
	}

	//m_mesh->ComputeNormals(w * uvres, (w + 1) * uvres - 1, false);			// ** This is quite slow. Avoid if possible. false = smooth normals
	mesh->MarkDirty();					// geometry changed for rendering

	return 2 * res.x * (vmax-1);		// final face count
}

void Loft::Run ( float time )
{	
	m_shapes = getInputShapes("shapes");			// input shapes
	if (m_shapes == 0x0) { dbgprintf("WARNING: Loft has no shape input.\n"); return; }
	
	//if ( m_shapes->bHasSegments ) {			//--- need to fix

	// find the chains and lengths
	CountChainsAndLengths();

	// Output shapes
	Vec3I res = getMeshRes();
	int uvres = res.x * res.y;
	int faces;	

	int wmax = std::min( res.z, (int) m_lofts.size());		// total number of lofts

	ClearShapes ();

	for (int w=0; w < wmax; w++) {
		
		// Add shape
		Shape* s = AddShape();
		s->type = S_MESH;
		s->pos = Vec3F(0,0,0);

		faces = GenerateComplete ( w );		

		s->meshids.x = m_meshes[w]->getID();
		s->meshids.y = 0;
		s->meshids.z = faces;
		s->meshids.w = 0;

		//dbgprintf ( "loft: %d %d %d\n", m_lofts.size(), lofts, lofts*faces_per_shape );	
		
		s->scale.Set(1,1,1);
		s->pivot.Set(0,0,0);
	}
	
	/* --- old method, multiple shapes /w sub-faces
	Shape* s;
	int wmax = min(res.z, m_lofts.size());
	for (int w = 0; w < wmax; w++) {
		s = AddShape(w);
		s->type = S_MESH;
		s->pos = m_shapes->getShape( m_lofts[w].links[0] )->pos;		// start pos of each loft		
		s->meshids.x = m_mesh->getID();				// <--- generated mesh 		
		s->meshids.y = getInputID( "shader", 'Ashd' );
		int faces = 2 * res.x * (res.y - 1);		// total faces per shape: 2 * ures * (vres-1)
		s->meshids.z = faces;						// number of faces
		s->meshids.w = w * faces;					// sub-range of face buffer
		s->texids = getInputTex("tex");
		s->scale.Set(1,1,1);
		s->pivot.Set(0,0,0);
	}*/

	MarkClean();
	
	MarkComplete();		// NOTE: This is where we check if the input is complete, and mark self as complete. (not at top of func)
}

void Loft::Render ()
{
}

bool Loft::Select3D (int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir)
{
	// select shape from loft geometry

	Mesh* mesh = dynamic_cast<Mesh*>(getOutput());			// output mesh	
	Matrix4F xform = mesh->getLocalXform()->getXform();
	Vec3I res = getMeshRes();

	Vec3I vndx; Vec3F vnear, vhit, vnorm;
	if (mesh->Raytrace(orig, dir, xform, vndx, vnear, vhit, vnorm)) {
		
		int vert = vndx.y;		// nearest vertex
		int w = vert / (res.x*res.y); vert -= w * (res.x*res.y);
		int v = vert / res.x; vert -= (v * res.x);
		int u = vert;
		sel.x = getInput("shapes")->getID();
		sel.y = w;
		sel.z = 0;
		sel.w = 0;
		return true;
	}
	return false;
}

void Loft::Sketch (int w, int h, Camera3D* cam )
{

}




