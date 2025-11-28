
// GLSL shaders - common functions

#define NULL_NDX  65530
#define PI		    3.14159256f
#define M_1_PI	  (1.0f/3.14159256f)

// shader globals
uniform vec3	camPos;
uniform int		numLgt;
uniform	ivec4	envMap;
uniform vec4	envClr;

uniform vec4	shadowFars1;
uniform vec4	shadowFars2;
uniform vec2	shadowSize;
uniform mat4  shadowLgtProj;
uniform mat4  shadowLgtMV;
uniform mat4	shadowMtx[8];
uniform sampler2DArray shadowTex;

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

vec4 getEnvReflect ( vec3 viewdir, vec3 vnorm)
{
	//-- environment map
	vec3 refl;
	refl = normalize( reflect( viewdir, vnorm ));
	vec2 ec;	
	ec.x = atan ( refl.x, refl.z ) / (2*PI) + 0.5;
	ec.y = 1.0 - (asin ( refl.y ) / PI + 0.5);	
	return (envMap.x == NULL_NDX) ? vec4(0,0,0,0) : texture( tex[ int(envMap.x) ], ec ) * 0.3;
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


float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
vec2 randomRotation(vec2 fragCoord) {
	float angle = rand(fragCoord) * 6.2831853;
	return vec2(cos(angle), sin(angle));
}

float calcShadowBias(vec3 N, vec3 L, float minBias, float maxBias )
{
  float cos_t = max(dot(N,L), 0.0);
  return minBias + maxBias * (1.0 - cos_t);
}

const int BLOCKER_SAMPLES = 32;
const int PCF_SAMPLES = 32;
const float LIGHT_SIZE = 0.5;
const float CASCADE_SOFTNESS = 1.0;

#define PCSS  1

float pcf_sample(int idx, float r, float bias, int n)
{
  // shadowMtx is already in NDC coordinates[0, 1], from Tbias = > *0.5 + 0.5
  vec4 sc = shadowMtx[idx] * vviewpos;
  vec2 uv = sc.xy;
  float recv_depth = sc.z;
  vec2 rot = randomRotation (gl_FragCoord.xy);
  float cnt = 0.0;

  // compute number of occluded samples
  for (int i = 0; i < n; i++) {
    vec2 p = poisson[i];
    vec2 jit = uv + vec2(p.x * rot.x - p.y * rot.y, p.x * rot.y + p.y * rot.x) * r;
    //jit = clamp( jit, 0.0, 1.0);
    float d = texture (shadowTex, vec3(jit, float(idx))).r;
    if (d + bias >= recv_depth) cnt += 1.0;
  }
  return min( 2.0*cnt / float(n), 1.0);
}

float pcss_blocker_search (int idx, float radius, float bias )
{
  // shadowMtx is already in NDC coordinates, recv_depth [0, 1], from Tbias => *0.5 + 0.5
  vec4 sc = shadowMtx[idx] * vviewpos;      
  vec2 uv = sc.xy;
  float recv_depth = sc.z;  
  vec2 rot = randomRotation (gl_FragCoord.xy);
  float sum = 0.0, cnt = 0.0;

  // compute average blocker distance
 for (int i = 0; i < BLOCKER_SAMPLES; i++) {
    vec2 p = poisson[i];
    vec2 jit = vec2( p.x * rot.x - p.y * rot.y, p.x * rot.y + p.y * rot.x) * radius;
    float d = texture (shadowTex, vec3(uv + jit, float(idx)) ).r;
    if (d + bias <= recv_depth) { sum += d; cnt += 1.0; }
  }  
  return (cnt==0) ? max(recv_depth, 0.0) : sum / cnt;     // -1 = no blocker found
}

float pcss_compute_penumbra(float ave_d, float recv_d) 
{
  return (ave_d <= 0.0) ? 0.0 : (max(recv_d - ave_d, 0.0) / ave_d) * LIGHT_SIZE;
}
float pcss_cascade_scale(int idx) 
{ 
  return 1.0 + float(idx) * CASCADE_SOFTNESS; 
}

vec3 shadowCoeff ( float ndotl, float lgtwid, vec3 N, vec3 L, vec3 lgtclr )
{
  vec3 clr;
	float sh;
	int index = 7;		    // cascade index, correct depth map
	
  //float d = gl_FragCoord.z ;                           // incorrect  
  float d = (shadowLgtMV * invMatrix * vviewpos).z;      // correct - surface point depth (in light space)

	if (d < shadowFars1.x)		index = 0;
	else if (d < shadowFars1.y)	index = 1;
	else if (d < shadowFars1.z)	index = 2;	
	else if (d < shadowFars1.w)	index = 3;	
	else if (d < shadowFars2.x)	index = 4;	
	else if (d < shadowFars2.y)	index = 5;	
	else if (d < shadowFars2.z)	index = 6;

  // vec4 shadow_coord = shadowMtx[index] * vviewpos;		// transform into light space. we require viewpos because we need to recover the z-depth of the test point
	// shadow_coord.w = shadow_coord.z;										// R value = depth value of the current sample point (vviewpos) in lights view
	// shadow_coord.z = float(index);											// layer (split)																
  // return shadow2DArray(shadowTex, shadow_coord).x;		// [optional] single sample only (fast)

  int srch_idx = max(index-1, 0);

  #ifdef PCF 
    float bias = calcShadowBias(N, L, 0.001, 0.010);    	  
    sh = pcf_sample ( index, 0.001, bias, PCF_SAMPLES );  
    clr = vec3(sh, sh, sh) * lgtclr;
  #endif
  #ifdef PCSS
    float bias = calcShadowBias(N, L, 0.001, 0.010);
    float blk_search_radius = max(lgtwid*0.001, 1e-6);       
    float blk_ave_depth = pcss_blocker_search ( srch_idx, blk_search_radius, bias );    
     
    if (blk_ave_depth < 0.0 ) {
      clr = vec3(0,0,1);   //pcf_sample ( index, 0.001, bias, PCF_SAMPLES );   // no blocker 
    } else {
      float penumbra_radius = 0.10 * pcss_compute_penumbra ( blk_ave_depth, (shadowMtx[srch_idx]*vviewpos).z ) 
                                    * pcss_cascade_scale( srch_idx );      
      sh = pcf_sample ( index, penumbra_radius, bias*0.5, PCF_SAMPLES);
      clr = vec3(sh, sh, sh) * lgtclr;      
    }
  #endif
	return clr;
}

vec3 sampleEnv (sampler2D envMap, vec3 dir) {
	float phi = atan(dir.z, dir.x);
	float theta = acos(clamp(dir.y, -1.0, 1.0));
	vec2 uv = vec2(phi < 0.0 ? phi + 2.0 * PI : phi, theta) / vec2(2.0 * PI, PI);
	return texture(envMap, uv).rgb;
}

// Van Der Corpus sequence
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float vdcSequence(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley sequence
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 hammersleySequence(uint i, uint N)
{
	return vec2(float(i) / float(N), vdcSequence(i));
}
// Sample to uniform hemisphere
vec3 sampleToDir (vec2 xi) {
	float phi = 2.0 * 3.14159265 * xi.x;
	float cosTheta = sqrt( 1.0 - xi.y );  // 1.0 - xi.x;			
	float sinTheta = sqrt( xi.y );
	return vec3(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta );
} 

// Direction to texture UV
vec2 dirToUV (vec3 dir) {
	float u = atan(dir.z, dir.x) * 0.5 / 3.141592 + 0.5;
	float v = 1.0 - (asin(dir.y) / 3.141592 + 0.5);
	return vec2(u,v);
}
