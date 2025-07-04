

#include "displace.h"
#include "shapes.h"
#include "camera.h"
#include "object_list.h"
#include "scene.h"
#include "mesh.h"
#include "image.h"
#include "material.h"

#include "gxlib.h"

#include <stack>
#include <vector>

#define D_SUBDIVS	0

Displace::Displace() : Object()
{
}

void Displace::Define(int x, int y)
{
	AddInput("mesh", 'Amsh');
	AddInput("displace", 'Amtl');		// surface material (with displacement maps)
	AddInput("material", 'Amtl');		// output material
	AddInput("mesh0", 'Amsh');

	AddParam(D_SUBDIVS, "subdivs", "i");	SetParamI(D_SUBDIVS, 0, 16);

	//----------- static common
	// LOD grids	
	// define a global object for grid LODs
	std::string mesh_name = "trimesh";  // + iToStr(lod);
	Mesh* mesh = (Mesh*)gAssets.getObj(mesh_name);			 // fixed name. this is a global object for all heightfields
	if (mesh == 0x0) {
		mesh = (Mesh*)gAssets.AddObject('Amsh', mesh_name);
		mesh->CreateFV();
		// create here but don't build yet		
	}
	//--------- end common

	SetInput("mesh0", "trimesh");

	MarkDirty();
}


bool Displace::RunCommand(std::string cmd, vecStrs args)
{
	if (cmd.compare("finish") == 0) { return true; }

	return false;
}

void Displace::BuildSubdiv ()
{
	Mesh* mesh = (Mesh*) gAssets.FindObject ( "trimesh" );
	if (mesh==0x0) return;

	int subdivs = 1 + getParamI( D_SUBDIVS );

	Shape* xfm = mesh->getLocalXform();
	xfm->pos.Set( 0, 0, 0 );
			
	//-- triangular grid 
	// #v = r(r+1)/2, #f = (r-1)^2

	// vertices			
	int numv = subdivs * (subdivs+1) / 2;
	Vec2F p;
	for (int v = 0; v < subdivs; v++) 
		for (int u=0; u < subdivs - v; u++) {
			p.x = float(u)/(subdivs-1); p.y = float(v)/(subdivs-1);
			mesh->AddVert ( p.x, 0, p.y );
			mesh->AddVertTex ( p );
			mesh->AddVertNorm ( Vec3F(0,1,0) );			
		}
	

	// faces	
	int numf = (subdivs-1)*(subdivs-1);
	int c0=0, c=0;
	for ( int v = 0; v < subdivs-1; v++ ) {				
		c = c0;								// vertex ndx for start of this row
		for ( int u = 0; u <= subdivs - v - 2; u++ ) {
			mesh->AddFaceFast3FV ( c + (subdivs-v), c+1, c );					
			if (u < subdivs-v-2) mesh->AddFaceFast3FV ( c+1, c+(subdivs-v), c+(subdivs-v)+1 );
			c++;
		}		
		c0 += subdivs-v;
	}
	
	mesh->MarkDirty();		// geometry changed for rendering
}


void Displace::Generate (int x, int y)
{
	Shape* s;

	Object* src = getInputResult ( "mesh" );

	// get result of input obj, either mesh or shape /w mesh
	Mesh* mesh_in = 0;
	if (src->getType()=='Amsh') 
		mesh_in = (Mesh*) src;

	if (src->getType()=='Ashp') {
		int mid = ((Shapes*) src)->getShape(0)->meshids.x;
		mesh_in = (Mesh*) gAssets.getObj(mid);
	}	
	if (mesh_in==0x0) return;

	// Get input material and displacement
	Material* mtl = (Material*) getInput ( "displace" );			// source material for displacement
	Image* img = (Image*) mtl->getInput( "displace" );				// displacement map on material
	if (img == 0x0) return;	
	Vec3I res (img->GetWidth(), img->GetHeight(), 1 );	
	Material* out_mtl = (Material*)getInput("material");			// output material 

	// Construct output mesh
	std::string mesh_name = "Displace_"+ mesh_in->getName();
	Mesh* mesh_out = (Mesh*) gAssets.AddObject( 'Amsh', mesh_name );			// cache pointer for efficient regeneration	
	mesh_out->CreateFV();
	
	// Create output shape
	CreateOutput ( 'Ashp' );	
	s = AddShape ();
	s->type = S_MESH;
	s->meshids = Vec4F( mesh_out->getID(), 0, 0, 0 );
	s->matids = Vec8S( out_mtl->getID(), NULL_NDX, NULL_NDX);
	s->texsub.Set ( 0, 0, 1, 1 );
	s->pos.Set ( 0, 0, 0 );	
	s->scale.Set ( 1, 1, 1 );
	s->rot.Identity();		
	
	// Get pre-divided triangle grid
	Mesh* trimesh;
	trimesh = (Mesh*) getInput ( "mesh0" );	
	if ( trimesh->GetNumVert()==0 )
		BuildSubdiv ();		// build subdiv tri grid

	int subdivs = 1 + getParamI( D_SUBDIVS );

	// Sub-divide each triangle in source
	int src_numv, src_numf;
	int tri_numv, tri_numf;
	float b0, b1, b2;
	Vec3F v[3], pos;
	Vec3F n[3], norm;
	Vec2F t[3], uv;

	src_numv = mesh_in->GetNumVert();
	src_numf = mesh_in->GetNumFace3();
	tri_numv = trimesh->GetNumVert();
	tri_numf = trimesh->GetNumFace3();

	int maxv = src_numf * tri_numv;
	int maxf = src_numf * tri_numf;

	dbgprintf( "  Displace node:\n");

	dbgprintf ( "    Sub-dividing.." );
	for (int f=0; f < src_numf; f++) {

		if (f % 100==0) dbgprintf ( "%2.0f%% ", float(f)*100.0 / src_numf);

		// get source triangle
		AttrV3* face = mesh_in->GetFace3(f);
		v[0] = *mesh_in->GetVertPos(face->v1);
		v[1] = *mesh_in->GetVertPos(face->v2);
		v[2] = *mesh_in->GetVertPos(face->v3);
		t[0] = *mesh_in->GetVertTex(face->v1);
		t[1] = *mesh_in->GetVertTex(face->v2);
		t[2] = *mesh_in->GetVertTex(face->v3);
		n[0] = *mesh_in->GetVertNorm(face->v1);
		n[1] = *mesh_in->GetVertNorm(face->v2);
		n[2] = *mesh_in->GetVertNorm(face->v3);

		// replace with triangle sub-grid
		mesh_out->AppendMesh ( trimesh, maxf, maxv );

		int first_v = mesh_out->GetNumVert() - tri_numv;	// index of first vertex for this group

		// destination buffers  
		Vec3F* dest_vpos =	mesh_out->GetElemVec3 ( BVERTPOS,	first_v );
		Vec3F* dest_vnorm =	mesh_out->GetElemVec3 ( BVERTNORM,	first_v );
		Vec2F* dest_vtex =	mesh_out->GetElemVec2 ( BVERTTEX,	first_v );

		for (int vi = 0; vi < subdivs; vi++) {
			for (int ui = 0; ui < subdivs - vi; ui++) {
				b1 = float(ui)/(subdivs-1);		// barycentric coordinates
				b0 = float(vi)/(subdivs-1);
				b2 = 1-b0-b1;
				uv =  t[0]*b0 + t[1]*b1 + t[2]*b2;		// interpolate vertex components
				norm= n[0]*b0 + n[1]*b1 + n[2]*b2;
				pos = v[0]*b0 + v[1]*b1 + v[2]*b2;
				norm.Normalize();
				*dest_vpos++ = pos;
				*dest_vnorm++ = norm;
				*dest_vtex++ = uv;				
			}
		}		
	}
	dbgprintf ( "\n" );


	// Displace vertices
	src_numv = mesh_out->GetNumVert();
	src_numf = mesh_out->GetNumFace3();
	Vec3F* dest_vpos =	mesh_out->GetElemVec3 ( BVERTPOS,	0 );
	Vec3F* dest_vnorm =	mesh_out->GetElemVec3 ( BVERTNORM,	0 );
	Vec2F* dest_vtex =	mesh_out->GetElemVec2 ( BVERTTEX,	0 );
	float h;

	Vec4F displace_amt   = mtl->getParamV4 ( M_DISPLACE_AMT );
	Vec4F displace_depth = mtl->getParamV4 ( M_DISPLACE_DEPTH );		

	dbgprintf ( "    Displacing.." );
	for (int v=0; v < src_numv; v++) {

		if (v % 10000==0) dbgprintf ( "%2.0f%% ", float(v)*100.0 / src_numv);

		uv = *dest_vtex;
		uv.x = (uv.x > 0.999) ? 0 : uv.x;

		h = img->GetPixelFilteredUV16 ( uv.x, uv.y );				

		*dest_vpos = *dest_vpos + (*dest_vnorm) * h * displace_depth.y;

		dest_vpos++;
		dest_vnorm++;
		dest_vtex++;
	} 
	dbgprintf ("\n"); 


	Vec4F stat = mesh_out->GetStats();
	dbgprintf ( "    Mesh. %4.2f bytes, %8.0f faces\n", stat.x, stat.y );

	mesh_out->ComputeNormals ();

	mesh_out->MarkDirty ();

	MarkDirty ();
}
