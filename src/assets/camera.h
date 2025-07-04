

#ifndef DEF_VOLUME
	#define DEF_VOLUME

	#include "object.h"	
	#include "camera3d.h"
	#include "spline.h"
	
	#define C_FOV		0
	#define C_NEARFAR	1
	#define C_DISTDOL	2	
	#define C_ANGS		3	
	#define C_FROM		4
	#define C_TO		5
	#define C_DOF		6
	#define C_KEY		7

	class Camera : public Spline, public Object {			// Object wrapper for Camera3D 
	public:
		Camera() {};
		~Camera() {};

		virtual objType getType()	{ return 'camr'; }			
		virtual void Clear() {};
		virtual void Define (int x, int y);
		virtual void Generate(int x, int y);
		virtual void Run(float t);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Sketch ( int w, int h, Camera3D* cam );
		
		void		WriteFromCam3D ( Camera3D* cam);
		void		ReadToCam3D ( Camera3D* cam );
		Camera3D*	getCam3D() { return &cam3d; }

		void		UpdateKeys ();
		void		AddKey (float time);	
		float		getEndTime ()	{ return m_EndTime; }

		Camera3D	cam3d;

		std::vector<int>	m_Keys;
		float				m_EndTime;
	};

#endif

