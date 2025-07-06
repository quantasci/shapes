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

#ifndef DEF_OBJ_MESH
	#define DEF_OBJ_MESH

	#include <string>
	#include <vector>
	#include <map>	

	#include "meshx.h"	
  #include "object.h"
	
	class Mesh : public MeshX, public Object {
	public:
		Mesh () : MeshX() { 
			AddParam(0,"tex","i"); 
			SetTexture(-1); 
		}
			
		virtual objType getType()							{ return 'Amsh'; }		
		virtual Vec4F GetStats()							{ 
			return MeshX::GetStats(); 
		}
		virtual bool Load(std::string fname)	{ return MeshX::Load(fname); }

		void SetTexture(int id)  { SetParamI(0, 0, id); }
	
	};


#endif

