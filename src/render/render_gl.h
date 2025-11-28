
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

#ifndef DEF_RENDER_GL_H
	#define DEF_RENDER_GL_H

	#include "render_base.h"
		
	#define R_GL		0				// RenderID index for OpenGL

	#define P_NULL			-1

	#define P_MODELMTX	0
	#define P_INVMTX		1
	#define P_OBJMTX		2
	#define P_VIEWMTX		3
	#define P_PROJMTX		4
	#define P_CAMPOS		5
	#define P_CAMNF			6
	#define P_CAMUVW		7
	#define P_TEXUNI		8
	#define P_TEXTURES		9	
	#define P_LIGHTS		10
	#define P_LIGHTCNT		11
	#define P_MATERIALS		12
	#define P_PARAMS		13
	#define P_ENVMAP		14
	#define P_ENVCLR		15
	#define P_SFAR1			16
	#define P_SFAR2			17
  #define P_SLPROJ    18
  #define P_SLMV      19
	#define P_SMTX			20
	#define P_STEX			21
	#define P_SSIZE			22

	struct RTexture {
		int				assetID;
		int				texID;		
		GLuint			glID;			// GL texture
		GLuint64		handle;
		int				filter;
		int				bindtype;
	};	
	typedef std::vector<int>	RParamList;
	
	struct RShader {
		char			name[64];
		int				assetID;	
		GLuint			programID;		// GL program
		RParamList		params;			// List of parameters (GL uniform indices)
	};
	struct RMesh {
		RMesh() { meshVBO[0]=-1;meshVBO[1]=-1;meshVBO[2]=-1;meshVBO[3]=-1;meshVBO[4]=-1; }
		int				assetID;		
		int				primtype;		// Primitive type
		int				primcnt;		// Primitive count
		int			  meshVBO[5];		// VBOs: 0=pos, 1=clr, 2=norm, 3=tex, 4=faces
	};
	struct RLight {
		Vec4F		pos;
		Vec4F		target;
		Vec4F		ambclr;
		Vec4F		diffclr;
		Vec4F		specclr;
		Vec4F		inclr;
		Vec4F		shadowclr;
		Vec4F		cone;			// x=inner, y=middle, z=outer
	};
	struct RMaterial {		
		Vec4F		texids;				// texture maps. x=color, y=spec, z=displace, w=emission
		Vec4F		ambclr;
		Vec4F		diffclr;
		Vec4F		specclr;
		Vec4F		envclr;
		Vec4F		shadowclr;
		Vec4F		reflclr;
		Vec4F		refrclr;
		Vec4F		emisclr;
		Vec4F		surf_params;		// x=specular power, y=light_width, z=shadow bias, w=shadow width
		Vec4F		refl_params;		// x=refl_width, y=refl_bias
		Vec4F		refr_params;		// x=refr_width, y=refr_bias, z=ref_ior
		Vec4F		displace_depth;		// x=min depth, y=max depth
		Vec4F		displace_amt;		
		Vec4F		assetID;
	};

	// Cascade Shadow Maps
	struct frustum
	{
		float neard;
		float fard;
		float fov;
		float ratio;
		Vec3F point[8];
	};
	#define MAX_SPLITS		8

	class Scene;
	class LightSet;
	class Mesh;

	class RenderGL : public RenderBase {
	public:
		RenderGL ();

		// Rendering
		virtual void Initialize ();
		virtual void PrepareAssets ();
		virtual void StartRender ();
		virtual void Validate ();
		virtual bool Render ();
		virtual void EndRender ();
		virtual void RenderPicking ( int w, int h, int buf );
		virtual Vec4F Pick (int x, int y, int w, int h);
		virtual bool SaveFrame(char* filename);
		virtual void UpdateRes ( int w, int h, int MSAA );
		virtual void UpdateLights ();
		virtual void Sketch3D (int w, int h, Camera3D* cam);
		virtual void Sketch2D (int w, int h);

		// OpenGL Resources
		//	
		bool	CreateMesh ( Object* ast, std::string& msg);					// Meshes
		int		UpdateMesh ( Mesh* obj );
		int		UpdatePnts ( Object* obj );

		bool	CreateTexture ( Object* ast, std::string& msg );				// Textures		
		int		UpdateTexture ();		

		bool	CreateVolume ( Object* ast, std::string& msg);					// Volumes		
		
		bool	CreateLights ( Object* ast, std::string& msg);					// Lights		
		
		bool	CreateShader ( Object* ast, std::string& msg );					// Shaders
		bool	LoadShader( std::string fname, GLuint& program );
		int		LoadInternalShader ( std::string fname );
		char*	ReadShaderSource ( std::string fname, long& fsize );
		bool	AddParam ( int shader_id, int param_id, char* name );
		bool	AddParamBlock ( int shader_id, int param_id, char* name );
		int		getParam( RShader* sh, int param_id );
		void	CheckParams();
		int		UpdateShader ();
		void	OverrideShader ( int id=-1 )	{ mSHOverride = id; }

		bool	CreateMaterial ( Object* ast, std::string& msg );				// Materials
		int		UpdateMaterial ( Object* ast );
		void	UpdateMaterials ();

		// Bindings		
		void	BindShaderGlobals ( RShader* rshade );
		void	BindPointShaderGlobals ( RShader* rshade, Object* obj );
		void	BindAttribArrays ( int vcnt, int icnt );		
		bool	BindUniformBuffers ();
		bool	UpdateTextures ();
		float	SetCameraToShader ( RShader* sh );	

		// Render		
		void	RenderPoints ( Points* pnts, bool shadow_pass, bool use_shadows);
		void	RenderVolume ( Object* vol, int dtex, int vol_tex );		

		void	TransmitShapesGL();
		void	BindShapesGL();		
		void	RenderByGroup (bool shadow=false);

		// CSM - Cascade Shadow Maps
		void	CSMUpdateSplits(frustum f[MAX_SPLITS], float nd, float fd);
		void	CSMUpdateFrustum(frustum& f, Camera3D* cam);
		float	CSMApplyCropMatrix(frustum& f);
		void	CSMViewSplits( Camera3D* cam );
		void	CSMShadowLightAsCamera(RShader* sh);
		void	CSMRenderShadowMaps();
		void	BindShaderShadowMapVars ( RShader* rshade );		// set on standard shaders during beauty pass to support shadows
		int		getCSMTex()		{ return depth_tex_ar;}

		// Sketching
		void	SketchShapes(Shapes* shapes);
		void	SketchNormals(Shapes* shapes);
		void	SketchGrid(Camera3D* cam);
		void	SketchState(int w, int h);
		
		// Asset helper functions
		int			getAssetRID ( int obj_id ) { return gAssets.getRIDs (obj_id).x; }			// .x = OpenGL RID
		std::string getAssetName( int obj_id ) { return gAssets.getObjName(obj_id); }

		inline std::string getShapesName( Shapes* rset )		{ return "NOT YET IMPLEMENTED"; }
		
		inline RShader* getShaderByID ( int shader_aid, char* name ) {
			if ( shader_aid == NULL_NDX )	{ dbgprintf("ERROR: Shapes have no shader. src: %s\n", name );return 0x0; }		// asset does not exist						
			int rid = getAssetRID ( shader_aid );
			if ( rid == NULL_NDX )			{ dbgprintf("ERROR: Shader has no render ID. src: %s\n", name ); return 0x0; }	// asset not yet loaded by renderer
			return &mShaders[ rid ]; // render shader
		}
		inline RShader* getInternalShader ( int rid ) {
			return &mShaders[ rid ];
		}
		void SetDebug ( bool v )	{ mbDebug = v;}
		
	private:
	
		bool					mbDebug;

		int						mXres, mYres;
		
		GLuint					mShapesVBO, mShapesXformVBO;
		int						mShader;							// currently active shader ID

		std::vector<RShader>	mShaders;							// shaders
		
		std::vector<RMesh>		mMeshes;							// meshes
			
		std::vector<RTexture>	mTextures;							// textures
		GLuint					mTextureBuffer;
		std::uint64_t		mTextureArray[384];
		int							mTexBufCnt;

		std::vector<RMaterial>  mMaterials;							// materials
		GLuint					mMatBuffer;		
		int						mMatCnt;

		RLight					mLights[64];						// lights (static buffer for fast opengl UNIFORM_BUF)
		GLuint					mLightBuffer;
		int						mLightCnt;	

		GLuint					mVAO;								// Vertex Array Object
		GLuint					mFBO_R, mFBO_W, mFBO_PICK;			// Framebuffers
		GLuint					mCBO, mDBO;							// Color/Depth buffers		
		int						mVolTex, mVDB;						// Volume outputs
		int						mGLTexMSAA, mGLTex, mGLPickTex;		// Render output textures
		int						mDTex, mDTPick;						// Depth textures
		int						mSHPick, mSHOverride;				// Shader ID (built-in shaders)				
		
		Shapes					mSingleInst;
		int						mShademeshID;

		std::string				mLogFile;
		FILE*					mLogFP;


		// Cascade Shadow Maps
		bool	csm_enable;
		int		mSHMeshDepth, mSHPntsDepth, mSHPnts;
		int		csm_num_splits;					// CSM splits
		int		csm_depth_size;					// CSM res
		float	csm_split_weight;				// CSM split weight
		GLuint	depth_fb;					// CSM framebuffer
		GLuint	depth_rb;					// CSM renderbuffer
		GLuint  depth_tex_ar;				// CSM depth texture
		bool	  shad_csm_pass;
		Matrix4F  shad_light_mv;			// CSM light model-view matrix
		Matrix4F  shad_light_proj;			// CSM light projection matrix
		Vec3F shad_light_pos;			// CSM light pos
		frustum	f[MAX_SPLITS];				// CSM frustum
		float	far_bound[MAX_SPLITS];		// CSM far bounds
		Matrix4F shad_cpm[MAX_SPLITS];		// CSM shadow matrix
		Matrix4F shad_vmtx[MAX_SPLITS];		// CSM shadow view matrix

		static int IPOS;		// Instance offsets (position in Shape struct)
		static int IVEL;
		static int ICLR;
		static int IDS;
		static int IPIVOT;
		static int ISCALE;
		static int IROTATE;		
		static int IMATIDS;		
		static int ITEXSUB;
		static int ITEXPIV;		

		static int geomPos;
		static int geomClr;
		static int geomNorm;
		static int geomTex;

		static int instPos;
		static int instRotate;
		static int instPivot;
		static int instScale;
		static int instClr;
		static int instIDS;		
		static int instMatIDS;		
		static int instTexSub;				
		static int instXform;
	};



#endif