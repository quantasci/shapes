

#include "instance.h"
#include "object_list.h"
#include "scene.h"
#include "shapes.h"
#include "points.h"

#include "gxlib.h"
using namespace glib;

#define I_SCALE		0

Instance::Instance() : Object()
{	
}

void Instance::Define(int x, int y)
{
	AddInput("time",		'Atim');			// instance updated every frame
	AddInput("mesh",		'Amsh');
	AddInput("shader",	'Ashd');
	AddInput("tex",			'Aimg');
	AddInput("material", 'Amtl');			// surface material (with texture and displacement)
	AddInput("points",	'Apnt');			// input points
	
	AddParam (I_SCALE, "scale", "3");		SetParamV3(I_SCALE, 0, Vec3F(1, 1, 1));

	mTimeRange.Set(0, 10000, 0 );
}

void Instance::Generate(int x, int y)
{
	Shape* s;

	Points* pnts = (Points*) getInputResult("points");
	if (pnts==0x0) return;

	CreateOutput('Ashp');
	
	SetOutputXform();

	// get material & mesh
	Vec8S mat = getInputMat("material");
	Vec4F mesh = Vec4F(getInputID("mesh", 'Amsh'), 0, 0, 0);

	int frad = pnts->FindChannel ( "radius" );
	if (frad == -1) {
		dbgprintf ( "ERROR: radius channel not found. need to handle this.\n");
		exit(-22);
	}

	// get points
	Vec3F* ppos = pnts->getPos(0);
	uint*  pclr = pnts->getClr(0);
	float* prad = pnts->getBufF (frad);
	int num_pnts = pnts->getNumPoints ();

	Vec3F scaling = getParamV3(I_SCALE);

	// create instances at points
	for (int n=0; n < num_pnts; n++ ) {
	
		// add shape		
		s = AddShape();
		s->ids = getIDS(0, 0, 0);
		s->type = S_MESH;
		s->matids = mat;
		s->meshids = mesh;
		s->clr = *pclr;
		s->pos = *ppos;
		s->scale = Vec3F(*prad, *prad, *prad) * scaling;
		s->pivot = Vec3F(0, 0, 0);

		ppos++;
		pclr++;
		prad++;
	}
	MarkDirty();
}

void Instance::Run(float time)
{
}







