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

// per-pixel inputs (from vert program)
in vec4			vworldpos;
in vec3			vtexcoord;
flat in ivec4	vtexids;
flat in vec4	vtexsub;
flat in ivec4	vmatids;

#define NULL_NDX  65530

// shader output 
layout(location=0) out vec4 outColor;

// shader globals
uniform vec3	camPos;

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
	Material		mat[128];
};
// textures data block
layout(std140) uniform TEXTURE_BLOCK
{
    sampler2D		tex[384];
};


void main ()
{		
	int m = vmatids.x;
	int t = int( mat[m].texids.x );
	vec4 texclr = (t == NULL_NDX) ? vec4(1,1,1,1) : texture( tex[t], vtexcoord.xy );
	//if (texclr.w < 0.5) discard;												// discard for proper alpha blend

	outColor = vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1 );		// write depth value to output	
}

