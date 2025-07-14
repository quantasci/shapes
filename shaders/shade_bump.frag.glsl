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
flat in vec4 vtexsub;

// shader output 
layout(location=0) out vec4 outColor;

// shader globals
uniform vec3	camPos;
uniform int		numLgt;
uniform	ivec4	envMap;
uniform	vec4	envClr;

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
	Material		mat[128];
};
// textures data block
layout(std140) uniform TEXTURE_BLOCK
{
    sampler2D		tex[384];
};


#define PI		3.141592


vec4 getEnvReflect ( vec3 viewdir, vec3 vnorm)
{
	//-- environment map
	vec3 refl;
	refl = normalize( reflect( viewdir, vnorm ));
	vec2 ec;	
	ec.x = atan ( refl.x, refl.z ) / (2*PI) + 0.5;
	ec.y = 1.0 - (asin ( refl.y ) / PI + 0.5);	
	return (envMap.w == NULL_NDX) ? vec4(0,0,0,0) : texture( tex[ int(envMap.x) ], ec ) * 0.3;
}

uniform vec2 poisson[32] = {
vec2(-0.24211216, 0.47364676),
vec2(-0.26615779, -0.90170755),
vec2(0.73921032, 0.64433394),
vec2(0.04698852, 0.61150381),
vec2(-0.44439513, -0.00099443),
vec2(-0.76492574, 0.25985650),
vec2(-0.15847363, 0.02949928),
vec2(0.47698860, -0.20139980),
vec2(0.19856691, -0.24167847),
vec2(0.39941321, -0.47524531),
vec2(0.12379438, -0.71497718),
vec2(0.44992017, 0.52064212),
vec2(0.85906990, 0.26345301),
vec2(-0.63893728, -0.72189488),
vec2(-0.72236838, -0.41189440),
vec2(-0.62787651, 0.62525977),
vec2(-0.90117786, -0.16577598),
vec2(0.18857739, 0.33248861),
vec2(-0.07437943, 0.88148748),
vec2(-0.36199658, -0.57615895),
vec2(0.98153900, -0.15617891),
vec2(-0.37752622, 0.92119365),
vec2(0.69192458, -0.71806032),
vec2(0.42387261, 0.80070350),
vec2(0.58387514, 0.07783529),
vec2(0.39615168, -0.78912900),
vec2(-0.15483770, -0.37708327),
vec2(0.74519869, -0.32442702),
vec2(0.27457778, 0.02798797),
vec2(-0.46370503, 0.29205526),
vec2(-0.43198480, -0.30202311),
vec2(0.06044658, -0.99696076)
};

const int nsamples = 32;				// number of samples (shadow quality)

const float bias = -0.0001;

float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
vec2 randomRotation (vec2 fragCoord) {
	float angle = rand(fragCoord) * 6.2831853;
	return vec2(cos(angle), sin(angle));
}

float shadowCoeff(float ndotl, float lwid)
{
	float val, ret = 0;
	int j, k, index = 7;											// find the appropriate depth map to look up in based on the depth of this fragment
	float d = gl_FragCoord.z;
	if (d < shadowFars1.x)		index = 0;
	else if (d < shadowFars1.y)	index = 1;
	else if (d < shadowFars1.z)	index = 2;
	else if (d < shadowFars1.w)	index = 3;
	else if (d < shadowFars2.x)	index = 4;
	else if (d < shadowFars2.y)	index = 5;
	else if (d < shadowFars2.z)	index = 6;
	vec4 jit;
	vec4 shadow_coord = shadowMtx[index] * vviewpos;			// transform into light space. we require viewpos because we need to recover the z-depth of the test point
	shadow_coord.w = shadow_coord.z;											// R value = depth value of the current sample point (vviewpos) in lights view
	shadow_coord.z = float(index);												// layer (split)																
// return shadow2DArray(shadowTex, shadow_coord).x;		// [optional] single sample only (fast)

	vec2 randomRot = randomRotation ( gl_FragCoord.xy );

	// PCF filtering
	lwid *= 0.00002;
	for (int i = 0; i < nsamples; i++) {
		// poisson disc sample
		jit = vec4( (poisson[i].x * randomRot.x - poisson[i].y * randomRot.y) * lwid, 
							  (poisson[i].x * randomRot.y + poisson[i].y * randomRot.x) * lwid,
							   0, bias );																	
		// val: depends on depth compare mode, {1 if R <= Dt, 0 if R >= Dt}.. see OpenGL 2.0 spec, sec 3.8, p. 188			
		ret += shadow2DArray(shadowTex, shadow_coord + jit).x;
	}
	return ret / float(nsamples);
}

#define NULL_NDX  65530
#define M_1_PI	  (1.0f/3.141592f)

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
	vec3 vbumpnorm = vn*(1.0 - bump_amt) + bump_amt * normalize ( abs(det)*vn - surf_grad );	// bump normal

	// ambient light
	clr = mat[m].ambclr.xyz + light[0].ambclr.xyz;

	// primary light	
	lgtdir = normalize( light[0].pos.xyz - vworldpos.xyz );		
	ndotl = dot(vbumpnorm, lgtdir);
	R = normalize( vbumpnorm * (2.0f * ndotl ) - lgtdir );  
	spec = mat[m].specclr.xyz * pow( max(0.0f, dot(R, V)), mat[m].surfp.x );	
	clr += shadowCoeff(ndotl, mat[m].surfp.y) * light[0].diffclr.xyz * (texclr.xyz * mat[m].diffclr.xyz * max(0.0f, ndotl) + spec);
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
    clr += light[i].diffclr.xyz * (texclr.xyz * diff + spec) / (dist * dist / 20.0);
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
		clr += 2.0 * envClr.xyz * mat[m].envclr.xyz * eclr / float(5 * 5);
	}

	outColor = vec4(clr, 1 );
}
