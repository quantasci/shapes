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
in vec4			vclr;

// shadow maps
uniform vec4	shadowFars1;
uniform vec4	shadowFars2;
uniform vec2	shadowSize;
uniform mat4	shadowMtx[8];
uniform sampler2DArrayShadow shadowTex;

// shader output 
layout(location=0) out vec4 outColor;

// shader globals
uniform vec3	camPos;
uniform int		numLgt;

// lights data block
struct Light {
	vec4	pos, target;
	vec4	ambclr, diffclr, specclr, inclr, shadowclr, cone;
};
layout(std140) uniform LIGHT_BLOCK
{
	Light			light[64];
};


float shadow_depth_bias = 0.001;

const int nsamples = 8;
uniform vec4 offset[nsamples] = { vec4(0.000000, 0.000000, 0.0, 0.0),
								  vec4(0.079821, 0.165750, 0.0, 0.0),
								  vec4(-0.331500, 0.159642, 0.0, 0.0),
								  vec4(-0.239463, -0.497250, 0.0, 0.0),
								  vec4(0.662999, -0.319284, 0.0, 0.0),
								  vec4(0.399104, 0.828749, 0.0, 0.0),
								  vec4(-0.994499, 0.478925, 0.0, 0.0),
								  vec4(-0.558746, -1.160249, 0.0, 0.0) };

float shadowCoeff()
{
	int index = 7;											// find the appropriate depth map to look up in based on the depth of this fragment
	float d = gl_FragCoord.z ;
	if (d < shadowFars1.x)		index = 0;
	else if(d < shadowFars1.y)	index = 1;
	else if(d < shadowFars1.z)	index = 2;	
	else if(d < shadowFars1.w)	index = 3;	
	else if(d < shadowFars2.x)	index = 4;	
	else if(d < shadowFars2.y)	index = 5;	
	else if(d < shadowFars2.z)	index = 6;	
	vec4 shadow_coord = shadowMtx[index] * vviewpos;		// transform into light space. we require viewpos because we need to recover the z-depth of the test point
	shadow_coord.w = shadow_coord.z - shadow_depth_bias;	// R value  = depth value of the sample test point (vviewpos) in lights view
	shadow_coord.z = float(index);							// layer (split)																		
	return shadow2DArrayOffset(shadowTex, shadow_coord, ivec2(0,0)).x;
}

void main ()
{		
	float d, dist;
	vec3 lightdir;
	vec4 clr;
	
	// key light (/w shadows)
	float shadow = shadowCoeff();
	lightdir = light[0].pos.xyz - vworldpos.xyz; dist = length(lightdir);
	d = 0.2 + 0.8*dot( vec3(0,1,0), lightdir/dist );		// diffuse
	clr = vec4(shadow,shadow,shadow,1) * vclr * light[0].diffclr * vec4(d,d,d,1); // * min(1.0f, 5000.0f / (dist+0.01f) );		// distance intensuty scalar=5000

	// multiple lights
	int lgts = min(2, numLgt);		// maxLgt=2
	for (int i=1; i < lgts; i++ ) {
		lightdir = normalize( light[i].pos.xyz - vworldpos.xyz );								
		d = dot(vec3(.7,0,.7), lightdir);
		clr += vclr * light[i].diffclr * vec4(d,d,d,1);
	} 

	outColor = clr;	
}
