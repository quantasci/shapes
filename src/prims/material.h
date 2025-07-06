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

#ifndef DEF_MATERIAL
	#define DEF_MATERIAL

	#include <string>		
	#include "vec.h"
	#include "object.h"

	#define M_SHADER			0
	#define M_TEXIDS			1
	#define M_LGT_WIDTH		2
	#define M_SHADOW_CLR	3
	#define M_SHADOW_BIAS	4
	#define M_AMB_CLR			5
	#define M_DIFF_CLR		6
	#define M_SPEC_CLR		7
	#define M_SPEC_POWER	8	
	#define M_ENV_CLR			9
	#define M_REFL_CLR		10
	#define M_REFL_WIDTH	11
	#define M_REFL_BIAS		12
	#define M_REFR_CLR		13
	#define M_REFR_WIDTH	14
	#define M_REFR_BIAS		15
	#define M_REFR_IOR		16
	#define M_EMIS_CLR		17
	#define M_DISPLACE_AMT	18
	#define M_DISPLACE_DEPTH	19
	#define M_FLIP_Y			20

	class Material : public Object {
	public:
		Material ();
		virtual objType	getType()		{ return 'Amtl'; }
		virtual void	Clear();
		virtual void	Define (int x, int y);
		virtual void	Generate(int x, int y);
		virtual bool	Load ( std::string fname )	{ return true; }			// shaders are defer loaded for when there is a GL context

		//-- member vars		

	};

#endif



