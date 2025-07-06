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

#include "shapes.h"

Shapes::Shapes()
{
	AddBuffer ( BSHAPE, "shape", sizeof(Shape), 1 );
}

/*void Shapes::AddPhysics()
{
	AddBuffer ( BVEL, "vel", sizeof(Vec3F), 1 );
	AddBuffer ( BDIR, "dir", sizeof(Vec3F), 1 );
	AddBuffer ( BAGE, "age", sizeof(Vec3F), 1 );
}*/


void Shapes::Clear ()
{
	EmptyAllBuffers();
}

void Shapes::CopyFrom (Shapes* src)
{
	src->CopyBuffer ( 0, 0, this, DT_CPU );
}
void Shapes::AddFrom (int buf, Shapes* srclist, int lod, int max_lod )
{
	Shape* src = srclist->getShape(0);					// start of source list
	int i;

	for (int n=0; n  < srclist->getNumShapes(); n++) {	
		if ( (max_lod - src->lod) <= (max_lod - lod)) {		 // accept if shape lod <= target lod
			i = AddElem(buf);
			memcpy ( getShape(i), src, sizeof(Shape) );				
		}
		src++;
	}
}


Shape* Shapes::AddShapeByCopy ( Shape* s )
{
	int i = AddElem( 0 );
	Shape* dest = getShape(i);
	memcpy ( dest, s, sizeof(Shape) );
	return dest;
}

Shape* Shapes::Add (int& i)
{
	i = AddElem ();
	return getShape(i)->Clear();
}

void Shapes::Delete(int i)
{

}


