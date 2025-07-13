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

#include "render.h"
#include "render_gl.h"
#include "render_optix.h"
#include "timex.h"

#include "gxlib.h"
using namespace glib;


#include "scene.h"

RenderMgr::RenderMgr ()
{
	mScene = 0x0;

	m_RecordingEnabled = 0;
	m_RecordingNow = 0;
	m_AnimatingNow = 0;
	m_AnimatingCamera = 0;
	m_CurrentRenderer = 0;
	m_CurrentFrame = -1;
	m_FrameAdv = 0;
	m_FrameRange.Set(0,1000000,0);
}

void RenderMgr::AddRenderer ( RenderBase* rend, int render_tex )
{
	rend->SetOutputTex ( render_tex );
	mRenderers.push_back ( rend );	
}

void RenderMgr::Initialize ( Scene* scene )
{
	// Assign asset list and scene
	//  (for all renderers)
	mScene = scene;	

	for (int n=0; n < mRenderers.size(); n++) {
		mRenderers[n]->SetManager ( this );
		mRenderers[n]->Initialize ();
	}
}

void RenderMgr::PrepareAssets ()
{
	// Prepare assets on each renderer
	for (int n=0; n < mRenderers.size(); n++) 
		mRenderers[n]->PrepareAssets ();

	// Validate all renderers
	for (int n=0; n < mRenderers.size(); n++) 
		mRenderers[n]->Validate ();
}

// Set renderer - based on recording state
void RenderMgr::SetRendererForRecording ()
{
	if ( m_RecordingEnabled ) {
		if (m_CurrentFrame >= m_FrameRange.x-1) 
			m_RecordingNow = true;

		UpdateCamera();							// trigger camera update to reset sampling
		//SetAnimation ( true );		// animation may be on (video rec) or off (still frame)
	}

	Vec3F res = GetFrameRes();
	SetRenderer ( res.z );
}

Vec3F RenderMgr::GetFrameRes ()
{
	Vec3F res;
	if ( m_RecordingNow ) 
		res = mScene->getGlobals()->getParamV3( G_RECRES );
	else 
		res = mScene->getGlobals()->getParamV3( G_EDITRES );
	return res;
}

void RenderMgr::TriggerSceneUpdate ()
{
	Camera3D* cam = mScene->getCamera3D();
	Camera3D save_cam = *cam;						// save the user camera
	mScene->Execute( false, mScene->getTime(), 0.0, false);	// force new renderer to do scene update (may animate the camera)
	*cam = save_cam;								// restore user camera
	UpdateCamera();									// force camera update to saved
}

bool RenderMgr::DoAdvance ()
{
	if ( m_RecordingEnabled && m_CurrentFrame == m_FrameRange.x-1 ) {
		SetRendererForRecording ();
		TriggerSceneUpdate();
		m_CurrentFrame++;
		m_RecordingNow = true;
	}

	if ( m_AnimatingNow && (m_FrameDone || !m_RecordingNow ) ) {
		mRenderers[ m_CurrentRenderer ]->StartNewFrame ();
		return true;
	}

	return false;
}

void RenderMgr::Render (int w, int h, int pick_tex)
{
	PERF_PUSH ( " START" );
	mRenderers[ m_CurrentRenderer ]->StartRender ();	// Start Render
	PERF_POP();

	PERF_PUSH ( " RENDER" );
	m_FrameDone = mRenderers[ m_CurrentRenderer ]->Render ();	// Render in 3D
	PERF_POP();
	 
	PERF_PUSH (" SKETCH");
	Sketch3D ();										// Sketch 3D
	PERF_POP();
	
	PERF_PUSH(" FINISH");
	
	//drawAll();											// render into FBO

	mbSketchObj = false;
	mRenderers[ m_CurrentRenderer]->EndRender ();		// End Render
	PERF_POP();

	// Render picking buffer
	PERF_PUSH(" PICKING");
	RenderPicking ( w, h, pick_tex );
	PERF_POP();
}


bool RenderMgr::RecordFrame ()
{
	bool saved = false;

	if ( m_FrameDone ) {

		if ( m_RecordingNow && m_CurrentFrame >= m_FrameRange.x ) {

			char savename[512];			
			sprintf (savename, "out%05d.%s", m_CurrentFrame, (m_CurrentRenderer == 0) ? "png" : "tif" );

			bool result = mRenderers[  m_CurrentRenderer ]->SaveFrame ( savename );

			if (m_CurrentFrame == 0) {
				if (result) {
					dbgprintf ("Saved: %s\n", savename );
				} else {
					dbgprintf ("**** ERROR: Unable to save: %s\n", savename );
				}
			}
			
			/*char msg[256];
			strncpy ( msg, savefile.c_str(), 256 );
			setTextSz ( 32, 1 );
			start2D( w, h );
			drawCircle ( Vec2F(50, 50), 10, Vec4F(1,0,0,1) );	// recording dot
			drawText ( Vec2F(80, 40), msg, Vec4F(1,0,0,1) );	// frame #
			end2D();		 */

			saved = true;
		}
		m_CurrentFrame++;

	}

	return saved;	
}

//--------------------- RENDER SETTINGS
//
void RenderMgr::SetAnimation ( int state )
{
	if ( state==TOGGLE ) m_AnimatingNow = 1 - m_AnimatingNow;
	else				 m_AnimatingNow = state;
}

void RenderMgr::SetRenderer ( int render_id )
{
	if (render_id == TOGGLE)
		m_CurrentRenderer = 1 - m_CurrentRenderer;
	else
		m_CurrentRenderer = render_id;

	mScene->MarkDirty();

	if ( m_CurrentRenderer > mRenderers.size()-1 ) {
		dbgprintf ( "ERROR: Renderer #%d not available.\n", m_CurrentRenderer );
		m_CurrentRenderer = 0;		
	}

	mRenderers[ m_CurrentRenderer ]->PrepareAssets();
}

void RenderMgr::SetRecording ( int state )
{
	if ( state==TOGGLE ) m_RecordingEnabled = 1 - m_RecordingEnabled;
	else				 m_RecordingEnabled = state;

	if (m_RecordingEnabled==false) m_RecordingNow = false;
}

void RenderMgr::SetOption ( int opt, int val, int rend)
{
	if ( rend == -1 ) {
		for (int n=0; n < mRenderers.size(); n++)		// set option on all renderers
			mRenderers[n]->SetOption ( opt, val );

	} else if ( rend < mRenderers.size() ) {
		mRenderers[rend]->SetOption(opt, val);
	}
}

void RenderMgr::UpdateLights ()
{
	mRenderers[ m_CurrentRenderer ]->UpdateLights();
}

bool RenderMgr::SaveFrame (char* filename )
{
	return mRenderers[  m_CurrentRenderer ]->SaveFrame ( filename );
}

void RenderMgr::Sketch3D ()
{
	Camera3D* cam = mScene->getCamera3D();
	Vec3F res = mScene->getRes();

	// Sketch using OpenGL regardless of which renderer is active
	mRenderers[ REND_GL ]->Sketch3D ( res.x, res.y, cam );
}

void RenderMgr::Sketch2D () 
{
	Vec3F res = mScene->getRes();
	mRenderers[ REND_GL ]->Sketch2D (res.x, res.y);
}


void RenderMgr::RenderPicking ( int w, int h, int buf )
{
	if (m_CurrentRenderer==0) 
		mRenderers[0]->RenderPicking ( w, h, buf );
}
Vec4F RenderMgr::Pick (int x, int y, int w, int h)
{
	return mRenderers[0]->Pick (x, y, w, h);
}

void RenderMgr::UpdateRes (int w, int h, int MSAA)
{
	for (int n=0; n < mRenderers.size(); n++) 
		mRenderers[n]->UpdateRes (w, h, MSAA); 
}

void RenderMgr::UpdateCamera ()
{
	for (int n=0; n < mRenderers.size(); n++) 
		mRenderers[n]->UpdateCamera ();
}
