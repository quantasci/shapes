#version 450
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_explicit_attrib_location : enable
#extension GL_EXT_texture_array : enable

// shader parameters
uniform vec3	param0;
uniform vec3	param1;
uniform vec3	param2;
uniform vec3	param3;

// per-pixel inputs (from vert program)
in vec4			vworldpos;
in vec4			vviewpos;
in vec3			vnormal;
in vec3			vtexcoord;
in vec4			vclr;
flat in ivec4	vtexids;
flat in vec4	vtexsub;

// shader output 
layout(location=0) out vec4 outColor;

// shader globals
uniform vec3	camPos;
uniform int		numLgt;
uniform	vec4	envMap;

uniform vec4	shadowFars;
uniform vec2	shadowSize;
uniform mat4	shadowMtx[4];
uniform sampler2DArrayShadow shadowTex;

// lights data block
struct Light {
	vec4	pos, target;
	vec4	ambclr, diffclr, specclr, inclr, shadowclr, cone;
};
layout(std140) uniform LIGHT_BLOCK
{
	Light			light[64];
};
// textures data block
layout(std140) uniform TEXTURE_BLOCK
{
    sampler2D      tex[384];
};


#define PI		3.141592

vec4 getEnvReflect ( vec3 viewdir, vec3 vnorm)
{
	//-- environment map
	vec3 refl;
	refl = normalize( reflect( viewdir, vnorm ));
	vec2 ec;	
	ec.x = atan ( refl.x, refl.z ) / (2*PI) + 0.5;
	ec.y = 1.0 - (asin ( refl.y ) / PI + 0.5);	
	return (envMap.w == 0) ? vec4(0,0,0,0) : texture( tex[ int(envMap.x) ], ec ) * 0.3;
}

const int nsamples = 8;
uniform vec4 offset[nsamples] = { vec4(0.000000, 0.000000, 0.0, 0.0),
								  vec4(0.079821, 0.165750, 0.0, 0.0),
								  vec4(-0.331500, 0.159642, 0.0, 0.0),
								  vec4(-0.239463, -0.497250, 0.0, 0.0),
								  vec4(0.662999, -0.319284, 0.0, 0.0),
								  vec4(0.399104, 0.828749, 0.0, 0.0),
								  vec4(-0.994499, 0.478925, 0.0, 0.0),
								  vec4(-0.558746, -1.160249, 0.0, 0.0) };

uniform vec3 polar[6][3] = { {vec3(0.866, 0.5, 0.0),	vec3( 0.5,  0.7,  0.0),  vec3( 0.5, 0.9,  0.0) },
						     {vec3(0.433, 0.5, 0.75),	vec3( 0.25, 0.7,  0.433),vec3( 0.25, 0.9,  0.135) },
						     {vec3(-0.433, 0.5, 0.75), vec3(-0.25, 0.7, 0.433), vec3(-0.25, 0.9,  0.135) },
						     {vec3(-0.866, 0.5, 0.0 ), vec3(-0.5,  0.7,  0.0),  vec3(-0.5,0.9,  0.0) },
						     {vec3(-0.433, 0.5,-0.75), vec3(-0.25, 0.7,-0.433), vec3(-0.25, 0.9, -0.135) },
						     {vec3( 0.433, 0.5,-0.75), vec3( 0.25, 0.7,-0.433), vec3(-0.25, 0.9, -0.135) } };

float shadowCoeff()
{
	float ret = 0;
	int index = 3;											// find the appropriate depth map to look up in based on the depth of this fragment
	if(gl_FragCoord.z < shadowFars.x)		index = 0;
	else if(gl_FragCoord.z < shadowFars.y)	index = 1;
	else if(gl_FragCoord.z < shadowFars.z)	index = 2;
	vec4 shadow_coord = shadowMtx[index] * vviewpos;		// transform into light space. we require viewpos because we need to recover the z-depth of the test point
	shadow_coord.w = shadow_coord.z;						// R value  = depth value of the sample test point (vviewpos) in lights view
	shadow_coord.z = float(index);							// layer (split)															
	// ret = shadow2DArray(shadowTex, shadow_coord).x;		// tex value = depth value of the nearest occluder in lights view
	int j, k;												// ret = {1 if R <= Dt, 0 if R >= Dt}.. see OpenGL 2.0 spec, sec 3.8, p. 188	
	for (int i=0; i < 8; i++) {												
		j = int(vworldpos.x * 32743) & 7; k = int(vworldpos.z * 10691) & 7;					// Monte-carlo sampling
		ret += shadow2DArrayOffset(shadowTex, shadow_coord, ivec2( offset[j].x, offset[k].y )).x * 0.125;
	}	
	return ret;
}

void main ()
{		
	vec3 lightdir, viewdir;	
	vec4 clr;

	vec2 tc = vtexsub.xy + vtexsub.zw*vtexcoord.xy;
	vec4 texclr = (vtexids.w == 0) ? vec4(1,1,1,1) : texture( tex[ vtexids.x ], tc );		
	if (texclr.w < 0.5) discard;
	texclr *= vclr;

	// shadows
	float shadow = 0.5 + 0.5*shadowCoeff();
	lightdir = normalize( light[0].pos.xyz - vworldpos.xyz );				
	clr = vec4(shadow,shadow,shadow,1) * texclr * (light[0].ambclr + light[0].diffclr * (0.5f+max(0.5f, dot (vnormal, lightdir)) ));
	
	outColor = vec4( clr );
}
