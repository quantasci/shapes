
#ifndef DEF_SCATTER

	#include "object.h"	
	#include "mersenne.h"
	#include "wang_tiles.h"
	#include <vector>

	class Scatter : public Object {
	public:
		Scatter();

		virtual objType getType() { return 'scat'; }
		virtual void Define(int x, int y);
		virtual void Generate(int x, int y);
		virtual void Run(float time);
		virtual void Render();
		virtual void Sketch(int w, int h, Camera3D* cam);

		void ScatterOverHeightfield();

	private:

		Mersenne	m_rand;
		WangTiles	m_wt;
		int			m_numlod;
		int			m_numvari;
		int				m_numsrc;
		Shapes*		m_srcgrp[100];

		uchar		m_distlod[1024];
	};

#endif