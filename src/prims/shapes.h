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

#ifndef DEF_SHAPES
	#define DEF_SHAPES

	//#include "types.h"
	#include "vec.h"
	#include "quaternion.h"
	#include "datax.h"
	#include "object.h"		

	// SHAPES
	// Shapes are the definitive structure for the SHAPES rendering engine
	// - State-sorted Instancing is the core principle of SHAPES
	// - Each shape is an instance. The primary shapes struct contains data used in instancing shaders. 
	// - Shape model matrices are compute from pos, rot, scale, pivot to keep memory low --> computed by calling getXform, or on-the-fly in shaders	  *NOTE* perhaps significant if not computed per-vertex shader (slow)
	// - Shape meshids contains the shape mesh and shader --> which are used as the key for render state sorting
	// - Shape texids contains the shape texture --> which are ids for bindless texturing
	// - Shape data is maintained in DataX buffers
	// - Additional per-shape data is maintained in additional DataX buffers. Created on demand by the caller.

	// DataX buffers:
	#define BSHAPE		0			// pos, rot, scale, pivot, ids, texids, meshids, clr, type ==> shape struct
	
	#define BLEV		1			// hierarchical data	
	#define BCHILD		2
	#define BNEXT		3
	#define BVARI		4

	#define BVEL		5			// physics data
	#define BDIR		6
	#define BAGE		7	
	#define BSIDE		8	
	#define BGROW		9

	// Shape primary struct:
	struct Shape {	
	public:				
		Shape() { Clear(); }
		Shape(Vec3F p, uint c) { pos=p; clr=c; }
		Shape* Clear() { type = -1; lod = 0; invisible = 0; pos.Set(0,0,0); rot.Identity(); clr = COLORA(1,1,1,1); pivot.Set(0,0,0); scale.Set(1,1,1); 
				  	 texsub.Set(0,0,1,1); matids.Set(NULL_NDX); meshids.Set(-1,0,0,0); ids.Set(0,0,0); return this; }

		Matrix4F getXform() {
			Matrix4F m;
			// m.Identity(); m.Translate ( pos ); m.Rotate ( rot.getMatrix() ); 	m.Scale ( scale ); 	m.Translate ( pivot );		// slow version
			m.TRST ( pos, rot, scale, pivot );			// fast version
			return m;
		}			
		bool isSegment ()	{ return (type==S_INTERNODE) || (type==S_NODE); }
		bool isLeader ()	{ return type==S_LEADER; }		
		bool isInvisible()	{ return invisible; }
		// Instance data
		Vec3F		pos;
		Quaternion	rot;
		Vec3F		scale;
		Vec3F		pivot;
		Vec4F		ids;				// picking - make 3DI		
		Vec8S		matids;				// material		
		Vec4F		texsub;		
		Vec4F		meshids;			// mesh data. x=asset ID, y=shader ID, z=mesh face start, w=mesh face cnt.. --> 16-byte key for state-sorting
		uint		clr;				// color
		char		type;				// shape type - *move to group*
		char		invisible;
		uchar		lod;				// lod
	};

	class Shapes : public DataX, public Object {
	public:
		Shapes();
		virtual objType getType()		{ return 'Ashp'; }		

		void		Clear ();	
		void		AddFrom (int buf, Shapes* src, int lod=0, int max_lod=4);
		void		CopyFrom (Shapes* src);
		
		Shape*		Add (int& i);				// new shape
		Shape*		AddShapeByCopy (Shape* s);	// copy from another shape
		void		Delete(int i);
		int			getNumShapes()				{ return GetNumElem(0); }		
		int			getSize()					{ return GetBufSize(0); }
		char*		getData(int b)				{ return (char*) GetStart( b ); }
		char*		getElem (int b, int i)		{ return (char*) GetElem (b,i ); }
		Shape*		getShape (int i)			{ return (Shape*) GetElem (BSHAPE, i); }		
		void		SetData (int buf, int i, char* src, int sz ) { 
						char* dest = GetElem(buf, i);
						assert ( sz == GetBufStride(buf) );			// should not need sz
						memcpy ( dest, src, GetBufStride(buf) );
					}
	};

#endif