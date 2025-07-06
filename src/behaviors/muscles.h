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

#ifndef DEF_MUSCLES
	#define DEF_MUSCLES

	#include "object.h"	
	#include <vector>

	#define BMUSCLE		10		// muscle shape extra data

	struct Muscle {		
		char		name[128];
		Vec3F	p1, p2;		// attachment points (world units)
		Vec3I	b1, b2;		// bones connected
		Vec3F	specs;		// muscle dimensions (width, length, hgt)
		Vec3F	norm;		// up direction
		float		rest_len;	// rest length
		float		angle;		// up orientation
		float		curve;		// muscle curvature
	};

	class Muscles : public Object {
	public:
		Muscles() : Object() { }

		virtual objType getType()	{ return 'musl'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);	
		virtual void Run ( float time );
		virtual void Sketch ( int w, int h, Camera3D* cam );
		virtual bool Select3D(int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir);
		virtual void Adjust3D(int w, Vec4F& sel);
		
		void Clear();
		void Save(std::string fname);
		bool Load(std::string fname);

		int AddMuscle(Vec3F p1, Vec3F p2, Vec3I m1, Vec3I m2, Vec3F norm);
		void DeleteMuscle(int i);
		Muscle* getMuscle(int n) { return &mPartList[n]; }


	private:		

		std::vector< Muscle >		mPartList;
		
		Vec3I					mSel;
	};

#endif