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

#include "transform.h"
#include "object.h"
#include "shapes.h"
#include "mesh.h"

#include "gxlib.h"
using namespace glib;

Transform::Transform() : Object() 
{
	m_pos.Set(0,0,0);	
	m_scal.Set(1,1,1);
	m_rot.Identity();
}

bool Transform::RunCommand(std::string cmd, vecStrs args)
{
	if (cmd.compare("xform") == 0) { 
		SetXform ( strToVec3(args[0], ';'), strToVec3(args[1], ';'), strToVec3(args[2], ';') );
		return true; 
	}
	if (cmd.compare("finish") == 0) { return true; }

	return false;
}
void Transform::SetXform(Vec3F pos, Vec3F angs, Vec3F scal )
{
	m_pos = pos;	
	m_scal = scal;
	m_rot.fromEuler( angs );
}

void Transform::SetXform(Vec3F pos, Quaternion rot, Vec3F scal )
{
	m_pos = pos;	
	m_scal = scal;
	m_rot = rot;
}

void Transform::Define (int x, int y)
{
	AddInput ( "material",	'Amtl' );	
	AddInput ( "mesh",		'Amsh' );
}

void Transform::Generate (int x, int y)
{
	Mesh* mesh = dynamic_cast<Mesh*> (getInput("mesh"));
	if (mesh == 0) { dbgprintf("ERROR: Transform has no geometry.\n"); return; }

	CreateOutput('Ashp');

	SetOutputXform();

	// Add shape
	Shape* s;
	s = AddShape();
	s->ids = getIDS(0, 0, 0);
	s->type = S_MESH;

	s->matids = getInputMat( "material" );
	s->meshids.Set ( getInputID("mesh", 'Amsh'), 0, 0, 0);

	s->clr = COLORA(1, 1, 1, 1);
	s->pos = m_pos;
	s->scale = m_scal;
	s->rot = m_rot;
	s->pivot = Vec3F(0, 0, 0);

	MarkClean();
}

void Transform::Sketch(int w, int h, Camera3D* cam)
{
	MeshX* mesh = dynamic_cast<MeshX*> (getInput("mesh"));
	
	// Draw triangle edges
	AttrV3* f;
	Vec3F v1, v2, v3;

	for (f = (AttrV3*) mesh->GetStart(BFACEV3); f <= (AttrV3*) mesh->GetEnd(BFACEV3); f++) {
		v1 = *mesh->GetVertPos(f->v1);	
		v2 = *mesh->GetVertPos(f->v2);	
		v3 = *mesh->GetVertPos(f->v3);
		drawLine3D( v1, v2, Vec4F(1,1,1,1) );
		drawLine3D( v2, v3, Vec4F(1, 1, 1, 1));
		drawLine3D( v3, v1, Vec4F(1, 1, 1, 1));		
	}
}

