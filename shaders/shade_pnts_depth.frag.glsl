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

#version 400

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_explicit_attrib_location : enable

// shader parameters
uniform vec3	param0;
uniform vec3	param1;
uniform vec3	param2;
uniform vec3	param3;

// per-pixel inputs (from vert program)
in vec4			vworldpos;
in vec4			vclr;

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

void main ()
{		
	outColor = vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1 );		// write depth value to output
}
