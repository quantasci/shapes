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

#ifndef DEF_VOLUME
	#define DEF_VOLUME

	#include "object.h"	
	#include "camera3d.h"
	#include "spline.h"
	
	#define C_FOV		0
	#define C_NEARFAR	1
	#define C_DISTDOL	2	
	#define C_ANGS		3	
	#define C_FROM		4
	#define C_TO		5
	#define C_DOF		6
	#define C_KEY		7

	class Camera : public Spline, public Object {			// Object wrapper for Camera3D 
	public:
		Camera() {};
		~Camera() {};

		virtual objType getType()	{ return 'camr'; }			
		virtual void Clear() {};
		virtual void Define (int x, int y);
		virtual void Generate(int x, int y);
		virtual void Run(float t);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Sketch ( int w, int h, Camera3D* cam );
		
		void		WriteFromCam3D ( Camera3D* cam);
		void		ReadToCam3D ( Camera3D* cam );
		Camera3D*	getCam3D() { return &cam3d; }

		void		UpdateKeys ();
		void		AddKey (float time);	
		float		getEndTime ()	{ return m_EndTime; }

		Camera3D	cam3d;

		std::vector<int>	m_Keys;
		float				m_EndTime;
	};

#endif

