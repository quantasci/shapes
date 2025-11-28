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

#ifndef DEF_POINTS
	#define DEF_POINTS

	#include <iostream>
	#include <vector>
	#include <stdio.h>
	#include <stdlib.h>
	#include <math.h>	
	#include "vec.h"
	#include "camera.h"
	#include "object.h"
	#include "shapes.h"			// for debugpnts
	#include "mersenne.h"		// for point stealing
	#include "set.h"			// set only used here for shape globals

	#include "datax.h"
	#include "fluid.h"			// SPH fluid particles	

	#define FUNC_INSERT			0
	#define	FUNC_COUNTING_SORT	1
	#define FUNC_COMPUTE_PRESS	2
	#define FUNC_COMPUTE_FORCE	3
	#define FUNC_ADVANCE		4
	#define FUNC_FPREFIXSUM		5
	#define FUNC_FPREFIXFIXUP	6
	#define FUNC_FORCE_TERRAIN	7
	#define FUNC_POINTS_TO_DEM	8
	#define FUNC_SMOOTH_DEM		9
	#define FUNC_GRAVITY		10
	#define FUNC_ADVECT_FIRE	11
	#define FUNC_SPREAD_FIRE	12

	#define FUNC_MAX			16

	class Image;

	class Points : public Set, public Object {			// set only used here for shape globals
	public:
		Points ();
		~Points ();
		virtual objType getType()		{ return 'Apnt'; }	
		virtual void Sketch (int w, int h, Camera3D* cam);
	
		// Allocation
		void AllocatePoints (int pmax );

		// Create 
		void CreateFrom ( Points* src );
		void CreateSubset ( Points* src, int cnt );
		void CreatePointsRandomInVolume(Vec3F min, Vec3F max);
		void CreatePointsUniformInVolume ( Vec3F min, Vec3F max );	
		void CreatePointsFromBuf ( int srcnum, Vec3F* pos, uint64_t* srcextra, Vec3F reorig, Vec3F rescal, Vec3F repos );
		int  CreateSphere( int cnt, float radius );
		//void CreateRing ( )
		int  AddPoint ();
		int  AddPoint ( Vec3F pos, uint clr );
		int  AddPointRandom ( Vec3F pos, uint clr );		

		// Setup		
		void Setup ( Vec3F bmin, Vec3F bmax, float dt=0.04f, float sim_scale=0.01f, float grid_space=0.75f, float smoothradius=0.01f );
		void LoadKernel ( int id, std::string kname );				
		void Colorize ( Vec4F recolor, Vec4F offset );					
		void Clear ();
		void Map();
		void Unmap();
		void Restart (bool sym=true);
		void DebugPrintMemory ();		
		void SetColor ( Vec3F clr );		
		void Draw ( int frame, Camera3D* cam, float rad );		// OpenGL points

		// Data Channels
		void AddChannel (int id, std::string name, char dt);
		int FindChannel ( std::string name );
		void SetChannelData ( int id, int first, int last, float val );

		// Advection		
		void Advect_Init();
		void Run_GravityForce (float factor);
		void Run_Advect ();
		void AdvanceTime ();		

		// Acceleration grid					
		void Run_Accel ();		
		void Accel_Init ();		
		void Accel_RebuildGrid ();	
		void Accel_InsertParticles ();		
		void Accel_PrefixScanParticles ();
		void Accel_CountingSort ();	
		
		// SPH Simulation (CPU & GPU)
		void Run_SPH ();
		void SPH_Init ();		
		void SPH_SetupParams ();
		void SPH_SetupExample ();
		void SPH_UpdateParams ();
		void SPH_ComputePressure ();
		void SPH_ComputeForce ();

		// Fire Sim
		void Run_Fire ( Points* pntsB, float time );
		void Fire_Init ( Points* pntsB );

		// DEM Models
		void Run_DEMTerrainForce();
		void DEM_Init (Image* img=0x0);				
		void DEM_PointsToDEM ( Image* img, bool over=false );
		void DEM_Smooth ( Image* img );

		// Data transfers
		void UpdateGPUAccess(bool sym=true);
		void CommitParams();
		void CommitAll ();
		void Commit(int b = FPOS);
		void CopyAllToTemp ();		
		void Retrieve ( int buf );		
		void DebugPrint ( int buf, int start=0, int disp=20 );		
		void Randomize ( Vec3F vmin, Vec3F vmax );

		// Acceleration Queries
		int getGridCell ( int p, Vec3I& gc );
		int getGridCell ( Vec3F& p, Vec3I& gc );		
		Vec3I getCell ( int gc );		
	
		// Query functions
		int getNumPoints ()						{ return mNumPoints; }
		Vec3F* getPos ( int n )				{ return m_Points.bufF3(FPOS,n); }
		Vec3F* getVel ( int n )				{ return m_Points.bufF3(FVEL,n); }
		uint*  getClr ( int n )				{ return m_Points.bufUI(FCLR,n); }	
		float* getBufF (int i)				{ return m_Points.bufF(i); }
		int getBufSz(int i)						{ return m_Points.getBufSz(i); }
		int getVBO(int i)						{ return m_Points.glid(i); }
		void SetDebug(bool b)	{ m_bDebug = b; }
		Vec3F getBMin()		{ return m_Params.bound_min; }
		Vec3F getBMax()		{ return m_Params.bound_max; }
	
	private:
		bool					m_bGPU;					// CPU or GPU execution
		bool					m_bDebug;			
		int						m_Frame;	
		bool					m_ACCL, m_SPH, m_DEM, m_FIRE;		// sim modes
		Mersenne				m_rand;

		// Particle Buffers
		int						mNumPoints;
		int						mMaxPoints;
		DataX					m_Points;				// Particle buffers
		DataX					m_PointsTemp;
		DataX					m_Accel;				// Acceleration buffers
		FParams_t				m_Params;				// Fluid parameters (should be made DataX in future)

		#ifdef BUILD_CUDA
		CUmodule				m_Module;			// CUDA Kernels
		CUfunction				m_Func[ FUNC_MAX ];		
		CUdeviceptr				m_cuParams;
		#endif

		std::vector<Shape>		m_DebugPnts;			// debug draw

		Image*					m_Terrain;

		float m_lastfire;
	};	

	

#endif
