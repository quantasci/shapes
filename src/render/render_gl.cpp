
//-------------------------
// Copyright 2025 (c) Quanta Sciences, Rama Hoetzlein
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

#include <GL/glew.h>
#include <GL/wglew.h>

#include "render.h"
#include "render_gl.h"
#include "mersenne.h"

#include "gxlib.h"
using namespace glib;

#include "object.h"
#include "shapes.h"
#include "image.h"
#include "mesh.h"
#include "points.h"
#include "string_helper.h"
#include "main.h"
#include "scene.h"
#include "lightset.h"
#include "material.h"
#include "volume.h"
#include "object_list.h"		// has gAssets
#include "timex.h"

// #define DEBUG_CHECKS

#ifdef DEBUG_CHECKS
	#define CHECK_GL(a,b)		checkGL(a,b)
#else
	#define CHECK_GL(a,b)		// no op
#endif

int RenderGL::geomPos =		0;			// shader slots for geometry
int RenderGL::geomClr =		1;
int RenderGL::geomNorm =	2;
int RenderGL::geomTex =		3;

// mapping shapes to shader instances

Shape IP;								// instance buffer offset (set to match Shape struct in object.h)
int RenderGL::IPOS		= (char*)&IP.pos -		 (char*) &IP;
int RenderGL::IROTATE	= (char*)&IP.rot -		 (char*)&IP;
int RenderGL::ISCALE	= (char*)&IP.scale -	 (char*)&IP;
int RenderGL::IPIVOT	= (char*)&IP.pivot -	 (char*)&IP;

int RenderGL::ICLR		= (char*)&IP.clr -		 (char*)&IP;
int RenderGL::IDS		= (char*)&IP.ids -		 (char*)&IP;
int RenderGL::IMATIDS	= (char*)&IP.matids.x2 - (char*)&IP;	// GL material IDs are x2,y2,z2,w2 of the Vec8S
int RenderGL::ITEXSUB	= (char*)&IP.texsub -	 (char*)&IP;

int RenderGL::instPos =		4;			// instances shader slots (layout in vertex programs)
int RenderGL::instRotate =	5;
int RenderGL::instScale =	6;
int RenderGL::instPivot =	7;
int RenderGL::instClr =		8;
int RenderGL::instMatIDS =	9;
int RenderGL::instTexSub =	10;
int RenderGL::instXform =	12;
//----------- NOTE: MAXIMUM 16 attribute slots (xform consumes 4), OpenGL 3.3 spec


#define PTR_OFFSET(offset) reinterpret_cast<void*>(static_cast<uintptr_t>(offset))



RenderGL::RenderGL ()
{	
	InitializeStateSort ();

	// textures	
	mTextureBuffer = NULL_NDX;	
	mTexBufCnt = 0;

	// materials
	mMatBuffer = NULL_NDX;

	// lights
	mLightBuffer = NULL_NDX;
	mLightCnt = 0;	

	mShademeshID = OBJ_NULL;
	mGLTex = -1; mGLTexMSAA = -1; mGLPickTex = -1;

	mDTex = -1; mDTPick = -1;
	mVAO = -1; mFBO_R = -1; mFBO_W = -1; mFBO_PICK = -1;
	mCBO = -1; mDBO = -1; 	
	mVolTex = -1; mVDB = -1;
	
	
	mSHPick = -1; mSHOverride = -1;

	int i;
	Shape* s = mSingleInst.Add(i);		// default shape set for drawing

	for (int g=0; g<2; g++)
		for (int y=0; y < 512; y++)
			for (int x=0; x < 512; x++)
				mState[g][x][y] = COLORA(0,0,0,0);

	// 256-color palette
	Vec4F c;	
	for (int n=0; n < 256; n++) {
		c.x = (     n & 0x07) / 7.0f;
		c.y = ((n>>3) & 0x07) / 7.0f;
		c.z = ((n>>6) & 0x03) / 3.0f;
		c.w = 1.0;
		mPalette[n] = VECCLR(c);
	}
	int j;
	CLRVAL tmp;
	Mersenne r; 
	r.seed(98);
	for (int n = 0; n < 256; n++) {
		j = r.randI( 256 );
		tmp = mPalette[n];
		mPalette[n] = mPalette[j];
		mPalette[j] = tmp;
	}

	
		
}

void RenderGL::Initialize ()
{
	// Create a VAO
	glGenVertexArrays ( 1, &mVAO );	
	glBindVertexArray ( mVAO );

	// Create framebuffers
	glGenFramebuffers(1, &mFBO_R);
	glGenFramebuffers(1 ,&mFBO_W);
	glGenFramebuffers(1, &mFBO_PICK);
	glBindFramebuffer( GL_READ_FRAMEBUFFER, mFBO_R );	
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, mFBO_W );
	glBindFramebuffer (GL_FRAMEBUFFER, mFBO_PICK );

	// Create global shape instances
	glGenBuffers(1, &mShapesVBO);					
	glGenBuffers(1, &mShapesXformVBO);

	// Load internal shaders
	mSHPick = LoadInternalShader ( "shade_pick.frag.glsl" );
	mSHPnts = LoadInternalShader ( "shade_pnts.frag.glsl" );

	// CSM - Cascade Shadow Maps
	csm_enable = true;
	csm_num_splits = 4;				// CSM splits
	//csm_depth_size = 16384;		// CSM res
	//csm_depth_size = 8192;			// CSM res
	csm_depth_size = 4096;			// CSM res
	csm_split_weight = 0.5;			// CSM split weight

	shad_csm_pass = false;

	if ( csm_enable ) {		
		mSHMeshDepth = LoadInternalShader ("shade_mesh_depth.frag.glsl");
		mSHPntsDepth = LoadInternalShader ("shade_pnts_depth.frag.glsl");
		glGenFramebuffers (1, &depth_fb);							// CSM depth framebuffer
		glBindFramebuffer (GL_FRAMEBUFFER, depth_fb);
		glDrawBuffer(GL_NONE);
		glBindFramebuffer (GL_FRAMEBUFFER, 0);
		CHECK_GL("CSM depth framebuffer", mbDebug);

		glGenTextures(1, &depth_tex_ar);							// CSM depth texture
		glBindTexture(GL_TEXTURE_2D_ARRAY, depth_tex_ar);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, csm_depth_size, csm_depth_size, csm_num_splits, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);			// see OpenGL 2.0 spec, sec 3.8, p. 188
		CHECK_GL( "CSM depth texture", mbDebug);
	}
}


void RenderGL::UpdateRes ( int w, int h, int MSAA )
{
	mXres = w; mYres = h;
	glViewport ( 0, 0, w, h );

	createTexGL (mVolTex,w,h);
	createTexGL (mGLTex,w,h);
	createTexGL (mGLPickTex,w/2,h/2);

	// SETUP READ FBO 
	glBindFramebuffer (GL_READ_FRAMEBUFFER, mFBO_R);		// apply to read FBO
	
	// Multi-sampled GL texture for read
	//  (we don't use createTexGl as we need multisample textures)	
	if (mGLTexMSAA == -1) glGenTextures(1,(GLuint*) &mGLTexMSAA);

	if (MSAA > 1) {
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mGLTexMSAA);
		//--- note: glTexParameteri will generate GL_INVALID_ENUM if used on GL_TEXTURE_2D_MULTISAMPLE 
		//glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA, GL_RGBA8, w, h, GL_TRUE);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);	
		glFramebufferTexture2D ( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mGLTexMSAA, 0);
	} else {
		glBindTexture(GL_TEXTURE_2D, mGLTexMSAA);		
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
		glBindTexture(GL_TEXTURE_2D,0);
		glFramebufferTexture2D (GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mGLTexMSAA, 0);
	}

	// Multi-sampled Depth render buffer
	if (mDBO == -1) glGenRenderbuffers(1,&mDBO);
	glBindRenderbuffer (GL_RENDERBUFFER,mDBO);
	if ( MSAA > 1 ) {
		glRenderbufferStorageMultisample (GL_RENDERBUFFER, MSAA, GL_DEPTH_COMPONENT24, w, h);
	} else {
		glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w,h);
	}
	glBindRenderbuffer (GL_RENDERBUFFER,0);
	glFramebufferRenderbuffer (GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER, mDBO);


	if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		dbgprintf ("  OpenGL. ERROR: Read Framebuffer attachment incomplete.\n");
		exit(-9);
	}

	// SETUP DRAW FBO 
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, mFBO_W);		// apply to write FBO
	
	// Standard GL texture for write	
	glFramebufferTexture  (GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, mGLTex, 0);

	// Standard Depth texture
	if (mDTex == -1) glGenTextures(1,(GLuint*) &mDTex);
	glBindTexture (GL_TEXTURE_2D,mDTex);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexImage2D  (GL_TEXTURE_2D,0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT,GL_FLOAT,0);
	glBindTexture (GL_TEXTURE_2D,0);
	glFramebufferTexture (GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT, mDTex,0);

	if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		dbgprintf ("  OpenGL. ERROR: Draw Framebuffer attachment incomplete.\n");
		exit(-9);
	}
	
	// SETUP PICKING FBO 
	glBindFramebuffer (GL_FRAMEBUFFER, mFBO_PICK );

	// Picking GL render buffer -- NO MSAA, standard depth, half width & height	
	glFramebufferTexture (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mGLPickTex, 0);
	GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, DrawBuffers);
	glBindTexture(GL_TEXTURE_2D,0);

	// Picking depth (texture)
	if ( mDTPick == -1) glGenTextures (1, (GLuint*) &mDTPick);
	glBindTexture (GL_TEXTURE_2D, mDTPick);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);		// no filtering on picking!
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexImage2D  (GL_TEXTURE_2D,0, GL_DEPTH_COMPONENT16, w/2, h/2, 0, GL_DEPTH_COMPONENT,GL_FLOAT,0);
	glFramebufferTexture (GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mDTPick, 0);

	// Picking depth (render buffer)
	/*if (mDTPick == -1) glGenRenderbuffers(1, (GLuint*) &mDTPick);
	glBindRenderbuffer (GL_RENDERBUFFER, mDTPick);
	glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w/2, h/2);	
	glBindRenderbuffer (GL_RENDERBUFFER, 0 );
	glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDTPick); */

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		dbgprintf ("  OpenGL. ERROR: Pick Framebuffer attachment incomplete.\n");
		exit(-9);
	}

	// Reattach depth and texture to FBO
	/*glFramebufferTexture ( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mGTex, 0 );
	GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, DrawBuffers);*/

}

// PrepareAssets - build OpenGL resources from the Asset list
// 1. Load texture assets to GPU, assign slot, set GLID as asset renderID
// 2. Load shader assets and compile, assign slot, set as asset renderID
// 3. Create default 'box' mesh and add to assets
// 4. Load mesh assets, assign ID, set as asset renderID
//
void RenderGL::PrepareAssets ()
{
	std::string what, msg;
	bool result;
	Object* obj;
	int ok = 0;

	dbgprintf ("  OpenGL. Preparing assets.\n");

	// Build OpenGL resources from the Asset list
	//
	for (int n=0; n < gAssets.getNumObj(); n++ ) {
		
		obj = gAssets.getObj (n);
		if (obj == 0x0) continue;

		if ( obj->getRIDs().x == NULL_NDX ) {

			result =  false;
			switch ( obj->getType() ) {
			case 'Aimg':		result = CreateTexture (obj, msg);	what = "tex";		break;
			case 'Ashd':		result = CreateShader (obj, msg);	what = "shader";	break;			
			case 'Amsh':		result = CreateMesh (obj, msg);		what = "mesh";		break;
			case 'Apnt':		result = CreateMesh (obj, msg);		what = "pnts";		break;
			case 'Avol':		result = CreateVolume (obj, msg);	what = "vol";	mVDB = n; break;			
			case 'Ashp':		result = true;						what = "shapes"; 	msg = iToStr(obj->getNumShapes()) + " shapes"; break;
			case 'Amtl':		result = CreateMaterial (obj, msg);	what = "material";	break;
			case 'lgts':		result = CreateLights (obj, msg);	what = "lgts";		break;
			case 'camr':		result = true;						what = "cam";		msg = "camera"; break;				
			default:			result = true;						what = "behv";		msg = obj->getOutputName(); break;	
			};
			if ( result ) {
				ok++;
				//dbgprintf ( "  Prepared: asset %d, %s (%s) -> %s\n", n,  obj->getName().c_str(), what.c_str(), msg.c_str() );
			} else {
				dbgprintf ( "  OpenGL. ERROR: Preparing: asset %d, %s (%s) -> %s\n", n, obj->getName().c_str(), what.c_str(), msg.c_str());
			}		
		}
	}	

	// Update uniform buffers

	UpdateTextures();

	UpdateMaterials();
	
	BindUniformBuffers ();
	
	// Save mesh shader	
	obj = gAssets.getObj ( "shade_bump" );
	if ( obj != 0x0 ) mShademeshID = obj->getID();

	dbgprintf ("  OpenGL. Prepared Assets, %d ok, Tex:%d, Lights:%d, Mesh:%d, Shader:%d \n", ok, (int) mTextures.size(), mLightCnt, (int)mMeshes.size(), (int) mShaders.size() );
}


void RenderGL::StartRender ()
{
	Scene* scn = getRenderMgr()->getScene ();
	
	CHECK_GL ( "----------------------- START FRAME", mbDebug );

	// Bind framebuffer	
	glBindFramebuffer ( GL_FRAMEBUFFER, mFBO_R );
	CHECK_GL ("Bind FBO_R", mbDebug);

	// Bind VAO once
	glBindVertexArray (mVAO);
	CHECK_GL ("Bind VAO", mbDebug);

	//glBindFramebuffer ( GL_DRAW_FRAMEBUFFER, mFBO_W );
	//glFramebufferTexture ( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mGLTex, 0 );	// attach to intermediate GL output
	glClear ( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );

	glClearColor ( 0.1, 0.1, 0.1, 1);			// clear color

	glEnable (GL_BLEND);	
	glBlendFunc (GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);	
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	CHECK_GL ("Enable blend & mask", mbDebug);

	glEnable(GL_DEPTH_TEST);

	// Bind arrays
	BindAttribArrays ( 4, 1 );

	mShader = -1;		// reset current shader
}


void RenderGL::Validate ()
{
	for (int n = 0; n < mShaders.size(); n++) {
		//printf("Shader: %p = %s\n", &mShaders[n], mShaders[n].name);
		if (mShaders[n].params.size() > 100) {
			dbgprintf("  OpenGL. Validating GL shaders. ERROR! %d: %s, paramsize=%d\n", n, mShaders[n].name, mShaders[n].params.size() );
			exit(-9);
		}
	}
}


void RenderGL::RenderVolume ( Object* vol, int depth_tex, int vol_tex )
{
	// Render GVDB volume
	Scene* scn = getRenderMgr()->getScene ();		
	LightSet* lgts = dynamic_cast<LightSet*>( scn->FindByType ( 'lgts' ) );
	if (lgts == 0x0) {
		dbgprintf ( "  OpenGL. ERROR: Unable to find lights for rendering.\n");
		exit(-1);
	}
	#ifdef USE_CUDA
		Vec3I res = scn->getRes();
 		dynamic_cast<Volume*>(vol)->Render ( scn->getCamera3D(), lgts, res.x, res.y, vol_tex, depth_tex );
	#endif
}

#include "file_png.h"

void RenderGL::TransmitShapesGL ()
{
	//dbgprintf(" Trasmitted shapes.\n");
	
	glBindBuffer ( GL_ARRAY_BUFFER, mShapesVBO );
	glBufferData ( GL_ARRAY_BUFFER, mSB.GetBufSize(BSHAPES), mSB.GetBufData(BSHAPES), GL_DYNAMIC_DRAW );

	glBindBuffer ( GL_ARRAY_BUFFER, mShapesXformVBO);
	glBufferData ( GL_ARRAY_BUFFER, mSB.GetBufSize(BXFORMS), mSB.GetBufData(BXFORMS), GL_DYNAMIC_DRAW );
}

void RenderGL::BindShapesGL ()
{
	int indx = 0;		// all instances
	glBindBuffer ( GL_ARRAY_BUFFER, mShapesVBO );
	glVertexAttribPointer ( instPos, 3, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(IPOS + indx) );
	glVertexAttribDivisor ( instPos, 1 );
	glVertexAttribPointer ( instRotate, 4, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(IROTATE + indx));
	glVertexAttribDivisor ( instRotate, 1);
	glVertexAttribPointer ( instScale, 3, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(ISCALE + indx));
	glVertexAttribDivisor ( instScale, 1);
	glVertexAttribPointer ( instPivot, 3, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(IPIVOT + indx));
	glVertexAttribDivisor ( instPivot, 1);	
	glVertexAttribPointer ( instClr, 1, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(ICLR + indx) );			// **NOTE**: GL_UNSIGNED_INT does not work
	glVertexAttribDivisor ( instClr, 1 );
	//glVertexAttribPointer ( instIDS, 4, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(IDS + indx) );
	//glVertexAttribDivisor ( instIDS, 1 );
	glVertexAttribPointer ( instMatIDS, 4, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(Shape), PTR_OFFSET(IMATIDS + indx) );
	glVertexAttribDivisor ( instMatIDS, 1 );	
	glVertexAttribPointer ( instTexSub, 4, GL_FLOAT, GL_FALSE, sizeof(Shape), PTR_OFFSET(ITEXSUB + indx) );
	glVertexAttribDivisor ( instTexSub, 1 );
	CHECK_GL ( "Bind shapes gl", mbDebug );

	glBindBuffer(GL_ARRAY_BUFFER, mShapesXformVBO);
	glVertexAttribPointer ( instXform + 0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 16, (GLvoid*) 00 );
	glVertexAttribDivisor ( instXform + 0, 1);
	glVertexAttribPointer ( instXform + 1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 16, (GLvoid*) 16 );
	glVertexAttribDivisor ( instXform + 1, 1);
	glVertexAttribPointer ( instXform + 2, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 16, (GLvoid*) 32 );
	glVertexAttribDivisor ( instXform + 2, 1);
	glVertexAttribPointer ( instXform + 3, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 16, (GLvoid*) 48 );
	glVertexAttribDivisor ( instXform + 3, 1);	

	CHECK_GL ( "Bind instances", mbDebug);
}

void RenderGL::RenderByGroup (bool shadow)
{
	// Mesh shadow shader 
	if (shadow) {
		RShader* rshade = getInternalShader ( mSHMeshDepth );
		glUseProgram ( rshade->programID );						// run depth pass
		CSMShadowLightAsCamera( rshade );			// send light matrices for model/view/proj	
		CHECK_GL("CSM::ShadowLightAsCamera", mbDebug);	
	}	

	// Render mesh shape groups
	for (int g = 0; g < mSGCnt; g++) {

		int inst_offset = mSG[g].offset;		// starting instance
		int inst_count = mSG[g].count;			// number of instances
		int mesh_id = mSG[g].meshids.x;			// mesh ID for this group		
		int faces = mSG[g].meshids.z;			// number of faces (triangles)
		int shader = mSG[g].shader;				// shader ID

		std::string name = mSG[g].name;			// debugging

		if (!shadow && mShader != shader) {			// check for shader reuse
			// Select shader
			RShader* rshade = getShaderByID ( shader, "" );	// shader_id = asset ID
			glUseProgram (rshade->programID );
			CHECK_GL("Use program", mbDebug);

			// Bind Shader globals
			BindShaderGlobals ( rshade );
			BindShaderShadowMapVars ( rshade );
			mShader = shader;
			CHECK_GL("Bind global", mbDebug);
		}
	
		// Bind Mesh 
		Mesh* mesh = (Mesh*) gAssets.getObj(mesh_id);						// mesh asset		
		if (mesh->isDirty()) {
			UpdateMesh( mesh );												// update geometry if dirty
			mesh->MarkClean();
			CHECK_GL("UpdateMesh", mbDebug);
		}
		int mid = mesh->getRIDs().x;										// Mesh ID
		if ( mid==-1 ) {
			dbgprintf ( "  OpenGL. ERROR: Mesh not on GPU. Need to mark mesh dirty.\n" );
			assert(0);
		}
		RMesh* m = &mMeshes[mid];											// render mesh (VBOs)

		glBindBuffer(GL_ARRAY_BUFFER, m->meshVBO[0]);
		glVertexAttribPointer(geomPos, 3, GL_FLOAT, GL_FALSE, 0x0, 0);		// Bind vertices				
		CHECK_GL("glBindBuffer (vert)", mbDebug);

		if (m->meshVBO[1] != OBJ_NULL) {
			glBindBuffer(GL_ARRAY_BUFFER, m->meshVBO[1]);
			glEnableVertexAttribArray ( geomClr );
			glVertexAttribIPointer(geomClr, 1, GL_UNSIGNED_INT, 0x0, 0);		// Bind color (uint=RGBA)				
			CHECK_GL("glBindBuffer (clr)", mbDebug);
		} else {
			glDisableVertexAttribArray ( geomClr );
			glVertexAttribI1ui( geomClr, (unsigned int) COLORA(1,1,1,1) );	// value when color not bound
		}

		glBindBuffer(GL_ARRAY_BUFFER, m->meshVBO[2]);
		glVertexAttribPointer(geomNorm, 3, GL_FLOAT, GL_FALSE, 0x0, 0);		// Bind normals
		CHECK_GL("glBindBuffer (norm)", mbDebug);

		if (m->meshVBO[3] != OBJ_NULL) {			
			glBindBuffer(GL_ARRAY_BUFFER, m->meshVBO[3]);
			glEnableVertexAttribArray ( geomTex );
			glVertexAttribPointer(geomTex, 2, GL_FLOAT, GL_FALSE, 0x0, 0);	// Bind texture coordinates					
			CHECK_GL("glBindBuffer (uv)", mbDebug);
		} else {
			glDisableVertexAttribArray ( geomTex );
			glVertexAttrib2f( geomTex, 0.0, 0.0 );	
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->meshVBO[4]);				// Bind face indices		
		CHECK_GL("glBindBuffer (elem)", mbDebug);
	
		// Draw instance		
		if ( faces == 0 ) {				
			// draw entire mesh. get face count from mesh itself. 						
			
			glDrawElementsInstancedBaseInstance (m->primtype, m->primcnt * 3, GL_UNSIGNED_INT, 0, inst_count, inst_offset );		// render multiple instances using a sub-range given by the instance group
			CHECK_GL("glDrawElementsInstBaseInst", mbDebug);
			
			//dbgprintf("Render: grp: %d, shader: %s, mesh: %s, count: %d, offset: %d\n", g, getShaderByID(mShader, "")->name, mesh->getName().c_str(), inst_count, inst_offset);
		} else {
			// draw portion of mesh VBO. get faces from instace data
			int elem_offset = mSG[g].meshids.w * 3 * sizeof(unsigned int);		// byte offset into GL_ELEMENT_ARRAY_BUFFER (see GL specs for 'indices')			
			
			glDrawElementsInstancedBaseInstance (m->primtype, faces * 3, GL_UNSIGNED_INT, PTR_OFFSET(elem_offset), inst_count, inst_offset);	// render multiple instances using a sub-range given by the instance group
			CHECK_GL("glDrawElementsInstBaseInst (range)", mbDebug);
			//dbgprintf("Render: grp: %d, shader: %s, mesh: %s (%d,%d), count: %d, offset: %d\n", g, getShaderByID(mShader, "")->name, mesh->getName().c_str(), faces, elem_offset, inst_count, inst_offset);
		}
	}

	// Render special objects - particles, volumes, etc.	
	Object *obj, *output;	
	for (int n = 0; n < gScene->getNumScene(); n++) {
		obj = gScene->getSceneObj(n);		
		if (obj->isVisible()) {
			if (obj->getType()=='Apnt' ) 
				RenderPoints ( (Points*) obj, shad_csm_pass, obj->hasShadows() );		// assets in scene list, render directly
			output = obj->getOutput();
			if (output != 0x0 && output->getType()=='Apnt' ) 
				RenderPoints ( (Points*) output, shad_csm_pass, obj->hasShadows() );	// behavior in scene list, render outputs
		}
	}
}

bool RenderGL::Render ()
{	
	Scene* scn = getRenderMgr()->getScene ();	

	// Clear the debug state view
	memset ( mState, 0, 512*512*2*sizeof(CLRVAL) );

	// Step 0. Update materials
	UpdateMaterials ();

	// Steps 1-3. State sorting
	InsertAndSortShapes ();

	// Step 4. Bind and transmit *all* instances (shapes)
	//dbgprintf( "transmit: %d shapes\n", mShapeCnt );
	PERF_PUSH("  Transmit");	
	if ( mChkSum != mChkSumL ) TransmitShapesGL ();	
	BindShapesGL ();
	PERF_POP(); 

	// Step 5. CSM - Render Shadow Maps
	if (csm_enable) {
		PERF_PUSH("  Shadows");
		CSMRenderShadowMaps ();
		PERF_POP();
	}

	// Step 6. Depth Pre-pass
	/*PERF_PUSH("  Depthpass");
	RShader* rshade = getInternalShader ( mSHDepth );
	glUseProgram(rshade->programID);							// run depth pass
	BindShaderGlobals( rshade );
	shad_csm_pass = true;										// inform render we are doing depth pass (no shader changes)	
		glDepthMask(GL_TRUE);	
		glDisable(GL_BLEND);
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_FRONT);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		RenderByGroup();
	shad_csm_pass = false;
	PERF_POP();
	glDepthMask(GL_FALSE);  */
	
	// Step 7. Final render
	glEnable(GL_BLEND);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// Wireframe option
	if (isEnabled(OPT_SKETCH_WIRE)) {
		// Enable wireframe
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_BACK);
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(0, 2);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth ( 2 );
		// Temporarily override light colors (for lines)
		LightSet* lset = dynamic_cast<LightSet*>(gScene->FindByType('lgts'));
		for (int n=0; n < lset->getNumLights(); n++) {
			RLight* lgt = lset->getLight(n);
			lgt->inclr = lgt->diffclr;
			lgt->ambclr = Vec3F(.4, .4, .4);			// <-- line color
			lgt->diffclr = Vec3F(1,1,1);
		}
		UpdateLights();		
		// Draw scene as lines
		RenderByGroup ();
		// Restore lights
		for (int n=0; n < lset->getNumLights(); n++) {
			RLight* lgt = lset->getLight(n);
			lgt->ambclr = Vec3F(0,0,0);
			lgt->diffclr = lgt->inclr;
		}		
		UpdateLights();
		// Disable wireframe		
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_POLYGON_OFFSET_LINE);

	} else {

		// Render scene (by group)
		PERF_PUSH("  Beauty");				
			RenderByGroup ();
		PERF_POP();

	}
	
	return true;
}


void RenderGL::EndRender ()
{	
	CHECK_GL("EndRender start", mbDebug);

	Scene* scn = getRenderMgr()->getScene ();
	Vec3I res = scn->getRes();
	Object* obj;
	
	// Composite msaa into standard (this "resolves" the MSAA FBO)
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO_R);										// Read from MSAA buffer		
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO_W);										// Write to standard texture
	glFramebufferTexture (GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mGLTex,0);
	glBlitFramebuffer(0 ,0, res.x,res.y, 0, 0,res.x,res.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);	// Blit will "resolve" the MSAA
	CHECK_GL("Blit", mbDebug);
	
	//SaveFrame ( "test.png" );	

	// Volumes must be rendered now (to get the resolved opengl depth buffer)
	if (mVDB >= 0) {
		obj = gAssets.getObj(mVDB);
		RenderVolume (obj, mDTex, mVolTex);		// Volume texture output
	}

	// Composite volumes over opengl
	int out_tex = getOutputTex();		
	glBindFramebuffer ( GL_FRAMEBUFFER, mFBO_W );
	glFramebufferTexture ( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_tex, 0 );			// Write to final output
	
	compositeTexGL ( 1.0, res.x, res.y, mGLTex, mVolTex, 1, 0 );						// Composite resolved GL buffer with volume (alpha blend)

	CHECK_GL("composite", mbDebug);
	//compositeTexGL ( 1.0, res.x, res.y, mDTex, mVolTex, 1, 0 );						

	// Unbind the framebuffer
	glBindFramebuffer ( GL_FRAMEBUFFER, 0 );
	
	if (csm_enable) {
		// CSM - Disable texture unit #1
		glActiveTexture(GL_TEXTURE1);
		glBindTexture ( GL_TEXTURE_2D, 0 );
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0 );
	}
}

// CSM - UpdateSplits - computes the near and far distances for every frustum slice
// in camera eye space - that is, at what distance does a slice start and end
void RenderGL::CSMUpdateSplits (frustum f[MAX_SPLITS], float nd, float fd)
{
	float lambda = csm_split_weight;
	float ratio = fd / nd;
	f[0].neard = nd;

	for (int i = 1; i < csm_num_splits; i++)
	{
		float si = i / (float)csm_num_splits;
		f[i].neard = lambda * (nd * powf(ratio, si)) + (1 - lambda) * (nd + (fd - nd) * si);
		f[i - 1].fard = f[i].neard * 1.005f;
	}
	f[csm_num_splits - 1].fard = fd;
}

// CSM - UpdateFrustum - compute the 8 corner points of the current view frustum
void RenderGL::CSMUpdateFrustum (frustum& f, Camera3D* cam)
{
	// camera parameters
	Vec3F up(0.0f, 1.0f, 0.0f);
	Vec3F viewdir = cam->getToPos() - cam->getPos();	viewdir.Normalize();		
	Vec3F right = viewdir.Cross ( up);					right.Normalize();
	up = right.Cross(viewdir); up.Normalize();

	f.fov = cam->getFov() / 57.2957795 + 0.1f;			// note: 57.2957795 = DEGtoRAD 
	f.ratio = float(mXres) / float(mYres);
	
	Vec3F fc = cam->getPos() + viewdir * f.fard;
	Vec3F nc = cam->getPos() + viewdir * f.neard;

	// these heights and widths are half the heights and widths of the near and far plane rectangles
	float near_height = tan(f.fov / 2.0f) * f.neard;
	float near_width = near_height * f.ratio;
	float far_height = tan(f.fov / 2.0f) * f.fard;
	float far_width = far_height * f.ratio;

	f.point[0] = nc - up * near_height - right * near_width;
	f.point[1] = nc + up * near_height - right * near_width;
	f.point[2] = nc + up * near_height + right * near_width;
	f.point[3] = nc - up * near_height + right * near_width;

	f.point[4] = fc - up * far_height - right * far_width;
	f.point[5] = fc + up * far_height - right * far_width;
	f.point[6] = fc + up * far_height + right * far_width;
	f.point[7] = fc - up * far_height + right * far_width;
}

// CSM - ApplyCropMatrix
// This function builds a projection matrix for rendering from the shadow's POV. First, it computes the appropriate z-range and sets an 
// orthogonal projection. Then, it translates and scales it, so that it exactly captures the bounding box of the current frustum slice
float RenderGL::CSMApplyCropMatrix(frustum& f)
{
	//float shad_modelview[16];
	Matrix4F split_proj;
	Matrix4F split_crop;
	Matrix4F split_mvp;

	float maxX = -100000.0f;
	float maxY = -100000.0f;
	float maxZ;
	float minX = 100000.0f;
	float minY = 100000.0f;
	float minZ;

	Vec4F	transf;

	// find the z-range of the current frustum as seen from the light
	// in order to increase precision
	transf = Vec4F(f.point[0]) * shad_light_mv;		// note that only the z-component is need and thus the multiplication is simplified (single point transform)
	minZ = transf.z; maxZ = transf.z;
	for (int i = 1; i < 8; i++) {				
		transf = Vec4F(f.point[i]) * shad_light_mv;
		if (transf.z > maxZ) maxZ = transf.z;
		if (transf.z < minZ) minZ = transf.z;
	}
	//minZ = 1.0;
	//maxZ = 1000.0;
	// here, minZ/maxZ will be the nearest/farthest camera frustum point as viewed on the lights z-axis

	// make sure all relevant shadow casters are included
	// note that these here are dummy objects at the edges of our scene
	/*	for (int i = 0; i < NUM_OBJECTS; i++)
	{
		transf = obj_BSphere[i].center * shadowMtx;
		if (transf.z + obj_BSphere[i].radius > maxZ) maxZ = transf.z + obj_BSphere[i].radius;
		//	if(transf.z - obj_BSphere[i].radius < minZ) minZ = transf.z - obj_BSphere[i].radius;
	}*/

	// set the projection matrix with the new z-bounds note the inversion because the light looks at the neg z axis.
	// the minZ/maxZ is specific to the split, so then is shad_proj and shad_mvp
	split_proj.makeOrtho (-1.0, 1.0, -1.0, 1.0, -maxZ, -minZ);			// gluPerspective(LIGHT_FOV, 1.0, maxZ, minZ); // for point lights
	
	split_mvp = split_proj;
	split_mvp *= shad_light_mv;						// p' = proj * model * view * p     world -> light
	

	// get the crop matrix:
	// find the extents of the frustum slice as projected in light's homogeneous coordinates
	// this is what the split frustum looks like in the light's 2D view plane (region of interest)
	for (int i = 0; i < 8; i++) {
		transf = Vec4F(f.point[i]) * split_mvp;
		transf.x /= transf.w; transf.y /= transf.w;
		if (transf.x > maxX) maxX = transf.x;
		if (transf.x < minX) minX = transf.x;
		if (transf.y > maxY) maxY = transf.y;
		if (transf.y < minY) minY = transf.y;
	}
	float scaleX = 2.0f / (maxX - minX);
	float scaleY = 2.0f / (maxY - minY);
	float offsetX = -0.5f * (maxX + minX) * scaleX;
	float offsetY = -0.5f * (maxY + minY) * scaleY;

	// apply a crop matrix to modify the projection matrix we got from glOrtho.
	split_crop.Identity();
	split_crop(0, 0) = scaleX;	split_crop(1, 1) = scaleY;
	split_crop(3, 0) = offsetX;	split_crop(3, 1) = offsetY;
	split_crop.Transpose ();
	
	// current light projection matrix (will be sent to shader)
	shad_light_proj = split_crop;
	shad_light_proj *= split_proj;


	return minZ;
}


void RenderGL::CSMViewSplits (Camera3D* cam )
{	
	// get camera matrices	
	const float Tbias[16] = { 0.5f, 0.0f, 0.0f, 0.0f,
								0.0f, 0.5f, 0.0f, 0.0f,
								0.0f, 0.0f, 0.5f, 0.0f,
								0.5f, 0.5f, 0.5f, 1.0f };
	float* cam_proj = cam->getProjMatrix().GetDataF();
	
	Matrix4F inv;
	inv = cam->getViewInv();

	// for every active split
	for (int i = 0; i < csm_num_splits; i++) {
	
		// Compute: The far distance of each split as a depth value.
		//  f[i].fard is originally in eye space - tell's us how far we can see.
		//  Here we compute it in camera homogeneous coordinates (ie. depth value). Basically, we calculate cam_proj * (0, 0, f[i].fard, 1)^t and then normalize to [0; 1]
		far_bound[i] = 0.5f * (-f[i].fard * *(cam_proj+10) + *(cam_proj+14) ) / f[i].fard + 0.5f;

		// Compute a matrix that transforms from camera eye space to light clip space 
		// to be passed to the view shader later
		shad_vmtx[i] = Tbias;			// texture lookup. projected (-1,1) --> texture (0,1)     t = p*0.5 + 0.5
		shad_vmtx[i] *= shad_cpm[i];		
		shad_vmtx[i] *= inv;			// we must transform from the camera view because we need the camera z-depth value 
										// of the test points, otherwise we could have used their world pos

	}
}


void RenderGL::CSMShadowLightAsCamera (RShader* sh)
{
	int progid = sh->programID;
	int id = getParam(sh, P_VIEWMTX);	if (id != -1) glProgramUniformMatrix4fv(progid, id, 1, GL_FALSE, shad_light_mv.GetDataF());	
	id = getParam(sh, P_PROJMTX);		if (id != -1) glProgramUniformMatrix4fv(progid, id, 1, GL_FALSE, shad_light_proj.GetDataF());
	id = getParam(sh, P_CAMPOS);		if (id != -1) glProgramUniform3fv(progid, id, 1, &shad_light_pos.x);
}

// CSM - Cascade Shadow Maps
//
void RenderGL::CSMRenderShadowMaps()
{	
	CHECK_GL("CSM::Start", mbDebug);

	// Generate lights model-view matrix
	LightSet* lgts = dynamic_cast<LightSet*>(gScene->FindByType('lgts'));
	if ( lgts == 0x0 ) return;
	if ( lgts->getNumLights()==0 ) return;
	RLight* lgt = lgts->getLight(0);
	Vec3F	ldir = lgt->pos - lgt->target;	ldir.Normalize();			// directional light
	shad_light_mv.makeLookAt (Vec3F(0, 0, 0), Vec3F(-ldir.x, -ldir.y, -ldir.z), Vec3F(0.0f, 1.0f, 0.0f) );

	// Redirect rendering to the depth texture	
	glBindFramebuffer(GL_FRAMEBUFFER, depth_fb);
	glViewport(0, 0, csm_depth_size, csm_depth_size);				// shadow map size
	
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 4096.0f);								// offset the geometry slightly to prevent z-fighting note that this introduces some light-leakage artifacts
	glDisable(GL_CULL_FACE);									// draw all faces since our terrain is not closed.
	glDisable(GL_BLEND);
	glDepthMask (GL_TRUE);
	glColorMask ( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	
	// Compute the z-distances for each split as seen in camera space
	Camera3D* cam = getRenderMgr()->getScene()->getCamera3D();
	CSMUpdateSplits(f, cam->getNear(), cam->getFar());	

	// Prepare the depth shader	
	shad_csm_pass = true;										// inform render we are doing CSM pass (no shader changes)
	CHECK_GL("CSM::Prepare", mbDebug);

	// Render the shadow maps
	for (int i = 0; i < csm_num_splits; i++)
	{
		CSMUpdateFrustum (f[i], cam);							// compute the camera frustum slice boundary points in world space
		
		float minZ = CSMApplyCropMatrix(f[i]);		// adjust the view frustum of the light, so that it encloses the camera frustum slice fully.		
		
		glFramebufferTextureLayer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex_ar, 0, i);		// make the current depth map a rendering target
		CHECK_GL("CSM::glFramebufferTextureLayer", mbDebug);

		glClear(GL_DEPTH_BUFFER_BIT);							// clear the depth texture from last time
		CHECK_GL("CSM::glClear(DEPTH)", mbDebug);
	
		RenderByGroup (true);									// draw scene. true = shadow
		CHECK_GL("CSM::RenderByGroup", mbDebug);
		
		shad_cpm[i] = shad_light_proj;							// store the product of all shadow matries for later. cpm = crop-proj-mv
		shad_cpm[i] *= shad_light_mv;							// Plight = crop * proj * view * model * Pworld		(world -> light)
	}
	CHECK_GL("CSM::ShadowMap", mbDebug);
	shad_csm_pass = false;

	// Restore primary render
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO_R);
	glViewport(0, 0, mXres, mYres);									// restore render viewport
	glDisable(GL_POLYGON_OFFSET_FILL);
	
	// Prepare for camera view for shadows
	glActiveTexture (GL_TEXTURE1);									// bind to texture unit #1  (this is the value, 1, sent to shader uniform)
	glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, depth_tex_ar);			// bind shadow map textures (array of them in one GLID)
	glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);			// see OpenGL 2.0 spec, sec 3.8, p. 188

	CSMViewSplits (cam);								// compute the far_bound and shad_vmtx from the camera 

	CHECK_GL("CSM::Complete", mbDebug);
}

void RenderGL::BindAttribArrays ( int vcnt, int icnt )
{
	if ( vcnt >= 1 ) glEnableVertexAttribArray ( geomPos );	 else glDisableVertexAttribArray ( geomPos );
	if ( vcnt >= 2 ) glEnableVertexAttribArray ( geomClr );  else glDisableVertexAttribArray ( geomClr ); 
	if ( vcnt >= 3 ) glEnableVertexAttribArray ( geomNorm ); else glDisableVertexAttribArray ( geomNorm ); 
	if ( vcnt >= 4 ) glEnableVertexAttribArray ( geomTex );  else glDisableVertexAttribArray ( geomTex );
	CHECK_GL ("Enable vertex arrays", mbDebug);

	if ( icnt > 0 ) {		// enable instances
		glEnableVertexAttribArray (instPos);		glEnableVertexAttribArray(instScale);
		glEnableVertexAttribArray (instRotate);		glEnableVertexAttribArray(instPivot);
		glEnableVertexAttribArray (instClr);		glEnableVertexAttribArray (instMatIDS);		
		glEnableVertexAttribArray (instTexSub);		
		glEnableVertexAttribArray (instXform); glEnableVertexAttribArray(instXform + 1); glEnableVertexAttribArray(instXform + 2); glEnableVertexAttribArray(instXform + 3);
		//glEnableVertexAttribArray (instIDS);	
		CHECK_GL ("Enable inst arrays", mbDebug);
	} else {				// disable instances
		glDisableVertexAttribArray (instPos);		glDisableVertexAttribArray(instScale);
		glDisableVertexAttribArray (instRotate);	glDisableVertexAttribArray(instPivot);
		glDisableVertexAttribArray (instClr);		glDisableVertexAttribArray (instMatIDS);		
		glDisableVertexAttribArray (instTexSub);	
		glDisableVertexAttribArray (instXform);	glDisableVertexAttribArray(instXform + 1); glDisableVertexAttribArray(instXform + 2); glDisableVertexAttribArray(instXform + 3);
		//glDisableVertexAttribArray (instIDS);	
		CHECK_GL ("Disable inst arrays", mbDebug);
	}
}

// Picking
// - minimal rendering pipeline (half res, no msaa) to generate picking buffer
//
void RenderGL::RenderPicking ( int w, int h, int tex )
{
	Scene* scn = getRenderMgr()->getScene ();	

	glBindFramebuffer ( GL_FRAMEBUFFER, 0 );
	glViewport ( 0, 0, w/2, h/2 );											// half res

	// Bind framebuffer		
	glBindFramebuffer ( GL_FRAMEBUFFER, mFBO_PICK );						// bind to picking buffer (FBO)
	glFramebufferTexture (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);		
	glEnable ( GL_DEPTH_TEST );
	glDepthFunc ( GL_LEQUAL );
	glDepthMask ( GL_TRUE );
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	glClear ( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
	glDisable (GL_BLEND);													// disable blend for picking	
	
	//BindAttribArrays ( 4, 1 );

	/*OverrideShader ( mSHPick );												// enable the picking shader

	RenderByGroup ();				// *** ASSUMES ALREADY STATE-SORTED DURING PRIMARY RENDER *
	
	OverrideShader (); */

	// Unbind the framebuffer
	glBindFramebuffer ( GL_FRAMEBUFFER, 0 ); 
	glDepthFunc ( GL_LEQUAL );

	glViewport ( 0, 0, w, h );												// return to full res
}

Vec4F RenderGL::Pick (int x, int y, int w, int h)
{
	Vec4F vec;
	glBindFramebuffer ( GL_FRAMEBUFFER, mFBO_PICK );		
	glClampColor (GL_CLAMP_READ_COLOR, GL_FALSE);
	glReadPixels ( x/2, (h-y)/2, 1, 1, GL_RGBA, GL_FLOAT, &vec.x );
	//dbgprintf ( "Pick: %d,%d -> %d,%d,%d,%d\n", x, y, int(vec.x), int(vec.y), int(vec.z), int(vec.w) );
	return vec;
}


void RenderGL::Sketch3D (int w, int h, Camera3D* cam )
{
	Object* obj;
	Vec4F clr;
	Scene* scn = getRenderMgr()->getScene();

	bool bObj = isEnabled(OPT_SKETCH_OBJ);
	bool bShapes = isEnabled(OPT_SKETCH_SHAPE);
	bool bNorms = isEnabled(OPT_SKETCH_NORM);

	start3D(cam);			// start sketching

	// Sketch mesh normals
	if ( bNorms) {
		for (int n=0; n < scn->getNumScene(); n++) {
			obj = scn->getSceneObj(n);		
			if ( obj->isVisible() && obj->getType()=='Ashp' ) 
				SketchNormals ( (Shapes*) obj );
		}
	}

	// Sketch grid
	//if ( bObj ) SketchGrid (cam);

	// Sketch objects
	if ( bObj ) {
		for (int n = 0; n < scn->getNumScene(); n++) {
			obj = scn->getSceneObj(n);
			if ( obj->isVisible() ) obj->Sketch(w, h, cam);		// object self-sketch
		}
	}

	// Sketch render shapes
	if ( bShapes ) {
		for (int n = 0; n < scn->getNumScene(); n++) {
			obj = scn->getSceneObj(n);
			if ( obj->isVisible() && obj->getType() == 'Ashp' ) 
				SketchShapes((Shapes*)obj);						// shapes sketch
		}
	}
	
	end3D();			// complete 3D sketching
}

void RenderGL::SketchShapes(Shapes* shapes)
{
	Shape* s;
	Vec3F p1, p2;
	Matrix4F xf;

	int clrid = shapes->getID() % 256;
	Vec4F clr = CLRVEC( mPalette[clrid] );

	for (int sid = 0; sid < shapes->getNumShapes(); sid++) {
		s = shapes->getShape(sid);
		if ( s->type == S_SHAPEGRP ) 
			SketchShapes ( (Shapes*) gAssets.getObj ( s->meshids.x ) );

		p1 = Vec3F(-1,-1,-1);			// see cube.obj
		p2 = Vec3F(1, 1, 1);
		xf = shapes->getXform();			// global transform
		xf *= s->getXform();				// shape transform (includes pos, rotate, scale)

		drawBox3D (p1, p2, clr, xf);
	}
}

void RenderGL::SketchNormals (Shapes* shapes)
{
	float ln = 0.1f;

	Vec3F d(0, ln*0.1, 0);
	Vec3F fwd0(ln, 0, 0), up0(0, ln, 0), side0(0, 0, ln);
	Vec3F fwd, up, side, orig;

	Shape* s;
	Vec3F p1, p2;
	Matrix4F xf;	

	for (int sid = 0; sid < shapes->getNumShapes(); sid++) {
		s = shapes->getShape(sid);
		
		xf.Multiply ( shapes->getXform(), s->getXform() );				

		// local axes
		orig =	(Vec3F(0,0,0) * xf);		// origin of shape
		fwd =	(fwd0 * xf);	fwd -= orig;	fwd.Normalize();
		up =	(up0 * xf);		up -= orig;		up.Normalize();	
		side =	(side0 * xf);	side -= orig;	side.Normalize();
		drawLine3D (d + orig, d + orig+(fwd*ln), Vec4F(1, 0, 0, 1));
		drawLine3D (d + orig, d + orig+(up*ln), Vec4F(0, 1, 0, 1));
		drawLine3D (d + orig, d + orig+(side*ln), Vec4F(0, 0, 1, 1));
	
		// mesh normals
		/*mesh_id = s->meshids.x;
		if (mesh_id < MESH_MARK) {
			mesh = (MeshX*)gAssets.getObj(mesh_id);					// mesh asset
			if ( mesh != 0x0 ) mesh->DrawNormals( ln, xf );	
		}*/
	}
}

void RenderGL::Sketch2D (int w, int h)
{
	bool bState = isEnabled(OPT_SKETCH_STATE);

	start2D( w, h );

	// Sketch state
	if (bState) {	
		SketchState(w, h);	
	}

	end2D();
}

void RenderGL::SketchState(int w, int h)
{
	#ifdef DEBUG_STATE

		Vec4F c;
		float cell = 4;
	
		int sz = 800;
		int cnt = sz / cell;
		int o;
	
		for (int g=0; g < 2; g++ ) {
			o = 20 + g*(sz+10);
			for (int y = 0; y < cnt; y++) {
				for (int x=0; x < cnt; x++ ) {			
					if ( mState[g][x][y] > 0 ) {
						c = CLRVEC(mState[g][x][y]);
						drawFill( Vec2F(o+x*cell, 20+y*cell), Vec2F(o+(x+1)*cell, 20+(y+1)*cell), c );
						drawRect( Vec2F(o+x*cell, 20+y*cell), Vec2F(o+(x+1)*cell, 20+(y + 1)*cell), Vec4F(.5,.5,.5, 0.2) );
					}
				}
			}
			drawRect( Vec2F(o, 20), Vec2F(o + sz, 20 + sz), Vec4F(.5, .5, .5, 1) );
		}
	
	#endif
}

void RenderGL::SketchGrid (Camera3D* cam)
{
	
	int sz = 100;
	float o = 0.02f;
	float c = 0.4f;

	for (int i = -sz; i <= sz; i += 10) {
		drawLine3D( Vec3F(float(i), o, -sz), Vec3F(float(i), o, sz), Vec4F(c, c, c, 1.f) );
		drawLine3D( Vec3F(-sz, o, float(i)), Vec3F(sz, o, float(i)), Vec4F(c, c, c, 1.f) );
	}
	Vec3F p(-5, 0, 0);
	drawLine3D(p, p + Vec3F(1, 0, 0), Vec4F(1, 0, 0, 1));
	drawLine3D(p, p + Vec3F(0, 1, 0), Vec4F(0, 1, 0, 1));
	drawLine3D(p, p + Vec3F(0, 0, 1), Vec4F(0, 0, 1, 1));

	// Ruler
	/* Vec3F e(0.002, 0.002, 0.002);
	float hgt = 1.6;		// meters	
	setText(0.2, 1);
	//drawLine3D ( 1,0,0, 1,hgt,0, 1,1,0, 1);
	// drawText3D ( cam, 1,hgt,0, "1.6m", 1,1,0,1);
	//drawLine3D ( m_M1, m_M2, Vec4F(0,1,0,1) );
	drawBox3D(m_M1 - e, m_M1 + e, 1, 0, 0, 1);
	drawBox3D(m_M2 - e, m_M2 + e, 1, 0, 0, 1); */
}


bool RenderGL::SaveFrame ( char* filename )
{
    // Read back pixels
	Scene* scn = getRenderMgr()->getScene ();
	Vec3I res = scn->getRes();
    unsigned char* pixbuf = (unsigned char*) malloc( res.x * res.y * 3);

	//glBindFramebuffer ( GL_READ_FRAMEBUFFER, mFBO_W ); 
    //glReadPixels (0, 0, res.x, res.y, GL_RGB, GL_UNSIGNED_BYTE, pixbuf);

	int gltex = getOutputTex();		
	glGetTextureImage ( gltex, 0, GL_RGB, GL_UNSIGNED_BYTE, res.x*res.y*3, (void*) pixbuf );		// output is GL_RGB

    // Flip Y
    int pitch = res.x * 3;
    unsigned char* buf = (unsigned char*) malloc(pitch);
    for (int y = 0; y < res.y / 2; y++) {
        memcpy(buf, pixbuf + (y * pitch), pitch);
        memcpy(pixbuf + (y * pitch), pixbuf + ((res.y - y - 1) * pitch), pitch);
        memcpy(pixbuf + ((res.y - y - 1) * pitch), buf, pitch);
    }
    
	Image img;
    img.Create ( res.x, res.y, ImageOp::RGB8 );
    img.TransferData ( (char*) pixbuf );

	bool ret = img.Save ( filename );		

  free(pixbuf);
  free(buf);	

	glBindFramebuffer ( GL_FRAMEBUFFER, 0 ); 

	return ret;
}


float RenderGL::SetCameraToShader ( RShader* sh )
{
	Camera3D* cam = getRenderMgr()->getScene()->getCamera3D();

	if ( cam==0x0 ) {
		dbgprintf ( "  OpenGL. ERROR: No camera assigned to renderer.\n");
		return 0.0;
	}
	int progid = sh->programID;

	int id = getParam(sh, P_VIEWMTX);	if ( id != -1) glProgramUniformMatrix4fv( progid, id, 1, GL_FALSE, cam->getViewMatrix().GetDataF() );	
	id = getParam(sh, P_PROJMTX);		if ( id != -1) glProgramUniformMatrix4fv( progid, id, 1, GL_FALSE, cam->getProjMatrix().GetDataF() );
	id = getParam(sh, P_CAMPOS);		if ( id != -1) glProgramUniform3fv( progid, id, 1, &cam->getPos().x );
	id = getParam(sh, P_CAMNF);		
	Vec3F nf = cam->getNearFar();
	if ( id != -1) glProgramUniform3fv( progid, id, 1, &nf.x );
	
	id = getParam(sh, P_CAMUVW);		if ( id != -1) glProgramUniformMatrix4fv( progid, id, 1, GL_FALSE, cam->getUVWMatrix().GetDataF());
	
	return cam->getOrbitDist();
}


// Get a render parameter on a specific shader
int RenderGL::getParam( RShader* sh, int param_id )
{
	//return sh->params[param_id];
	return (param_id < sh->params.size()) ? sh->params[param_id] : -1;
}

void RenderGL::BindPointShaderGlobals ( RShader* rshade, Object* obj )
{
	glUseProgram ( rshade->programID );
	CHECK_GL ( "Use program", mbDebug);

	// Set camera uniforms (to shader)
	SetCameraToShader ( rshade );
	CHECK_GL ("Set camera uniforms to shader", mbDebug);

	// Bind global transform matrix (to shader)
	int id = getParam(rshade, P_OBJMTX);
	float* objmtx = obj->getLocalXform()->getXform().GetDataF();
	glProgramUniformMatrix4fv( rshade->programID, id, 1, GL_FALSE, objmtx );
}

void RenderGL::RenderPoints ( Points* pnts, bool shadow_pass, bool use_shadow )
{
	if ( pnts->getNumPoints()==0 ) return;

	// Check Shader					
	RShader* rshade = getInternalShader ( (shadow_pass ? mSHPntsDepth : mSHPnts) );
	if (rshade == 0x0) { dbgprintf ("ERROR: Points have no shader.\n"); return; }
	
	// Bind Shader		
	BindPointShaderGlobals ( rshade, pnts );		// bind shader
	if (shadow_pass) {
		if (!use_shadow) return;
		CSMShadowLightAsCamera( rshade );			// send light matrices instead for model/view/proj	
	} else {		
		BindShaderShadowMapVars ( rshade );			// send shader variables to beauty pass
	}

	// Get point VBOs
	int num_pnts = pnts->getNumPoints();
	int pnt_vbo = pnts->getVBO(0);		// see func: Points::ReallocateParticles 
	int clr_vbo = pnts->getVBO(1);		

	BindAttribArrays ( 2, 0 );		// bind arrays for points

		glBindBuffer ( GL_ARRAY_BUFFER, pnt_vbo );		
		glVertexAttribPointer ( geomPos, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3F), 0 );		// pos

		glBindBuffer ( GL_ARRAY_BUFFER, clr_vbo );		
		glVertexAttribIPointer ( geomClr, 1, GL_UNSIGNED_INT, sizeof(uint), 0 );		// color 
		
		// Render
		//std::string mname = gAssets.getObj( m->assetID )->getName();
		//dbgprintf ("Rendering %s, #inst:%d %s, meshID:%d (%d prims) shadeID:%d (prog %d) \n", shapes->getName().c_str(), shapes->getNum(), mname.c_str(), m->assetID,  m->primcnt, rshade->assetID, rshade->programID );
	
		/*Camera3D* cam = getRenderMgr()->getScene()->getCamera3D();
		float cz = cam->getOrbitDist();
		glPointSize ( 40.0 / cz ); */

		glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);			// enable gl_PointSize in vertex shader

		glDrawArrays ( GL_POINTS, 0, num_pnts );

	BindAttribArrays ( 4, 1 );		// return bind to normal (mesh instances)
}


void RenderGL::BindShaderGlobals ( RShader* rshade )
{
	int id;

	// Set camera uniforms (to shader)
	SetCameraToShader (rshade);
	CHECK_GL("Set camera uniforms to shader", mbDebug);

	// Set params to shader
	// SetParamsToShader (rshade, shapes);		//--- need to fix

	// Bind environment map
	Globals* g = getRenderMgr()->getScene()->getGlobals();
	Vec8S* tex = g->getEnvmapTex();
	Vec4F eclr = g->getEnvmapClr();
	if ( tex != 0x0 ) {
		ResolveTexture ( tex );		// get environment map
		id = getParam(rshade, P_ENVMAP);
		if ( id != P_NULL ) {
			glProgramUniform4i (rshade->programID, id, tex->x2, tex->y2, tex->z2, tex->w2 ); 
		}
		id = getParam(rshade, P_ENVCLR);
		if (id != P_NULL) {
			glProgramUniform4f(rshade->programID, id, eclr.x, eclr.y, eclr.z, eclr.w);
		}
	}

	CHECK_GL("Bind env map", mbDebug);
}

// Bind shadow maps
void RenderGL::BindShaderShadowMapVars ( RShader* rshade )
{
	int id, id2;
	id = getParam(rshade, P_SFAR1); 
	if (id != P_NULL)	{
		id2 = getParam(rshade, P_SFAR2); 
		glProgramUniform4fv	(rshade->programID, id , 1, &far_bound[0]);			// shadow bounds
		glProgramUniform4fv	(rshade->programID, id2, 1, &far_bound[4]);			
	}

	id = getParam(rshade, P_SMTX);
	if ( id != P_NULL) {
		for (int s=0; s < csm_num_splits; s++) 
			glProgramUniformMatrix4fv (rshade->programID, id+s, 1, GL_FALSE, shad_vmtx[s].GetDataF() );	// shadow view matrices			
	}
	id = getParam(rshade, P_STEX);	if (id != P_NULL) glProgramUniform1i	(rshade->programID, id, 1);			// shadow texture unit (#1)
	id = getParam(rshade, P_SSIZE);	if (id != P_NULL) glProgramUniform2f	(rshade->programID, id, (float)csm_depth_size, 1.0f / (float)csm_depth_size);	// shadow texture size 
}


bool RenderGL::BindUniformBuffers ()
{
	// Bind textures & lights to ALL shaders
	GLint tex_bind_point = 0;
	GLint mat_bind_point = 1;
	GLint lght_bind_point = 2;
	RShader* sh;
	int id;

	for (int s=0; s < mShaders.size(); s++ ) {
		sh = &mShaders[s];
		
		// texture buffers
		id = getParam(sh, P_TEXTURES);
		if ( id != P_NULL) {
			// update textures as sampler array
			//int texcnt = mTextures.size();
			//glProgramUniformHandleui64vARB ( sh->programID, id, texcnt, &mTextureArray[0] );

			// update textures as uniform buffer
			glBindBufferBase ( GL_UNIFORM_BUFFER, tex_bind_point, mTextureBuffer );			
			glUniformBlockBinding ( sh->programID, id, tex_bind_point );
			CHECK_GL("glUniformBlockBinding (textures)", mbDebug);
		} 
		
		// material buffers
		id = getParam(sh, P_MATERIALS);
		if ( id != P_NULL ) {
			glBindBufferBase ( GL_UNIFORM_BUFFER, mat_bind_point, mMatBuffer );			
			glUniformBlockBinding ( sh->programID, id, mat_bind_point );
			CHECK_GL("glUniformBlockBinding (materials)", mbDebug);
		}

		// light buffers
		id = getParam(sh, P_LIGHTS);
		if ( id != P_NULL) {			
			glBindBufferBase( GL_UNIFORM_BUFFER, lght_bind_point, mLightBuffer );				
			glUniformBlockBinding ( sh->programID, id, lght_bind_point );
			CHECK_GL("glUniformBlockBinding (lights)", mbDebug);
			glProgramUniform1i ( sh->programID, getParam(sh, P_LIGHTCNT), mLightCnt );
			CHECK_GL("glProgramUniform1i (light cnt)", mbDebug);
		}		

	}
	return true;
}

bool RenderGL::UpdateTextures ()
{
	// Bindless textures
	//  see: https://www.khronos.org/opengl/wiki/Bindless_Texture#Handle_creation
	//       https://stackoverflow.com/questions/40875564/opengl-bindless-textures-bind-to-uniform-sampler2d-array
	// GLSL shader:
	// layout(std140) uniform TEXTURE_BLOCK {
  //    sampler2D		tex[384];             // <---- sampler2D == uint64_t 
  // };

	int texcnt = mTextures.size();
	if ( texcnt == 0 ) return false;

	// Update texture array (not a GL_UNIFORM_BUFFER)	
	/* for (int i=0; i < texcnt; i++) {
		mTextures[i].handle = glGetTextureHandleARB ( mTextures[i].glID );			// get bindless texture handle from texture GLID				
		glMakeTextureHandleResidentARB ( mTextures[i].handle );									// will return invalid op if already resident		
		mTextureArray[i] = mTextures[i].handle;
	}	 */

	// Build bindless uniform [if needed]
	if ( mTextureBuffer == NULL_NDX ) {
		glGenBuffers ( 1, &mTextureBuffer );	
		glBindBuffer ( GL_UNIFORM_BUFFER, mTextureBuffer );	
		mTexBufCnt = 384;				// pre-allocate texture pool
		glBufferStorage ( GL_UNIFORM_BUFFER, mTexBufCnt * 2*sizeof(GLuint64), nullptr, GL_MAP_WRITE_BIT );	 // resize buffer		
		CHECK_GL ( "glBufferStorage (textures)", mbDebug);
	}	 

	glBindBuffer ( GL_UNIFORM_BUFFER, mTextureBuffer );
	GLuint64* uniform_array = (GLuint64*) glMapBufferRange ( GL_UNIFORM_BUFFER, 0, texcnt * 2*sizeof(GLuint64), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);	
	for (int i=0; i < texcnt; i++) {				
		mTextures[i].handle = glGetTextureHandleARB ( mTextures[i].glID );			// get bindless texture handle from texture GLID				
		glMakeTextureHandleResidentARB ( mTextures[i].handle );		// will return invalid op if already resident		
		uniform_array[ 2*i ] = mTextures[i].handle;																// <-- set the texture buffer value here	
		//dbgprintf ( "  Texture: slot %d -> %s\n", i, getAssetName(mTextures[i].assetID).c_str() );
	}
	glUnmapBuffer ( GL_UNIFORM_BUFFER );	 	

	glBindBuffer ( GL_UNIFORM_BUFFER, 0 );			

	return true;
}

bool RenderGL::CreateLights ( Object* obj, std::string& msg)
{
	LightSet* lset = dynamic_cast<LightSet*>( obj );
	mLightCnt = lset->getNumLights(); if (mLightCnt == 0) return 0;
	
	// Map Lights to buffer				
	if ( mLightBuffer == NULL_NDX ) {
		glGenBuffers ( 1, &mLightBuffer );
		glBindBuffer ( GL_UNIFORM_BUFFER, mLightBuffer );
		// immutable storage (fixed size)
		int lgt_max = 64;
		glBufferStorage ( GL_UNIFORM_BUFFER, lgt_max * sizeof(RLight), nullptr, GL_MAP_WRITE_BIT );
		CHECK_GL ( "lgt buffer storage", mbDebug);
	}
	UpdateLights ();
	return true;
}

void RenderGL::UpdateLights()
{
	LightSet* lset = dynamic_cast<LightSet*>(gScene->FindByType('lgts'));

	glBindBuffer(GL_UNIFORM_BUFFER, mLightBuffer);
	CHECK_GL("bind lgt", mbDebug);	
	int lgt_max = 64;
	RLight* pLGT = (RLight*) glMapBufferRange(GL_UNIFORM_BUFFER, 0, lgt_max * sizeof(RLight), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
	CHECK_GL("map range", mbDebug);
	memcpy(pLGT, lset->getLightBuf(), mLightCnt * sizeof(RLight));
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	CHECK_GL("unmap", mbDebug);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


bool RenderGL::CreateMaterial ( Object* obj, std::string& msg )
{
	Material* mtl = dynamic_cast<Material*> (obj);	
	
	// Create render material
	RMaterial mat;
	mat.assetID.x = obj->mObjID;	
	mMaterials.push_back ( mat );	
	int rid = mMaterials.size()-1;
	obj->SetRID( 0, rid );			// assign RID to material

	msg = "mtl: " + obj->getName();

	return true;
}

int RenderGL::UpdateMaterial ( Object* obj )
{
	Material* mtl = dynamic_cast<Material*> (obj);

	// get render material
	int rid = mtl->getRIDs().x;
	if (rid==-1) {
		std::string msg;
		if ( !CreateMaterial( obj, msg )) return -1;
		rid = mtl->getRIDs().x;
	}
	
	// update material GLID
	RMaterial* m = &mMaterials[rid];

	// update material textures
	Vec8S texids;	
	texids = Vec8S( mtl->getParamV4( M_TEXIDS ), NULL_NDX );

	ResolveTexture ( &texids );
	
	//--- debugging material textures
	std::string t0 = (texids.x2==NULL_NDX) ? "none" : gAssets.getObj(mTextures[texids.x2].assetID)->getName();
	std::string t1 = (texids.y2==NULL_NDX) ? "none" : gAssets.getObj(mTextures[texids.y2].assetID)->getName();
	printf("  Material: %s, tex0: %s tex1: %s\n", obj->getName().c_str(), t0.c_str(), t1.c_str());
	//---

	// pack material object params into render material	
	m->texids =			Vec4F(texids.x2, texids.y2, texids.z2, texids.w2 );
	m->ambclr =			Vec4F(mtl->getParamV3 ( M_AMB_CLR ), 1);
	m->diffclr =		Vec4F(mtl->getParamV3 ( M_DIFF_CLR ), 1);
	m->specclr =		Vec4F(mtl->getParamV3 ( M_SPEC_CLR ), 1);
	m->envclr =			Vec4F(mtl->getParamV3 ( M_ENV_CLR ), 1);
	m->shadowclr =	Vec4F(mtl->getParamV3 ( M_SHADOW_CLR ), 1);
	m->reflclr =		Vec4F(mtl->getParamV3 ( M_REFL_CLR ), 1);
	m->refrclr =		Vec4F(mtl->getParamV3 ( M_REFR_CLR ), 1);
	m->emisclr =		Vec4F(mtl->getParamV3 (M_EMIS_CLR), 1);
	m->surf_params.x =	mtl->getParamF ( M_SPEC_POWER);
	m->surf_params.y =  mtl->getParamF ( M_LGT_WIDTH);
	m->surf_params.z =  mtl->getParamF ( M_SHADOW_BIAS);
	m->refl_params.x =	mtl->getParamF ( M_REFL_WIDTH);
	m->refl_params.y =  mtl->getParamF ( M_REFL_BIAS);
	m->refr_params.x =	mtl->getParamF ( M_REFR_WIDTH);
	m->refr_params.y =  mtl->getParamF ( M_REFR_BIAS);
	m->refr_params.z =  mtl->getParamF ( M_REFR_IOR);
	m->displace_amt   = mtl->getParamV4 ( M_DISPLACE_AMT );
	m->displace_depth = mtl->getParamV4 ( M_DISPLACE_DEPTH );
	
	

	return rid;
}


void RenderGL::UpdateMaterials ()
{	
	if ( mMaterials.size() == 0 ) return;

	// Cycle through known materials
	Object* obj;
	int num_updated = 0;
	for (int i=0; i < mMaterials.size(); i++) {		
		obj = gAssets.getObj( mMaterials[i].assetID.x );		
		if ( obj->isDirty() ) {					// check if material dirty
			if ( obj->getNumParam() > 0 ) {		// check if valid
				UpdateMaterial ( obj );			// update it
				obj->MarkClean ();
				num_updated++;
			} 
		}
	}
	if ( num_updated > 0 ) {

		// Build bindless uniform [if needed]	
		if ( mMatBuffer == NULL_NDX ) {
			glGenBuffers ( 1, &mMatBuffer );	
			glBindBuffer ( GL_UNIFORM_BUFFER, mMatBuffer );	
			int mat_max = 64;				// pre-allocate material pool
			glBufferStorage ( GL_UNIFORM_BUFFER, mat_max * sizeof(RMaterial), nullptr, GL_MAP_WRITE_BIT );	 // resize buffer
			CHECK_GL ( "glBufferStorage (materials)", mbDebug);
		}	

		// Map and update material buffer	
		int mtlcnt = mMaterials.size();
		glBindBuffer ( GL_UNIFORM_BUFFER, mMatBuffer );
		RMaterial* matdata = (RMaterial*) glMapBufferRange ( GL_UNIFORM_BUFFER, 0, mtlcnt * sizeof(RMaterial), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);	
		for (int i=0; i < mtlcnt; i++) {				
			memcpy ( matdata, (char*) &mMaterials[i], sizeof(RMaterial) );
			matdata++;		
		}
		glUnmapBuffer ( GL_UNIFORM_BUFFER );	
		glBindBuffer ( GL_UNIFORM_BUFFER, 0 );	

	}
}


bool RenderGL::CreateTexture ( Object* obj, std::string& msg)
{
	Image* img = dynamic_cast<Image*> (obj);	
	GLuint glid;

	// Check if already on the GPU	
	glid = img->getGLID(); 
	//img->SetFilter ( GL
	if ( glid == -1 ) glGenTextures ( 1, &glid );		// Generate new texture
		
	// Image information
	int imgcnt = img->IsCube() ? 6 : 1;
	int bind_type = img->IsCube() ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
	int filter_type = img->MipFilter() ? (img->NoFilter() ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR) :
											(img->NoFilter() ? GL_NEAREST : GL_LINEAR );	
	int format_type = img->GetFormat();

	// Bind texture
	glBindTexture ( bind_type, glid );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1);								// pixel alignment
	glTexParameterf ( bind_type, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameterf ( bind_type, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri ( bind_type, GL_TEXTURE_MIN_FILTER, filter_type);		// filtering       
	glTexParameteri ( bind_type, GL_TEXTURE_MAG_FILTER, (filter_type == GL_LINEAR_MIPMAP_LINEAR) ? GL_LINEAR : filter_type );
	int ms = GL_MAX_TEXTURE_SIZE;

	// Load pixels	
	char* pixbuf = (char*) img->GetData();
	switch ( img->GetFormat() ) {				
	case ImageOp::BW8:		glTexImage2D ( bind_type, 0, GL_R8,   img->mXres, img->mYres,		0, GL_RED,		GL_UNSIGNED_BYTE, pixbuf );	break;
	case ImageOp::BW16:		glTexImage2D ( bind_type, 0, GL_R16F,  img->mXres, img->mYres,	0, GL_RED,		GL_UNSIGNED_SHORT,	pixbuf );	break;
	case ImageOp::BW32:		glTexImage2D ( bind_type, 0, GL_R32F,  img->mXres, img->mYres,	0, GL_RED,		GL_UNSIGNED_INT,  pixbuf );	break;
	case ImageOp::RGBA8:	glTexImage2D ( bind_type, 0, GL_RGBA, img->mXres, img->mYres,		0, GL_RGBA,		GL_UNSIGNED_BYTE, pixbuf );		break;	
	case ImageOp::RGB8:		glTexImage2D ( bind_type, 0, GL_RGB8, img->mXres, img->mYres,		0, GL_RGB,		GL_UNSIGNED_BYTE, pixbuf );		break;
	case ImageOp::RGB16:	glTexImage2D ( bind_type, 0, GL_RGB16, img->mXres, img->mYres,	0, GL_RGB16,	GL_UNSIGNED_SHORT, pixbuf );	break;
	case ImageOp::BGR8:		glTexImage2D ( bind_type, 0, GL_RGB8, img->mXres, img->mYres,		0, GL_BGR_EXT,	GL_UNSIGNED_BYTE, pixbuf );	break;
	case ImageOp::F32:		glTexImage2D ( bind_type, 0, GL_R32F, img->mXres, img->mYres,		0, GL_RED,		GL_FLOAT, pixbuf );			break;
	case ImageOp::RGBA32F:	glTexImage2D ( bind_type, 0, GL_RGBA32F, img->mXres, img->mYres,	0, GL_RGBA,		GL_FLOAT, pixbuf );			break;
	};			
	CHECK_GL ( "CreateTexture, glTexImage2D", mbDebug);
	
	// Generate mipmaps
	if ( img->MipFilter() )	glGenerateMipmap ( bind_type );
	
	// Create render texture 
	RTexture tex;
	tex.assetID = obj->mObjID;
	tex.glID = glid;	
	tex.filter = filter_type;
	tex.bindtype = bind_type;	
	mTextures.push_back ( tex );
	
	// Assign RID to texture for use by Behaviors
	int rid = (int) mTextures.size() - 1;
	obj->SetRID( 0, rid );

	char cmsg[1024];
	sprintf ( cmsg, "tid:%d glid:%d (%dx%d)", rid, glid, img->mXres, img->mYres );
	msg = cmsg;
	
	return true;
}

int RenderGL::UpdateTexture ()
{
	return 1;
}

// Read a shader file into a character string
char* RenderGL::ReadShaderSource ( std::string fname )
{
	char fpath[2048];
	strncpy ( fpath, fname.c_str(), 2048 );
	FILE *fp = fopen( fpath, "rb" );
	if (!fp) return NULL;
	fseek( fp, 0L, SEEK_END );
	long fileSz = ftell( fp );
	fseek( fp, 0L, SEEK_SET );
	char *buf = (char *) malloc( fileSz+1 );
	if (!buf) { fclose(fp); return NULL; }
	fread( buf, 1, fileSz, fp );
	buf[fileSz] = '\0';
	fclose( fp );
	return buf;
}


bool RenderGL::AddParam ( int shader_id, int param_id, char* name )
{
	int prog = mShaders[shader_id].programID;
	int active = 0;
	glGetProgramiv ( prog, GL_ACTIVE_UNIFORMS, &active );

	// get uniform location
	// - if not available, getParam will return P_NULL
	int ndx = glGetUniformLocation ( prog, name );	
	if (ndx==-1) return false;		

	// reserve slot for param_id
	if ( param_id >= mShaders[shader_id].params.size() ) {
		int start_param = (int) mShaders[shader_id].params.size();
		mShaders[shader_id].params.reserve ( param_id+1 ); 
		for (int n = start_param; n <= param_id; n++ ) mShaders[shader_id].params.push_back ( P_NULL );
	}	
	// set param location
	mShaders[shader_id].params[ param_id ] = ndx;

	//dbgprintf ( " Add param: %s, paramid:%d <= locid:%d (%s)\n", mShaders[shader_id].name, param_id, ndx, name);

	return true;
}

bool RenderGL::AddParamBlock ( int shader_id, int param_id, char* name )
{
	int prog = mShaders[shader_id].programID;

	// get uniform location
	// - if not available, getParam will return P_NULL
	int ndx = glGetUniformBlockIndex ( prog, name );		// uniform block
	if (ndx==-1) return false;	

// reserve slot for param_id
	if ( param_id >= mShaders[shader_id].params.size() ) {
		int start_param = (int) mShaders[shader_id].params.size();
		mShaders[shader_id].params.reserve ( param_id+1 ); 
		for (int n = start_param; n <= param_id; n++ ) mShaders[shader_id].params.push_back ( P_NULL );
	}	
	// set param location
	mShaders[shader_id].params[ param_id ] = ndx;
	
	return true;
}


int RenderGL::LoadInternalShader ( std::string fname )
{
	std::string msg;
	Object obj;	
	obj.SetName ( fname );
	obj.mFilename = fname;	
	
	if ( CreateShader ( &obj, msg ) ) {
		return obj.getRIDs().x;
	}
	return -1;
}

bool RenderGL::CreateShader ( Object* obj, std::string& msg)
{
	// Prepare RShader
	mShaders.push_back( RShader() );
	int rid = (int)mShaders.size() - 1;
	obj->SetRID( 0, rid);
	
	RShader* sh = &mShaders[ rid ];
	strncpy(sh->name, obj->getName().c_str(), 64);
	sh->assetID = obj->getID();	
	sh->params.clear();		

	// Load shader asset(s)
	// - loads vert and frag shader into single program
	bool result = LoadShader( obj->mFilename, sh->programID );	

	// Get default params
	glUseProgram( sh->programID );	
	AddParam(rid, P_OBJMTX,		"objMatrix");
	AddParam(rid, P_VIEWMTX,	"viewMatrix");
	AddParam(rid, P_INVMTX,		"invMatrix");
	AddParam(rid, P_PROJMTX,	"projMatrix");
	AddParam(rid, P_CAMPOS,		"camPos");
	AddParam(rid, P_CAMNF,		"camNearFar" );
	AddParam(rid, P_CAMUVW,		"camUVW");
	AddParam(rid, P_TEXUNI,		"texUni" );

	AddParam(rid, P_ENVMAP,		"envMap");
	AddParam(rid, P_ENVCLR,		"envClr");

	AddParam(rid, P_LIGHTCNT,	"numLgt");
	AddParam(rid, P_PARAMS+0,	"param0");
	AddParam(rid, P_PARAMS+1,	"param1");
	AddParam(rid, P_PARAMS+2,	"param2");
	AddParam(rid, P_PARAMS+3,	"param3");		
	AddParam(rid, P_SFAR1,		"shadowFars1");
	AddParam(rid, P_SFAR2,		"shadowFars2");
	AddParam(rid, P_SMTX,		  "shadowMtx");
	AddParam(rid, P_STEX,		  "shadowTex");
	AddParam(rid, P_SSIZE,		"shadowSize");

	AddParamBlock(rid, P_TEXTURES,	"TEXTURE_BLOCK");
	AddParamBlock(rid, P_MATERIALS, "MATERIAL_BLOCK");
	AddParamBlock(rid, P_LIGHTS,	  "LIGHT_BLOCK");	

	glUseProgram(0);

	// verify attrib layout
	/* int ndx = glGetAttribLocation( sh->programID, "instXform");
	if ( ndx != instXform ) {
		dbgprintf ( "  OpenGL. ERROR: Attrib layout mismatch in %s. instXform found at %d, should be %d.\n",  obj->mFilename.c_str(), ndx, instXform );
		assert ( 0 );
	} */ 


	msg = "sid:" + iToStr ( rid ) +" program:"+iToStr ( sh->programID ) ;

	return result;
}

bool RenderGL::LoadShader (std::string filename, GLuint& program )
{
	int statusOK;
	int maxLog = 65536, lenLog;
	char log[65536];
	char fname[1024], fpath[1024];
	std::string fragpath, vertpath, geompath;

	// Get shader filenames
	std::string fragfile = filename;
	strcpy ( fname, fragfile.c_str() );
	bool hasFrag = getFileLocation( fname, fpath); fragpath = fpath;

	std::string vertfile = strReplace( filename, ".frag", ".vert");
	strcpy ( fname, vertfile.c_str() );
	bool hasVert = getFileLocation( fname, fpath); vertpath = fpath;

	std::string geomfile = strReplace( filename, ".frag", ".geom");
	strcpy ( fname, geomfile.c_str() );
	bool hasGeom = getFileLocation( fname, fpath); geompath = fpath;
	
	if (!hasFrag) { dbgprintf ("  OpenGL. ERROR: Unable to find fragment program %s\n", vertfile.c_str()); return false; }
	if (!hasVert) { dbgprintf ("  OpenGL. ERROR: Unable to find vertex program %s\n", vertfile.c_str());	return false; }

	// Read vertex program	
	char *vertSource = ReadShaderSource(vertpath.c_str());
	if (!vertSource) { dbgprintf ("  OpenGL. ERROR: Unable to read source '%s'\n", vertfile.c_str());  return false; }
	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vShader, 1, (const GLchar**)&vertSource, NULL);
	glCompileShader(vShader);
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &statusOK);
	
	free(vertSource);

	if (!statusOK) {
		glGetShaderInfoLog(vShader, maxLog, &lenLog, log);
		dbgprintf("  OpenGL. ***Vert Compile Error in '%s'!\n", vertfile.c_str());
		dbgprintf("  %s\n", log);
		exit(-1);
		return false;
	}

	// Read fragment program
	char *fragSource = ReadShaderSource(fragpath);
	if (!fragSource) {
		dbgprintf("  OpenGL. ERROR: Unable to read source '%s'\n", fragfile.c_str());
		exit(-1);
		return false;
	}
	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fShader, 1, (const GLchar**)&fragSource, NULL);
	glCompileShader(fShader);
	glGetShaderiv(fShader, GL_COMPILE_STATUS, &statusOK);
	free(fragSource);
	if (!statusOK) {
		glGetShaderInfoLog(fShader, maxLog, &lenLog, log);
		dbgprintf("  OpenGL. ***Fragment Compile Error in '%s'!\n", fragfile.c_str());
		dbgprintf("  %s\n", log);
		exit(-1);
		return false;
	}

	// Read geometry program
	char *geomSource = 0x0;
	GLuint gShader;
	if (hasGeom) {
		geomSource = ReadShaderSource(geompath);
		if (!geomSource) { dbgprintf("  OpenGL. ERROR: Unable to read source '%s'\n", geomfile.c_str()); return false; }
		gShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(gShader, 1, (const GLchar**)&geomSource, NULL);
		glCompileShader(gShader);
		glGetShaderiv(gShader, GL_COMPILE_STATUS, &statusOK);
		free(geomSource);
		if (!statusOK) {
			glGetShaderInfoLog(gShader, maxLog, &lenLog, log);
			dbgprintf("  OpenGL. ***Geom Compile Error in '%s'!\n", geomfile.c_str());
			dbgprintf("  %s\n", log);
			return false;
		}
	}

	// Create GL program
	program = glCreateProgram();
	glAttachShader(program, vShader);
	glAttachShader(program, fShader);
	if (hasGeom) glAttachShader(program, gShader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &statusOK);
	if (!statusOK) {
		glGetProgramInfoLog(program, maxLog, &lenLog, log);
		dbgprintf("  OpenGL. ***Linking Error with '%s'!\n", vertfile.c_str());
		dbgprintf("  %s\n", log);
		return false;
	}

	return true;
}
int RenderGL::UpdateShader ()
{
	return 1;
}

bool RenderGL::CreateVolume ( Object* obj, std::string& msg)
{
	msg = "OK";
	return true;
}

// Mesh here should be renamed Geom
// Covers mesh, pnts, lines, curves
//
bool RenderGL::CreateMesh ( Object* obj, std::string& msg )
{
	int rid = -1;
	char cmsg[1024];

	if ( obj->getType()=='Amsh') {
		Mesh* mesh = dynamic_cast<Mesh*> (obj);

		// Create triangle mesh
		RMesh mx;
		mMeshes.push_back ( mx );
		rid = (int) mMeshes.size() - 1;
		RMesh* m = &mMeshes[rid];
		m->assetID = obj->getID();
		m->primtype = GL_TRIANGLES;
		m->primcnt = mesh->GetNumElem ( BFACEV3 );
		obj->SetRID( 0, rid );
		char stats[6];
		memset ( stats, '.', 6 ); stats[5]='\0';
		if ( mesh->isActive ( BVERTPOS ) ) 	{glGenBuffers ( 1, (GLuint*) &m->meshVBO[0] ); stats[0]='P'; }
		if ( mesh->isActive ( BVERTCLR ) ) 	{glGenBuffers ( 1, (GLuint*) &m->meshVBO[1] ); stats[1]='C'; }
		if ( mesh->isActive ( BVERTNORM ) ) {glGenBuffers ( 1, (GLuint*) &m->meshVBO[2] ); stats[2]='N'; }
		if ( mesh->isActive ( BVERTTEX ) )  {glGenBuffers ( 1, (GLuint*) &m->meshVBO[3] ); stats[3]='T'; }
		if ( mesh->isActive ( BFACEV3 ) ) 	{glGenBuffers ( 1, (GLuint*) &m->meshVBO[4] ); stats[4]='F'; }
		UpdateMesh ( (Mesh*) obj );
		sprintf ( cmsg, "mid:%d vbo:%d (%d tris) %s", rid, m->meshVBO[0], m->primcnt, stats);
	
	} else if ( obj->getType()=='Apnt') {

		Points* pnts = dynamic_cast<Points*> (obj);

		// Create points mesh
		RMesh mx;
		mMeshes.push_back ( mx );
		rid = (int) mMeshes.size() - 1;
		RMesh* m = &mMeshes[rid];
		m->assetID = pnts->getID();
		m->primtype = GL_POINTS;
		m->primcnt = pnts->getNumPoints ();
		pnts->SetRID ( 0, rid );

		if (pnts->getNumPoints() > 0) {
			// check if already on the GPU
			int vbopos = pnts->getVBO(FPOS);
			int vboclr = pnts->getVBO(FCLR);
			if (vbopos == -1) glGenBuffers(1, (GLuint*)&vbopos);		// generate vbo
			if (vboclr == -1) glGenBuffers(1, (GLuint*)&vboclr);		// generate vbo
			m->meshVBO[0] = vbopos;
			m->meshVBO[1] = vboclr;
			// update points
			UpdatePnts(pnts);
		}
		sprintf ( cmsg, "mid:%d vbo:%d (%d pnts)", rid, m->meshVBO[0], m->primcnt);
	}	
	msg = cmsg;

	return (rid>=0) ? true : false;
}

int	RenderGL::UpdatePnts ( Object* obj )
{
	Points* pnts = dynamic_cast<Points*> (obj);
	int rid = obj->getRIDs().x;
	RMesh* m = &mMeshes[rid];
	
	m->primcnt = pnts->getNumPoints ();

	glBindBufferARB ( GL_ARRAY_BUFFER_ARB, m->meshVBO[ 0 ] );	
	glBufferDataARB ( GL_ARRAY_BUFFER_ARB, pnts->getBufSz(FPOS), pnts->getBufF(FPOS), GL_DYNAMIC_DRAW );	// point positions

	glBindBufferARB ( GL_ARRAY_BUFFER_ARB, m->meshVBO[ 1 ] );	
	glBufferDataARB ( GL_ARRAY_BUFFER_ARB, pnts->getBufSz(FCLR), pnts->getBufF(FCLR), GL_DYNAMIC_DRAW );	// point positions
	CHECK_GL ( "Update pnts", mbDebug);

	//dbgprintf ( "Apnts: %s, mid:%d vbo:%d (%d pnts)\n", obj->getName().c_str(), rid, m->meshVBO[0], m->primcnt);
	
	return 1;
}

int RenderGL::UpdateMesh (Mesh* mesh )
{
	int rid = mesh->getRIDs().x;
	if (rid==-1) {
		std::string msg;
		if ( !CreateMesh( mesh, msg )) return -1;
		rid = mesh->getRIDs().x;
	}
	RMesh* m = &mMeshes[rid];

	if ( mesh->isActive ( BVERTPOS ) ) {				
		glBindBufferARB ( GL_ARRAY_BUFFER_ARB, m->meshVBO[ 0 ] );		
		glBufferDataARB ( GL_ARRAY_BUFFER_ARB, mesh->GetBufSize(BVERTPOS), mesh->GetBufData(BVERTPOS), GL_DYNAMIC_DRAW);
		CHECK_GL ( "Update mesh pos", mbDebug);
	}
	if ( mesh->isActive ( BVERTCLR ) ) {
		glBindBufferARB ( GL_ARRAY_BUFFER_ARB, m->meshVBO[ 1 ] );		
		glBufferDataARB ( GL_ARRAY_BUFFER_ARB, mesh->GetBufSize(BVERTCLR), mesh->GetBufData(BVERTCLR), GL_DYNAMIC_DRAW);
		CHECK_GL ( "Update mesh clr", mbDebug);
	}	

	if ( mesh->isActive ( BVERTNORM ) ) {				
		glBindBufferARB ( GL_ARRAY_BUFFER_ARB, m->meshVBO[ 2 ] );		
		glBufferDataARB ( GL_ARRAY_BUFFER_ARB, mesh->GetBufSize(BVERTNORM), mesh->GetBufData(BVERTNORM), GL_DYNAMIC_DRAW);
		CHECK_GL ( "Update mesh norm", mbDebug);
	}
	if ( mesh->isActive ( BVERTTEX ) ) {		
		glBindBufferARB ( GL_ARRAY_BUFFER_ARB, m->meshVBO[ 3 ] );		
		glBufferDataARB ( GL_ARRAY_BUFFER_ARB, mesh->GetBufSize(BVERTTEX), mesh->GetBufData(BVERTTEX), GL_DYNAMIC_DRAW);
		CHECK_GL ( "Update mesh uv", mbDebug);
	}
	if ( mesh->isActive ( BFACEV3 ) ) {		
		glBindBufferARB ( GL_ELEMENT_ARRAY_BUFFER_ARB, m->meshVBO[ 4 ] );				

		#ifdef LARGE_MESHES
			// MeshX supports very large meshes natively, with int64_t so we 
			// need to repack 64-bit vertex indices into 32-bits for OpenGL.
			int ndx_cnt = mesh->GetBufSize( BFACEV3 ) / sizeof(int64_t);
			int repack_sz =  ndx_cnt * sizeof(int32_t);
			int32_t* repack_buf = (int32_t*) malloc ( repack_sz );
			int64_t* src_buf = (int64_t*) mesh->GetBufData( BFACEV3 );
			int32_t* dst_buf = repack_buf;
			for (int n=0; n < ndx_cnt; n++) {
				*dst_buf++ = (int32_t) *src_buf++;		// cast to 32-bit int
			}
			glBufferDataARB ( GL_ELEMENT_ARRAY_BUFFER_ARB, repack_sz, repack_buf, GL_DYNAMIC_DRAW);
			free ( repack_buf );
		#else
			glBufferDataARB ( GL_ELEMENT_ARRAY_BUFFER_ARB, mesh->GetBufSize(BFACEV3), mesh->GetBufData(BFACEV3), GL_DYNAMIC_DRAW);
		#endif	

		CHECK_GL ( "Update mesh array", mbDebug);
	}	
	//dbgprintf ( " Updated mesh.\n" );

	return 1;
}

