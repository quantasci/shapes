
#ifndef DEF_DEFORM
	#define DEF_DEFORM

	#include "object.h"	
	
	class Deform : public Object {
	public:

		virtual objType getType()	{ return 'defm'; }
		virtual void Define (int x, int y);
		virtual void Generate (int x, int y);
		//virtual void Sketch (int w,int h, Camera3D* cam);
		//virtual void Run(float time);
		
		void DeformBend();
		void DeformTwist();
		void DeformFold ();

	private:

			
	};

#endif