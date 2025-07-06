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

#ifndef DEF_TRANSFORM
	#define DEF_TRANSFORM

	#include "object.h"	
	#include <vector>

	class Transform : public Object {
	public:
		Transform();

		virtual objType getType()	{ return 'tfrm'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);
		virtual void Sketch (int w, int h, Camera3D* cam);

		void SetXform (Vec3F pos, Vec3F angs, Vec3F scal );
		void SetXform (Vec3F pos, Quaternion rot, Vec3F scal );
		
	private:
		
		Vec3F m_pos;		
		Vec3F m_scal;
		Quaternion m_rot;
	};

#endif