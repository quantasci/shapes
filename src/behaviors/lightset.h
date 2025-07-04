

#ifndef DEF_LIGHTSET
	#define DEF_LIGHTSET

	#include "object.h"	
	#include "render_gl.h"
	
	class LightSet : public Object {
	public:

		virtual objType getType()		{ return 'lgts'; }		
		virtual void Define (int x, int y);
		virtual void Generate (int x, int y);		
		virtual void Run(float time);
		virtual void Sketch (int w, int h, Camera3D* cam );	

		void Update ();

		int getNumLights()			{ return (int) m_Lights.size(); }
		RLight* getLight(int n)		{ return (n < m_Lights.size()) ? &m_Lights[n] : &m_Lights[0]; }
		RLight* getLightBuf()		{ return (RLight*) &m_Lights[0]; }

		void SetLight ( int lgt, Vec3F pos, Vec3F amb, Vec3F diff, Vec3F shad );

	private:

		std::vector<RLight>		m_Lights;
	};

#endif