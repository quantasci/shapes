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

#ifndef DEF_MODULE
	#define DEF_MODULE

	#include "object.h"	
	#include "mersenne.h"
	#include <vector>

	class Params;

	class Module : public Object {
	public:
		Module();

		virtual objType getType()	{ return 'modl'; }
		virtual void Define (int x, int y);
		virtual void Generate (int x, int y);
		virtual void Run ( float time );		
		virtual void Render ();
		virtual void Sketch ( int w, int h, Camera3D* cam );

		void Rebuild (bool bRun);

		Shapes* GenerateVariant (Vec3I v, std::string vname, int rnd, Vec3I numvari, std::vector<Object*>& objlist, Params* params);
		Shapes* GenerateProxy   (Vec3I v, std::string vname, int rnd, Vec3I numvari );

		Shape SelectVariant(Vec3F vari, Vec3F& sz);
		Shape SelectVariant(int vari, Vec3F& sz);

		bool CheckForParams ();
		void RandomizeParams ();
		std::string RandomizeValueTypeless ( std::string vmin, std::string vmax, uchar typ );

	private:		

		Mersenne	m_rand;

		Params*		m_params;
	};

#endif