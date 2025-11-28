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

#ifdef BUILD_OPTIX

#ifndef DEF_RENDER_OPTIX_H
	#define DEF_RENDER_OPTIX_H

	#include "optix_scene.h"		// lib_optix

	#include "render_base.h"	
	#include "object_list.h"
	#include "set.h"
	#include "camera.h"
	#include "shapes.h"

	#define R_OPTIX		1			// RenderID index for OptiX

	struct RAssetOptix {
		RAssetOptix()	{ src=0; defaultShapes = 0; }
		void*			src;
		int				assetID;
		int				optixID;
		Shapes*			defaultShapes;
	};	

	class Mesh;
	class Scene;
	class LightSet;

	class RenderOptiX : public RenderBase {
	public:
		RenderOptiX ();
		
		// Rendering
		virtual void Initialize ();
		virtual void PrepareAssets ();
		virtual void StartNewFrame() ;
		virtual void StartRender ();
		virtual void Validate ();
		virtual bool Render ();		
		virtual void UpdateRes ( int w, int h, int MSAA );
		virtual void UpdateCamera ();
		virtual bool SaveFrame ( char* filename );

		// OptiX Resources
		//	
		void		RestartRender ();
		void		SetOptix ( OptixScene* o )	{ optix = o; }
		bool		CreateVolume	( Object* ast );		
		bool		CreateTexture	( Object* ast );		
		bool		CreateMesh		( Object* ast );
		bool		CreateMaterial	( Object* ast );

		bool		UpdateLights	( LightSet* lgts );

		bool		UpdateMesh ( Object* obj, Vec8S& matids );		
		OptixMeshInfo	getMeshBufferInfo ( Mesh* mesh, int face_cnt=0 );
		int			getOptixTexture ( int tid );
		
		void		UpdateMaterial ( Object* obj );
		void		UpdateMaterials ();
		int			getOptixMaterial ( Vec8S* matids );
		void		SetRegion ()	{ mRegionX=-1; }
		void		SetRegion ( int rx, int ry, int rw, int rh ) {mRegionX=rx; mRegionY=ry; mRegionW=rw; mRegionH=rh; }
		void		SetSample (int s)	{ mSample = s; }

		int			CountRays ();
		void		BuildRenderGraph ();
		
	private:
		OptixScene*	optix;

		int			mSample;
		int			mMaxSamples;
		bool		mDenoise;
		int			mRegionX, mRegionY, mRegionW, mRegionH;

		int			mMatDeep, mMatSurf1;
		int			mVDB;		

		float		m_ttotal, m_tave, m_tsumsq;

		std::vector<RAssetOptix>	mTextures;
		std::vector<RAssetOptix>	mMeshes;		
		std::vector<RAssetOptix>	mMaterials;
		
		int			mLightCnt;

		int			mShademeshID;
	};

#endif

#endif