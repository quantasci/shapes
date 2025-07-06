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

#ifndef DEF_POINT_PSYS
	#define DEF_POINT_PSYS

	#include "point_base.h"	

	class PointPSys : public PointBase {
	public:

		virtual objType getType()	{ return 'psys'; }
		virtual void Define (int x, int y);
		virtual void Generate (int x, int y);
		
		virtual void Run ( float time );		// particle systems animate

	protected:

	};

#endif