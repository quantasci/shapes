


#ifdef USE_OPTIX

//#define WGL_NV_gpu_affinity
#include "render_optix.h" 
#include "render.h"

#include "object.h"
#include "shapes.h"
#include "image.h"
#include "mesh.h"
#include "scene.h"
#include "lightset.h"
#include "material.h"
#include "volume.h"
#include "timex.h"

#ifdef USE_GVDB
	#include "gvdb.h"
#endif

#include <time.h>

RenderOptiX::RenderOptiX ()
{
	// Set max samples
	mMaxSamples = 64;			// can be low since AI-denoiser if enabled
	mDenoise = false;

	mSample = 0;
	mVDB = -1;
	mLightCnt = 0;
	mMatSurf1 = -1;
	mRegionX = -1;
} 

void RenderOptiX::Initialize ()
{
	Scene* scn = getRenderMgr()->getScene();
	Vec3F res = scn->getRes();

	if ( optix==0x0 ) {
		dbgprintf ( "  OptiX. ERROR: Optix not set for Renderoptix->\n" );
		exit(-10);
	}
	//optix->InitializeOptix ( res.x, res.y );

	InitializeStateSort ();
}


void RenderOptiX::RestartRender ()
{
	// Reset sampling & pix counts
	UpdateCamera();

	// Clear entire inst graph 
	optix->ClearInstGroups(0);
	
	// Mark all meshes & materials dirty
	gScene->MarkSceneByType('Amsh', MARK_DIRTY);
	gScene->MarkSceneByType('Amtl', MARK_DIRTY);	
	gScene->MarkSceneByType('Aimg', MARK_DIRTY);

	// Clear Optix internal graph
	optix->ClearGraph();

	// Clear render meshes, textures, materials
	mMeshes.clear ();
	mTextures.clear ();
	mMaterials.clear ();

	// Reset all RIDs
	for (int n = 0; n < gAssets.getNumObj(); n++) {
		Object* obj = gAssets.getObj(n);
		if (obj == 0x0) continue;		
		obj->SetRID( 1, NULL_NDX );
	}

	// Default env map
	optix->SetEnvMap(0);		// default env map (none)

	// Rebuild all
	PrepareAssets();
}


void RenderOptiX::PrepareAssets ()
{
	std::string what;
	bool result;
	Object* obj;
	int ok=0;

	dbgprintf ("  OptiX. Preparing Assets\n");

	// Build OptiX resources from the Asset list	
	for (int n=0; n < gAssets.getNumObj(); n++ ) {		
		
		obj = gAssets.getObj (n);
		if (obj==0x0) continue;

		if ( obj->getRIDs().y == NULL_NDX ) {

			result =  false;
			switch ( obj->getType() ) {
			case 'camr':	result = true;					what = "cam";	break;
			case 'Amsh':	result = CreateMesh(obj);		what = "mesh";	break;
			case 'Avol':	result = CreateVolume(obj);		what = "vol";  mVDB = n;  break;
			case 'Aimg':	result = CreateTexture(obj);	what = "tex";	break;
			case 'Amtl':	result = CreateMaterial(obj);	what = "mtl"; break;
			//case 'Ashp':	result = CreateShapes(obj);		what = "shp"; break;
			};
			if ( result ) {
				ok++;
				//dbgprintf ( "  Prepared: asset %d, %s (%s)\n", n, obj->getName().c_str(), what.c_str() );
			}		
		}
		// Mark all assets as dirty (when switching renderers)		
		obj->MarkDirty ();

	}	
	dbgprintf ("  OptiX. Prepared Assets, %d ok, Tex:%d, Lights:%d, Mesh:%d \n", ok, (int) mTextures.size(), mLightCnt, mMeshes.size() );

	// Mark all meshes as dirty
	gScene->MarkSceneByType ( 'Amsh', MARK_DIRTY );
}


bool RenderOptiX::UpdateLights ( LightSet* lgts )
{
	mLightCnt = lgts->getNumLights();
	RLight* lgt;
	for (int n=0; n < lgts->getNumLights(); n++) {
		lgt = lgts->getLight(n);
		optix->SetLight ( n, lgt->pos, lgt->diffclr, lgt->shadowclr );
	}	
	optix->UpdateLights ();

	return true;
}



bool RenderOptiX::CreateVolume (Object* obj)
{
	Volume* vol = dynamic_cast<Volume*>( obj );

	#ifdef USE_GVDB
		VolumeGVDB* gvdb = vol->getGVDB();

		int shading =	vol->mRenderShade;		// desired shading
		int chan =		vol->mRenderChan;		// desired channel

		// Add GVDB volume to the OptiX scene
		dbgprintf ( "  OptiX. Adding Volume \n" );
		int matid;
		char isect = '0';
		switch (shading) {
		case SHADE_EMPTYSKIP:	matid = mMatSurf1;	isect = 'E';	break;	
		case SHADE_TRILINEAR:	matid = mMatSurf1;	isect = 'S';	break;
		case SHADE_VOLUME:		matid = mMatDeep;	isect = 'D';	break;
		}
		if ( isect == '0' ) {
			dbgprintf ( "  OptiX. ERROR: Volume no shading method selected.\n" );
			exit(-10);
		}
		Vec3F volmin = gvdb->getVoxMin ();
		Vec3F volmax = gvdb->getVoxMax ();
		Matrix4F xform;	
		xform.Identity();
	

		// Set Transfer Function (once before validate)
		Vec4F* src = gvdb->getScene()->getTransferFunc();
		optix->SetTransferFunc ( src );

		int atlas_glid = gvdb->getAtlasGLID ( 0 );
		optix->AddVolume ( atlas_glid, volmin, volmax, xform, matid, isect );	

		// Assign GVDB data to OptiX	
		dbgprintf ( "  OptiX. Update Volume.\n" );	
		optix->UpdateVolume ( gvdb, shading, chan );	
	#endif
		
	return true;
}

bool RenderOptiX::CreateMaterial ( Object* obj )
{
	::Material* mtl = dynamic_cast<::Material*> (obj);

	//printf ( "  sizeof optixmat: %d, program: %d\n", (int)  sizeof(OptixMat), (int) sizeof(optix::Program) );

	// register with optix library
	int oid = optix->AddMaterial ( "optix_trace_surface", "trace_surface", "trace_surface_anyhit", "trace_shadow_anyhit" );
	OptixMat* m = optix->getMaterial (oid);

	int rid = mMaterials.size();

	RAssetOptix a;
	a.assetID = obj->getID();
	a.optixID = oid;
	obj->SetRID ( 1, rid );			// optix material to use

	// add to material list	
	mMaterials.push_back ( a );		

	return true;
}

void RenderOptiX::UpdateMaterial ( Object* obj )
{
	int rid, oid, aid;

	::Material* mtl = dynamic_cast<::Material*>( obj );
	if (mtl==0x0) return;

	rid = obj->getRIDs().y; if (rid==NULL_NDX) return;	

	// Update optix material from material params
	oid = mMaterials[ rid ].optixID;
	OptixMat* m = optix->getMaterial (oid);

	m->ambclr =			Vec4F(mtl->getParamV3 ( M_AMB_CLR ), 1);
	m->diffclr =		Vec4F(mtl->getParamV3 ( M_DIFF_CLR ), 1);
	m->specclr =		Vec4F(mtl->getParamV3 ( M_SPEC_CLR ), 1);
	m->envclr =			Vec4F(mtl->getParamV3 ( M_ENV_CLR ), 1);
	m->shadowclr =		Vec4F(mtl->getParamV3 ( M_SHADOW_CLR ), 1);
	m->reflclr =		Vec4F(mtl->getParamV3 ( M_REFL_CLR ), 1);
	m->refrclr =		Vec4F(mtl->getParamV3 ( M_REFR_CLR ), 1);
	m->emisclr =		Vec4F(mtl->getParamV3( M_EMIS_CLR), 1);
	
	m->surf_params.x =	mtl->getParamF ( M_SPEC_POWER);
	m->surf_params.y =  mtl->getParamF ( M_LGT_WIDTH);
	m->surf_params.z =  mtl->getParamF ( M_SHADOW_BIAS);
	
  // dbgprintf("mtl: %s, spec: %f, lwidth: %f, sh bias: %f\n", obj->getName().c_str(), m->surf_params.x, m->surf_params.y, m->surf_params.z );

	m->refl_params.x =	mtl->getParamF ( M_REFL_WIDTH);
	m->refl_params.y =  mtl->getParamF ( M_REFL_BIAS);
	m->refr_params.x =	mtl->getParamF ( M_REFR_WIDTH);
	m->refr_params.y =  mtl->getParamF ( M_REFR_BIAS);
	m->refr_params.z =  mtl->getParamF ( M_REFR_IOR);

	m->displace_amt   = mtl->getParamV4 ( M_DISPLACE_AMT );

	if (m->displace_amt.z==0)
		printf ( "WARNING: fine displacement is zero.\n");
	
	m->displace_depth = mtl->getParamV4 ( M_DISPLACE_DEPTH );	

	// Update material textures
	Vec8S texids;	
	texids = Vec8S( mtl->getParamV4( M_TEXIDS ), NULL_NDX );

	// Resolve texture(s):
	// Convert texture asset IDs to optix texture IDs
	Image* img;	
	Vec8S otexids;
	for (int i=0; i < 4; i++) {
		aid = texids.get(i);		// texture asset id
		otexids.Set(i, 0 );			// default tex (empty)
		if ( aid != NULL_NDX ) {
			
			// get image asset
			img = dynamic_cast<Image*> ( gAssets.getObj( aid ) );	

			if (img!=0x0 ) {					
				rid = img->getRIDs().y;					// get texture render ID (index into mTextures)				
				otexids.Set(i, mTextures[rid].optixID);	// get optix texture ID

				// texture was modified. update it.
				if ( img->isDirty() ) {					
					//dbgprintf ("Optix update tex. %s\n", img->getName().c_str() );
					optix->UpdateTexture ( mTextures[rid].optixID, img->GetWidth(), img->GetHeight(), img->GetBitsPerPix(), img->GetData(), img->getDirtyRegion() );					
					img->SetDirtyRegion( Vec4F(0,0,0,0) );
					img->MarkClean ();
				}

			}
		}
	}	


  // update material data on GPU - including displacement vals
	optix->UpdateMaterial ( oid, otexids );
}

void RenderOptiX::UpdateMaterials ()
{	
	if ( mMaterials.size() == 0 ) return;

	// Cycle through known materials
	Object* obj;
	int nump;	
	int num_updated = 0;
	for (int i=0; i < mMaterials.size(); i++) {		
		obj = gAssets.getObj( mMaterials[i].assetID );		
		if ( obj->isDirty() ) {							// check if material dirty
			if ( obj->getNumParam() > 0 ) {		// check if valid
				UpdateMaterial ( obj );					// update it
				obj->MarkClean ();
				num_updated++;
			} 
		}
	}
}


int	RenderOptiX::getOptixMaterial ( Vec8S* matids )
{
	if ( matids == 0x0 ) return 0;
	if ( matids->y2 == NULL_NDX ) return 0;
	return mMaterials[ matids->y2 ].optixID;	
}

bool RenderOptiX::CreateTexture (Object* obj)
{
	// Get texture data
	Image* img = dynamic_cast<Image*> (obj);	
	unsigned char* pixbuf = (unsigned char*) img->GetData();
	if ( pixbuf==0x0) {
		dbgprintf ( "  OptiX. ERROR: Image texture is empty.\n" );
		return false;
	}
	//dbgprintf ("program: %d\n", sizeof(optix::Program));

	int w = img->mXres;
	int h = img->mYres;
	int bpp = img->GetBitsPerPix();
	bool cuda = false;

	// Create Optix bindless texture (buffer)

	int oid = optix->AddTexture ( w, h, bpp, pixbuf, cuda );

	// Add to render textures
	int rid = (int) mTextures.size();
	RAssetOptix rt;
	rt.assetID = obj->getID();
	rt.optixID = oid;
	mTextures.push_back ( rt );	
	obj->SetRID ( 1, rid );			// texture to render with

	return true;
}


int	RenderOptiX::getOptixTexture ( int tid )
{
	if ( tid == NULL_NDX ) return 0;
	Image* img = dynamic_cast<Image*> ( gAssets.getObj(tid) );  // tid = asset ID
	if (img==0x0 ) return 0;
	int rid = img->getRIDs().y;					// rid = render ID
	return mTextures[rid].optixID;				// oid = optix ID
}


OptixMeshInfo RenderOptiX::getMeshBufferInfo ( Mesh* mesh, int face_cnt )
{					
	int faces = (face_cnt == 0) ? mesh->GetNumElem(BFACEV3) : face_cnt;

	return OptixMeshInfo(   mesh->GetNumElem(BVERTPOS), faces,   
							mesh->GetBufData(BVERTPOS), mesh->GetBufStride(BVERTPOS), 
							mesh->GetBufData(BVERTNORM), mesh->GetBufStride(BVERTNORM),
							mesh->GetBufData(BVERTTEX), mesh->GetBufStride(BVERTTEX),
							mesh->GetBufData(BFACEV3), mesh->GetBufStride(BFACEV3) );
}


bool RenderOptiX::CreateMesh ( Object* obj )
{
	Mesh* mesh = dynamic_cast<Mesh*> (obj);

	Matrix4F xform;
	int rid = (int) mMeshes.size();

	OptixMeshInfo om_info = getMeshBufferInfo (mesh);
	int omid = optix->AddMesh ( om_info );
	
	//dbgprintf ( "omidh: %d\n", omid );

	// Add to mesh assets
	RAssetOptix rt;
	rt.src = mesh;
	rt.assetID = obj->getID();
	rt.optixID = omid;	
	obj->SetRID ( 1, rid );			// mesh to render with

	// Default shape set
	if ( rt.defaultShapes == 0 ) {
		int i;
		rt.defaultShapes = new Shapes;
		rt.defaultShapes->Add(i);		
		rt.defaultShapes->AddParam ( 0, "MESH", "i"); 
		rt.defaultShapes->SetParamI ( 0, 0, obj->getID() );
		//optix->AddInstances ( rt.defaultShapes );
	}
	mMeshes.push_back ( rt ); 
	
	return true;
}


bool RenderOptiX::UpdateMesh ( Object* obj, Vec8S& matids )
{
	// Get OptiX mesh ID
	Mesh* mesh = dynamic_cast<Mesh*> (obj);
	int rid = mesh->getRIDs().y;
	if (rid==-1) {
		dbgprintf ( "WARNING: Mesh has null optix RID.\n" );
		CreateMesh ( obj );
		rid = mesh->getRIDs().y;
	}
	int omid = mMeshes[rid].optixID;

	// Get new mesh info
	OptixMeshInfo om_info = getMeshBufferInfo (mesh, 0 );

	// Get material (for displacement params)
	::Material* mtl = dynamic_cast<::Material*> ( gAssets.getObj( matids.x ) );
	Vec4F displace_depth;

	if ( mtl != 0x0 ) {
		// Retrieve mesh-level parameters
		// get displacement
		displace_depth = mtl->getParamV4 ( M_DISPLACE_DEPTH );
		displace_depth.w = mtl->getParamV4 ( M_DISPLACE_AMT ).x;

		// update the optix mesh
		optix->UpdateMesh ( omid, om_info, displace_depth );

		// update intersection program (shader)
		std::string shader_name = mtl->getInputAsName ( "shader" );
		int shaderID = MI_MESH;							// standard trimesh used for 'shade_mesh' and 'shade_terrain' (baked)	
		if (shader_name.compare("shade_bump")==0 )		// displacement used for 'shade_bump'
			shaderID = MI_DISPLACE;	

		optix->UpdateMeshShader ( omid, shaderID );

		// only mark clean if material found
		obj->MarkClean ();		
	}	

	return true;
}

void RenderOptiX::Validate ()
{
	// Validate OptiX graph
	dbgprintf ( "  OptiX. Validating.\n" );
	optix->ValidateGraph ();	
}


void RenderOptiX::StartRender ()
{
	Scene* scn = getRenderMgr()->getScene ();

	// Update volumes
	#ifdef USE_GVDB
		if ( mVDB >= 0 ) {
			// Find *the* volume asset 
			//  (optix_scene only supports one right now)
			Volume* vol = dynamic_cast<Volume*> ( gAssets.getObj(mVDB) );
			int shading =	vol->mRenderShade;		// desired shading
			int chan =		vol->mRenderChan;		// desired channel
			VolumeGVDB* gvdb =	vol->getGVDB();	
		
			Camera3D* cam = scn->getCamera3D();
			gvdb->getScene()->SetCamera ( cam );		// gvdb camera
		
			optix->UpdateVolume ( gvdb, shading, chan );

			/// Add default deep volume material		
			//optix->SetMaterialParams ( mMatDeep, matp );
		}
#endif
}


/*int RenderOptiX::getMeshFromShapes ( Shapes* shapes )
{
	int objID = shapes->getParamI ( 0, 0 );			// mesh asset ID
	if ( objID == OBJ_NULL ) return 0x0;
	int mid = gAssets.getObj(objID)->getRIDs().y;   // mesh RID for OptiX (.y)
	if ( mid == OBJ_NULL ) return 0x0;
	return mMeshes[ mid ].optixID;					// optix mesh ID
}*/

void RenderOptiX::BuildRenderGraph ()
{
	int ig;

	Shape IP;
	int ITEXSUB	= (char*) &IP.texsub - (char*)&IP;

	for (int g = 0; g < mSGCnt; g++) {			// state sorted shape groups

												// SHAPE GROUP
		int inst_offset = mSG[g].offset;		// starting instance
		int inst_count = mSG[g].count;			// number of instances
		int mesh_id = mSG[g].meshids.x;			// mesh ID for this group
		//int face_cnt = mSG[g].meshids.z;		// number of faces (triangles) - not used 
		//int face_off = mSG[g].meshids.w;		// face offset (NOT USED) - not used
		int shader_id = mSG[g].meshids.y;		// shader ID for this group		

		if ( mSG[g].meshids.w==-1 ) continue;

		// Get optix Material and Texture (for this group)	
		// *Note* this is the shape within the SHAPE buffer, not the original shape
		// so we cannot store render state in these shapes (eg. matids, texids).
		Shape* shp = (Shape*) mSB.GetElem (BSHAPES, inst_offset);				// first shape in group

		int omat_id = getOptixMaterial ( &shp->matids );				// material optix id

		// Get optix Mesh 
		Mesh* mesh = (Mesh*) gAssets.getObj(mesh_id);					// mesh asset		
		if ( mesh->isDirty() ) {
			UpdateMesh ( mesh, shp->matids );
		}
		int rid = mesh->getRIDs().y;									// mesh rid
		int omesh_id = mMeshes[rid].optixID;							// mesh optix id
			
		// Create instance group
		// *NOTE* This includes any textures assigned to the material
		ig = optix->AddInstGroup ( g, omesh_id, omat_id );

		// Add instances
		Matrix4F* xforms = (Matrix4F*) mSB.GetElem(BXFORMS, inst_offset);	// starting offset
		char* inst_data = (char*) mSB.GetElem(BSHAPES, inst_offset );				
			
		optix->AddInstances ( ig, inst_count, xforms, inst_data, sizeof(Shape), ITEXSUB );

		int vert = optix->getModel(rid)->info.num_vert;
		
		//----------- Debugging - USE THIS
		//dbgprintf ("  render: shadegrp %d, # inst %d, mesh: %s,%d (vert %d), omid: %d\n", g, inst_count, mesh->getName().c_str(), rid, vert, omesh_id );
	}
	
	// clear inst groups
  // - clear all instances *after* the last one
	optix->ClearInstGroups ( mSGCnt );
}

bool RenderOptiX::Render ()
{
	Scene* scn = getRenderMgr()->getScene ();			

	// Get globals
	Globals* g = getRenderMgr()->getScene()->getGlobals();
	Vec8S* envtex = g->getEnvmapTex();
	Vec4F backclr = g->getBackgrdClr();	
	Vec4F ray_depths = g->getRayDepths();
	mMaxSamples = g->getMaxSamples();


	if ( mDenoise && mSample >= mMaxSamples ) 
		return true;

	// Set globals to OptiX
	if (envtex != 0x0) {
		int otid = getOptixTexture(envtex->x);
		optix->SetEnvMap(otid);
	}  
	optix->SetBackClr ( backclr );
	optix->SetRayDepths ( ray_depths );
	optix->SetMaxSamples ( mMaxSamples );

	// Step 0. Update materials
	UpdateMaterials ();

	// Steps 1-3. State sorting
	InsertAndSortShapes ();

	// Step 4.
	BuildRenderGraph ();

	// Step 5. Update Lights & Camera		
	Camera3D* cam = scn->getCamera3D();
	optix->SetCamera ( cam );
	optix->SetDOF ( cam->getDOF() );
	
	LightSet* lgts = dynamic_cast<LightSet*>(gScene->FindByType('lgts'));
	UpdateLights ( lgts );

	// Step 6. Launch OptiX render
	clock_t t1, t2;
	t1 = clock();
	int frame = getRenderMgr()->getFrame();	

	// set number of samples
	optix->SetSample ( frame, mSample, mMaxSamples, mDenoise );	
	int downsample = 1;
	
	// render entire image
	if (mRegionX == -1) {
		mRegionX = 0; mRegionY = 0; 
		optix->getRes( mRegionW, mRegionH );
 
		//downsample = (mSample==0) ? 2 : 1;		// <-- half res on animate / camera motion
	}

	PERF_PUSH ( "OptixRender" );

	optix->Render ( downsample, mRegionX, mRegionY, mRegionW, mRegionH );

	PERF_POP ();

	t2 = clock(); 
	float elapsed = (double)(t2-t1)*1000.0f / CLOCKS_PER_SEC;

	if (mSample == 0) {
		m_ttotal = 0;	m_tave = 0;	m_tsumsq = 0;
	}
	m_ttotal = m_ttotal + elapsed;
	m_tave = m_ttotal / (mSample+1);
	float stdiff = elapsed - m_tave;
	m_tsumsq += stdiff * stdiff ;
	float stdev = sqrt(m_tsumsq / (mSample+1));		// Sdev = sqrt(1/N * SUM_N( (xi - xave)^2 ))

	// Step 7. Read back output texture & ray count
	PERF_PUSH ( "Readback" );
	int gltex = getOutputTex();
	optix->ReadOutputTex ( gltex );
	PERF_POP();

	float mrays = optix->CountRays() / 1000000.0f;
	float mraysec = mrays * 1000.0f / elapsed;
	int xr, yr;
	optix->getRes(xr, yr);
	xr /= downsample;
	yr /= downsample;
	float raypix = (mrays*1000000.0f) / (xr*yr);
	
	if ( mSample <= mMaxSamples ) {
  	dbgprintf("Raytrace. Time: %.2fs, Frame: %.2f ms, %.2f ave, %.2f s.dv, Res:%dx%d, Samples %d/%d, MRays %.2f, MRays/sec %.2f, Rays/pix %.2f\n", m_ttotal/1000.0f, elapsed, m_tave, stdev, xr, yr, mSample, mMaxSamples, mrays, mraysec, raypix);
  }

	/* if (mSample == mMaxSamples) {
		
		// Increment DT of Displace Meshes
		Object *obj, *mtl;
		for (int n = 0; n < gAssets.getNumObj(); n++) {
			obj = gAssets.getObj(n);
			if (obj != 0x0) {
				if (obj->getType()=='tfrm' && obj->isVisible()) {
					// get material on mesh
					mtl = dynamic_cast<::Material*> ( obj->getInput ( "material" ) );
					if (mtl != 0x0 ) {
						Vec4F disp = mtl->getParamV4(M_DISPLACE_AMT);
						disp.y -= 0.0005;
						disp.z -= 0.0005;
						dbgprintf ("DT: %f, %s\n", disp.z, obj->getName().c_str() );
						mtl->SetParamV4(M_DISPLACE_AMT, 0, disp );
						obj->MarkDirty ();
						mtl->MarkDirty ();
					}
				}
			}
		}		
		RestartRender ();		
	} */
	

	//dbgprintf ( "Raytrace. Time: %f msec\n", elapsed );

	// Update sample convergence
	// - continue to accumulate samples beyond max if frame is paused
	if ( ++mSample >= mMaxSamples ) {
		return true;		
	}
	return false;		// frame not ready
}

void RenderOptiX::StartNewFrame() 
{
	// Frame has advanced. Reset sample convergence
	mSample = 0;
}


int RenderOptiX::CountRays ()
{
	// Read back pixels
	Scene* scn = getRenderMgr()->getScene ();
	Vec3I res = scn->getRes();
	int size_bytes = res.x * res.y * 3 * sizeof(ushort);
    unsigned char* pixbuf = (unsigned char*) malloc( size_bytes );

	int gltex = getOutputTex();		// input is GL_RGBA32F

	glGetTextureImage ( gltex, 0, GL_RGB, GL_UNSIGNED_SHORT, size_bytes, (void*) pixbuf );		// output is GL_RGB
 
	ImageX img;
  img.Create ( res.x, res.y, ImageOp::RGB8 );	
  img.TransferData ( (char*) pixbuf );

	char* pix = (char*) img.GetData();
	int cnt = 0;
	for (int y=0; y < res.y; y++)
		for (int x=0; x < res.x; x++) {
			cnt += *pix;
			pix += 3;
		}

	dbgprintf ( "# Rays: %d\n", cnt);

	return cnt;
}

bool RenderOptiX::SaveFrame ( char* filename )
{
    // Read back pixels
	Scene* scn = getRenderMgr()->getScene ();
	Vec3I res = scn->getRes();
	int size_bytes = res.x * res.y * 3 * sizeof(ushort);
    unsigned char* pixbuf = (unsigned char*) malloc( size_bytes );

	int gltex = getOutputTex();		// input is GL_RGBA32F

	glGetTextureImage ( gltex, 0, GL_RGB, GL_UNSIGNED_SHORT, size_bytes, (void*) pixbuf );		// output is GL_RGB
 
	ImageX img;
  img.Create ( res.x, res.y, ImageOp::RGB16 );		// 16-bit/chan = 48-bit/pixel image
  img.TransferData ( (char*) pixbuf );

	bool ret = img.Save ( filename );		

  free(pixbuf);
    //free(buf);	

	glBindFramebuffer ( GL_FRAMEBUFFER, 0 ); 

	return ret;
}

void RenderOptiX::UpdateRes ( int w, int h, int MSAA )
{
	optix->ResizeOutput ( w, h );	
}
void RenderOptiX::UpdateCamera ()
{
	mSample = 0;

	m_ttotal = 0;
	m_tave = 0;	
	m_tsumsq = 0;

	SetRegion();		// update all pixels
}

#endif
