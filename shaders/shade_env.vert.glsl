
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
uniform vec3	camNearFar;

// per-pixel outputs
out vec4 vworldpos;
out	vec3 vnormal;
out vec3 vtexcoord;
out vec4 vclr;
flat out ivec4 vmatids;
flat out vec4 vtexsub;

vec3 getViewDir (float x, float y)
{
	vec3 camU = vec3( viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0] );
	vec3 camV = vec3( viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1] );
	vec3 camS = vec3( viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2] );

    return - normalize( x*camU + y*camV + camS );		// direction only,  range: x,y = [-1,1]  (from model_square mesh)
}

void main() 
{
	int inst = gl_InstanceID;		

	vnormal = getViewDir ( inPos.x, inPos.z );					

	vworldpos =  vec4( camPos + vnormal * camNearFar.y, 1 );			// construct 3D frustum quad at cam far distance

	vtexcoord = vec3 ( inTexCoord*2.0 - 1.0, gl_InstanceID );	
	vtexsub = instTexSub;
	vmatids = ivec4 ( instMatIDS );

	gl_Position = projMatrix * viewMatrix * vworldpos;
}
