
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
flat in vec4 vtexsub;

// shader output 
layout(location=0) out vec4 outColor;

#include "common.glsl"


void main ()
{	
	int m = vmatids.x;					// material
	int t = int( mat[m].texids.x );		// color texture
	int b = int( mat[m].texids.y );		// bump map
	vec3 lgtdir, R, V, spec, diff, clr;
	vec4 texclr;	
	float ndotl;
	
	// need both tc and vtexcoord below
	vec2 tc = vtexsub.xy + vtexsub.zw*vtexcoord.xy;
	texclr = (t == NULL_NDX) ? vec4(1,1,1,1) : texture( tex[ t ], fract( tc ) );			
	if (texclr.w < 0.5) discard;													// discard for proper alpha blend

	// vertex color
	texclr *= vclr;

	V = normalize( camPos - vworldpos.xyz );
	
	// Bump mapping 
	// from paper: Bump Mapping Unparametrized Surfaces on the GPU
	vec3 vn = normalize( vnormal );
	vec3 posDX = dFdxFine ( vworldpos.xyz );			// choose dFdx (#version 420) or dFdxFine (#version 450)
	vec3 posDY = dFdyFine ( vworldpos.xyz );
	vec3 r1 = cross ( posDY, vn );
	vec3 r2 = cross ( vn , posDX );
	float det = dot (posDX , r1);
	float Hll = texture( tex[ b ], tc ).x;		//-- height from bump map texture, tc=texture coordinates
	float Hlr = texture( tex[ b ], tc + dFdx(vtexcoord.xy) ).x;
	float Hul = texture( tex[ b ], tc + dFdy(vtexcoord.xy) ).x;
	// float dBs = ddx_fine ( height );					//-- explicit height
	// float dBt = ddy_fine ( height );

	vec3 surf_grad = sign(det) * ( (Hlr - Hll) * r1 + (Hul - Hll)* r2 );	// gradient of surface texture. dBs=Hlr-Hll, dBt=Hul-Hll

	float bump_amt = 16.0 * mat[m].displacep.y;
	vec3 vbumpnorm = normalize( vn*(1.0 - bump_amt) + bump_amt * normalize ( abs(det)*vn - surf_grad ) );	// bump normal

	// ambient light
	clr = mat[m].ambclr.xyz + light[0].ambclr.xyz;

	// primary light	
	lgtdir = normalize( light[0].pos.xyz - vworldpos.xyz );		
	ndotl = max(0.0f, dot(vbumpnorm, lgtdir) );
	R = normalize( vbumpnorm * (2.0f * ndotl ) - lgtdir );  
	spec = mat[m].specclr.xyz * pow( max(0.0f, dot(R, V)), mat[m].surfp.x );	
	clr += shadowCoeff(ndotl, mat[m].surfp.y, vnormal, lgtdir, light[0].diffclr.xyz) * (texclr.xyz * mat[m].diffclr.xyz * max(0.0f, ndotl) + spec);
  //clr += light[0].diffclr.xyz * (texclr.xyz * mat[m].diffclr.xyz * max(0.0f, ndotl) + spec);

	// environment reflection
	vec2 ec;
	//R = normalize(vbumpnorm * (2.0f * dot(vbumpnorm, +V)) - V);		// reflection vector
	R = normalize( reflect( V, vbumpnorm ));
	ec.x = atan(R.z, R.x) * (0.5 * M_1_PI) + 0.5f;
	ec.y = 1.0 - (asin(R.y) * M_1_PI + 0.5f);
	clr += mat[m].reflclr.xyz * texture( tex[ int(envMap.x) ], ec ).xyz;	

	// multiple lights
  float dist;
  for (int i = 1; i < numLgt; i++) {
    lgtdir = light[i].pos.xyz - vworldpos.xyz;
    dist = length(lgtdir);
    lgtdir /= dist;
    R = reflect(-lgtdir, vbumpnorm);
    spec = mat[m].specclr.xyz * pow(max(0.0f, dot(R, V)), mat[m].surfp.x);
    diff = mat[m].diffclr.xyz * max(0.0f, dot(lgtdir, vnormal));
    clr += light[i].diffclr.xyz * (texclr.xyz * diff + spec); // (dist * dist / 20.0);
  }

	// environment probe lighting	
	if (envClr.w != 0) {
		vec2 ec;
		vec3 dir;
		vec3 eclr = vec3(0.0);
		for (ec.x = 0; ec.x < 1; ec.x += 0.2) {
			for (ec.y = 0; ec.y < 1; ec.y += 0.2) {
				dir = vec3(cos(ec.x * 2 * PI) * sin(ec.y * PI), cos(ec.y * PI), sin(ec.x * 2 * PI) * sin(ec.y * PI));
				float pdf = max(0.0f, dot(vnormal, dir));
				eclr += texture(tex[int(envMap.x)], ec).xyz * pdf;
			}
		}
		clr += 1.0 * envClr.xyz * mat[m].envclr.xyz * eclr / float(5 * 5);
	}

	outColor = vec4(clr, 1 );
}
