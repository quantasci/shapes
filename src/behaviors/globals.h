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

#ifndef DEF_GLOBALS
	#define DEF_GLOBALS

	#include "object.h"	
	#include <vector>

	#define G_FPS				0
	#define G_RATE					1
	#define G_ENVCLR				2
	#define G_EDITRES				3
	#define G_RECRES				4	
	#define G_RECORD				5	
	#define G_RECORD_START	6
	#define G_TIMERANGE			7
  #define G_RAY_SAMPLES		8
	#define G_RAY_DEPTHS		9
	#define G_BACKGROUND		10

	class Globals : public Object {
	public:
		Globals() : Object() {};

		virtual objType getType()	{ return 'glbs'; }
		virtual void Define (int x, int y);
		virtual void Generate(int x, int y);

		// Retrieve global vals
		float		getFPS()		{ return getParamF(G_FPS); }
		float		getRate()		{ return getParamF(G_RATE); }		
		Vec8S*	getEnvmapTex();
		Vec4F		getEnvmapClr();
		int			getMaxSamples()	{ return getParamI(G_RAY_SAMPLES); }
		Vec4F		getRayDepths()  { return getParamV4(G_RAY_DEPTHS); }
		Vec4F		getBackgrdClr()	{ return getParamV4(G_BACKGROUND); }
	

	private:

		Vec8S	mEnvMap;
	};

#endif