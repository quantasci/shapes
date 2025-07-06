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
flat in ivec4	vmatids;

// shader output 
layout(location = 0) out vec4 outColor;

// shader globals
uniform vec3	camPos;
uniform int		numLgt;
uniform	ivec4	envMap;
uniform vec4	envClr;

uniform vec4	shadowFars1;
uniform vec4	shadowFars2;
uniform vec2	shadowSize;
uniform mat4	shadowMtx[8];
uniform sampler2DArrayShadow shadowTex;

#define NULL_NDX  65530

// lights data block
struct Light {
	vec4	pos, target, ambclr, diffclr, specclr, inclr, shadowclr, cone;
};
layout(std140) uniform LIGHT_BLOCK
{
	Light			light[64];
};
// material data block
struct Material {
	vec4	texids, ambclr, diffclr, specclr, envclr, shadowclr, reflclr, refrclr, emisclr, surfp, reflp, refrp, displacep, displacea, info;
};
layout(std140) uniform MATERIAL_BLOCK
{
	Material		mat[64];
};
// textures data block
layout(std140) uniform TEXTURE_BLOCK
{
		sampler2D		tex[384];
};


#define PI		3.141592
#define M_1_PI	  (1.0f/3.141592f)

vec4 getEnvReflect(vec3 viewdir, vec3 vnorm)
{
	//-- environment map
	vec3 refl;
	refl = normalize(reflect(viewdir, vnorm));
	vec2 ec;
	ec.x = atan(refl.x, refl.z) / (2 * PI) + 0.5;
	ec.y = 1.0 - (asin(refl.y) / PI + 0.5);
	return (envMap.x == NULL_NDX) ? vec4(0, 0, 0, 0) : texture(tex[int(envMap.x)], ec) * 0.3;
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

uniform vec3 polar[6][3] = { {vec3(0.866, 0.5, 0.0),	vec3(0.5,  0.7,  0.0),  vec3(0.5, 0.9,  0.0) },
								 {vec3(0.433, 0.5, 0.75),	vec3(0.25, 0.7,  0.433),vec3(0.25, 0.9,  0.135) },
								 {vec3(-0.433, 0.5, 0.75), vec3(-0.25, 0.7, 0.433), vec3(-0.25, 0.9,  0.135) },
								 {vec3(-0.866, 0.5, 0.0), vec3(-0.5,  0.7,  0.0),  vec3(-0.5,0.9,  0.0) },
								 {vec3(-0.433, 0.5,-0.75), vec3(-0.25, 0.7,-0.433), vec3(-0.25, 0.9, -0.135) },
								 {vec3(0.433, 0.5,-0.75), vec3(0.25, 0.7,-0.433), vec3(-0.25, 0.9, -0.135) } };

float shadowCoeff()
{
	float ret = 0;
	int index = 7;											// find the appropriate depth map to look up in based on the depth of this fragment
	float d = gl_FragCoord.z;
	if (d < shadowFars1.x)		index = 0;
	else if (d < shadowFars1.y)	index = 1;
	else if (d < shadowFars1.z)	index = 2;
	else if (d < shadowFars1.w)	index = 3;
	else if (d < shadowFars2.x)	index = 4;
	else if (d < shadowFars2.y)	index = 5;
	else if (d < shadowFars2.z)	index = 6;
	vec4 shadow_coord = shadowMtx[index] * vviewpos;		// transform into light space. we require viewpos because we need to recover the z-depth of the test point
	shadow_coord.w = shadow_coord.z;						// R value  = depth value of the sample test point (vviewpos) in lights view
	shadow_coord.z = float(index);							// layer (split)															
	// ret = shadow2DArray(shadowTex, shadow_coord).x;		// tex value = depth value of the nearest occluder in lights view
	int j, k;												// ret = {1 if R <= Dt, 0 if R >= Dt}.. see OpenGL 2.0 spec, sec 3.8, p. 188	
	for (int i = 0; i < 8; i++) {
		j = int(vworldpos.x * 32743) & 7; k = int(vworldpos.z * 10691) & 7;					// Monte-carlo sampling
		ret += shadow2DArrayOffset(shadowTex, shadow_coord, ivec2(offset[j].x, offset[k].y)).x * 0.125;
	}
	return ret;
}


void main ()
{		
	int m = vmatids.x;
	int t = int(mat[m].texids.x);
	vec3 lgtdir, R, V, diff, spec, clr;
	vec4 texclr;	

	texclr = (t == NULL_NDX) ? vec4(1, 1, 1, 1) : texture(tex[t], vtexcoord.xy);
	if (texclr.w < 0.5) discard;
	
	clr = vec3(0,0,0);

	V = normalize(vworldpos.xyz - camPos);
			
	// primary light
	lgtdir = normalize(light[0].pos.xyz - vworldpos.xyz);
	R = normalize(vnormal * (2.0f * dot(vnormal, lgtdir)) - lgtdir);
	spec = mat[m].specclr.xyz * pow(max(0.0f, dot(R, V)), mat[m].surfp.x);
	diff = mat[m].diffclr.xyz * max(0.0f, dot(vnormal, lgtdir));
	float shadow = shadowCoeff();
	clr = mat[m].ambclr.xyz + light[0].ambclr.xyz + shadow * light[0].diffclr.xyz * (texclr.xyz * diff + spec);

	// env refraction  
	vec2 ec;	
	R = refract ( V, vnormal, mat[m].refrp.z );						// refraction vector
	ec.x = atan(R.z, R.x) * (0.5 * M_1_PI) + 0.5f;
	ec.y = 1.0 - (asin(R.y) * M_1_PI + 0.5f);
	clr += mat[m].refrclr.xyz * texture(tex[int(envMap.x)], ec).xyz;

	// translucency
	float alpha = (mat[m].refrclr.x + mat[m].refrclr.y + mat[m].refrclr.z)/3.0;

	// environment probe lighting	
	if (envClr.w != 0) {
		vec2 ec;
		vec3 dir;
		vec3 eclr = envClr.xyz * mat[m].envclr.xyz * vec3(1 / 6.0f, 1 / 6.0f, 1 / 6.0f);
		for (ec.x = 0; ec.x < 1; ec.x += 0.2) {
			for (ec.y = 0; ec.y < 1; ec.y += 0.2) {
				dir = vec3(cos(ec.x * 2 * PI) * sin(ec.y * PI), cos(ec.y * PI), sin(ec.x * 2 * PI) * sin(ec.y * PI)) * 3000.0f;
				lgtdir = normalize(dir - vworldpos.xyz);
				clr += eclr * texture(tex[int(envMap.x)], ec).xyz * max(0.0f, dot(vnormal, lgtdir));
			}
		}
	}

	//outColor = vec4( vnormal.xyz, 1 );
	outColor = vec4(clr, 1.0 - alpha );
}
