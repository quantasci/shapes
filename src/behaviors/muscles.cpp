

#include "muscles.h"
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

bool Muscles::RunCommand(std::string cmd, vecStrs args)
{
	if (cmd.compare("load") == 0) { Load( args[0] ); return true; }

	if (cmd.compare("finish") == 0) { return true; }

	return false;
}

void Muscles::Define (int x, int y)
{
	mSel.x = -1;

	AddInput ( "shader", 'Ashd' );
	AddInput ( "mesh",	 'Amsh' );
	AddInput ( "tex",    'Aimg' );		
	AddInput ( "folder", '****' );

	SetInput("shader",	"shade_mesh");
	SetInput("mesh",	"model_cube");		
	SetInput("tex",		"color_white");
}

void Muscles::Generate (int x, int y)
{
	if ( mOutput != OBJ_NULL ) return;
	
	CreateOutput ( 'Ashp' );	
	//SetShapeShaderFromInput("shader");		//--- obsolete
	//SetShapeMeshFromInput("mesh");			//--- obsolete
	//SetShapeTex(G_TEX_UNI, "tex", "");		//--- obsolete
	SetOutputXform ();

	MarkClean();
}
void Muscles::Run ( float time )
{
}

void Muscles::Clear()
{
	mPartList.clear();
}

void Muscles::Save(std::string fname)
{
	if (mOutput == OBJ_NULL) return;

	char buf[1024];
	std::string name;
	strcpy(buf, fname.c_str());
	FILE* fp = fopen(buf, "wt");
	Muscle* m;

	for (int n = 0; n < mPartList.size(); n++) {
		m = &mPartList[n];
		name = m->name;
		fprintf(fp, "%s:p1<%f,%f,%f>:p2<%f,%f,%f>:b1<%d,%d,%d>:b2<%d,%d,%d>:s<%f,%f,%f>:lac<%f,%f,%f>:norm<%f,%f,%f>\n", name.c_str(), m->p1.x, m->p1.y, m->p1.z, m->p2.x, m->p2.y, m->p2.z, m->b1.x, m->b1.y, m->b1.z, m->b2.x, m->b2.y, m->b2.z, m->specs.x, m->specs.y, m->specs.z, m->rest_len, m->angle, m->curve, m->norm.x, m->norm.y, m->norm.z );
	}
	fclose(fp);
}

bool Muscles::Load(std::string fname)
{
	if (mOutput == OBJ_NULL) Generate(0, 0);

	char buf[1024];
	Vec3F vec;
	std::string str, val, name;
	Muscle m;
	Muscle* ms;

	// Read part file
	strcpy(buf, fname.c_str());
	FILE* fp = fopen(buf, "rt");
	if (fp == 0x0) return false;

	// Read parts
	int n = 0;
	while (!feof(fp)) {
		fgets(buf, 1024, fp);		// read parts
		str = buf;

		if (str.find(":") != std::string::npos) {

			// load part data			
			name = strSplitLeft(str, ":");												// name of asset 
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x); m.p1 = vec;		// p1
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x); m.p2 = vec;		// p2
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x); m.b1 = vec;		// b1
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x); m.b2 = vec;		// b2
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x); m.specs = vec;	// specs			
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x);					// len, angle, curve
			m.rest_len = vec.x;	m.angle = vec.y; m.curve = vec.z;
			val = strSplitLeft(str, ":");	strToVec3(val, '<', ',', '>', &vec.x); m.norm = vec;	// norm
			
			// add muscle
			int i = AddMuscle (m.p1, m.p2, m.b1, m.b2, m.norm);
			ms = getMuscle(i);	
			strncpy(ms->name, name.c_str(), 128);
			ms->specs = m.specs;
			ms->rest_len = m.rest_len;
			ms->curve = m.curve;
			ms->angle = m.angle;
		}
	}
	fclose(fp);

	MarkClean();
	return true;
}

int Muscles::AddMuscle(Vec3F p1, Vec3F p2, Vec3I b1, Vec3I b2, Vec3F norm)
{
	// add muscle
	Muscle m;
	m.p1 = p1;
	m.p2 = p2;
	m.b1 = b1;
	m.b2 = b2;
	p2 -= p1;
	float muscle_len = p2.Length();
	strcpy(m.name, "");
	m.rest_len = muscle_len;			// muscle rest length
	m.norm = norm;	
	m.specs = Vec3F(.15f, 1.0f, .15f);
	m.angle = 0;
	m.curve = 0;
	mPartList.push_back(m);

	// add shape
	int n = getNumShapes();
	Shape* s = AddShape();
	s->ids = getIDS( n, 0, 0 );
	s->type = S_MESH;
	s->clr = COLORA(1.0, 0.7, 0.7, 1.0);
	s->pos = p1;	
	s->pivot = Vec3F(-.5, 0, -.5);
	
	s->rot.fromRotationFromTo(Vec3F(0, 1, 0), p2);	// base-pose orientation
	s->scale = m.specs * muscle_len;				// length of muscle

	// shape muscle data
	Vec3F* sh_muscle = (Vec3F*) getShapeData (BMUSCLE);		
	sh_muscle[n] = Vec3F(m.angle, m.curve, 0.f);

	return (int) mPartList.size() - 1;
}
void Muscles::DeleteMuscle(int i)
{
	DeleteShape(i);

	int last = (int) mPartList.size() - 1;
	if (i < last) {		
		mPartList[i] = mPartList[last];				// copy last muscle to deleted
	}
	mPartList.erase(mPartList.begin() + last);		// erase last entry
}


bool Muscles::Select3D(int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir)
{
	Shape* s;
	if (mSel.y != -1) {
		s = getShape(mSel.y);
		s->clr = COLORA(1.0, 0.7, 0.7, 1);					// de-highlight previously selected
	}
	mSel = sel;
	s = getShape(mSel.y);									// select a sub-shape (given by sel.x), from picking buffer
	if (s == 0x0) { mSel.y = -1;  return false; }
	s->clr = COLORA(1.0, 0.3, 0.3, 1);						// color as highlighted

	AssignShapeToWidget3D(s, w, sel);					// assign shape orientation to 3D edit widget	
	name = mPartList[sel.y].name;

	return true;
}

void Muscles::Adjust3D (int w, Vec4F& sel)
{
	if (sel.y == -1) return;
	Shape* s = getShape(sel.y);
	if (s == 0x0) return;
			
	std::string name;
	AssignWidgetToShape3D(s, w, name);							// assign the 3D edit widget back to the shape (on mouse release)
	strncpy( mPartList[sel.y].name, name.c_str(), 128 );		// set the new name
}

void Muscles::Sketch ( int w, int h, Camera3D* cam )
{
	Vec3F p1, p2;
	Shape* s;
	
	for (int n = 0; n < getNumShapes(); n++) {
		s = getShape(n);
		p1 = s->pos;
		p2 = Vec3F(0, s->scale.y, 0); p2 *= s->rot; p2 += p1;
		drawLine3D(p1, p2, Vec4F(1, 0, 0, 1));
	}
	if (mSel.x != -1) {
		s = getShape(mSel.x);
		Matrix4F xform = s->getXform();
		drawBox3D (Vec3F(-0.5, -0.5, -0.5), Vec3F(.5, .5, .5), Vec4F(1,1,1,1), xform);
	}
}

