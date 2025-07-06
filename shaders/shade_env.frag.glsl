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

#version 420
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_explicit_attrib_location : enable

// shader parameters
uniform vec3	param0;
uniform vec3	param1;
uniform vec3	param2;
uniform vec3	param3;

// per-pixel inputs (from vert program)
in vec4			vworldpos;
in vec3			vnormal;
in vec3			vtexcoord;
in vec4			vclr;
flat in ivec4	vmatids;
flat in vec4	vtexsub;

uniform mat4	viewMatrix;

// shader output 
layout(location=0) out vec4 outColor;

// shader globals
uniform vec3	camPos;
uniform int		numLgt;
uniform	ivec4	envMap;
uniform vec4	envClr;

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

#define PI		  3.141592653589
#define M_1_PI  0.318309886183871

vec3 getViewDir (float x, float y)
{
	vec3 camU = vec3( viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0] );
	vec3 camV = vec3( viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1] );
	vec3 camS = vec3( viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2] );
  return - normalize( x*camU + y*camV + camS );			// direction only, range: x,y = [-1,1]
}

#define NULL_NDX	  65530

void main ()
{		
	vec3 v = getViewDir ( vtexcoord.x, vtexcoord.y );		// must do here to avoid distortion. in texcoord range: [-1, 1]

	vec2 tc;
	tc.x = atan ( v.z, v.x ) * (0.5 * M_1_PI) + 0.5; 
	tc.y = 1.0 - (asin ( v.y ) * M_1_PI + 0.5);				// range: asin(-PI/2, PI/2) --> (-1/2, 1/2) --> (0, 1) --> (1, 0)	
	
	vec4 texclr = texture( tex[ int(envMap.x) ], tc );

	// outColor = vec4( .2, .22, .2, 1 );
	outColor = vec4( texclr.xyz, 1.0 );	
}

