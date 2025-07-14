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

#include "module.h"
#include "object_list.h"
#include "shapes.h"
#include "scene.h"
#include "imagex.h"
#include "params.h"
#include "timex.h"

#define M_NAME		0
#define M_SEED		1
#define M_RES		2
#define M_SPACING	3
#define M_HGT		4

#define M_VAR1PARAM	5
#define M_VAR1MIN	6
#define M_VAR1MAX	7
#define M_VAR1DIV	8

#define M_VAR2PARAM	9
#define M_VAR2MIN	10
#define M_VAR2MAX	11
#define M_VAR2DIV	12

// MODULE
// --------
// Input: objlist = <obj1, obj2.. objn>    // list of behavior objects
// Regenerate:
//   objk -> outk                          // behavior is re-evaluated to generate new output shapes
// New Assets:
//   variant0 = Shapes< out1[0], out2[0], .. outn[0] >   // regenerated variant. See GenerateVariant
//   variant1 = Shapes< out1[1], out2[1], .. outn[1] >
//   variantn = Shapes< out1[n], out2[n], .. outn[n] >
// Output:
//   Shapes < variant1, variant2, .. variantn >    // list of all variants. Shapes type = OBJ_SHAPEGRP
//

Module::Module() : Object()
{

}

void Module::Define (int x, int y)
{
//	AddInput( "time",	 'Atim');		
	AddInput( "proxy0", 'Aimg');
	AddInput( "proxy1", 'Aimg');
	AddInput( "proxy2", 'Aimg');
	AddInput( "mesh",   'Amsh');
	AddInput( "shader", 'Ashd');
	AddInput( "minobj", '****');
	AddInput( "maxobj", '****');	
	AddInput( "object", 'LIST' );		// list of objects

	SetInput ( "mesh", "square" );
	SetInput ( "shader", "shade_proxy" );
	
	AddParam(M_NAME,		"name",		"s");	SetParamStr(M_NAME, 0, "module" );
	AddParam(M_SEED,		"seed",		"i");	SetParamI  (M_SEED, 0, 156 );
	AddParam(M_RES,			"res",		"I");	SetParamI3 (M_RES, 0, Vec3I(4,4,1) );
	AddParam(M_SPACING,		"spacing",	"3");	SetParamV3 (M_SPACING, 0, Vec3I(1,1,1) );
	AddParam(M_HGT,			"height",	"f");	SetParamF ( M_HGT, 0, 1 );

	AddParam(M_VAR1PARAM,	"param1",	"s");	SetParamStr(M_VAR1PARAM, 0, "" );					// "lod" = variable lod (X-axis)
	AddParam(M_VAR1MIN,		"min1",		"3");	SetParamV3 (M_VAR1MIN, 0, Vec3F(0,0,0) );
	AddParam(M_VAR1MAX,		"max1",		"3");	SetParamV3 (M_VAR1MAX, 0, Vec3F(3,1,1));		// 0 to 3
	AddParam(M_VAR1DIV,		"div1",		"i");	SetParamI(M_VAR1DIV, 0, 0);

	AddParam(M_VAR2PARAM,	"param2",	"s");	SetParamStr(M_VAR2PARAM, 0, "");
	AddParam(M_VAR2MIN,		"min2",		"3");	SetParamV3 (M_VAR2MIN, 0, Vec3F(0, 0, 0));
	AddParam(M_VAR2MAX,		"max2",		"3");	SetParamV3 (M_VAR2MAX, 0, Vec3F(1, 1, 1));
	AddParam(M_VAR2DIV,		"div2",		"i");	SetParamI (M_VAR2DIV, 0, 0);

	mTimeRange.Set(0,10000,0);
}


Shapes* Module::GenerateProxy(Vec3I v, std::string vname, int rnd, Vec3I numvari)
{
	Shapes* vari_shapes;		// variant shapes
	
	// pack proxy textures into texid
	Vec8S texids (NULL_NDX);
	getInputTex( "proxy0", texids, 0 );
	getInputTex( "proxy1", texids, 1 );
	getInputTex( "proxy2", texids, 2 );

	// get proxy width & height
	ImageX* img = (ImageX*) getInput ( "proxy0" );
	float hgt = getParamF ( M_HGT );
	float wid = hgt * float(img->GetHeight()) / img->GetWidth();

	int mesh, shdr;
	mesh = getInputID ( "mesh", 'Amsh' );
	shdr = getInputID ( "shader", 'Ashd' );

	// create variant 
	std::string name = getName() + vname;				// output = object + variant
	vari_shapes = (Shapes*) gAssets.FindObj(name);
	if (vari_shapes == 0x0) vari_shapes = (Shapes*) gAssets.AddObject('Ashp', name);
	vari_shapes->Clear();

	// add proxy shapes
	Shape* s;
	int si;
	Quaternion q, r;
	r.fromAngleAxis( 90 * DEGtoRAD, Vec3F(0, 0, 1));		// rotate vertically
	
	float ang = float(v.y) * 180.0f / numvari.y;
	
	s = vari_shapes->Add(si);
	q.fromAngleAxis( ang * DEGtoRAD, Vec3F(0, 1, 0));
	s->rot = q * r;
	s->meshids = Vec4F( mesh, shdr, 0, 0 );
	s->pos.Set(0,0,0);
	s->scale.Set (hgt/2, .01, wid/2);
	s->pivot.Set (1, 0, 0);	
	
	return vari_shapes;
}

Shapes* Module::GenerateVariant (Vec3I v, std::string vname, int rnd, Vec3I numvari, std::vector<Object*>& objlist, Params* params)
{
	Shapes* vari_shapes;				// variant shapes
	Vec3F pos, v1, v2, v3;
	int pid;
	Vec3F v1min, v1max, v2min, v2max, v3min, v3max;
	std::string key, val;
	int v1div, v2div;
	Object* obj;
	Shapes* src_shapes;
	
	std::string v1param = getParamStr(M_VAR1PARAM);
	std::string v2param = getParamStr(M_VAR2PARAM);
	std::string v3param = "";
	
	v1min = getParamV3(M_VAR1MIN);
	v1max = getParamV3(M_VAR1MAX);
	v1div = getParamI(M_VAR1DIV);

	v2min = getParamV3(M_VAR2MIN);
	v2max = getParamV3(M_VAR2MAX);
	v2div = getParamI(M_VAR2DIV);

	// objects in module
	for (int n = 0; n < objlist.size(); n++) {
		obj = objlist[n];

		// set the variant name
		pid = obj->getParamByName("name");
		if (pid != -1) obj->SetParamStr(pid, 0, vname);		// set variant name for inputs that self-generate 

		// apply variant parameters
		if (!v1param.empty()) {			
			if (v1div == 0) {
				v1 = (numvari.x==1) ? v1min : v1min + (v1max - v1min) * (float(v.x) / (numvari.x - 1));
			}
			else {
				float t = float(v.x) / (numvari.x - 1);
				float t2 = float(v.x + 1) / (numvari.x - 1); if (t2 > 1) t2 = 1;
				v1 = Vec3F(v1min.x + t * (v1max.x - v1min.x), v1min.x + t2 * (v1max.x - v1min.x), 0);
				int q = 1;
			}
			pid = obj->getParamByName(v1param);			// find the parameter
			if (pid != -1) obj->SetParam(pid, v1);		// typeless set
		}
		if (!v2param.empty()) {
			v2 = v2min + (v2max - v2min) * float(v.y) / (numvari.y - 1);
			pid = obj->getParamByName(v2param);			// find the parameter
			if (pid != -1) obj->SetParam(pid, v2);		// typeless set
		}
		if (!v3param.empty()) {
			v3 = v3min + (v3max - v3min) * float(v.z) / (numvari.z - 1);
			pid = obj->getParamByName(v3param);				// find the parameter
			if (pid != -1) obj->SetParam(pid, v3);		// typeless set
		}

		// apply parameter space 
		if ( params ) {
			for (int i=0; i < params->getNumParam(); i++ ) {
				key = params->getParamName (i);					// get param from parameter space
				val = params->getParamValueAsStr ( i );			// get param value
				pid = obj->getParamByName ( key );				// find the param on target object
				if ( pid != -1) obj->FindOrCreateParam ( key, val );		// set the param (only if found)
			}
		}
	}

	// create variant 
	std::string name = getName() + "_" + vname;				// output = object + variant
	vari_shapes = (Shapes*) gAssets.FindObj(name);
	if (vari_shapes == 0x0) vari_shapes = (Shapes*) gAssets.AddObject('Ashp', name);
	vari_shapes->Clear();

	// Regenerate subgraph - one timestep 
	//gScene->RegenerateSubgraph(objlist, rnd, true);

	// Rebuild subgraph - wait for object(s) to complete
	PERF_PUSH ( "Generate" );
	gScene->RebuildSubgraph(objlist, rnd);		
	PERF_POP(); 

	// Add all output shapes to variant 	
	for (int i = 0; i < objlist.size(); i++) {
		obj = objlist[i];
		if (obj->isVisible()) {
			src_shapes = objlist[i]->getOutShapes();
			vari_shapes->AddFrom(0, src_shapes, 0, 4);			// filter based on lod (optional)				
		}
	}
	
	return vari_shapes;
}

bool Module::CheckForParams ()
{	
	std::string key, val;
	uchar typ;
	
	Object* minobj = getInput ( "minobj" );
	Object* maxobj = getInput ( "maxobj" );

	if ( minobj == 0x0 || maxobj == 0x0 ) {			
		m_params = 0x0;							// no params
		return false; 
	}		

	// build parameter space
	m_params = new Params;	
	for (int n=0; n < minobj->getNumParam(); n++ ) {
		key = minobj->getParamName(n);
		val = minobj->getParamValueAsStr ( n );
		typ = minobj->getParamType ( n );
		m_params->AddParam ( n, key, std::string(1,typ) );
	}
	return true;
}

std::string Module::RandomizeValueTypeless ( std::string valmin, std::string valmax, uchar typ )
{
	std::string val;

	switch (typ) {
	case 'f': {
		float fmin = strToF(valmin);
		float fmax = strToF(valmax);
		val = fToStr ( m_rand.randF(fmin, fmax) );
		} break;
	case 'i':	{
		float imin = strToI(valmin);
		float imax = strToI(valmax);
		val = iToStr ( imin + m_rand.randI(imax-imin) );	
		} break;
	case '3': {
		Vec3F vmin = strToVec3 ( valmin, ',' );
		Vec3F vmax = strToVec3 ( valmax, ',' );
		Vec3F vrnd = m_rand.randV3 ( vmin, vmax );
		val = "<" + fToStr(vrnd.x) + "," + fToStr(vrnd.y) + "," + fToStr(vrnd.z) + ">";		break;
		} break;
	case '4': {
		Vec4F vmin = strToVec4 ( valmin, ',' );
		Vec4F vmax = strToVec4 ( valmax, ',' );
		Vec4F vrnd = m_rand.randV4 ( vmin, vmax );
		val = "<" + fToStr(vrnd.x) + "," + fToStr(vrnd.y) + "," + fToStr(vrnd.z) + "," + fToStr(vrnd.w) + ">";		break;		
		} break;
	case 'I': {
		Vec3I vmin = strToVec3 ( valmin, ',' );
		Vec3I vmax = strToVec3 ( valmax, ',' );
		Vec3I vrnd = m_rand.randV3 ( vmin, vmax );
		val = "<" + iToStr(vrnd.x) + "," + iToStr(vrnd.y) + "," + iToStr(vrnd.z) + ">";		break;	
		} break;
	};
	return val;
}

void Module::RandomizeParams ()
{
	Object* minobj = getInput ( "minobj" );
	Object* maxobj = getInput ( "maxobj" );
	std::string val_min, val_max, val;
	uchar typ;

	for (int n=0; n < minobj->getNumParam(); n++ ) {
		typ = minobj->getParamType ( n );
		val_min = minobj->getParamValueAsStr ( n );
		val_max = maxobj->getParamValueAsStr ( n );
		val = RandomizeValueTypeless  ( val_min, val_max, typ );
		m_params->SetParam ( n, val );
	}
}


void Module::Rebuild(bool bGenerate)
{
	Shape* s;	
	Shapes* vari_shapes;		// variant shapes
	Vec3F pos;
	Quaternion rot;
	int rnd;

	std::vector<Object*> objlist;
	int objs = getInputList("object", objlist);
	if (objs == 0) return;

	ImageX* img = (ImageX*) getInput("proxy0");

	Vec3I v;
	Vec3I numvari = getParamI3(M_RES);
	if (numvari.x <= 0) numvari.x = 1;
	if (numvari.y <= 0)	numvari.y = 1;
	if (numvari.z <= 0) numvari.z = 1;

	// Create LOD shapes
	ClearShapes();
	for (int n = 0; n < numvari.x * numvari.y * numvari.z; n++)
		AddShape();
	
	Vec3F spacing = getParamV3(M_SPACING);

	bool bParamSpace = CheckForParams();

	// Generate variants
	int i = 0;
	for (v.z = 0; v.z < numvari.z; v.z++) {
		for (v.y = 0; v.y < numvari.y; v.y++) {
			for (v.x = 0; v.x < numvari.x; v.x++) {

				// randomize for each Y/Z variant
				rnd = getParamI(M_SEED) + i;

				if ( bParamSpace ) 
					RandomizeParams ();				

				// get variant name
				std::string vname = "V" + iToStr(v.x) + "x" + iToStr(v.y) + "x" + iToStr(v.z);

				if ( v.x==numvari.x-1 && img != 0x0 ) { 					
					vari_shapes = GenerateProxy (v, vname, rnd, numvari);						// last lod
				} else {
					vari_shapes = GenerateVariant (v, vname, rnd, numvari, objlist, m_params);	// module variant
				}
				pos = Vec3F(v.x, v.z, v.y) * spacing;										// optional spatial layout					
				vari_shapes->SetTransform(pos, Vec3F(1, 1, 1), rot);

				// Add to master shape group (module output)
				s = getShape(v.z * numvari.y * numvari.x + v.y * numvari.x + v.x);
				s->type = S_SHAPEGRP;
				s->meshids.x = vari_shapes->getID();			// shape group
				s->meshids.y = OBJ_SHAPEGRP;					// this a *shape group*, not a mesh instance

				//dbgprintf("VARIANT: %s, %d shapes\n", name.c_str(), vari_shapes->getNumShapes());				
				i++;
			}
		}
	}

	MarkClean();
}

void Module::Generate (int x, int y)
{
	CreateOutput ( 'Ashp' );	
	
	m_rand.seed( getParamI(M_SEED) );

	Rebuild(true);
}

void Module::Run(float time)
{
}


Shape Module::SelectVariant(Vec3F vari, Vec3F& sz)
{
	// select a variant - only if loft input is a module
	//
	int res_slot = getParamByName("res");
	Vec3I num_vari = getParamI3(res_slot);				// variant ranges
															// vari range <0,0,0> <1,1,1>
	vari -= Vec3F(0.001,0.001,0.001);					// correction if vari=1
	vari *= num_vari;										// vari range <0,0,0> <nx,ny,nz>
	int variant = int(vari.z) * num_vari.y * num_vari.x + int(vari.y) * num_vari.x + int(vari.x);	// specific variant

	Shape* s = getShape( variant );							// shape group
	Shapes* sh = (Shapes*) gAssets.getObj ( s->meshids.x );	// shapes
	s = sh->getShape(0);									// first shape
	
	return *s;												// return shape
}

Shape Module::SelectVariant(int vari, Vec3F& sz)
{
	Shape* s = getShape(vari);								// shape group
	Shapes* sh = (Shapes*)gAssets.getObj(s->meshids.x);		// shapes
	s = sh->getShape(0);									// first shape
	
	return *s;												// return shape
}

void Module::Render ()
{
}


void Module::Sketch (int w, int h, Camera3D* cam )
{

}





