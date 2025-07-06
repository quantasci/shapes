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

#include "point_psys.h"
#include "points.h"
#include "image.h"
#include "object.h"

#define PSYS_MAX	0
#define PSYS_VMIN	1
#define PSYS_VMAX	2

void PointPSys::Define (int x, int y)
{
	AddInput ( "time",		'Atim' );		// time must come first
	AddInput ( "start",		'pnts' );		// initial positions
	AddInput ( "height",	'Aimg' );		// terrain collisions
	AddInput ( "shader",	'Ashd' );

	AddParam (PSYS_MAX,	"max_particles",	"i");	SetParamI (PSYS_MAX, 0, 20000 );
	AddParam (PSYS_VMIN, "vol_min", "3");	SetParamV3(PSYS_VMIN, 0, Vec3F(0,0,0) );
	AddParam (PSYS_VMAX, "vol_max", "3");	SetParamV3(PSYS_VMAX, 0, Vec3F(20,20,20));

	SetInput ( "shader", "shade_pnts" );

	mTimeRange = Vec3F(0, 10000, 0);
}


void PointPSys::Generate (int x, int y)
{
	CreateOutput ( 'Apnt' );
	Points* pnts = getOutputPoints();
	if (pnts == 0x0) return;

	pnts->AllocatePoints(getParamI(PSYS_MAX));
	pnts->Setup(Vec3F(0, 0, 0), Vec3F(500, 100, 500), 0.02f, 0.008f, 0.75f, 0.02f);

	// Inital positions
	Points* starts = (Points*) getInputResult ( "start" );		
	if ( starts != 0x0 ) {	
		// copy subset of initial points		
		if (starts->getNumPoints() == 0) return;		
		pnts->CreateSubset(starts, getParamI(PSYS_MAX));

	} else {	
		// random init in box
		Vec3F vmin = getParamV3(PSYS_VMIN);
		Vec3F vmax = getParamV3(PSYS_VMAX);
		pnts->CreatePointsRandomInVolume ( vmin, vmax );
	}	
	pnts->CommitAll ();	

	// Setup simulation		
	pnts->Advect_Init();		// init advect		
	pnts->Accel_Init();
  //pnts->SPH_Init();	
	pnts->Restart();				
		
	MarkDirty();
}


void PointPSys::Run (float time)
{
	Points* pnts = getOutputPoints();
	if (pnts==0x0) return;
	
	pnts->Run_Accel();
	//pnts->Run_SPH();
	pnts->Run_Advect();

	pnts->Commit();

	// mark clean to avoid render UpdatePoints. point VBOs are directly updated by Advect via interop
	MarkClean();	
}