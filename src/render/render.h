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

#ifndef DEF_RENDER_MGR_H
	#define DEF_RENDER_MGR_H

	#include "set.h"
	#include "render_base.h"
	
	#define REND_GL				0
	#define REND_OPTIX			1

	// #define DEBUG_STATE				// debug state sorting

	class RenderMgr {
	public:
		RenderMgr ();

		// General Functions
		void	AddRenderer ( RenderBase* rend, int render_tex );
		void	Initialize ( Scene* scene );		// build renderer list
		bool	RecordFrame ();		
		bool	SaveFrame ( char* filename );
		void	TriggerSceneUpdate ();
		bool	DoAdvance ();

		// Render Settings
		void	SetOption ( int opt, int val, int rend=-1);
		void	SetFrame (int f)					{ m_CurrentFrame = f; }
		void	SetFrameRange ( Vec3I range )	{m_FrameRange = range;}
		void	SetFrameAdvance ( int adv )			{m_FrameAdv = adv; }
		void	SetAnimation ( int state );
		void	SetAnimateCamera ( int state );
		void	SetRecording ( int state );
		void	SetRenderer ( int id );		
		void	SetRendererForRecording();	
		Vec3F GetFrameRes ();
		int		getCurrentRenderer()	{ return m_CurrentRenderer; }
		bool	isAnimating ()			{ return m_AnimatingNow; }
		bool	isRecording()			{ return m_RecordingEnabled; }

		// Rendering Specific		
		void	Render (int w, int h, int pick_tex);
		void	PrepareAssets ();					
		void	Sketch3D ();
		void	Sketch2D ();		
		void	RenderPicking ( int w, int h, int buf );
		void	UpdateRes (int w, int h, int MSAA);
		void	UpdateCamera ();
		void	UpdateLights();
		Vec4F Pick (int x, int y, int w, int h);
		bool	getOptGL (int i)	{ return mRenderers[ REND_GL ]->isEnabled(i); }
		RenderGL* getGL()			{ return (RenderGL*) mRenderers[REND_GL]; }

		int		getFrame()		{return m_CurrentFrame;}
		Scene*	getScene()		{return mScene;}			
		int		getSel()		{return mSelect;}
		
	private:		
		Scene*					mScene;	

		std::vector<RenderBase*>	mRenderers;

		int						m_RecordingEnabled;
		int						m_RecordingNow;
		int						m_AnimatingNow;
		int						m_AnimatingCamera;
		int						m_CurrentRenderer;
		int						m_CurrentFrame;
		int						m_FrameDone;
		int						m_FrameAdv;
		Vec3I					m_FrameRange;
		Vec3F					m_FrameSize;
		
		int						mSelect;
		bool					mbSketchObj;
		bool					mbSketchShps;
	};



#endif