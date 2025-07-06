//-------------------------
// Copyright 2020-2025 (c) Quanta Sciences, Rama Hoetzlein
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//--------------------------

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