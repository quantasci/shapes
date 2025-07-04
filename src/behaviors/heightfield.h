

#ifndef DEF_HEIGHTFIELD
	#define DEF_HEIGHTFIELD

	#include "object.h"	
	#include <vector>

	class Heightfield : public Object {
	public:
		Heightfield();

		virtual objType getType()	{ return 'hgtf'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);
		virtual void Run ( float time );		
		virtual void Render ();
		virtual void Sketch ( int w, int h, Camera3D* cam );
	
	private:		

		Vec3I	m_res;		// cache image res		
	};

#endif