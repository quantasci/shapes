

#ifndef DEF_RENDER_BASE_H
	#define DEF_RENDER_BASE_H

	#include "shapes.h"	
	#include "camera.h"
	#include "object_list.h"
	
	class RenderMgr;
	class Scene;
	class LightSet;
	class Material;

	#define OPT_SKETCH			0
	#define OPT_SKETCH_OBJ		0
	#define OPT_SKETCH_SHAPE	1
	#define OPT_SKETCH_NORM		2
	#define OPT_SKETCH_STATE	3
	#define OPT_SKETCH_WIRE		4
	#define OPT_SKETCH_PICK		5
	#define OPT_ENABLE			6
	#define OPT_RECORD			7
	#define OPT_MAX				8

	// State buffers
	#define BSHAPES				0		// sorted shapes
	#define BXFORMS				1		// sorted transforms
	#define BOFFSETS			2		// group offsets
	#define BBINS				3		// group bin cnts
	
	#define TOGGLE				-1

	// ShapeGroup
	// - This is the primary structure that is sorted and rebuilt each frame

	#define KEY_NULL	0x2540BE400		// max 64-bit int
	#define NODE_NULL	-1				// null index

	struct ShapeGroup {
		ShapeGroup()	{key = KEY_NULL; shader=NULL_NDX; left=-1; right=-1; count=0; offset=0; }
		char			name[16];				// name for debugging
		uint64_t		key;
		int				left, right;			// binary search tree

		Vec4F		meshids;				// mesah IDs		
		int				meshRID;				// mesh render ID
		int				shader;					// shader
		int				shaderRID;				// shader render ID

		int				count, offset;			// prefix scan of shapes		
	};

	class RenderBase {
	public:
		RenderBase () { mRenderMgr = 0x0; for (int n=0; n < OPT_MAX; n++) mbOpt[n]=false;  mOutTex=-1; }
		
		virtual void Initialize ()		{};
		virtual void PrepareAssets ()	 {};
		virtual void StartNewFrame()    {};
		virtual void StartRender ()		{};
		virtual void Validate ()		{};
		virtual bool Render()			{return false;}
		virtual void EndRender ()		{};
		virtual void RenderPicking ( int w, int h, int buf ) {};
		virtual Vec4F Pick (int x, int y, int w, int h) {return Vec4F(-1,-1,-1,-1);}
		virtual bool SaveFrame (char* filename) {return false;}
		virtual void Sketch3D (int w, int h, Camera3D* cam) {};
		virtual void Sketch2D (int w, int h) {};
		virtual void UpdateRes ( int w, int h, int MSAA ) {};
		virtual void UpdateLights () {};
		virtual void UpdateCamera () {};

		void	SetOption ( int id, int v )	{ mbOpt[id] = (v==TOGGLE) ? !mbOpt[id] : (bool) v; }
		bool	isEnabled ( int id )			{ return mbOpt[id]; }		

		// State Sorting
		uint64_t getShapeKey(float s1, float s2, float s3, float s4);
		void	SetStateDebug(int grp, int& x, int& y, uint64_t key);
		void	InitializeStateSort ();
		void	ResetNodes();
		int		AddNode();
		bool	FindNode(uint64_t key, int& node);
		int		InsertNode(uint64_t key, Vec4F state, int shader, std::string name, int& parent);		
		int		IncrementNode(int node);		
		int		ExpandShapeBuffers(int est_added, int*& bins, int*& offs);
		void	ResolveTexture ( Vec8S* texids );
		void	ResolveMaterial ( Vec8S* matids, int& shader );
		void	InsertShapes(Shapes* shapes, int& x, int& y);
		int		PrefixScanShapes();
		void	SortShapes(Shapes* shapes, Matrix4F& shapes_xform );
		void	InsertAndSortShapes ();

		bool	getMaterialObj ( Vec8S* matids, ::Material*& obj );

		void	SetOutputTex ( int rt )	{ mOutTex = rt; }
		int		getOutputTex()				{return mOutTex; }
		void	SetManager (RenderMgr* m)	{ mRenderMgr = m;}
		RenderMgr* getRenderMgr()		{ return mRenderMgr; }

	public:
		RenderMgr*		mRenderMgr;		

		bool			mbOpt[OPT_MAX];
		int				mOutTex;		
		

		// State Sorting 		
		DataX					mSB;						// State-Sorted shape buffers
		ShapeGroup*				mSG;						// Binary Search Tree on ShapeGroups
		int						mShapeCnt;
		int						mSGRoot, mNode, mID;
		int						mSGCnt, mSGMax;			
		uint64_t				mChkSum, mChkSumL;			// Frame-to-frame change check


		// Debugging State
		CLRVAL					mState[2][512][512];			
		CLRVAL					mPalette[256];

	};

#endif