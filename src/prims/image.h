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

#ifndef DEF_IMAGEX
	#define DEF_IMAGEX

	#include <string>	
	#include "imagex.h"	
	#include "object.h"
	#include "dataptr.h"
	

	class Image : public ImageX, public Object {
	public:
		virtual objType getType() { return 'Aimg'; }

		Image () : ImageX() {};
		Image ( int xr, int yr, ImageOp::Format fmt, uchar use_flags=DT_CPU ) : ImageX(xr,yr,fmt,use_flags) {};
		Image ( std::string name, int xr, int yr, ImageOp::Format fmt ) : ImageX(name,xr,yr,fmt) {};

		virtual bool Load(std::string fname) { 
			bool result = ImageX::Load(fname);				// load image
			if (result) {
				SetFilter ( ImageOp::Filter::Linear );		// default filtering
				
				Commit ( DT_CPU | DT_GLTEX );
			}
			return result;
		}
	};


#endif




