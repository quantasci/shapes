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

#include "lightset.h"
#include "object.h"
#include "render.h"

#include "gxlib.h"
using namespace glib;

#define L_LIGHT0		0

void LightSet::Define (int x, int y)
{
	CreateOutput('Ashp');

	AddParam( L_LIGHT0,	 "light",	"33333");	
		SetParamV3( L_LIGHT0, 0, Vec3F(100,200,50) );		// pos
		SetParamV3( L_LIGHT0, 1, Vec3F(0, 0, 0) );				// target
		SetParamV3( L_LIGHT0, 2, Vec3F(0.1, 0.1, 0.1) );			// ambient clr
		SetParamV3( L_LIGHT0, 3, Vec3F(0.7,0.7,0.7) );		// diffuse clr
		SetParamV3( L_LIGHT0, 4, Vec3F(0,0,0) );					// shadow clr
}

void LightSet::SetLight(int lgt, Vec3F pos, Vec3F amb, Vec3F diff, Vec3F shad)
{
	SetParamV3( L_LIGHT0 + lgt, 0, pos );
	SetParamV3( L_LIGHT0 + lgt, 1, Vec3F(0,0,0) );
	SetParamV3( L_LIGHT0 + lgt, 2, amb );
	SetParamV3( L_LIGHT0 + lgt, 3, diff );
	SetParamV3( L_LIGHT0 + lgt, 4, shad );
}

void LightSet::Update()
{
	RLight lgt;
	lgt = m_Lights[0];

	// Update the parameters from the RLights
	SetParamV3 ( L_LIGHT0, 0, lgt.pos );
	SetParamV3 ( L_LIGHT0, 1, lgt.target );
	SetParamV3 ( L_LIGHT0, 2, lgt.ambclr );
	SetParamV3 ( L_LIGHT0, 3, lgt.diffclr );
	SetParamV3 ( L_LIGHT0, 4, lgt.shadowclr );	
}

void LightSet::Generate (int x, int y)
{
	int j;
	std::vector<int> list;
	GetParamArray ( "light", list );		// get parameter array, e.g. light[00], light[01], ..
	
	RLight lgt;
	m_Lights.clear ();
	for (int n=0; n < list.size(); n++ ) {
		j = list[n];
		lgt.pos =		Vec4F(getParamV3 ( j, 0 ), 0);
		lgt.target =	Vec4F(getParamV3 ( j, 1 ), 0);
		lgt.ambclr =	Vec4F(getParamV3 ( j, 2 ), 1);
		lgt.diffclr =	Vec4F(getParamV3 ( j, 3 ), 1);
		lgt.shadowclr = Vec4F(getParamV3 ( j, 4 ), 1);
		lgt.specclr.Set(0,0,0);
		m_Lights.push_back ( lgt );
	}
}
/*void LightSet::SetCone ( int l, float a1, float a2 )
{
	m_Lights[l].cone.Set (  cos(0*DEGtoRAD), cos(a1*DEGtoRAD), cos(a2*DEGtoRAD) );
}*/


void LightSet::Run(float time)
{
	MarkClean();
}

void LightSet::Sketch ( int w, int h, Camera3D* cam )
{
	RLight lgt;

	Vec3F p1, p2;
	for (int n=0; n < m_Lights.size(); n++) {
		lgt = m_Lights[n];
		p1 = lgt.pos - Vec3F(1,1,1);
		p2 = lgt.pos + Vec3F(1,1,1);
		drawBox3D ( p1, p2, Vec4F(lgt.diffclr, 1.0f) );
	}
}
