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

#ifndef DEF_OBJECT_LIST_H
	#define DEF_OBJECT_LIST_H

	#include <vector>
	#include <map>
	#include <assert.h>

	#include "object.h"
	#include "vec.h"	
	#include "main.h"			// for dbgprintf
	#include "set.h"

	class Directory;
	class RenderGL;

	typedef std::map<std::string, objType>	typeMap;
	typedef std::map<std::string, int>		objMap;
	
	class ObjectList {
	public:		
		ObjectList();
		~ObjectList();

		void Clear();
		void RegisterTypes();
		std::string getRevTypeMap ( objType ot );
    objType	getObjTypeFromName (std::string name); 
    objType	getObjTypeFromExtension ( std::string ext );						
    std::string AddAsset(std::string filestr, bool bChk = false);
    int     AddAssetPath ( std::string path);		

		// Creation
		Object*		AddObject ( objType otype, std::string name );
		int			DeleteObject ( Object* obj );
		bool		LoadObjectFromFile (objType otype, std::string name, std::string fname, Object*& obj);
		int			LoadAllAssets ();								// load all from asset paths
		Object*	LoadObject ( std::string asset_name );			// load specific object
		Object*	FindObject ( std::string asset_name );
		Object*	FindOrLoadObject ( std::string asset_name );	// lazy loading		
		bool		SaveObject ( Object* obj, FILE* fp );


		// Access
		int			getNumObj ()				{ return (int) mObjList.size(); }
		Object*		getObj ( objID id )			{ return (id==OBJ_NULL) ? 0x0 : mObjList[id]; }
		std::string getObjName ( objID id )		{ return (id==OBJ_NULL) ? "(null)" : mObjList[id]->getName(); }
		Object*		FindObj (std::string name)	{ return getObj(name); }
		int			FindObjID (std::string name) { Object* obj = getObj(name); return (obj==0x0) ? OBJ_NULL : obj->getID(); }
		Object*		getObj (std::string name );					// find by name		
		Object*		getObj (std::string name, objType typ );	// find by name	& type
		Object*		getObjByType (objType typ);					// type
		
		// Helper functions		
		void		SetRender ( RenderGL* r )			{ mRender = r; }		
		Vec3I	getRIDs( objID id );		
		//int			getRIDByType ( std::string name, objType atyp );
		
	private:
		std::vector<std::string> mAssetPaths;	  // locations to search for assets
		std::vector<std::string> mAssetFiles;	  // list of file names

		std::vector<Object*>	mObjList;		// list of objects

		objMap					mObjMap;		// map from object names to asset IDs, e.g. Human=7
		
		typeMap					mTypeMap;		// map from named types to objType, e.g. MOTION='mcyc'

		RenderGL*				mRender;
	};

	extern ObjectList gAssets;


#endif