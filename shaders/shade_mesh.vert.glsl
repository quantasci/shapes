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
#extension GL_ARB_explicit_attrib_location : enable

// vertex buffers (geometry)
layout(location = 0) in vec3 inPos;
layout(location = 1) in uint inClr;
layout(location = 2) in vec3 inNorm;
layout(location = 3) in vec2 inTexCoord;

// instance buffers (shapes)
layout(location = 4) in vec3 instPos;
layout(location = 5) in vec4 instRot;
layout(location = 6) in vec3 instScale;
layout(location = 7) in vec3 instPivot;
layout(location = 8) in uint instClr;
layout(location = 9) in vec4 instMatIDS;
layout(location = 10) in vec4 instTexSub;
layout(location = 12) in mat4 instXform;

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

// per-pixel outputs
out vec4 vworldpos;
out vec4 vviewpos;
out vec3 vnormal;       // specify 'flat' here for non-interpolated normal
out vec3 vtexcoord;
out vec4 vtexsub;
out vec4 vclr;
flat out ivec4 vmatids;

vec4 CLRtoVEC(uint c) { return vec4( float(c & uint(0xFF))/255.0, float((c>>8) & uint(0xFF))/255.0, float((c>>16) & uint(0xFF))/255.0, float((c>>24) & uint(0xFF))/255.0 ); }

mat3 getInvTranspose( mat4 a )  { return transpose(inverse(mat3(a))); }

void main() 
{
	int inst = gl_InstanceID;		

	vworldpos = instXform * vec4(inPos, 1);	
	vviewpos = viewMatrix * vworldpos;
	vnormal = normalize ( getInvTranspose ( instXform ) * inNorm );

	vtexcoord = vec3 ( inTexCoord* instTexSub.zw + instTexSub.xy, gl_InstanceID );
	vmatids = ivec4 ( instMatIDS );
	vclr = CLRtoVEC(inClr) * CLRtoVEC( instClr );

	gl_Position = projMatrix * vviewpos;
}
