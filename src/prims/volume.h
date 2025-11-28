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

#ifndef DEF_VOLUME_H
	#define DEF_VOLUME_H

	#include "object.h"
	#ifdef BUILD_GVDB	
		#include "gvdb.h"
	#endif

	class LightSet;

	class Volume : public Object {
	public:
		Volume () { 
			#ifdef BUILD_GVDB
				gvdb = 0x0; 
			#endif	
		}
		~Volume () {};	
		virtual objType getType()		{ return 'Avol'; }		
		virtual void Clear() {};
		//virtual void Sketch (int w, int h, Camera3D* cam);		
		void Sketch3D (int w, int h, Camera3D* cam);

		void Initialize (int w, int h);
		void SetTransferFunc ();
		void Render (Camera3D* cam, LightSet* lset, int w, int h, int out_tex, int depth_tex=-1);

		#ifdef BUILD_GVDB
			VolumeGVDB* getGVDB() {return gvdb;}
		#endif	
	
	public:
		#ifdef BUILD_GVDB
			VolumeGVDB*		gvdb;
		#endif

		int				mTexRID;			// texture applied

		int				mRX, mRY;
		int				mRenderShade;
		int				mRenderChan;
	};

#endif