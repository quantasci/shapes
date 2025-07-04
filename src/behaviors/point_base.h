

#ifndef DEF_POINT_BASE
	#define DEF_POINT_BASE

	#include "object.h"	

	class PointBase : public Object {
	public:

		virtual objType getType()	{ return 'pnts'; }
		virtual void Sketch ( int w, int h, Camera3D* cam );

	protected:

	};

#endif