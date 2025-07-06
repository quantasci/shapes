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

#ifndef DEF_SHADER
	#define DEF_SHADER

	#include <string>		
	#include "vec.h"
	#include "object.h"

	class Shader : public Object {
	public:
		Shader ();
		virtual objType	getType()		{ return 'Ashd'; }		
		virtual void	Clear ();		
		virtual bool	Load ( std::string fname )	{ return true; }			// shaders are defer loaded for when there is a GL context

		//-- member vars

		// mFilename contains shader name, inherited from Object

	};

#endif


