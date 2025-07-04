

#ifndef DEF_POINT_PSYS
	#define DEF_POINT_PSYS

	#include "point_base.h"	

	class PointPSys : public PointBase {
	public:

		virtual objType getType()	{ return 'psys'; }
		virtual void Define (int x, int y);
		virtual void Generate (int x, int y);
		
		virtual void Run ( float time );		// particle systems animate

	protected:

	};

#endif