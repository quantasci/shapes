
#include "point_base.h"
#include "points.h"

#include "gxlib.h"
using namespace glib;

void PointBase::Sketch  ( int w, int h, Camera3D* cam )
{
	Points* pnts = getOutputPoints();
	if (pnts==0x0) return;

	Vec3F bmin = pnts->getBMin();
	Vec3F bmax = pnts->getBMax();
	drawBox3D ( bmin, bmax, Vec4F(1,1,1,1) );
}





