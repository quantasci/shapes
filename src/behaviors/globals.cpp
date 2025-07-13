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

#include "globals.h"
#include "shapes.h"
#include "material.h"
#include "object_list.h"


void Globals::Define (int x, int y)
{
	AddInput( "material", 'Amtl' );		
	AddInput( "mesh",			'Amsh' );
	AddInput( "envmap",   'Aimg' );	

	AddParam( G_FPS,					"fps",	"f");					SetParamF  ( G_FPS, 0, 60.0 );
	AddParam( G_RATE,					"rate", "f");					SetParamF  ( G_RATE, 0,  1.0 );
	AddParam( G_ENVCLR,				"envclr", "4");				SetParamV4 ( G_ENVCLR, 0, Vec4F(0,0,0,0) );
	AddParam( G_EDITRES,			"edit_res", "3");			SetParamV3 ( G_EDITRES, 0, Vec3F(1024, 1024, 0) );
	AddParam( G_RECRES,				"record_res", "3");		SetParamV3 ( G_RECRES, 0, Vec3F(3840, 2160, 1) );
	AddParam( G_RECORD,				"record", "i" );			SetParamI  ( G_RECORD, 0, 0 );	
	AddParam( G_RECORD_START, "record_start", "i" ); SetParamI ( G_RECORD_START, 0, 0 );
	AddParam( G_TIMERANGE,		"timerange", "3" );		SetParamV3 ( G_TIMERANGE, 0, Vec3F(0, 0, 0) );		
	AddParam( G_RAY_SAMPLES,	"max_samples", "i");	SetParamI	 ( G_RAY_SAMPLES, 0, 64 );
	AddParam( G_RAY_DEPTHS,		"ray_depths", "4");		SetParamV4 ( G_RAY_DEPTHS, 0, Vec4F(1, 0, 1, 1));
	AddParam( G_BACKGROUND,   "backclr", "4");			SetParamV4 ( G_BACKGROUND, 0, Vec4F(0.1, 0.1, 0.25, 1));

	mEnvMap.Set ( 0, TEX_SETUP );		// need env setup
	mEnvMap.Set ( 4, NULL_NDX );

	SetInput ("mesh",	"model_square");	// screen quad 

	//--- Create global environment material	
	//
	Material* mtl = (Material*) gAssets.getObj ( "EnvMat_internal" );		
	if ( mtl==0x0 ) {		
			// use environment shader: orients screen quad at far dist of camera frustum
			mtl = (Material*) gAssets.AddObject ( 'Amtl', "EnvMat_internal" );			
			mtl->Define (0,0);
			mtl->SetInput ("shader", "shade_env");						// assign environment shader			
			std::string env_name = getInputAsName ( "envmap" );
			if (env_name.size()==0) {
				env_name = "color_white";				 
			}
			mtl->SetInput ("texture", env_name );	// assign environment image (global texture)
			mtl->Generate (0,0);			
	}
	SetInput ("material", "EnvMat_internal" );
}

Vec8S* Globals::getEnvmapTex()
{	
	if ( mEnvMap.x  == NULL_NDX ) return 0x0;					// no texture set
	if ( mEnvMap.x2 != NULL_NDX ) return &mEnvMap;		// texture set and ready
	
	// Find envmap image input
	mEnvMap = getInputTex ( "envmap" );

	return &mEnvMap;
}
Vec4F Globals::getEnvmapClr()
{
	return getParamV4 (G_ENVCLR);
}

void Globals::Generate(int x, int y)
{
	CreateOutput('Ashp');
	
	Vec8S tex = getInputTex( "envmap" );	
	Vec4F envclr = getEnvmapClr();

	if ( tex.x != NULL_NDX && envclr.w > 0) {

		// Create environment sphere
		dbgprintf ( "  Environment map. %s\n", getInputAsName("envmap").c_str() );
		Shape* s = AddShape();		
		s->matids = getInputMat ( "material" );
		s->meshids.x = getInputID ( "mesh", 'Amsh' );
		s->meshids.y = 0;
		s->meshids.w = -1;
		s->ids = getIDS(0, 0, 0);
		s->type = S_MESH;
		s->scale = Vec3F(1, 1, 1);		// shape transforms are not used by env shader

		// use object transform instead
		SetTransform( Vec3F(0,0,0), Vec3F(1,1,1) );
	}
	else {
		dbgprintf("  NO environment map.\n" );
	}
	
	MarkClean();
}




