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

#ifndef DEF_SCENE
	#define DEF_SCENE

	#include "object_list.h"
	#include "string_helper.h"
	#include "globals.h"
	#include "mersenne.h"
	#include <vector>

	class Camera;
	
	class Scene : public Object {
	public:
		Scene();

		virtual objType getType()	{ return 'scen'; }
		virtual void Generate (int x, int y);		
		virtual bool Validate ();

		void	Execute (bool bRun, float time, float dt, bool dbg_eval );
		
		bool  Load  (std::string fname, int w, int h);
		bool	LoadScene (std::string fname, int w, int h);
		bool	LoadGLTF (std::string fname, int w, int h);
		void	CreateSceneDefaults();
		void	SaveScene ( std::string fname );		
		void	CreateScene (int w, int h);		// test scene		
		void	Stats ();

		Object* CreateObject ( objType ot, std::string name );		// Define is performed here
		int		AddObject ( Object* obj );
		void	AddOutputToScene(Object* obj);
		void	ShowAssets ();
		int		ClearObject(Object* obj);				// clear outputs
		void	RemoveFromScene(int id);				// remove object from scene
		void	DeleteFromScene(Object* obj);			// delete object from scene and assets	

		void	RegenerateScene( bool bnew );
		void	RegenerateSubgraph(std::vector<objID>& objlist, bool bnew=true);
		int		RegenerateSubgraph(std::vector<Object*>& objlist, int seed, bool bRun=false);
		void	RebuildSubgraph(std::vector<Object*>& objlist, int seed);						// run until complete

		void	Clear();		
		void	MarkScene ( char dirt=MARK_DIRTY );
		void	MarkSceneByType  ( objType otype, char dirt=MARK_DIRTY );
		bool	Select3D (int w, Vec4F& sel, std::string& name, Vec3F orig=Vec3F(0,0,0), Vec3F dir=Vec3F(0,0,0) );
		void	Adjust3D (int w);		
		void	SetVisible(std::string name, bool v);
		Vec4F	getSelected() { return m_Sel; }

		Object*	FindObject(std::string name)	{ return gAssets.getObj(name); }	// Find asset		
		Object* FindByType(objType ot);
		Object* FindInScene(std::string name);			// Find in scene by name	
		int			FindInScene(Object* obj);				// Find in scene by pointer
		int			FindInScene(int obj_id);				// Find in scene by ID

		int			getNumScene()		{ return (int) mSceneList.size(); }
		Object*		getSceneObj(int n)	{ return gAssets.getObj(mSceneList[n]); }		// Find asset in scene		
		
		Camera3D*	getCamera3D();
		Globals*	getGlobals();
		Camera*		getCameraObj();
		float		getFPS()	{ return (mGlobals==0) ? 30 : mGlobals->getFPS(); }
		float		getRate()	{ return (mGlobals==0) ? 1.0 : mGlobals->getRate(); }

		void		getTimeObjects(std::vector<Object*>& objlist);

		// Scene resolution & time
		Vec3F	getRes() { return mRes; }
		void		setRes( float w, float h )		{ mRes.Set(w,h,0.f); }

		void		setTime( float t )	{ m_Time = t; }
		float		getTime ()			{ return m_Time; }
		
	private:
		Vec3F				mRes;				// render resolution

		float					m_Time;				// secs
		Vec4F				m_Sel;

		// scene is an indexed subset of the global object list 
		std::vector<objID>		mSceneList;		
		Globals*				mGlobals;
		Mersenne				m_rand;
		int						m_seed;
	};

	extern Scene* gScene;

#endif