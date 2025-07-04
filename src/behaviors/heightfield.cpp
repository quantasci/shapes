

#include "heightfield.h"
#include "shapes.h"
#include "camera.h"
#include "object_list.h"
#include "scene.h"
#include "mesh.h"
#include "image.h"
#include "material.h"

#include "gxlib.h"
using namespace glib;

#include <stack>
#include <vector>

#define HF_CELL_CNT		0
#define HF_CELL_LODS	1

Heightfield::Heightfield() : Object()
{
}

bool Heightfield::RunCommand(std::string cmd, vecStrs args)
{
	if (cmd.compare("finish") == 0) { return true; }

	return false;
}


void Heightfield::Define (int x, int y)
{
	AddInput ( "material",	'Amtl' );		// surface material (with texture and displacement maps)			
	AddInput ( "mesh0",		'Amsh' );		// static LOD meshes
	AddInput ( "mesh1",		'Amsh' );	
	AddInput ( "mesh2",		'Amsh' );	
	AddInput ( "mesh3",		'Amsh' );	
	AddInput ( "bake",		'Ashp' );		// baked output
	AddInput ( "bakemesh",	'Amsh' );	

	AddParam ( HF_CELL_CNT,	"cell_cnt",		"I");	SetParamI3 ( HF_CELL_CNT, 0, Vec3I(8, 8, 1) );
	AddParam ( HF_CELL_LODS,"cell_lods",	"4");	SetParamV4 ( HF_CELL_LODS, 0, Vec4F(512, 256, 128, 64) );

	//-- bake 
	Shapes* sh = (Shapes*) gAssets.AddObject ( 'Ashp', "HFbakeout" );
	SetInput ( "bake", "HFbakeout" );

	Mesh* mesh = (Mesh*) gAssets.AddObject ( 'Amsh', "HFbakemesh" );
	mesh->CreateFV ();	
	SetInput ( "bakemesh", "HFbakemesh" );	

	
	//----------- static common
	// LOD grids
	for (int lod=0; lod < 4; lod++ ) {

		// define a global object for grid LODs
		std::string mesh_name = "gridmesh" + iToStr(lod);
		Mesh* mesh = (Mesh*) gAssets.getObj ( mesh_name );			 // fixed name. this is a global object for all heightfields
		
		// create if not yet defined globally
		if ( mesh==0x0 ) {
			mesh = (Mesh*) gAssets.AddObject ( 'Amsh', mesh_name );
			mesh->CreateFV ();	

			// define resolution by LOD
			Vec4F cell_lods = getParamV4( HF_CELL_LODS );			
			int ures = cell_lods.C(lod);
			int vres = cell_lods.C(lod);

			Shape* xfm = mesh->getLocalXform();
			xfm->pos.Set( 0, 0, 0 );

			// vertices			
			Vec2F p;
			for (int v = 0; v < vres; v++) 
				for (int u=0; u < ures; u++) {
					p.x = float(u)/(ures-1); p.y = float(v)/(vres-1);
					mesh->AddVert ( p.x, 0, p.y );
					mesh->AddVertTex ( p );
					mesh->AddVertNorm ( Vec3F(0,1,0) );			
				}
	
			// faces	
			int c = 0;
			for ( int v = 0; v < vres-1; v++ ) {
				c = v * ures;
				for ( int u = 0; u < ures-1; u++ ) {
					mesh->AddFaceFast3FV ( c+ures, c+1, c );
					mesh->AddFaceFast3FV ( c+ures, c+ures+1, c+1 );			
					c++;
				}		
			}
			//mesh->setElemCount ( m_ures * m_vres * 2 );
			mesh->MarkDirty();		// geometry changed for rendering
		}
	}
	//--------- end common
	
	// default inputs	
	SetInput ( "mesh0",		"gridmesh0" );			// use the gridmesh created above
	SetInput ( "mesh1",		"gridmesh1" );
	SetInput ( "mesh2",		"gridmesh2" );
	SetInput ( "mesh3",		"gridmesh3" );	
	
	// heightfield is always dirty for camera frustum culling
	MarkDirty ();
} 


void Heightfield::Generate (int x, int y)
{
	Shape* s;

	// Each shape represents a sub-region of the terrain 
	// Shape detail selects a trimesh on the fly for camera LOD
	ClearShapes();


	// Create new shapes	
	CreateOutput ( 'Ashp' );

	// Grid meshes - fixed LOD tesselations
	Mesh* gridmesh[4];
	gridmesh[0] = (Mesh*) getInput ( "mesh0" );
	gridmesh[1] = (Mesh*) getInput ( "mesh1" );
	gridmesh[2] = (Mesh*) getInput ( "mesh2" );
	gridmesh[3] = (Mesh*) getInput ( "mesh3" );
	
	Vec8S mtl_id = getInputMat ( "material" );
	if (mtl_id.x == NULL_NDX) {
		dbgprintf("WARNING: No material assigned to heightfield.\n");
		return;
	}

	// Get height texture dimensions
	Image* img = (Image*) getInputOnObj (mtl_id.x, "displace" );
	if (img == 0x0) {
		dbgprintf("WARNING: No displacement assigned to material for heightfield.\n");
		return;
	}
	m_res = Vec3I(img->GetWidth(), img->GetHeight(), 1 );
	 
	Vec3F dtc (2.0/m_res.x, 2.0/m_res.y, 0);		// pixel delta

	Vec3I cell_cnt = getParamI3( HF_CELL_CNT );	
	Vec3I cell_res = m_res / cell_cnt;

	float ratio_x = 1.0;  //0.67;
	float ratio_y = 1.0;	

	// get displacement amount (for baking)
	Material* mtl = (Material*) gAssets.getObj(mtl_id.x );	
	Vec4F displace_amt = mtl->getParamV4( M_DISPLACE_DEPTH );
	
	// Generate dynamic grid (openGL only)
	// - instance sub-shapes with dynamic LOD (see Run func) covering terrain domain
	for (int y=0; y < cell_cnt.y; y++) {
		for (int x=0; x < cell_cnt.x; x++) {

			s = AddShape ();
			s->type = S_MESH;			
			s->meshids = Vec4F(gridmesh[0]->getID(), 0, 0, 0 );		// grid mesh
			s->matids = mtl_id;			
			s->ids = getIDS(y*cell_cnt.x + x,0,0);
			s->texsub.Set ( float(x)*ratio_x/cell_cnt.x, float(y)*ratio_y/cell_cnt.y, ratio_x/cell_cnt.x, ratio_y/cell_cnt.y );
			s->pos.Set ( float(x)/cell_cnt.x, 0, float(y)/cell_cnt.y );
			s->scale.Set ( 1.0/cell_cnt.x, displace_amt.y, 1.0/cell_cnt.y );		// y = vertical scale
			s->rot.Identity();
		}
	}

	// Generate bake shape (optiX raytrace)
	// - geometry baked into a single mesh object for raytracing
	int i;
	
	Mesh* bakem = (Mesh*)getInput("bakemesh");		// use baked mesh	

	Shapes* sh = (Shapes*) getInput ( "bake" );			// output bake shape
	s = sh->Add (i);
	s->type = S_MESH;
	s->meshids = Vec4F( bakem->getID(), 0, 0, 0 );
	s->matids = mtl_id;			
	s->texsub.Set ( 0, 0, 1, 1 );
	s->pos.Set ( 0, 0, 0 );	
	s->scale.Set ( 1, 1, 1 );
	s->rot.Identity();		

	// bake all instance cells into single dest mesh
	Mesh* src;
	
	int lod = 1;		
	
	dbgprintf("Baking heightfield..");
	for (int y=0; y < cell_cnt.y; y++) {
		for (int x=0; x < cell_cnt.x; x++) {

			dbgprintf ( "%2.0f%% ", float(y*cell_cnt.x+x)*100.0 / (cell_cnt.x*cell_cnt.y) );
			// source mesh at given lod
			src = gridmesh[lod];

			// append grid to baked mesh
			bakem->AppendMesh ( src );
			int src_numv = src->GetNumVert();
			int src_numf = src->GetNumFace3();
			int first_v = bakem->GetNumVert() - src_numv;	// index of first vertex is bakemesh to modify

			// destination buffers for most recent grid mesh
			Vec3F* dest_vpos =	bakem->GetElemVec3 ( BVERTPOS,	first_v );
			Vec3F* dest_vnorm =	bakem->GetElemVec3 ( BVERTNORM,	first_v );
			Vec2F* dest_vtex =	bakem->GetElemVec2 ( BVERTTEX,	first_v );

			// get instance cell parameters
			Shape* cell = getShape( y*cell_cnt.x + x );
			float h, hx, hy;			

			Vec3F normal_scale = cell->scale / getLocalXform()->scale;

			// displace each vertex while computing
			// uv coord, height, position, and normal
			//
			if (img->GetFormat() == ImageOp::BW16) {
				// 16-bit integer displacement
				for (int v = 0; v < src_numv; v++) {
					*dest_vtex = *dest_vtex * Vec2F(cell->texsub.z, cell->texsub.w) + Vec2F(cell->texsub.x, cell->texsub.y);		// cell texture range

					h = img->GetPixelUV16(dest_vtex->x, dest_vtex->y);
					hx = 0.5 * (img->GetPixelUV16(dest_vtex->x + dtc.x, dest_vtex->y) - img->GetPixelUV16(dest_vtex->x - dtc.x, dest_vtex->y));
					hy = 0.5 * (img->GetPixelUV16(dest_vtex->x, dest_vtex->y + dtc.y) - img->GetPixelUV16(dest_vtex->x, dest_vtex->y - dtc.y));

					dest_vpos->y += h;											// height displace
					*dest_vpos = *dest_vpos * cell->scale + cell->pos;			// cell position & scale		
					*dest_vnorm = Vec3F(-hx, 1, -hy) / normal_scale;			// surface normal (of a heightmap)				
					dest_vnorm->Normalize();
					dest_vpos++; dest_vnorm++; dest_vtex++;
				}
			}
			else if (img->GetFormat() == ImageOp::F32) {
				// 32-bit float displacement
				for (int v = 0; v < src_numv; v++) {
					*dest_vtex = *dest_vtex * Vec2F(cell->texsub.z, cell->texsub.w) + Vec2F(cell->texsub.x, cell->texsub.y);		// cell texture range

					h = img->GetPixelUV(dest_vtex->x, dest_vtex->y).x;
					hx = 0.5 * (img->GetPixelUV(dest_vtex->x + dtc.x, dest_vtex->y).x - img->GetPixelUV(dest_vtex->x - dtc.x, dest_vtex->y).x);
					hy = 0.5 * (img->GetPixelUV(dest_vtex->x, dest_vtex->y + dtc.y).x - img->GetPixelUV(dest_vtex->x, dest_vtex->y - dtc.y).x);

					dest_vpos->y += h;											// height displace
					*dest_vpos = *dest_vpos * cell->scale + cell->pos;			// cell position & scale		
					*dest_vnorm = Vec3F(-hx, 1, -hy) / normal_scale;			// surface normal (of a heightmap)				
					dest_vnorm->Normalize();
					dest_vpos++; dest_vnorm++; dest_vtex++;
				}
			}
			
		}
	}
	dbgprintf ( "\n" );
	
	bakem->ComputeNormals ();

	bakem->MarkDirty ();

	SetBake ( true );		// enable bake 
	

	dbgprintf ( "  Heightfield: in %s, out %s, cell_cnt:%d,%d\n", img->getName().c_str(), getOutputName().c_str(), cell_cnt.x, cell_cnt.y );

	// heightfield is always dirty for camera frustum culling
	MarkDirty ();

}



void Heightfield::Run ( float time )
{	
	Shape* s;
	Vec3F a, b;
	
	Camera3D* cam = gScene->getCamera3D();

	Vec3I cell_cnt = getParamI3( HF_CELL_CNT );	
	Vec3I cell_res = m_res / cell_cnt;
	Matrix4F objxform = getXform();
	int gridmesh[4];
	gridmesh[0] = getInputID ( "mesh0", 'Amsh' );
	gridmesh[1] = getInputID ( "mesh1", 'Amsh' );
	gridmesh[2] = getInputID ( "mesh2", 'Amsh' );
	gridmesh[3] = getInputID ( "mesh3", 'Amsh' );

	int lod;
	int max_lod = 3;
	Vec3F scal = getLocalXform()->scale; scal.y = 0;
	float range = scal.Length() / (max_lod+1);			// divided by lods

	for (int n = 0; n < getNumShapes(); n++) {			// skip every other pnt (faster)
		
		s = getShape(n);			
		a = s->pos * objxform;
		b = (s->pos + s->scale + Vec3F(0,100,0) ) * objxform;		
		
		lod = fmin(max_lod, fmax(0, (a - cam->getPos()).Length() / range) );
		s->meshids.x = gridmesh[lod];					// select gridcell res based on LOD		
		
		s->invisible = !cam->boxInFrustum(a, b);		// set visibility based on gridcell in camera frustum
	}

	// heightfield is always dirty for camera frustum culling
	MarkDirty ();
}

void Heightfield::Render ()
{
}

void Heightfield::Sketch (int w, int h, Camera3D* cam )
{
	Shape* s;
	Vec4F pix(1, 1, 1, 1);
	Vec3F a,b;
	float c;
	int lod;

	Vec3F pal[4];
	pal[0].Set(1,0,0);
	pal[1].Set(0,1,0);
	pal[2].Set(0,0,1);
	pal[3].Set(1,0,1);

	end3D();

	start2D( w, h );

	// grid resolution
	Vec3I cell_cnt = getParamI3( HF_CELL_CNT );	
	Vec3I cell_res = m_res / cell_cnt;
	Matrix4F objxform = getXform();

	// world scale
	int max_lod = 3;
	Vec3F wmin = getLocalXform()->pos;
	Vec3F wscal = getLocalXform()->scale; wscal.y = 0;
	float ws = 500 / wscal.x;
	float range = wscal.Length() / (max_lod+1);
	
	// draw view-frustum grid tiles	
	for (int n = 0; n < getNumShapes(); n++) {			// skip every other pnt (faster)
		s = getShape( n );
		a = s->pos * objxform;
		b = (s->pos + s->scale + Vec3F(0, 100, 1) ) * objxform;
		c = (cam->boxInFrustum(a, b)) ? 1 : .1;
		lod = fmin(max_lod, fmax(0, (a - cam->getPos()).Length() / range) );
		drawRect ( Vec2F((a.x-wmin.x)*ws, (a.z-wmin.z)*ws), Vec2F((b.x-wmin.x)*ws, (b.z-wmin.z)*ws), Vec4F(pal[lod], c) );
	}
	end2D();

	start3D(cam);
}





