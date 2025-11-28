
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
uniform mat4	invMatrix;			// inverse view

// per-pixel inputs (from vert program)
in vec4			vworldpos;
in vec4			vviewpos;
in vec3			vnormal;
in vec3			vtexcoord;
in vec4			vclr;
flat in ivec4	vmatids;

// shader output 
layout(location=0) out vec4 outColor;

#include "common.glsl"

void main ()
{		
	int m = vmatids.x;
	int t = int( mat[m].texids.x );
	vec3 lgtdir, R, V, diff, spec, clr;
	vec4 texclr;
	float ndotl;
		
	texclr = (t == NULL_NDX) ? vec4(1,1,1,1) : texture( tex[t], vtexcoord.xy );
	if (texclr.w < 0.5) discard;

	// vertex color
	texclr *= vclr;

	V = normalize( camPos - vworldpos.xyz );

	// primary light (/w shadows)
	lgtdir = normalize( light[0].pos.xyz - vworldpos.xyz );					
  ndotl = dot( lgtdir, vnormal );
	R = reflect ( -lgtdir, vnormal );
	spec = mat[m].specclr.xyz * pow( max(0.0f, dot(R, V)), mat[m].surfp.x );
	diff =  mat[m].diffclr.xyz * max(0.0f, ndotl );	
	clr = mat[m].ambclr.xyz + light[0].ambclr.xyz + shadowCoeff(ndotl, mat[m].surfp.y, vnormal, lgtdir, light[0].diffclr.xyz) * (texclr.xyz * diff + spec);	
  //clr = texclr.xyz * diff;      //-- debug just textures

	// env reflection  
	vec2 ec;
	R = normalize ( reflect ( -V, vnormal ) );  
	ec.x = atan(R.z, R.x) / (2*PI) + 0.5f;
	ec.y = R.y * 0.5f + 0.5f;
	clr += mat[m].reflclr.xyz * texture( tex[ int(envMap.x) ], ec ).xyz;

	// multiple lights
  float dist;
	for (int i=1; i < numLgt; i++ ) {
    lgtdir = light[i].pos.xyz - vworldpos.xyz;
    dist = length(lgtdir);
		lgtdir /= dist;
    R = reflect(-lgtdir, vnormal);
		spec = mat[m].specclr.xyz * pow( max(0.0f, dot(R, V)), mat[m].surfp.x );
		diff = mat[m].diffclr.xyz * max(0.0f, dot(lgtdir, vnormal) );
		clr += light[i].diffclr.xyz * (texclr.xyz * diff + spec); // (dist*dist/100.0) ;
	}
	
	// environment probe lighting	
	if ( envClr.w != 0 ) {
		vec2 ec;
		vec3 dir;		
		vec3 eclr = vec3(0.0);
		for (ec.x = 0; ec.x < 1; ec.x += 0.2) {
			for (ec.y = 0; ec.y < 1; ec.y += 0.2) {
				dir = vec3( cos(ec.x*2*PI)*sin(ec.y*PI), cos(ec.y*PI), sin(ec.x*2*PI)*sin(ec.y*PI) );
				float pdf = max(0.0f, dot(vnormal, dir));
				eclr += texture( tex[ int(envMap.x) ], ec ).xyz * pdf ;
			}
		}		
		clr += 2.0 * envClr.xyz * mat[m].envclr.xyz * eclr / float(5*5);
	}

  // importance sampling
	/*if (envClr.w != 0.0) {
		const int N = 32;	
		vec3 eclr = vec3(0.0);
		for (int k = 0; k < N; k++) {
			vec2 xi = hammersleySequence(k, N);			
			vec3 dir = normalize( sampleToDir ( xi ) * 0.5 + vnormal );
			float pdf = max(0.0, dot( vnormal, dir )) / 3.14159264359;						
			vec2 uv = dirToUV ( dir );			
			eclr += texture(tex[int(envMap.x)], uv ).xyz * pdf;
		}	
		clr += 1.0 * envClr.xyz * mat[m].envclr.xyz * eclr / float(N);
	}*/
	
	outColor = vec4(clr, 1);
}
