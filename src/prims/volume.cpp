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

#include "volume.h"
#include "common_defs.h"
#include "lightset.h"
#include "image.h"
#include "gxlib.h"
using namespace glib;


void Volume::Initialize (int w, int h)
{
	#ifdef USE_GVDB
		gvdb = new VolumeGVDB;
		gvdb->SetVerbose ( true );
		gvdb->SetCudaDevice ( GVDB_DEV_CURRENT );		// Use OptiX context already created
		gvdb->Initialize ();	
		gvdb->UseOpenGLAtlas ( true );
	
		/*char fpath[1024];
		getFileLocation ( "explosion.vbx", fpath );
		gvdb->LoadVBX ( fpath );
		gvdb->UpdateApron();
		Vec3I a = gvdb->getVoxMin();
		Vec3I b = gvdb->getVoxMax();
		dbgprintf ( " GVDB Loaded: %s, min:<%d,%d,%d>, max:<%d,%d,%d>\n", fpath, a.x, a.y, a.z, b.x, b.y, b.z ); */

		gvdb->SetTransform ( Vec3F(0, 0, 0), Vec3F(1, 1, 1), Vec3F(0, 0, 0), Vec3F(0,0,0) );

		mRX = w; mRY = h;
		gvdb->AddRenderBuf ( 0, w, h, 4 );
		gvdb->AddDepthBuf ( 1, w, h );	
		mRenderShade = SHADE_TRILINEAR;
	#endif
	
	mRenderChan = 0;
}

void Volume::SetTransferFunc ()
{
	#ifdef USE_GVDB

		gvdb->SetEpsilon( 0.001, 256 );
		gvdb->getScene()->SetSteps ( 0.2f, 16, 0.2f );			// SCN_PSTEP, SCN_SSTEP, SCN_FSTEP - Raycasting steps
		gvdb->getScene()->SetExtinct ( -2.0f, 2.0f, 1.0f );		// SCN_EXTINCT, SCN_ALBEDO - Volume extinction	
		gvdb->getScene()->SetVolumeRange (0.1f, 0.0f, 0.3f);		// Threshold: Isoval, Vmin, Vmax
		gvdb->getScene()->SetCutoff(0.001f, 0.001f, 0.0f);		// SCN_MINVAL, SCN_ALPHACUT
		gvdb->getScene()->SetBackgroundClr(0.08f, 0.09f, 0.10f, 0.0f);			// alpha=0.0 for compositing

		gvdb->getScene()->LinearTransferFunc ( 0.00f, 0.25f, Vec4F(0.f, 0.f,  0.f, 0.0f), Vec4F(0.0f, 0.0f, 0.f, 0.0f) );
		gvdb->getScene()->LinearTransferFunc ( 0.25f, 0.50f, Vec4F(0.f, 0.5f, 1.f, 0.0f), Vec4F(0.0f, 0.f,  1.f, 0.2f) );
		gvdb->getScene()->LinearTransferFunc ( 0.50f, 0.75f, Vec4F(0.f, 0.f,  1.f, 0.2f), Vec4F(0.0f, 1.f,  1.f, 0.3f) );
		gvdb->getScene()->LinearTransferFunc ( 0.75f, 1.00f, Vec4F(0.f, 1.f,  1.f, 0.3f), Vec4F(0.0f, 1.f,  1.f, 1.0f) );
		gvdb->CommitTransferFunc ();	
	#endif
}

void Volume::Render (Camera3D* cam, LightSet* lset, int w, int h, int out_tex, int depth_tex)
{
	#ifdef USE_GVDB
		if ( mRX != w || mRY != h ) {
			mRX = w; mRY = h;
			gvdb->ResizeRenderBuf ( 0, w, h, 4 );	
			gvdb->ResizeDepthBuf ( 1, w, h );
		}
		gvdb->getScene()->SetLight ( 0, lset->getLight(0)->pos, lset->getLight(0)->diffclr );
		gvdb->getScene()->SetCamera ( cam );
		if ( depth_tex >= 0 ) {
			gvdb->getScene()->SetDepthBuf( 1 );
			gvdb->WriteDepthTexGL ( 1, depth_tex );
		}
		gvdb->Render ( mRenderShade, mRenderChan, 0 );
		gvdb->ReadRenderTexGL ( 0, out_tex );
	#endif
}


void Volume::Sketch3D (int w, int h, Camera3D* cam)
{
	#ifdef USE_GVDB
		Matrix4F xform;
		GVDBNode* node;
		Vec3F bmin, bmax;

		Vec3F clrs[5];
		clrs[0] = Vec3F(0,0,1);			// blue
		clrs[1] = Vec3F(0,1,0);			// green
		clrs[2] = Vec3F(1,0,0);			// red
		clrs[3] = Vec3F(1,1,0);			// yellow
		clrs[4] = Vec3F(1,0,1);			// purple		

		xform = gvdb->getTransform ();
	
		for (int lev=0; lev < 5; lev++ ) {
			for (int n=0; n < gvdb->getNumNodes(lev); n++) {			
				node = gvdb->getNodeAtLevel ( n, lev );
				bmin = gvdb->getNodeMin ( node ); bmin.y=0.1;
				bmax = gvdb->getNodeMax ( node ); bmax.y=0.1;
				// drawBox3DXform ( bmin, bmax, clrs[lev], xform );
			}		
		}
	#endif
}

