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

vec4 CLRtoVEC(uint c) { return vec4(float(c & uint(0xFF)) / 255.0, float((c >> 8) & uint(0xFF)) / 255.0, float((c >> 16) & uint(0xFF)) / 255.0, float((c >> 24) & uint(0xFF)) / 255.0); }


mat4 getInvTranspose(mat4 a)
{
  float s0 = a[0][0] * a[1][1] - a[1][0] * a[0][1];
  float s1 = a[0][0] * a[1][2] - a[1][0] * a[0][2];
  float s2 = a[0][0] * a[1][3] - a[1][0] * a[0][3];
  float s3 = a[0][1] * a[1][2] - a[1][1] * a[0][2];
  float s4 = a[0][1] * a[1][3] - a[1][1] * a[0][3];
  float s5 = a[0][2] * a[1][3] - a[1][2] * a[0][3];

  float c5 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
  float c4 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
  float c3 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
  float c2 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
  float c1 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
  float c0 = a[2][0] * a[3][1] - a[3][0] * a[2][1];

  // Should check for 0 determinant
  float invdet = 1.0 / (s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0);

  mat4 m;
  m[0][0] = (a[1][1] * c5 - a[1][2] * c4 + a[1][3] * c3) * invdet;
  m[0][1] = (-a[0][1] * c5 + a[0][2] * c4 - a[0][3] * c3) * invdet;
  m[0][2] = (a[3][1] * s5 - a[3][2] * s4 + a[3][3] * s3) * invdet;
  m[0][3] = (-a[2][1] * s5 + a[2][2] * s4 - a[2][3] * s3) * invdet;

  m[1][0] = (-a[1][0] * c5 + a[1][2] * c2 - a[1][3] * c1) * invdet;
  m[1][1] = (a[0][0] * c5 - a[0][2] * c2 + a[0][3] * c1) * invdet;
  m[1][2] = (-a[3][0] * s5 + a[3][2] * s2 - a[3][3] * s1) * invdet;
  m[1][3] = (a[2][0] * s5 - a[2][2] * s2 + a[2][3] * s1) * invdet;

  m[2][0] = (a[1][0] * c4 - a[1][1] * c2 + a[1][3] * c0) * invdet;
  m[2][1] = (-a[0][0] * c4 + a[0][1] * c2 - a[0][3] * c0) * invdet;
  m[2][2] = (a[3][0] * s4 - a[3][1] * s2 + a[3][3] * s0) * invdet;
  m[2][3] = (-a[2][0] * s4 + a[2][1] * s2 - a[2][3] * s0) * invdet;

  m[3][0] = (-a[1][0] * c3 + a[1][1] * c1 - a[1][2] * c0) * invdet;
  m[3][1] = (a[0][0] * c3 - a[0][1] * c1 + a[0][2] * c0) * invdet;
  m[3][2] = (-a[3][0] * s3 + a[3][1] * s1 - a[3][2] * s0) * invdet;
  m[3][3] = (a[2][0] * s3 - a[2][1] * s1 + a[2][2] * s0) * invdet;

  return m;
}


void main() 
{
	int inst = gl_InstanceID;

	vworldpos = instXform * vec4(inPos, 1);	
	vviewpos = viewMatrix * vworldpos;

	vnormal = normalize((getInvTranspose(instXform) * vec4(inNorm, 0)).xyz);              //-- correct one. norm w-component should be 0

	vtexcoord = vec3(inTexCoord * instTexSub.zw + instTexSub.xy, gl_InstanceID);
	vmatids = ivec4(instMatIDS);
	vclr = CLRtoVEC(inClr) * CLRtoVEC(instClr);

	gl_Position = projMatrix * vviewpos;
}
