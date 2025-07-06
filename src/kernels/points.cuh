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

#ifndef DEF_PARTICLES_CUH
	#define DEF_PARTICLES_CUH

	#include <curand.h>
	#include <curand_kernel.h>
	#include <stdio.h>
	#include <math.h>

	#define CUDA_KERNEL
	#include "fluid.h"

	#define EPSILON				0.0001f
//	#define EPSILON				0.00001f

	#define GRID_UCHAR			0xFF
	#define GRID_UNDEF			2147483647			// max int
	#define TOTAL_THREADS		1000000
	#define BLOCK_THREADS		256
	#define MAX_NBR				80		
	#define FCOLORA(r,g,b,a)	( (uint((a)*255.0f)<<24) | (uint((b)*255.0f)<<16) | (uint((g)*255.0f)<<8) | uint((r)*255.0f) )

	typedef unsigned int		uint;
	typedef unsigned short int	ushort;
	typedef unsigned char		uchar;
	
	extern "C" {
		__global__ void insertParticles ( int pnum );		
		__global__ void countingSortFull ( int pnum );		
		__global__ void computeQuery ( int pnum );	
		__global__ void computePressure ( int pnum );		
		__global__ void computeForce ( int pnum );	
		__global__ void advanceParticles ( float time, float dt, float ss, int numPnts );
		__global__ void emitParticles ( float frame, int emit, int numPnts );
		__global__ void randomInit ( int seed, int numPnts );
		__global__ void sampleParticles ( float* brick, uint3 res, float3 bmin, float3 bmax, int numPnts, float scalar );	
		__global__ void prefixFixup ( uint *input, uint *aux, int len);
		__global__ void prefixSum ( uint* input, uint* output, uint* aux, int len, int zeroff );
		__global__ void countActiveCells ( int pnum );		
	}

#endif
