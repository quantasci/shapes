
#include "material.h"
#include "imagex.h"

Material::Material()
{

}

void Material::Define (int x, int y)
{
	AddInput ( "shader",		'Ashd' );
	AddInput ( "texture",		'Aimg' );
	AddInput ( "displace",	'Aimg' );
	AddInput ( "emission",	'Aimg' );	

	SetInput ( "texture",		"color_white" );
	SetInput ( "shader",		"shade_mesh" );

	AddParam( M_SHADER,				"shader",		"i");	SetParamI   ( M_SHADER, 0, 0 );
	AddParam( M_TEXIDS,				"texids",		"4");	SetParamV4	( M_TEXIDS, 0, Vec4F(0,0,0,0) );
	AddParam( M_LGT_WIDTH,		"light_width",	"f");	SetParamF	( M_LGT_WIDTH, 0, 32.0 );
	AddParam( M_SHADOW_CLR,		"shadow_color",	"3" );	SetParamV3	( M_SHADOW_CLR, 0, Vec3F(0,0,0) );
	AddParam( M_SHADOW_BIAS,	"shadow_bias",	"f");	SetParamF	( M_SHADOW_BIAS, 0, 0.01f );

	AddParam( M_AMB_CLR,			"amb_color",	"3" );	SetParamV3	( M_AMB_CLR, 0, Vec3F(0, 0, 0) );
	AddParam( M_DIFF_CLR,			"diff_color",	"3" );	SetParamV3	( M_DIFF_CLR, 0, Vec3F(0.7,0.7,0.8) );
	AddParam( M_SPEC_CLR,			"spec_color",	"3" );	SetParamV3	( M_SPEC_CLR, 0, Vec3F(0.3,0.3,0.3) );
	AddParam( M_SPEC_POWER,		"spec_power",	"f" );	SetParamF	( M_SPEC_POWER, 0, 40 );
	AddParam( M_ENV_CLR,			"env_color",	"3" );	SetParamV3	( M_ENV_CLR, 0, Vec3F(0.2,0.2,0.2) );

	AddParam( M_REFL_CLR,			"refl_color",	"3" );	SetParamV3	( M_REFL_CLR, 0, Vec3F(0.2,0.2,0.2) );
	AddParam( M_REFL_WIDTH,		"refl_width",	"f" );	SetParamF	( M_REFL_WIDTH, 0, 6.0 );
	AddParam( M_REFL_BIAS,		"refl_bias",	"f" );	SetParamF	( M_REFL_BIAS, 0, 0.01f );
	
	AddParam( M_REFR_CLR,			"refr_color",	"3" );	SetParamV3	( M_REFR_CLR, 0, Vec3F(0.3,0.3,0.3) );
	AddParam( M_REFR_WIDTH,		"refr_width",	"f" );	SetParamF	( M_REFR_WIDTH, 0, -1.0 );		
	AddParam( M_REFR_BIAS,		"refr_bias",	"f" );	SetParamF	( M_REFR_BIAS, 0, 0.05f );
	AddParam( M_REFR_IOR,			"refr_ior",		"f" );	SetParamF	( M_REFR_IOR, 0, 1.05f );

	AddParam( M_EMIS_CLR,			"emis_color", "3");	SetParamV3(M_EMIS_CLR, 0, Vec3F(0, 0, 0));

	AddParam( M_DISPLACE_AMT,	"displace_amt",	"4" );		SetParamV4	( M_DISPLACE_AMT, 0, Vec4F(0,0,0,0) );
	AddParam( M_DISPLACE_DEPTH,	"displace_depth", "4" );	SetParamV4	( M_DISPLACE_DEPTH, 0, Vec4F(0,0,0,0) );

	AddParam( M_FLIP_Y,			"flipy",		"i");	SetParamI	( M_FLIP_Y, 0, 0 );	
}
 
void Material::Generate(int x, int y)
{
	Vec8S texids;

	bool flipy = getParamI ( M_FLIP_Y );
	if (flipy) {
		ImageX* img = (ImageX*) getInput( "texture" );
		img->FlipY();
		img = (ImageX*) getInput( "displace" );
		img->FlipY();
	}

	// set textures from inputs
	getInputTex("texture",  texids, 0);	
	getInputTex("displace", texids, 1);
	getInputTex("emission", texids, 2);

	SetParamV4 ( M_TEXIDS, 0, Vec4F(texids.x, texids.y, texids.z, texids.w) );

	// set shader ID from input
	int shader = getInputID ( "shader", 'Ashd' );
	SetParamI ( M_SHADER, 0, shader );	

	MarkDirty ();
}

void Material::Clear()
{
}