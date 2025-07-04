


#include "timex.h"
#include "scene.h"
#include "main.h"				// for logf/dbgprintf
#include "object.h"
#include "shapes.h"
#include "material.h"
#include "image.h"
#include "transform.h"


#include "mesh.h"			// for mesh marking

Scene* gScene = 0x0;

Scene::Scene()
{
	gScene = this;		// singleton
	m_Time = 0;
		
	m_rand.seed(678);
	
	m_seed = 1;
}

bool Scene::Validate ()
{
	for (int n=0; n < mSceneList.size(); n++ ) {
		Object* obj = gAssets.getObj ( mSceneList[n] );
		/*if ( obj->Validate () == false ) {
			dbgprintf ( "ERROR: Behavior %s validate failed.\n", obj->getName().c_str() );
			return false;
		}*/
	}
	return true;
}

void Scene::Clear()
{
	// scene does not own the objects. deleted by assets manager
	mSceneList.clear();
}

void Scene::ShowAssets ()
{
	Object* obj;
	int vis = 0;

	for (int n=0; n < gAssets.getNumObj(); n++) {
		obj = gAssets.getObj(n);
		if ( obj != 0x0 ) {
			if (obj->isLoaded() && !obj->isVisible() ) {
				if ( obj->getType()=='Amsh' ) {
					//obj->SetVisible ( true );
					//sxform = obj->getLocalXform();
					//sxform->pos.Set ( -100 + int(vis / 10)*10, 0, -100 + int(vis % 10)*10 );			
					mSceneList.push_back ( obj->getID() );
					vis++;
				}
			}
		}
	}
}

// Add object to the scene
int Scene::AddObject ( Object* obj )
{	
	//dbgprintf ( "add: %s\n", obj->getName().c_str() );
	if ( obj == 0x0 ) {
		dbgprintf ( "ERROR: AddObject is null.\n" );
		exit (-7);
	}
	obj->SetVisible ( true );
	mSceneList.push_back ( obj->getID() );
	return (int) mSceneList.size() -1 ;
}

// Create object, outputs and add to scene
Object* Scene::CreateObject ( objType otype, std::string name )
{	
	// scene does not own objects. ask asset manager to allocate it		
	Object* obj = gAssets.AddObject ( otype, name );

	dbgprintf ( "  Created: %s, %s, %p\n", name.c_str(), obj->getTypeStr().c_str(), obj );

	// define behavior to create input slots & defaults
	obj->Define ( mRes.x, mRes.y );	

	// default geometry & shaders
	if ( obj->getInput("shader")==0x0 )
		obj->SetInput ("shader", "shade_mesh");	// default shader for this object

	obj->MarkDirty();
	
	// add to scene list
	AddObject ( obj );

	return obj;
}

void Scene::getTimeObjects( std::vector<Object*>& objlist)
{
	Object* obj;
	objlist.clear ();
	
	for (int n=0; n < mSceneList.size(); n++ ) {
		obj = gAssets.getObj(mSceneList[n]);
		if ( obj!=0x0 && obj->isVisible () && obj->mTimeRange.y !=0 )
			objlist.push_back ( obj );
	}
}

Object* Scene::FindInScene (std::string name)
{
	Object* obj;
	for (int n = 0; n < mSceneList.size(); n++) {
		obj = gAssets.getObj(mSceneList[n]);
		if (obj!=0x0 && name.compare(obj->mName) == 0 )
			return obj;
	}
	return 0x0;
}
int Scene::FindInScene (Object* obj)
{
	Object* scnobj;
	for (int n = 0; n < mSceneList.size(); n++) {
		scnobj = gAssets.getObj(mSceneList[n]);
		if (obj == scnobj)
			return n;
	}
	return OBJ_NULL;
}
int Scene::FindInScene ( int obj_id )
{
	for (int n = 0; n < mSceneList.size(); n++) 
		if (obj_id == mSceneList[n])
			return n;
	return OBJ_NULL;
}

Object* Scene::FindByType ( objType obj_type )
{
	Object* obj;
	for (int n=0; n < mSceneList.size(); n++) {
		obj = gAssets.getObj ( mSceneList[n] );
		if ( obj->getType() == obj_type ) 
			return obj;
	}
	return 0x0;
}

void Scene::MarkScene ( char m )
{		
	for (int n=0; n < mSceneList.size(); n++) {
		Object* obj = gAssets.getObj ( mSceneList[n] );
		if (obj != 0x0) obj->Mark ( m, true );
	}
}

void Scene::MarkSceneByType  ( objType otype, char m ) 
{	
	Object *obj;
	Shapes* shp;
	Shape* s;
	Mesh *mesh;

	for (int n=0; n < mSceneList.size(); n++ ) {
		obj = gAssets.getObj( mSceneList[n] );
		if ( obj !=0x0 ) {
			if (obj->getType()==otype ) obj->Mark ( m, true );

			if (obj->getType() == 'Ashp') {
				// scan shape groups for assets to mark
				shp = dynamic_cast<Shapes*> (obj);
				for (int i = 0; i < shp->getNumShapes(); i++) {
					s = shp->getShape(i);
					if (s->meshids.x != MESH_NULL) {
						mesh = dynamic_cast<Mesh*> ( gAssets.getObj ( s->meshids.x ) );
						if (mesh != 0x0 ) mesh->Mark(m , true );
					}
				}				
			}			
		}
	}
}
void Scene::Stats ()
{
	Object* obj;
	Vec3F stats;
	Vec3F total;
	
	dbgprintf ( "------ STATS\n");
	total.Set(0,0,0);
	for (int n = 0; n < mSceneList.size(); n++) {
		obj = gAssets.getObj(mSceneList[n]);
		if (obj != 0x0 && !obj->isAsset() ) {
			stats = obj->getStats();
			total += stats;
			dbgprintf ( "%s: obj %lld, inst %lld, faces %lld\n", obj->getName().c_str(), (uint64_t) stats.x, (uint64_t) stats.y, (uint64_t) stats.z );			
		}
	};
	dbgprintf ( "TOTAL: obj %lld, inst %lld, faces %lld\n", (uint64_t) total.x, (uint64_t) total.y, (uint64_t) total.z );			
}

void Scene::Execute (bool bRun, float time, float dt, bool dbg_eval )
{
	TimeX clk1, clk2;
	Object* obj;
	
	if (dbg_eval) {
		PERF_PUSH ( "EXEC" ); 
		dbgprintf ( "  EXEC\n" );
		clk1.SetTimeNSec();
	}
	
	// Run (advance time)
	if (bRun) {
		if (time >= 0) m_Time = time;

		if (dbg_eval) {
			PERF_PUSH( "RUN" );
			dbgprintf( "    RUN(t=%f): ", m_Time);
		}		

		// Mark time objects dirty		
		for (int n = 0; n < mSceneList.size(); n++) {
			obj = gAssets.getObj(mSceneList[n]);
			if (obj != 0x0 && !obj->isAsset() && obj->hasInputTime()) {
				if (time >= obj->mTimeRange.x && time <= obj->mTimeRange.y ) {
					if (dbg_eval) dbgprintf ("  %s ", obj->getName().c_str());
					obj->MarkDirty();
				} else {
					obj->MarkClean();
				}
			}
		}
		if (dbg_eval) {
			PERF_POP();
			dbgprintf("\n");
		}
	}

	// Execute
	// run nodes until all are no longer dirty, or..
	// the number of dirty nodes is not reducing further.
	int num_dirtylast = -1;
	int num_dirty =  1000000;
	char msg[256];
	while (num_dirty > 0 && num_dirty != num_dirtylast) {
		num_dirtylast = num_dirty;
		num_dirty = 0;
		for (int n = 0; n < mSceneList.size(); n++) {
			obj = gAssets.getObj(mSceneList[n]);
			if (obj != 0x0 && !obj->isAsset() && obj->isDirty()) {
				if (dbg_eval) {
					sprintf ( msg, "Exec:%s", obj->getName().c_str() );
					dbgprintf("    %s\n", msg );
					PERF_PUSH ( msg );
				}
				obj->Run (time);
				num_dirty++;
				if (dbg_eval) {
					PERF_POP();
				}
			}
		}
	}
	
	// elapsed time
	if (dbg_eval) {	
		clk2.SetTimeNSec();
		float t = clk2.GetElapsedMSec(clk1);
		PERF_POP();
		dbgprintf( "    %f msec\n", t);
	}
}
void Scene::SetVisible(std::string name, bool v)
{
	Object* obj = gAssets.getObj(name);
	if (obj == 0x0) return;
	obj->SetVisible(v);
}

bool Scene::Select3D (int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir )
{
	m_Sel = sel;
	Object* obj = gAssets.getObj( sel.x );						// selected object
	if (obj == 0x0) { m_Sel.Set(-1,-1,-1,-1); return false; }

	AssignShapeToWidget3D(0, w, Vec3I(-1, -1, -1));			// assume no selection	

	return obj->Select3D ( w, sel, name, orig, dir );			// sub-selection	
}

void Scene::Adjust3D (int w)
{	
	Object* obj = gAssets.getObj( m_Sel.x );
	if (obj != 0x0 )
		obj->Adjust3D( w, m_Sel );
}

// Reset - Does not clear the graph, it removes all outputs.
/*void Scene::Reset ()
{
	for (int n = 0; n < mStartList.size(); n++) {
		obj = gAssets.getObj(mStartList[n]);

		if (obj != 0x0 && !obj->isAsset()) {
		
		}
	}
}*/


int	Scene::ClearObject ( Object* obj )
{	
	int outid = OBJ_NULL;
	Object* outobj = obj->ClearOutput();	// clear output references	 
	if ( outobj != 0x0 ) {
		outid = outobj->getID();
		DeleteFromScene ( outobj );			// delete output on object (does not delete obj)	
	}
	return outid;
}

void Scene::DeleteFromScene ( Object* obj )
{
	int id_del = obj->getID();
	int scnid = FindInScene ( id_del );
	if (scnid != OBJ_NULL )
		RemoveFromScene ( scnid );

	gAssets.DeleteObject ( obj );
}

void Scene::RemoveFromScene (int id)
{
	mSceneList.erase(mSceneList.begin() + id);
}

void Scene::RegenerateScene( bool bnew )
{
	RegenerateSubgraph( mSceneList, bnew );
}

void Scene::RegenerateSubgraph(std::vector<objID>& objlist, bool bnew )
{
	Object* obj;
	std::vector<Object*> objptrs;
	for (int n=0; n < objlist.size(); n++) {
		if (objlist[n] != OBJ_NULL) {
			obj = gAssets.getObj(objlist[n]);
			if ( !obj->isAsset() )
				objptrs.push_back ( obj );
		}
	}
	
	if ( bnew ) {
		//m_seed = m_rand.randI();		// randomize subgraph
		m_seed++;
	}

	RegenerateSubgraph ( objptrs, m_seed );
}

void Scene::RebuildSubgraph ( std::vector<Object*>& objlist, int seed )
{
	int complete = 0;

	RegenerateSubgraph(objlist, seed, false);					// generate

	while ( complete != objlist.size() ) {
		complete = RegenerateSubgraph(objlist, seed, true);		// run until done
	}
}


int Scene::RegenerateSubgraph( std::vector<Object*>& objlist, int seed, bool bRun )
{
	int pid;
	Object* obj;
	int complete = 0;

	for (int n = 0; n < objlist.size(); n++) {
		obj = objlist[n];		
		if (obj != 0x0 && !obj->isAsset()) {

			obj->MarkDirty();
			pid = obj->getParamByName("seed");		// Randomize object
			if (pid != -1) obj->SetParamI(pid, 0, seed + n );

			if ( bRun) {
				obj->Run(m_Time);						// Run subgraph
			} else {				
				ClearObject(obj);						// Clear outputs
				obj->MarkIncomplete();					// Start incomplete
				obj->Generate(mRes.x, mRes.y);			// Generate output(s)
				obj->Run(m_Time);						// Run once
				AddOutputToScene( obj );				// Add to output
			}
			if ( obj->isComplete() ) complete++;
		}
	}
	return complete;
}

void Scene::Generate (int x, int y)
{
	Object* obj;
	
	// Mark entire scene as dirty
	MarkScene(MARK_DIRTY);

	// Generate entire scene
	std::vector<objID> mStartList = mSceneList;		// start list = scene graph before generation
	for (int n = 0; n < mStartList.size(); n++) {
		obj = gAssets.getObj ( mStartList[n] );		
		if (obj != 0x0 ) {			
			ClearObject ( obj );					// Clear outputs
			obj->Generate (x, y);					// Generate output(s)
			if (!obj->isAsset())
				AddOutputToScene ( obj );
		}
	}
}

void Scene::AddOutputToScene ( Object* obj ) 
{
	if (!obj->isVisible()) return;

	// Add output to the scene graph  
	Object* output;
	output = obj->getOutput();				// name of shape is set object Start -> CreateOutput
	if (output != 0x0) {
		output->CopyLocalXform( obj->getLocalXform());
		if (FindInScene(output) == OBJ_NULL ) {
			AddObject(output);				// objects added to final scene graph
			output->SetVisible( true );		// object visible, so output visible
		}
	}
}

#include "lightset.h"
#include "motioncycles.h"
#include "curve.h"

Globals* Scene::getGlobals()
{
	Globals* obj = dynamic_cast<Globals*>( FindByType ( 'glbs' ) );
	if ( obj==0x0 ) { return 0x0; }	
	return obj;
}
Camera* Scene::getCameraObj ()
{
	Camera* camera = dynamic_cast<Camera*>( FindByType ( 'camr' ) );
	if ( camera==0x0 ) { dbgprintf ( "ERROR: No camera found.\n" ); }	
	return camera;
}
Camera3D* Scene::getCamera3D()
{
	Camera* camera = dynamic_cast<Camera*>(FindByType('camr'));
	if (camera == 0x0) { return 0x0; }
	return camera->getCam3D();
}


int Scene::ParseArgs ( std::string value, std::vector<std::string>& args)
{
	// change commas inside vectors to semi-colons
	int sub = 0;
	for (int n=0; n < value.length(); n++) {
		if (value.at(n)=='<') sub++;
		if (value.at(n)=='>') sub--;
		if (value.at(n)==',' && sub != 0) value.at(n) = ';' ;
	}

	// parse arguments
	args.clear ();
	std::string arg, rest;
	while ( strSplitLeft( value, "," , arg, rest ) ) {
		args.push_back ( strTrim(arg) );
		value = rest;
	}
	arg = value;
	args.push_back ( strTrim(arg) );
	return args.size();
}

void Scene::SaveScene ( std::string fname )
{
	// Append 'saved'
	std::string asset_path, filepath;
	std::string path, name, ext, tail;
	getFileParts ( fname, path, name, ext);	
	tail = (name.size() <=6) ? "" : name.substr( name.size()-6 );
	if (tail.compare("_saved") != 0)  
		name = name + "_saved";
	if ( path.length()==0) path = ASSET_PATH;
	filepath = path + name + "." + ext;

	// Open for saving
	FILE* fp = fopen ( filepath.c_str(), "wt" );

	// Traverse scene graph and save
	Object* obj;
	for (int n = 0; n < mSceneList.size(); n++) {
		obj = gAssets.getObj(mSceneList[n]);
		if (obj != 0x0 ) {			
			bool behav = !obj->isAsset();														// save behaviors (not assets)			
			if ( behav ) 
				gAssets.SaveObject ( obj, fp );
		}
	}	
	fclose(fp);	

	// Save edited materials
	// -- find active paint tool
	Object* paint = 0x0;
	for (int n = 0; n < mSceneList.size(); n++) {
		obj = gAssets.getObj(mSceneList[n]);
		if (!obj->isAsset() && obj->getName().find("PaintTool") == 0) paint = obj;		// find any paint tool
	}	
	if (paint != 0x0 ) {
		Material* mtl = (Material*) paint->getInput("material");
		if (mtl==0x0) dbgprintf ( "ERROR: No material on paint tool.\n");		
		Image* img_depth =	(Image*) mtl->getInput("displace");		
		Image* img_clr =		(Image*) mtl->getInput("texture");	
		img_depth->Save ("saved_disp.tif");
		img_clr->Save   ("saved_color.jpg");
	}

	dbgprintf ( "Saved.. %s\n", filepath.c_str() );
}



void Scene::CreateSceneDefaults ()
{
	Object* obj;

	// Create global
	if (FindByType('glbs') == 0x0) {
		obj = CreateObject('glbs', "Globals");
		obj->SetInput("envmap", "env_sky");
		obj->SetParamV4(G_ENVCLR, 0, Vec4F(1, 1, 1, 1));
	}

	// Create default materials
	if (FindByType('Amtl') == 0x0) {
		obj = CreateObject('Amtl', "BasicMtl");
	}

	// Create default light(s)
	if (FindByType('Algt') == 0x0) {
		obj = CreateObject('lgts', "Lights");
	}

	// Create default cameras
	if (FindByType('camr') == 0x0) {
		obj = CreateObject('camr', "Camera");
		obj->SetParamF(C_FOV, 0, 40.0);
	}

	// Create default object
	obj = CreateObject('tfrm', "Plane");
	obj->SetInput("material", "BasicMtl");
	obj->SetInput("mesh", "model_plane" );
	((Transform*) obj)->SetXform ( Vec3F(0,0,0), Vec3F(0,0,0), Vec3F(100,1,100) );
}

#ifdef BUILD_GLTF

  #define TINYGLTF_IMPLEMENTATION
  #define STB_IMAGE_IMPLEMENTATION
  #define STB_IMAGE_WRITE_IMPLEMENTATION

	#include "tiny_gltf.h"


	int getGLTFAccessor ( tinygltf::Mesh& mesh, std::string attr, int& mode )
	{
		// Mesh can have multiple primitives
		for (const auto& primitive : mesh.primitives) {			
			auto it = primitive.attributes.find(attr);
			if (it != primitive.attributes.end()) {
				mode = primitive.mode;
				return it->second;  // Return the accessor index
			}
		}
		return -1;
	}

	bool getGLTFBuffer ( tinygltf::Model& model, int access, uchar*& buf, int& len, int& cnt, int& csz, Vec3F& bmin, Vec3F& bmax )
	{	
		if (access < 0) return false;
		tinygltf::Accessor& acc = model.accessors[access];
		tinygltf::BufferView& bv = model.bufferViews[ acc.bufferView ];		
		tinygltf::Buffer& gltf_buf = model.buffers[ bv.buffer ];
		buf = (uchar*) gltf_buf.data.data() + bv.byteOffset + acc.byteOffset;

		if (acc.minValues.size() == 3) {
			bmin.Set( acc.minValues[0], acc.minValues[1], acc.minValues[2]);
		}
		if (acc.maxValues.size() == 3) {
			bmax.Set(acc.maxValues[0], acc.maxValues[1], acc.maxValues[2]);
		}

		int ccnt, stride;
		switch (acc.componentType) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:			csz = sizeof(char);	break;
		case TINYGLTF_COMPONENT_TYPE_BYTE:							csz = sizeof(char); break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:		csz = sizeof(short); break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:							csz = sizeof(short); break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:			csz = sizeof(int); break;
		case TINYGLTF_COMPONENT_TYPE_INT:								csz = sizeof(int); break;		
		case TINYGLTF_COMPONENT_TYPE_FLOAT:							csz = sizeof(float); break;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:						csz = sizeof(double); break;
		default: return false;
		}
		switch (acc.type) {
		case TINYGLTF_TYPE_SCALAR:		ccnt = 1; break;
		case TINYGLTF_TYPE_VEC2:			ccnt = 2; break;
		case TINYGLTF_TYPE_VEC3:			ccnt = 3; break;
		case TINYGLTF_TYPE_VEC4:			ccnt = 4; break;
		default: return false;
		}

		stride = csz * ccnt;				// stride = component size * # components  (e.g. Vec3F, clen=4 (float), ccnt=3 )
		len = stride * acc.count;		// len = stride * number of elements
		cnt = acc.count;
		return true;
	}

	Matrix4F getGLTFLocalTransform (const tinygltf::Node& node) 
	{
		// Construct from TRS (Translation, Rotation, Scale)
		Vec3F pos (0,0,0);		
		Vec3F scal (1,1,1);
		Quaternion rot;
		if (!node.translation.empty()) 	pos.Set ( node.translation[0], node.translation[1], node.translation[2] );
		if (!node.rotation.empty())			rot.set ( node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
		if (!node.scale.empty())				scal.Set ( node.scale[0], node.scale[1], node.scale[2] );

		Matrix4F mat;
		mat.TRST ( pos, rot, scal, Vec3F(0,0,0) );
		return mat;
	} 


	// Computes world transform
	Matrix4F getGLTFWorldTransform(const tinygltf::Model& model, int nodeIndex)
	{
		const tinygltf::Node& node = model.nodes[nodeIndex];
		Matrix4F local = getGLTFLocalTransform(node);

		// Check if this is a root node
		for (size_t i = 0; i < model.scenes.size(); ++i) {
			for (int root : model.scenes[i].nodes) {
				if (root == nodeIndex) {
					Matrix4F s;
					s.Scale ( 10,10,10);
					return s * local;			 // Root node: no parents
				}
			}
		}
		// Find parent (slow: linear scan)
		for (size_t i = 0; i < model.nodes.size(); ++i) {
			const auto& potentialParent = model.nodes[i];
			for (int child : potentialParent.children) {
				if (child == nodeIndex) {					
					return getGLTFWorldTransform(model, i) * local;
				}
			}
		}
		return local;			// If no parent found
	}

	bool Scene::LoadGLTF ( std::string fname, int w, int h )
	{
		bool debug_gltf = true;

		std::string filepath;
		if (!getFileLocation(fname, filepath)) {
			dbgprintf("ERROR: Unable to find GLTF file %s\n", fname.c_str());
			exit(-17);
			return false;
		}
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		std::string name, mesh_name;
		uchar* buf; 
		int len, cnt, csz;
		int verts=0, tris=0;
		Vec3F pos, scal, bmin, bmax;
		Quaternion rot;
		
		// supported mesh accessors
		std::string mesh_access_name[5] = {"NORMAL", "POSITION", "UV"};
		int mesh_access[5], mesh_indices;
		Object* obj;
		int mode;

		// Load GLTF
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath.c_str());
		
		if (!warn.empty())	{ dbgprintf("GLTF Warning: %s\n", warn.c_str()); 	}
		if (!err.empty())		{ dbgprintf("GLTF Error: %s\n", err.c_str());		}
		if (!ret)	{	
			dbgprintf("GLTF Failed to load: %s\n", fname.c_str() ); 
			return false; 
		}
		dbgprintf( "GLTF Loading.. %s\n", fname.c_str());
	
		// Load mesh assets
		//
		for (int m = 0; m < model.meshes.size(); m++) {

			// Get mesh accessors
			mesh_name = model.meshes[m].name;
			mesh_access[0] = getGLTFAccessor( model.meshes[m], "POSITION", mode );
			mesh_access[1] = getGLTFAccessor( model.meshes[m], "NORMAL", mode);
			mesh_access[2] = getGLTFAccessor( model.meshes[m], "UV", mode);
			mesh_access[3] = -1;
			mesh_access[4] = -1;
			mesh_indices = model.meshes[m].primitives[0].indices;		// triangle info

			// Get mesh type
			if (mode != TINYGLTF_MODE_TRIANGLES) {
				dbgprintf ( "ERROR: Shapes GLTF loader only supports GL_TRIANGLES for now.\n");
				exit(-3);
			}
		
			// Add mesh	asset
			obj = gAssets.AddObject('Amsh', mesh_name );
			MeshX* mesh = dynamic_cast<MeshX*>( obj );
			
			mesh->CreateFV();
	
			// add vertex positions
			if (mesh_access[0] >=0 ) {				
				getGLTFBuffer ( model, mesh_access[0], buf, len, cnt, csz, bmin, bmax);
				verts = cnt;
				mesh->MemcpyBuffer (BVERTPOS, buf, len, verts );
				for (int v=0; v < verts; v++) {
					pos = *mesh->GetVertPos(v);
					mesh->SetVertPos ( v, pos );
					if (pos < bmin || pos > bmax) {
						dbgprintf ( "GLTF Read ERROR: Pos buffer corrupt. Pos outside bounds.\n");
					}
				}						
			}

			// add vertex normals
			if (mesh_access[1] >= 0) {
				getGLTFBuffer(model, mesh_access[1], buf, len, cnt, csz, bmin, bmax);
				mesh->AddBuffer (BVERTNORM, "norm", sizeof(Vec3F), 1);
				mesh->MemcpyBuffer (BVERTNORM, buf, len, cnt);
			}						

			// add face indices
			if (mesh_indices >= 0) {
				getGLTFBuffer(model, mesh_indices, buf, len, cnt, csz, bmin, bmax);
				if (csz != sizeof(xref)) {
					tris = cnt / 3;
					if (csz==2) {
						// repack unsigned short into int		
						mesh->ExpandBuffer ( BFACEV3, tris );						
						uint16_t* indices = reinterpret_cast<uint16_t*>(buf);
						for (int i=0; i < tris; i++ ) {							
							mesh->SetFace3 (i, indices[3*i], indices[3*i+1], indices[3*i+2] );							
						}
						mesh->ReserveBuffer ( BFACEV3, tris );
					} 
				} else {
					mesh->MemcpyBuffer (BFACEV3, buf, len, cnt);
					tris = cnt;
				}				
			}
	
			dbgprintf("  Loaded: asset %d = %s (Amsh), vert %d, tri %d\n", obj->getID(), name.c_str(), verts, tris );
		}


		// Create defaults for required objects if not found
    //  (globals, camera, lights)

		CreateSceneDefaults ();
 

		// Load nodes (mesh instances / transforms)
		for (int n = 0; n < model.nodes.size(); n++) {

			name = model.nodes[n].name;
			tinygltf::Node& node = model.nodes[n];
			int mesh_id = node.mesh;	
			
			if ( mesh_id >= 0 ) {

				mesh_name = model.meshes[ mesh_id ].name;
				
			/* if (name.compare("pelvis_mesh")==0 || name.compare("torso_link_mesh") ==0  ||
						name.compare("pelvis_mesh.001")==0 || name.compare("right_shoulder_pitch_link_mesh") == 0
			   ) { */			  

				Transform* xform = dynamic_cast<Transform*>(CreateObject('tfrm', "T_" + name));		
				xform->SetInput("material", "BasicMtl");
				xform->SetInput("mesh", mesh_name);	

				/*pos.Set(0,0,0);
				scal.Set(1,1,1);
				rot.Identity();
				if (node.translation.size()==3) pos.Set ( node.translation[0], node.translation[1], node.translation[2] );
				if (node.scale.size()==3)				scal.Set( node.scale[0], node.scale[1], node.scale[2]);
				if (node.rotation.size()==4)		rot.set ( node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3] );
				xform->SetXform ( pos, rot, scal );  */

			if (name.compare("right_shoulder_pitch_link_mesh") == 0) {
				bool stop=true;
			}

				Matrix4F mtx = getGLTFWorldTransform(model, n);				
				mtx.ReverseTRS ( pos, rot, scal );
				xform->SetXform ( pos, rot, scal );
	
			
				// }
			
      }

		}		
		
		return true;
	}

#endif

bool Scene::LoadScene ( std::string fname, int w, int h )
{
	char buf[1024];
	std::string filepath, lin;
	std::string objtype, objname, cmd, value;
	std::string inpt;
	std::vector<std::string> args;
	Object* obj = 0x0;
	objType ot;

	if ( !getFileLocation ( fname, filepath ) ) {
		dbgprintf ( "ERROR: Unable to find scene %s\n", fname.c_str());
		exit(-17);
		return false;
	}
	
	ShowAssets();

	setRes(w, h);
	
	FILE* fp = fopen ( filepath.c_str(), "rt" );

	int lnum = 0;
	bool rem_obj = false;
	
	while (!feof(fp)) {

		fgets(buf, 1024, fp);		// read parts
		lnum++;
		lin = strLTrim(buf);		

		if ( strSplitLeft( lin, ":", cmd, value ) && obj != 0x0 ) {		// all sub-commands have a colon ':'

			if ( lin[0]=='#' || rem_obj ) continue;

			// Object sub-commands	
			cmd	= strTrim(cmd);

			if ( cmd.compare ("input")==0 ) {							// input: {input_semantic} = {value}
				if ( strSplitLeft ( value, "=", inpt, value ) ) {	
					inpt = strTrim(inpt);
					value = strTrim(value);										
					
					// Set input					
					obj->SetInput ( inpt, value );	

				} else {
					dbgprintf ( "LINE %d: Input command not in proper form. %s\n", lnum, lin.c_str() );
					exit(-2);
				}

			} else if ( cmd.compare("visible")==0) {					// visible: {true/false}
				value = strTrim(value);
				bool vis = true;
				if (value.compare("false")==0) vis=false;
				obj->SetVisible ( vis );

			} else if ( cmd.compare("pos")==0 ) {						// pos: <x,y,z>
				ParseArgs(value, args);
				obj->SetPos ( strToVec3(args[0], ';') );

			} else if (cmd.compare("xform") == 0) {						// xform: <x,y,z>, <sx,sy,sz>, <tx,ty,tz>
				ParseArgs(value, args);
				Vec3F p, s, r;				
				p = strToVec3(args[0], ';');
				s = strToVec3(args[1], ';');
				if (args.size() >= 3 ) {
					r = strToVec3(args[2], ';');
				}
				Quaternion q;
				q.fromEuler ( r );
				obj->SetTransform ( p, s, r );

			} else if ( cmd.compare("param")==0) {						// param: {name}, {value}
				ParseArgs(value, args);
				for (int k=0; k < args.size()-1; k++) 
					obj->SetParam ( args[0], args[k+1], ';', k );

			} else if ( cmd.compare("time")==0) {						// time: <start, end, 0>
				ParseArgs(value, args);
				obj->SetTimeRange ( strToVec3(args[0], ';') );
			} else {													// other command
				ParseArgs ( value, args );
				if ( !obj->RunCommand ( cmd, args ) ) {
					dbgprintf ( "LINE %d: Command not found '%s'\n", lnum, cmd.c_str() );
					exit(-7);
				}
			}
		} else {

			// New object
			if ( strParseOutDelim( lin, "[", "]", objtype, objname ) ) {

				rem_obj = false;
				if ( lin[0] == '#' ) {rem_obj=true; continue;}

				args.clear();
				if (obj !=0x0) obj->RunCommand("finish", args);	// finish previous object

				// New object
				objtype = strTrim(objtype);
				objname = strTrim(objname);
				ot = gAssets.getObjTypeFromName ( objtype );			
				if ( ot=='null' ) dbgprintf("LINE %d: WARNING: Object type not found, %s\n", lnum, objtype.c_str());
				if ( ot=='null' || lin[0]=='#' ) {				// # - means commented out	
					obj = 0x0;
				} else {			
					obj = CreateObject( ot, objname );		// Create new object
				}
			}
		}
	}
	args.clear();
	if (obj != 0x0) obj->RunCommand("finish", args); // finish previous object

	// Set Globals
	if ( FindByType ( 'glbs' ) == 0x0) {
		dbgprintf ( "ERROR: No globals.\n" );
		exit(-5);
	}
	fclose (fp);

	return true;
}


bool Scene::Load(std::string fname, int w, int h)
{
	// Load scene dispatcher

	bool ret = false;
	std::string path, name, ext;
	getFileParts(fname, path, name, ext);

	if (ext.compare("txt") == 0) {
		ret = LoadScene(fname, w, h);
	}
	if (ext.compare("gltf") == 0) {
#ifdef BUILD_GLTF
		ret = LoadGLTF(fname, w, h);
#else
		dbgprintf("ERROR. Unable to load %s. Must build with GLTF support.\n", fname.c_str());
#endif
	}
	return ret;
}


void Scene::CreateScene (int w, int h)
{	
	LightSet* lightset;		

	ShowAssets ();

	// Set Scene resolution
	setRes ( w, h );

	// Lights		
	lightset = (LightSet*) CreateObject ( 'lgts', "Lights" );		
	//lightset->AddLight ( Vec3F(  40,  100, 100),	Vec3F(0, 0, 0),	Vec3F(.6f, .55f,  .5f),		Vec3F(0,0,0) );				// primary light (brightest)
	//lightset->AddLight ( Vec3F( -80,   80,   0),	Vec3F(0, 0, 0),	Vec3F(.5f, .6f,  .6f),		Vec3F(.95f,.8f,.8f) );		// left side
	//lightset->AddLight ( Vec3F( 200,   60,-100),	Vec3F(0, 0, 0), Vec3F(0.2f, 0.15f, 0.15f),	Vec3F(1,1,1) );				// right side

	// Camera	
	/*obj = (Camera*) CreateObject ( 'camr', "Camera" );	
	cam = getCamera();		// Camera3D object	
	cam->setFov ( 80.0f );
	cam->setOrbit ( Vec3F(-50, 20, 0), Vec3F(0, 1.2, 0), 100, 0.5 ); */

	
	#ifdef RUN_MAP
	
		Earth* earth = (Earth*) CreateObject ( 'erth', "Earth" );			// Earth		
		earth->CreateGrids ();												// create grids	
		
		#ifdef RUN_MAP_GENERATE
			earth->GenerateTiles ();		
			dbgprintf ("DONE\n" ); exit(-1);
		#endif
		#ifdef RUN_MAP_GRID
			cam->setOrbit ( Vec3F(0, 89, 0), Vec3F(1000, 0, 500), 8000, 1 );
			TileGrid* tg = earth->LoadEarthGrid  ();	
			AddObject ( tg );		// add to scene to make sketch visible			
		#endif
		#ifdef RUN_MAP_SIMPLE
			cam->setOrbit ( Vec3F(45, 40, 0), Vec3F(250, 0, 250), 1200, 1 );
			ImageX* img1 = (ImageX*) FindObject ( "treman" );
			TileGrid* tg = earth->LoadSimpleGrid ( 3, 3, Vec3F(500.f,1/15.f,500.f), 4, Vec3F(3121000, 1003000, 0), Vec3F(3123000, 1005000, 0), 1000, img1, 0x0 );
		#endif
		#ifdef RUN_MAP_SINGLE

			cam->setOrbit ( Vec3F(45, 40, 0), Vec3F(250, 0, 250), 1200, 1 );

	
			ImageX* img1 = (ImageX*) FindObject ( "hood" );
			if (img1 == 0x0) {
				dbgprintf("ERROR: Default heightmap 'hood' not found in /assets\n");
				exit(-2);
			}
			
			//-- treman
			//TileGrid* tg = earth->LoadSimpleGrid ( 1, 1, Vec3F(500.f,1/15.f,500.f), 4, Vec3F(3122000, 1004000, 0), Vec3F(3122000, 1004000, 0), 100, img1, 0x0 );

			//-- buttermilk
			//TileGrid* tg = earth->LoadSimpleGrid ( 1, 1, Vec3F(500.f,1/15.f,500.f), 4, Vec3F(3125000, 1003000, 0), Vec3F(3122000, 1004000, 0), 100, img1, 0x0 );
			
			//-- ithaca falls
			//TileGrid* tg = earth->LoadSimpleGrid ( 1, 1, Vec3F(500.f,1/15.f,500.f), 4, Vec3F(3126000, 1002000, 0), Vec3F(3122000, 1002000, 0), 100, img1, 0x0 );

			//-- upper cornell
			TileGrid* tg = earth->LoadSimpleGrid ( 1, 1, Vec3F(500.f,1/15.f,500.f), 4, Vec3F(3127000, 1002000, 0), Vec3F(3122000, 1004000, 0), 100, img1, 0x0 );

			

			//------- Ft. Myers
			//earth->LoadSimpleGrid ( 1, 1, 5, Vec3F(1631100, 774500, 0), Vec3F(1631300, 774700, 0), 100, img1, 0x0 );	// 3x3 tile grid
			//earth->LoadSimpleGrid ( 1, 1, 5, Vec3F(1631200, 774800, 0), Vec3F(1631300, 774700, 0), 100, img1, 0x0 );	
			//earth->LoadSimpleGrid ( 1, 1, 5, Vec3F(1630500, 776300, 0), Vec3F(1631300, 774700, 0), 100, img1, 0x0 );	// condos
			//earth->LoadSimpleGrid ( 1, 1, 5, Vec3F(1631100, 776800, 0), Vec3F(1631300, 774700, 0), 100, img1, 0x0 );	// beach	
			//earth->LoadSimpleGrid ( 1, 1, 5, Vec3F(1631000, 776000, 0), Vec3F(1631300, 774700, 0), 100, img1, 0x0 );	// west side pnts
			//earth->LoadSingleTile ( 0, 0, 5, Vec3F(1631100, 774500, 0  ) );			// cultivated:  5,1631200,774500
			//earth->LoadSingleTile ( 5, Vec3F(1631100, 774500, 0  ) );			// marsh:	    5,1631100,774500
			// NOTE: Scaling of tiles current done in Tile::Sketch, setOffset.. Need to move it here as SetTransform
			// mixed forst: 5,1631200,776700 to 1631300, 776800 (2x2)
			// road,ocean:  5,1630800,776600
			// forest:      5,1630900,776500
		#endif		

		lightset->AddLight ( Vec3F(500, 1000, 500), Vec3F(0, 0, 0), Vec3F(0.2f, 0.2f, 0.25f),	Vec3F(1,1,1) );				// front left (z+)*/	

	#endif

	#ifdef RUN_HEAT
		// Heat Volume
		// (3D rendered one must be the last one in scene)
		obj = CreateObject ( 'heat', "Heat"  );
		obj->SetInput ( "tex", "sea" );
		//obj->SetPos ( Vec3F(-300,0,0) );

	#endif

	#ifdef RUN_TREE

		obj = FindObject ( "square" );
		obj->SetVisible ( true );
		obj->SetTransform ( Vec3F(0,0,0), Vec3F(10000, 1, 10000));

		obj = CreateObject ( 'tgro', "tree" );	
		obj->SetPos ( Vec3F(0,0,0) ); 		

		obj = CreateObject ( 'loft', "loft" );	
		obj->SetInput ( "shapes", "tree" );		
		obj->SetPos ( Vec3F(0,0,-20) ); 	
	#endif

	#ifdef RUN_CHAR
	
		// Motion Cycles
		mcyc = (MotionCycles*) CreateObject ( 'mcyc', "MotionCycles" );		 
		mcyc->SetVisible ( false );
		mcyc->LoadTPose	("z00_tpose2.bvh");

		// turning	
		mcyc->SetGroup ( "turn" );		
		c = mcyc->LoadBVH ( "z01_01.bvh" );	
		//mcyc->SplitCycle ( "jump", c, 158, 346 );
		mcyc->SplitCycle ( "jump", c, 1377, 1680 );	
		mcyc->SplitCycle ( "turnL", c, 1946, 2110 );
		mcyc->SplitCycle ( "turnR", c, 1076, 1194 );
		c = mcyc->LoadBVH ( "z16_17.bvh" );
		mcyc->SplitCycle ( "walkturnL", c, 179, 445 );
		c = mcyc->LoadBVH ( "z16_19.bvh" );
		mcyc->SplitCycle ( "walkturnR", c, 130, 411 );
		c = mcyc->LoadBVH ( "z82_08.bvh" );
		mcyc->SplitCycle ( "stand", c, 145, 175 );

		// walking
		mcyc->SetGroup ( "walk" );
		c = mcyc->LoadBVH ( "z82_08.bvh" );	
		mcyc->SplitCycle ( "stand", c, 50, 400 );		
		mcyc->SplitCycle ( "standwalk", c, 980, 1466 );			
		c = mcyc->LoadBVH ( "z127_04.bvh" );	
		mcyc->SplitCycle ( "walkrun", c, 160, 441 );			
		c = mcyc->LoadBVH ( "z07_04.bvh" );	
		mcyc->SplitCycle ( "walkslow", c, 150, 330 );			
		c = mcyc->LoadBVH ( "z07_01.bvh" );	
		mcyc->SplitCycle ( "walk", c, 2, 268 );			
		c = mcyc->LoadBVH ( "z07_12.bvh" );
		mcyc->SplitCycle ( "walkfast", c, 86-20, 190 );	
		c = mcyc->LoadBVH ( "z09_01.bvh" );
		mcyc->SplitCycle ( "run", c, 22, 140 );			// right heel contact
		c = mcyc->LoadBVH ( "z16_08.bvh" );
		mcyc->SplitCycle ( "runstand", c, 37, 240 );		

		mcyc->RemoveAllBVH ();

		//mcyc->CompareCycles ( "walk", "run" );
		//mcyc->CompareCycles ( "run", "walk" );	
		//mcyc->CompareCycles ( "run", "rond de jambe" );
	
		mcyc->ListCycles ();

		// Characters
		char cname[256]; 	
		Curve* crv;
		Vec3F rnd;
	
		// Character		
		float grnd = 1.5f;

		chr = (Character*) CreateObject ( 'char', "human" );
		chr->Generate(0,0);							// create shapes
		chr->SetHomePos  ( Vec3F(360.f, grnd + 0.92f, 0.f ) );		
		chr->SetCycleSet ( mcyc );	
		chr->SetInput ( "tex", "edges" );
		chr->SetInput ( "mesh", "cube" );
		chr->LoadBVH ( "z00_tpose.bvh" );			// soldier march
		chr->LoadBoneInfo ( "bone_info.txt" );
		chr->SetScene ( this );		
		chr->AddMotion ( "stand",		M_TARGET , 1.0 );			// 0.1 = speed
		chr->AddMotion ( "walkslow",	M_TARGET | M_PATH, 1.0 );		
		chr->AddMotion ( "walkfast",	M_TARGET | M_PATH, 1.0 );		
		for (int n=0; n < 500; n++)
			chr->AddMotion ( "walkfast",		M_TARGET | M_PATH, 1.0 );	
		
		
		// Create Path curve					
		crv = (Curve*) CreateObject ( 'Acrv', "Path" );		// primitive (behavior) to render the asset (curve)			 
		crv->SetColor ( Vec3F(1,0,0) );		
		srand(9403);
		crv->AddKey ( Vec3F(360.f, grnd, 0.f)  );
		crv->AddKey ( Vec3F(420.f, grnd, 370.f)  );		
		crv->AddKey ( Vec3F(100.f, grnd, 360.f)  );
		
		crv->RebuildPnts();
		chr->SetInput ( "path", "Path" );			// assign path to character  
	#endif

	#ifdef RUN_BODY

		cam->setFov ( 60.0f );
		cam->setOrbit ( Vec3F(-30, 20, 0), Vec3F(0.f, 0.7f, 0.f), 6, 0.01 );

		// Ground plane
		obj = FindObject ( "square" );
		if ( obj == 0x0 ) {
			dbgprintf ( "ERROR: Cannot find square.\n" );
			exit(-1);
		}
		obj->SetVisible ( true );
		obj->SetTransform ( Vec3F(0,0,0), Vec3F(10000, 1, 10000));

		// Motion Cycles (Animations)
		mcyc = (MotionCycles*) CreateObject ( 'mcyc', "MotionCycles" );		 
		mcyc->SetVisible ( false );
		mcyc->LoadTPose ( "z00_tpose2.bvh" );

		c = mcyc->LoadBVH ( "z00_tpose2.bvh" );
		mcyc->SplitCycle ( "stand", c, 0, 60 );		// stand (t-pose)
		c = mcyc->LoadBVH ( "z01_01.bvh" );			
		mcyc->SplitCycle ( "jump", c, 1377, 1680 );	
		mcyc->SplitCycle ( "turnL", c, 1946, 2110 );
		mcyc->SplitCycle ( "turnR", c, 1076, 1194 );
		c = mcyc->LoadBVH ( "z07_04.bvh" );	
		mcyc->SplitCycle ( "walkslow", c, 150, 330 );		
		c = mcyc->LoadBVH ( "z07_12.bvh" );
		mcyc->SplitCycle ( "walkfast", c, 86-20, 190 );	// right heel contact*/
		c = mcyc->LoadBVH("z00_finger_test.bvh");
		mcyc->SplitCycle( "fingers", c, 0, 100);
		mcyc->RemoveAllBVH ();
		mcyc->ListCycles ();

		// Bones
		Parts* parts = (Parts*) CreateObject ( 'prts', "bones" );
		parts->LoadFolder ( "obj\\" );				// load all .obj meshes in this folder
		parts->Save ( "all_bones_original.txt" );
		parts->Load ( "all_bones_saved.txt", 0.2f);

		// Muscles
		Object* muscles = CreateObject('musl', "muscles");			// muscles
		muscles->Load("all_muscles_saved.txt");

		Object* loft = CreateObject('loft', "muscle_loft");		// muscle lofts
		loft->SetInput("shapes", "muscles");
		loft->SetInput("tex", "muscle_texture");

		// Character		
		chr = (Character*) CreateObject ( 'char', "Human" );			
		chr->Generate(0,0);
		chr->SetHomePos  ( Vec3F(0.f, 0.94, 0.f ) );		// this should match default hip height in z00 pose
		chr->SetInput ( "motions", "MotionCycles" );
		chr->SetInput ( "tex", "edges" );
		chr->SetInput ( "mesh", "cube" );
		chr->LoadBVH ( "z00_tpose2.bvh" );						// bones
		chr->LoadBoneInfo ( "bone_info.txt" );			
		chr->SetScene ( this );		
		chr->AddMotion ( "stand", M_BASIC | M_RESET_POS | M_RESET_ROT , 1.0f );			
		chr->SetInput ( "bones", "bones" );
		chr->SetInput( "muscles", "muscles");
		chr->BindBones ();		
		chr->BindMuscles ();

		lightset->AddLight ( Vec3F(-20, 10, 30), Vec3F(0, 0, 0),  Vec3F(0.75f, 0.7f, 0.7f),  Vec3F(1,1,1) );	
		lightset->AddLight ( Vec3F(-20, 20, -30), Vec3F(0, 0, 0), Vec3F(0.7f, 0.7f, 0.75f), Vec3F(1,1,1));
			
	#endif			
	
}
