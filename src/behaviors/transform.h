

#ifndef DEF_TRANSFORM
	#define DEF_TRANSFORM

	#include "object.h"	
	#include <vector>

	class Transform : public Object {
	public:
		Transform();

		virtual objType getType()	{ return 'tfrm'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);
		virtual void Sketch (int w, int h, Camera3D* cam);

		void SetXform (Vec3F pos, Vec3F angs, Vec3F scal );
		void SetXform (Vec3F pos, Quaternion rot, Vec3F scal );
		
	private:
		
		Vec3F m_pos;		
		Vec3F m_scal;
		Quaternion m_rot;
	};

#endif