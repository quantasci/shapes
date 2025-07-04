
#include "parts.h"
#include "string_helper.h"
#include "camera.h"
#include "shapes.h"
#include "object_list.h"
#include "mesh.h"

#include "gxlib.h"
using namespace glib;

#include <stack>
#include <vector>

#ifdef USE_WIDGETS
	#include "widget.h"
#endif

bool Parts::RunCommand(std::string cmd, vecStrs args)
{
	if (cmd.compare("meshes") == 0) { LoadFolder(args[0]);			return true; }
	if (cmd.compare("save") == 0) { Save ( args[0] );				return true; }
	if (cmd.compare("load") == 0) { Load ( args[0], strToF(args[1])); return true; }

	if (cmd.compare("finish") == 0) { return true; }

	return false;
}

void Parts::Define (int x, int y)
{
	mSel.Set (-1, -1, -1);

	AddInput ( "shader", 'Ashd' );
	AddInput ( "tex",    'Aimg' );	
	AddInput ( "folder", '****' );

	SetInput ( "shader", "shade_mesh" );	// default shader for this object
}

void Parts::Generate (int x, int y)
{
	MarkClean();

	if ( mOutput != OBJ_NULL ) return;

	CreateOutput ( 'Ashp' );	

	// SetShapeShaderFromInput ("shader");		//--- obsolete
	// SetShapeTex ( G_TEX_UNI, "tex", "" );	//--- obsolete

	SetOutputXform ();
}

void Parts::Clear()
{
	mPartList.clear();		// clear parts
}

void Parts::Save ( std::string fname )
{
	if (mOutput == OBJ_NULL) return;

	char buf[1024];
	std::string name;
	Object* obj;
	Shape* s;
	Quaternion rot;
	std::string str, val, mesh_name, full_name, extra, append;
	strcpy ( buf, fname.c_str() );
	FILE* fp = fopen ( buf, "wt" );

	for (int n = 0; n < mPartList.size(); n++) {
		s = getShape(n);
		obj = gAssets.getObj( mPartList[n].asset_id );
		mesh_name = mPartList[n].asset_name;
		append = mPartList[n].append;
		extra = mPartList[n].extra;
		if (extra.length() > 0) extra = ":" + extra;
		rot = s->rot;
		fprintf(fp, "%s:%s:p<%f,%f,%f>:q<%f,%f,%f,%f>:v<%f,%f,%f>:s<%f,%f,%f>%s\n", mesh_name.c_str(), append.c_str(), s->pos.x, s->pos.y, s->pos.z, (float)rot.X, (float)rot.Y, (float)rot.Z, (float)rot.W, s->pivot.x, s->pivot.y, s->pivot.z, s->scale.x, s->scale.y, s->scale.z, extra.c_str() );
	}

	fclose(fp);
}

void Parts::AddPart (std::string mesh_name, int mid, std::string append, std::string extra, int inst)
{
	Part p;
	p.asset_id = mid;
	strncpy(p.asset_name, mesh_name.c_str(), 256);
	strncpy(p.append, append.c_str(), 256);
	strncpy(p.extra, extra.c_str(), 256);
	p.inst = inst;

	mPartList.push_back( p );
}


bool Parts::Load ( std::string fname, float sz )
{
	if (mOutput == OBJ_NULL) Generate(0, 0);

	char buf[1024];
	Shape* s;
	Vec3F ctr, pos, piv, scal;
	Vec4F q;
	Quaternion rot;	
	std::string str, val, mesh_name, full_name, extra, append;
	Vec3F unitscale;
	int inst = 0;
	Mesh* m;

	mUniformSz = sz;

	// Read part file
	strcpy ( buf, fname.c_str() );	
	FILE* fp = fopen ( buf, "rt" );	
	if ( fp==0x0 ) return false;		

	// Read parts
	int n = 0;
	while ( !feof (fp) ) {
		fgets ( buf, 1024, fp );		// read parts
		str = buf;

		if ( str.find(":") != std::string::npos ) {
			
			// load part data			
			mesh_name = strSplitLeft( str, ":" );												// name of asset (mesh)
			append = strSplitLeft(str, ":");													// appended to full name
			full_name = mesh_name + "_" + append;
			val = strSplitLeft ( str, ":" );	strToVec3( val, '<', ',', '>', &pos.x );		// position
			val = strSplitLeft ( str, ":" );	strToVec4( val, '<', ',', '>', &q.x ); rot.set(q.x, q.y, q.z, q.w);		// orientation
			val = strSplitLeft ( str, ":" );	strToVec3( val, '<', ',', '>', &piv.x );		// pivot
			val = strSplitLeft ( str, ":\n" );	strToVec3( val, '<', ',', '>', &scal.x );		// scale
			val = strSplitLeft(str, ":\n");	
			extra = "";
			if (val.length() > 0) {																// extra data
				extra = val;
				//dbgprintf("%s -> joint override: %s\n", full_name.c_str(), val.c_str());
			}

			// find matching asset object
			m = (Mesh*) gAssets.getObj ( mesh_name );
			if (m != 0x0) {
				// mesh found
				unitscale = m->NormalizeMesh (sz, ctr );		// normalize to unit cube. sz = uniform global scaling

				// add shape
				s = AddShape();				
				s->ids = getIDS(n, 0, 0);
				s->type = S_MESH;
				s->meshids.x = m->getID();						// mesh	asset ID				
				s->clr = COLORA(0.7, 0.7, 0.7, 1.0);
				s->pos = pos;
				s->rot = rot;
				s->scale = scal;
				s->pivot = piv;				
				n++;

				// add part
				AddPart (mesh_name, m->getID(), append, extra, inst);
			}
		}
	}
	fclose(fp);

	MarkClean();
	return true;
}

void Parts::LoadFolder (std::string path )
{
	if ( mOutput==OBJ_NULL ) Generate(0,0);

	// Load all obj files in path
	std::vector<int>	mMeshList;

	dbgprintf ( "ERROR: FIX THIS FUNC" );
	exit(-2);

	//int ok = gAssets.LoadDir ( path, "*.obj", failed, mMeshList );
}

void Parts::RescaleAll ( Vec3F scale ) 
{
	Shape* s;
	for (int n=0; n < getNumShapes(); n++ ) {		
		s = getShape ( n );			
		s->scale = scale;	
	}
}
void Parts::PlaceParts ( Vec3F vmin, Vec3F vmax, float spacing )
{
	Shape* s;
	Vec3F p = vmin;
	for (int n=0; n < getNumShapes(); n++ ) {
		s = getShape ( n );			
		p.x += spacing;
		if ( p.x > vmax.x ) {p.x = vmin.x; p.z += spacing; }
		if ( p.z > vmax.z ) {p.z = vmin.z; p.y += spacing; }
		s->pos = p;
	}
}

void Parts::Sketch ( int w, int h, Camera3D* cam )
{
	Shape* s;
	std::string name;
	Vec3F a, b;

	float txtsz = 0.01;			// part text size

	setTextSz ( txtsz, 1 );

	for (int n=0; n < getNumShapes(); n++ ) {
		s = getShape(n);
		name = mPartList[n].asset_name;
		//drawText3D ( cam, s->pos.x + txtsz, s->pos.y + txtsz, s->pos.z, (char*) name.c_str(), 1, 1,0, 1); 		
	}
	if (mSel.x != -1) {
		s = getShape(mSel.x);
		Matrix4F xform = s->getXform();
		drawBox3D (Vec3F(0, 0, 0), Vec3F(1, 1, 1), Vec4F(1,1,1,1), xform);
	}
}

bool Parts::Raytrace ( Vec3F pos, Vec3F dir, Vec3I& sel, Vec3F& hit, Vec3F& norm )
{
	Vec3I vndx;	
	Vec3F vnear, vhit, vnorm;
	float dist, best_dist;
	Shape* s;
	Mesh* obj;

	dir.Normalize();
	
	mSel.x = -1;
	best_dist = 1e10;
	
	for (int n=0; n < getNumShapes(); n++ ) {

		s = getShape(n);										// shape - orientation of mesh

		obj = (Mesh*) gAssets.getObj ( mPartList[n].asset_id );			// mesh geometry

		if ( obj != 0x0 ) {
			Matrix4F xform = s->getXform();
			if ( obj->Raytrace ( pos, dir, xform, vndx, vnear, vhit, vnorm ) ) {
				
				dist = pos.Dist(vhit);			// distance to mesh hit
				
				if (dist < best_dist) {			// get nearest distance mesh
					best_dist = dist;
					mSel.x = n;					// shape ID
					mSel.y = vndx.y;			// vertex #
					mSel.z = 0;
					sel = mSel;
					hit = vhit;					
					norm = vnorm;				// mesh normal at hit point
				}				
			}
		}
	}
	if (mSel.x == -1) return false;
	return true;
}

bool Parts::Select3D (int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir)
{
	Shape* s;
	if ( mSel.y != -1 ) {
		s = getShape ( mSel.y );
		s->clr = COLORA(0.7,0.7,0.7,1);						// de-highlight previously selected
	}
	mSel = sel;
	s = getShape( mSel.y );									// select a sub-shape (given by sel.x), from picking buffer
	if ( s == 0x0 ) return false; 
	s->clr = COLORA(1.0,1.0,1.0,1);							// color as highlighted
	
	AssignShapeToWidget3D ( s, w, sel );					// assign shape orientation to 3D edit widget	
	name = gAssets.getObj( mPartList[sel.y].asset_id )->getName(); // return the name of selected part

	return true;
}

void Parts::Adjust3D ( int w, Vec4F& sel ) 
{
	if (sel.y==-1) return;	
	Shape* s = getShape ( sel.y );
	if ( s==0x0 ) return;

	std::string name;
	AssignWidgetToShape3D(s, w, name);
	// gAssets.getObj(mPartList[sel.y].asset_id)->getName() = name; 
}
