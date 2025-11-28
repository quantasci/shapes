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

#include "object_list.h"
#include "set.h"
#include "render.h"
#include "main.h"
#include "object.h"
#include "directory.h"
#include "string_helper.h"
#include "common_defs.h"
#include "params.h"
#include "shapes.h"

// primitive objects
#include "globals.h"
#include "image.h"
#include "mesh.h"
#include "curve.h"
#include "shader.h"
#include "lightset.h"
#include "camera.h"
#include "material.h"
#include "instance.h"

// procedural objects
#include "module.h"
#include "scatter.h"
#include "deform.h"
#include "loft.h"
#include "heightfield.h"
#include "displace.h"

// characters
#include "character.h"
#include "motioncycles.h"
#include "muscles.h"

// rigid objects
#include "transform.h"
#include "parts.h"

// particle systems
#include "points.h"
#include "point_base.h"
#include "point_psys.h"

// volumes
#include "volume.h"


#include <algorithm>

ObjectList gAssets;

ObjectList::ObjectList()
{
	RegisterTypes ();
}

ObjectList::~ObjectList()
{
	Clear();
}

// Determine asset type from extension
//
objType ObjectList::getObjTypeFromExtension ( std::string ext )
{	
	// to lowercase
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

	if ( ext.compare( "jpg")==0 ) return 'Aimg';
  if ( ext.compare( "jpeg")== 0) return 'Aimg';
	if ( ext.compare( "png")==0 ) return 'Aimg';
	if ( ext.compare( "tga")==0 ) return 'Aimg';
	if ( ext.compare( "tif")==0 ) return 'Aimg';

	if ( ext.compare( "wav")==0 ) return 'Asnd';
	if ( ext.compare( "mp3")==0 ) return 'Asnd';

	if ( ext.compare( "obj")==0 ) return 'Amsh';
	if ( ext.compare( "ply")==0 ) return 'Amsh';

	if ( ext.compare( "frag.glsl")==0 ) return 'Ashd';
	//-- we only need one prefix to load them all
	//if ( ext.compare( "vert.glsl")==0 ) return 'shdr';	
	//if ( ext.compare( "geom.glsl")==0 ) return 'shdr';

	if ( ext.compare( "DIR")==0 ) return 'dir ';

	return 'null';
}


objType ObjectList::getObjTypeFromName ( std::string name )
{
	typeMap::iterator it = mTypeMap.find ( name );
	if ( it != mTypeMap.end() ) {
		return it->second;
	}
	return 'null';
}

//void ObjectList

void ObjectList::RegisterTypes ()
{
	// assets
	mTypeMap["IMAGE"] = 'Aimg';
	//mTypeMap["MESH"] = 'Amsh';
	//mTypeMap["CURVE"] = 'Acrv';
	mTypeMap["SHAPES"] = 'Ashp';	
	mTypeMap["SHADER"] = 'Ashd';		
	mTypeMap["PARAMS"] = 'Apms';
	mTypeMap["POINTS"] = 'Apnt';	
	mTypeMap["MATERIAL"] = 'Amtl';	
		
	// fundamentals	
	mTypeMap["GLOBALS"] = 'glbs';
	mTypeMap["MODULE"] = 'modl';
	mTypeMap["SCATTER"] = 'scat';	
	mTypeMap["INSTANCE"] = 'inst';
	mTypeMap["LIGHTS"] = 'lgts';
	mTypeMap["CAMERA"] = 'camr';	
	mTypeMap["CURVEGEN"] = 'cgen';
	mTypeMap["DEFORM"] = 'defm';	
	mTypeMap["BAKE"] = 'bake';

	// meshes
	mTypeMap["MESH"] = 'tfrm';			// MESH is the transform of an asset (Amsh)
	mTypeMap["LOFT"] = 'loft';	
	mTypeMap["HEIGHTFIELD"] = 'hgtf';
	mTypeMap["DISPLACE"] = 'disp';			// MESH is the transform of an asset (Amsh)

	// points
	mTypeMap["POINTCLOUD"] = 'pcld';
	mTypeMap["POINTSYS"] = 'psys';
	mTypeMap["POINTCELLS"] = 'pcel';

	// trees	
	mTypeMap["TREESYS"] = 'tsys';
	mTypeMap["TREEPARTS"] = 'tptr';

	// volumetric
	mTypeMap["VOLUME"] = 'Avol';
	mTypeMap["HEAT"] = 'heat';

	// characters
	mTypeMap["PARTS"] = 'prts';
	mTypeMap["MUSCLES"] = 'musl';
	mTypeMap["MOTION"] = 'mcyc';
	mTypeMap["CHARACTER"] = 'char';
	
	// geography	
	mTypeMap["TILEGRID"] = 'grid';
	mTypeMap["EARTH"] = 'erth';
	mTypeMap["TILE"] = 'tile';

	// tools
	mTypeMap["PAINT"] = 'Tpnt';
}


// Object Factory
//
Object* ObjectList::AddObject ( objType otype, std::string name )
{
	Object* obj = 0x0;
	std::string what, errmsg = "";	

	switch (otype) {

	// asset objects
	case 'Aimg':	obj = new Image;		break;		
	case 'Amsh':	obj = new Mesh;		break;
	case 'Acrv':	obj = new Curve;		break;
	case 'Ashp':	obj = new Shapes;		break;
	case 'Apnt':	obj = new Points;		break;	
	case 'Ashd':	obj = new Shader;		break;		
	case 'Amtl':	obj = new Material;		break;
	case 'Apms': {
		obj = new Params;
		obj->mParams = (Params*) obj;
		} break;
	
	// primitive objects
	case 'lgts':	obj = new LightSet;		break;
	case 'camr':	obj = new Camera;		break;	
	case 'tfrm':	obj = new Transform;	break;			
	case 'glbs':	obj = new Globals;		break;				
	case 'prts':	obj = new Parts;		break;

	// procedural objects
	case 'defm':	obj = new Deform;		break;
	case 'loft':	obj = new Loft;			break;
	case 'modl':	obj = new Module;		break;
	case 'scat':	obj = new Scatter;		break;
	case 'inst':	obj = new Instance;		break;
	case 'hgtf':	obj = new Heightfield;	break;
	case 'disp':	obj = new Displace;		break;
	
	// particle systems
	case 'psys':	obj = new PointPSys;	break;

	// volumes
	#ifdef BUILD_GVDB
	case 'Avol':	obj = new Volume;		break;		
	case 'heat':	obj = new Heat;			break;		
	#endif	

	// characters
	case 'char':	obj = new Character;	break;		
	case 'musl':	obj = new Muscles;		break;	
	case 'mcyc':	obj = new MotionCycles;	break;	
	

	default:
		dbgprintf("ERROR: Unknown type %s.\n", objTypeStr(otype).c_str());
		exit(-1);
		break;
	};
	
	obj->mObjID = (int) mObjList.size();		
	obj->mName = name;	
	obj->mVisible = true;
	obj->mLoaded = false;	
	obj->mRIDs.Set ( NULL_NDX, NULL_NDX, NULL_NDX );

	if ( obj == 0x0 ) {
		dbgprintf ( "ERROR: Object type unknown. %s", objTypeStr(otype).c_str() );
		exit(-6);
	}
	
	mObjList.push_back ( obj );

	mObjMap[ name ] = obj->mObjID;	// name map
	
	return obj;
}

int	ObjectList::DeleteObject(Object* obj)
{
	int id_del = obj->mObjID;
	
	delete obj;

	// remove from object list (DO NOT ERASE HERE)
	mObjList[id_del] = 0x0;

	// remove from name map
	for (objMap::iterator it = mObjMap.begin(); it != mObjMap.end(); it++) {
		if ( it->second == id_del ) {
			mObjMap.erase ( it );
			break;
		} 
	}

	return id_del;		// deleted asset ID
}


bool ObjectList::LoadObjectFromFile ( objType otype, std::string name, std::string fname, Object*& obj )
{	
	std::string errmsg = "";

	// Create asset
	obj = AddObject ( otype, name );
	if (obj == 0x0) {
		dbgprintf("ERROR: Unable to create object %s of type %s.\n", name.c_str(), objTypeStr(otype).c_str());
		return false;
	}
	obj->mFilename = fname;
	obj->mLoaded = false;	
		
	// Load object
	if ( obj->isAsset() && !fname.empty() ) {

    // images & meshes loaded here via libmin
		obj->mLoaded = obj->Load ( fname );		
	}
		
	// Return load result
	if ( obj->mLoaded ) {
		obj->SetVisible ( false );			// by default loaded assets are not rendered
		char extra[1024];
		extra[0] = '\0'; 
		if ( obj->getType()=='Amsh' ) {
			Vec4F mem = dynamic_cast<Mesh*> (obj)->GetStats();
			sprintf ( extra, "mesh: %.0f bytes, %d tris, %d verts", mem.x, (int) mem.y, (int) mem.z );
		}
		if ( obj->getType()=='Aimg') {
			Image* img = dynamic_cast<Image*> (obj);
			sprintf ( extra, "img: %d x %d, %d-bit", img->GetWidth(), img->GetHeight(), img->GetBitsPerPix() );
		}

		dbgprintf ( "  Loaded: asset %d = %s (%s) %s\n", obj->getID(), name.c_str(), objTypeStr(otype).c_str(), extra);
		return true;
	} else {
		dbgprintf ( "ERROR: Loading: asset %d, %s (%s) err:%s\n", obj->getID(), name.c_str(), objTypeStr(otype).c_str(), errmsg.c_str() );
		int id = obj->mObjID;		
		mObjList[id] = 0x0;
		mObjMap[ name ] = -1;
		delete (obj); 
		return false;
	}	
}

std::string ObjectList::getRevTypeMap ( objType ot )
{
	for (typeMap::iterator it = mTypeMap.begin(); it != mTypeMap.end(); it++) {
		if ( it->second == ot )
			return it->first;
	}
	return "";
}

bool ObjectList::SaveObject ( Object* obj, FILE* fp )
{
	std::string type_name = getRevTypeMap ( obj->getType() );

	if ( type_name.length()==0 ) return false;

	// object header
	fprintf ( fp, "[%s] %s\n", type_name.c_str(), obj->getName().c_str() );

	// save common vars
	Vec3F pos = obj->getPos();
	fprintf ( fp, "  visible: %s\n", obj->isVisible() ? "true" : "false" );	
	fprintf ( fp, "  pos: <%4.3f,%4.3f,%4.3f>\n", pos.x, pos.y, pos.z );

	// save inputs
	std::string name, val;
	for (int n=0; n < obj->getNumInputs(); n++) {
		name = obj->getNameOfInput ( n );
		val = obj->getInputAsName ( n );
		if ( val.length() > 0 ) {
			fprintf ( fp, "  input: %s = %s\n", name.c_str(), val.c_str() );
		}
	}

	// save params	
	for (int n=0; n < obj->getNumParam(); n++) {
		name = obj->getParamName ( n );
		val = obj->getParamValueAsStr ( n );
		if ( val.length() > 0 ) {
			fprintf ( fp, "  param: %s, %s\n", name.c_str(), val.c_str() );
		}
	}

	fprintf ( fp, "\n" );

	return true;
}


void ObjectList::Clear()
{
	dbgprintf ("Clearing assets.\n");
	int ok = 0;
	for (int n = 0; n < mObjList.size(); n++)
		if (mObjList[n] != 0x0) {
			ok++;
			delete mObjList[n];
			mObjList[n]  = 0x0;
		}
		
	dbgprintf ("  Deleted: %d ok.\n", ok );
	
	mObjList.clear();
}

// AddAssetPath
// Scan a directory and just record the names of potential assets,
// this function does NOT load the assets. 
int ObjectList::AddAssetPath ( std::string path )
{
	Directory* mDir;			// Directory listing
	dir_list_element d;
	std::string ext; 
	std::string filter = "*.*";	
	int ok = 0;
	
	// Load directory
	mDir = new Directory;	
	mDir->LoadDir ( path, filter );

	if (path.at( path.length()-1) != getPathDelim() ) {
		path += getPathDelim();
	} 

	mAssetPaths.push_back ( path );

	for (int n=0; n < mDir->getNumFiles(); n++ ) {
		d = mDir->getFile(n);		
    if ( !AddAsset ( path + d.text + "." + d.extension ).empty() ) {
      ok++;
    }
	}
	// Clear director listing
	delete mDir;
	
	return ok;
}
// AddAsset
// - add to list of available assets
// - optionally check if the file exists
std::string ObjectList::AddAsset (std::string filestr, bool bChk )
{
  std::string filepath = filestr;
  if (bChk && !getFileLocation(filestr, filepath)) {
    return "";
  }  
  std::string path, name, ext;  
  getFileParts( filepath, path, name, ext  );
  objType otype = getObjTypeFromExtension( ext );					// Get file type
  if (otype != 'null' && otype != 'dir ') {
    mAssetFiles.push_back(path + name + "." + ext);
    return name;
  }
  return "";
}

// LoadAllAssets - load all the files from scanned asset paths
int ObjectList::LoadAllAssets ()
{
	int ok = 0;
	int failed = 0;

	/*result = AddObjectFromFile ( otype, d.text, path + d.text + "." + d.extension, obj );
			if (result) {
				objlist.push_back ( obj->getID() );
				ok++; 
			} else {
				failed++;
			} */
	return ok;
}

// LoadObject - 
// Load an object from the asset list by base name
//
Object* ObjectList::LoadObject ( std::string asset_name )
{
	Object* obj;
	objType otype;
	std::string fname, path, name, ext;

	for (int n=0; n < mAssetFiles.size(); n++ ) {

		fname = mAssetFiles[n];
		getFileParts ( fname, path, name, ext );
		otype = getObjTypeFromExtension( ext);
		if ( asset_name.compare(name)==0) {		
      // add unloaded asset as loaded object
			if (LoadObjectFromFile ( otype, name, fname, obj ))
				return obj;
		}
	}
	return 0x0;
}


Object* ObjectList::FindObject ( std::string asset_name )
{
	Object* obj = gAssets.getObj ( asset_name );	
	return obj;
}

Object* ObjectList::FindOrLoadObject ( std::string asset_name )
{
	// Find - behavior or loaded asset in list
	Object* obj = gAssets.getObj ( asset_name );	
	if (obj == 0x0 ) {
		// Load - from unloaded asset file list
		obj = LoadObject ( asset_name );
	}
	return obj;
}


Object* ObjectList::getObj(std::string name)
{
	Object* obj;
	for (int n = 0; n < mObjList.size(); n++) {
		obj = mObjList[n];
		if (obj != 0x0 )
			if (name.compare(obj->mName) == 0) return obj;		
	}
	return 0x0;
}

Object* ObjectList::getObj(std::string name, objType typ)
{
	Object* obj;
	for (int n = 0; n < mObjList.size(); n++) {
		obj = mObjList[n];
		if (name.compare(obj->mName) == 0 && obj->getType() == typ) {
			return obj;
		}
	}
	return 0x0;
}

Object* ObjectList::getObjByType(objType typ)
{
	Object* obj;
	for (int n = 0; n < mObjList.size(); n++) {
		obj = mObjList[n];
		if (obj->getType() == typ) {
			return obj;
		}
	}
	return 0x0;
}


// Get RenderID
Vec3I ObjectList::getRIDs( objID id )
{
	return mObjList[id]->getRIDs();	
}

/*int ObjectList::getRIDByType ( std::string name, objType typ )
{

	assetID aid = getAssetID(name, typ); 
	if (aid==A_NULL) {
		dbgprintf ( "WARNING: Asset not found. Name: %s\n", name.c_str() );
		return NULL_NDX;
	}	
	return getRenderID(aid, typ);
}*/

