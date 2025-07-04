
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
layout(location = 11) in vec4 instIDS;
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
uniform vec3	camPos;
uniform mat4	objMatrix;

// per-pixel outputs
out vec4 vworldpos;
out	vec3 vnormal;
out vec3 vtexcoord;
out vec4 vtexpiv;
out vec4 vclr;
flat out vec4 vids;
flat out ivec4 vtexids;
flat out vec4 vtexsub;

mat4 getShapeMatrix ( vec3 pos, vec4 r, vec3 s, vec3 piv, out mat4 m )
{
    // quaternion rotation matrix
	m[0][0] = 1.0f - 2.0f * r.y * r.y - 2.0f * r.z * r.z;	m[1][0] = 2.0f * r.x * r.y + 2.0f * r.z * r.w;			m[2][0] = 2.0f * r.x * r.z - 2.0f * r.y * r.w;	m[3][0] = 0;
	m[0][1] = 2.0f * r.x * r.y - 2.0f * r.z * r.w;			m[1][1] = 1.0f - 2.0f * r.x * r.x - 2.0f * r.z * r.z;	m[2][1] = 2.0f * r.z * r.y + 2.0f * r.x * r.w;	m[3][1] = 0;
	m[0][2] = 2.0f * r.x * r.z + 2.0f * r.y * r.w;			m[1][2] = 2.0f * r.z * r.y - 2.0f * r.x * r.w;			m[2][2] = 1.0f - 2.0f * r.x * r.x - 2.0f * r.y * r.y;	m[3][2] = 0;
	m[0][3] = 0; m[1][3] = 0; m[2][3] = 0; m[3][3] = 1;
	
	// composite matrix: M' = Tpos R S Tpiv
	return mat4( s.x * m[0][0],	s.x * m[1][0],	s.x * m[2][0],		0,
				 s.y * m[0][1],	s.y * m[1][1],	s.y * m[2][1],		0,
				 s.z * m[0][2],	s.z * m[1][2],	s.z * m[2][2],		0,
				pos.x + (piv.x * s.x * m[0][0]) + (piv.y * s.y * m[0][1]) + (piv.z * s.z * m[0][2]),
				pos.y + (piv.x * s.x * m[1][0]) + (piv.y * s.y * m[1][1]) + (piv.z * s.z * m[1][2]),
				pos.z + (piv.x * s.x * m[2][0]) + (piv.y * s.y * m[2][1]) + (piv.z * s.z * m[2][2]), 1);
}

void main() 
{
	int inst = gl_InstanceID;		
	
	mat4 rotMtx;
	vworldpos =  objMatrix * getShapeMatrix( instPos, instRot, instScale, instPivot, rotMtx ) * vec4(inPos, 1);	
	
    vids = instIDS;		// picking IDs
	
	gl_Position = projMatrix * viewMatrix * vworldpos;
}
