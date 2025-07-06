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

// vertex buffers (geometry)
layout(location = 0) in vec3 inPos;
layout(location = 1) in uint inClr;

// shader parameters
uniform vec3	param0;
uniform vec3	param1;
uniform vec3	param2;
uniform vec3	param3;

// camera matrices
uniform mat4	viewMatrix;
uniform mat4	invMatrix;			// inverse view
uniform mat4	projMatrix;
uniform mat4	objMatrix;
uniform vec3	camPos;
uniform vec4    texUni;

// per-pixel outputs
out vec4 vworldpos;
out vec4 vviewpos;
out vec4 vclr;

vec4 CLR2VEC ( uint c ) {	return vec4( float(c & 255u)/255.0, float((c>>8u) & 255u)/255.0, float((c>>16u) & 255u)/255.0, float((c>>24u) & 255u)/255.0 ); }

void main() 
{
	vworldpos = objMatrix * vec4( inPos, 1 );	
	vviewpos = viewMatrix * vworldpos;
	vclr = CLR2VEC( inClr );

	gl_Position = projMatrix * viewMatrix * vworldpos;
	gl_PointSize = 10.0 / (length(vworldpos.xyz - camPos)*0.02);
}
