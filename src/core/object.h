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

#ifndef DEF_OBJECT
	#define DEF_OBJECT

	#include "common_defs.h"
	#include "string_helper.h"			
	#include "vec.h"
	#include "quaternion.h"
	#include <string>

	// Object types
	#define	OBJ_NULL		-1
	#define OBJ_SHAPEGRP	-2
	typedef unsigned int	objType;
	typedef int				objID;

	static std::string objTypeStr(objType ot)			// static function
	{
		char buf[5];
		buf[0] = ((char*)&ot)[3];	buf[1] = ((char*)&ot)[2]; buf[2] = ((char*)&ot)[1]; buf[3] = ((char*)&ot)[0]; buf[4] = '\0';
		return std::string(buf);
	}

	// Data types
	#define T_STR		0
	#define T_VEC3		1
	#define T_VEC4		2	
	#define T_FLOAT		3
	#define T_ID		4	
	
	#define S_NONE		0			//--- Shape types
	#define S_POINT		1			// point
	#define	S_IMAGE		2			// image
	#define S_MESH		3			// mesh
	#define S_SHAPEGRP	4
	#define S_BRD2D		5			// billboard 2D
	#define S_BRD3D		6			// billboard 3D

	#define S_LEADER	7
	#define S_NODE		8
	#define S_INTERNODE	9	
	#define S_PART		100			// custom part

	#define S_NULL		65535		// max of ushort (e.g. child or next)
	#define C_NULL		255			// max of uchar

	#define	MESH_MARK	65534
	#define MESH_NULL	65535

	#define NULL_NDX	65530		// null Tex ID

	#define TEX_SETUP	65531	

	#define MARK_NONE			0
	#define MARK_DIRTY			1		// dirty
	#define MARK_CLEAN			2		// clean
	#define MARK_COMPLETE		4		// complete

	typedef unsigned char	uchar;

	class Camera3D;		
	struct Shape;
	class Shapes;	
	class Points;
	class Params;
	class Set;

	class Input {
	public:
		Input()	{ in_name[0] = '\0'; value = OBJ_NULL; }
		char		in_name[16];		// input semantic
		objType		in_type;
		objID		value;
	};	

	typedef std::vector<std::string>		vecStrs;

	class Object {
	public:
		Object();
		virtual ~Object();
		virtual objType getType()		{ return '****'; }
				
		virtual void Sketch (int w, int h, Camera3D* cam) {};
		virtual bool Load(std::string fname) { return false; }
		virtual bool RunCommand(std::string cmd, vecStrs args) {return false;}

		virtual void Clear() {};
		virtual void Define (int x, int y) {};
		virtual void Generate(int x, int y) { MarkClean(); }
		virtual void Run(float time) {};
		virtual void Render() {};		
		virtual bool Validate() { return true; }
		virtual Vec4F GetStats() { return Vec4F(0,0,0,0); }
		virtual bool Select3D ( int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir ) {return false;}
		virtual void Adjust3D ( int w, Vec4F& sel ) {};
		virtual Vec3F getPosition() { return Vec3F(0, 0, 0); }		
		virtual Vec3F getStats() { return Vec3F(0,0,0); }    

		void		Build();

		// Object variables
		bool		isAsset()			{ objType ot = getType(); return ((char*)&ot)[3] == 'A'; }
		bool		isShapes()			{ return (getType()=='Ashp'); }
		bool		isLoaded()			{ return mLoaded; }
		objID		getID()				{ return mObjID; }
		Vec3I		getRIDs()			{ return mRIDs;	}
		void		SetRID (int pos, int rid)	{ *(((int*) &mRIDs.x) + pos) = rid; }
		std::string getName()			{ return mName; }
		void		SetName(std::string n) {mName = n; }
		std::string getTypeStr()		{ objType ot = getType(); return objTypeStr(ot); }		
		bool		isVisible()			{ return mVisible; }
		void		SetVisible(bool b);
		bool		hasShadows()		{ return mShadows; }
		void		SetShadows(bool b)	{ mShadows = b; }
		bool		useBake()			{ return mBake; }
		void		SetBake(bool b)		{ mBake = b; }
		Object*		getBake()			{ return getInput("bake"); }
		bool		isDirty()			{ return (mMark & MARK_DIRTY) != 0; }
		bool		isComplete()		{ return (mMark & MARK_COMPLETE) != 0; }
		char		getMark()			{ return mMark; }
		void		MarkDirty()			{ Mark(MARK_DIRTY, true); }
		void		MarkClean()			{ Mark(MARK_DIRTY, false); }
		void		MarkComplete()		{ Mark(MARK_COMPLETE, true);}
		void		MarkIncomplete()	{ Mark(MARK_COMPLETE, false); }
		void		Mark(char flag, bool on);

		// Input management
		bool		AddInput (std::string input_name, objType intype);
		void		SetInput (std::string input_name, std::string asset_name);	// find asset by name
		void		SetInput (std::string input_name, objID aid);
		//void		SetInputFromSource (std::string input_name, std::string srcinput_name);
		int			getNumInputs ()				{ return mInputs.size(); }
		std::string	getNameOfInput (int i)		{ return (i < mInputs.size() ? mInputs[i].in_name : ""); }	// name of input itself
		std::string getInputAsName (std::string input_name)	{return getInputAsName(getInputNdx(input_name)); }
		std::string getInputAsName (int i );					// name of object connected to input 
		int			getInputNdx(std::string input_name);
		Object*		getInputCheckType(std::string input_name, objType chktype);
		Object*		getInput ( std::string input_name);
		objID		getInput ( int input_id);		
		Object*		getInputOnObj ( int oid, std::string input_name );		
		Object*		getInputResult ( std::string input_name );
		Shapes*		getInputShapes ( std::string input_name );
		int			getInputID ( std::string input_name, objType at);
		Vec8S	getInputMat ( std::string input_name );	
		Vec8S	getInputTex ( std::string input_name );	
		void		getInputTex ( std::string input_name, Vec8S& texid, int slot );			
		Object*		getInputAsset( std::string input_name, objType ct );
		int			getInputList(std::string input_name, std::vector<Object*>& list);
		bool		hasInputTime();

		// Output management		
		Object*		CreateOutput ( objType at );
		Object*		ClearOutput();
		Object*		getOutput(); 
		Shapes*		getOutputShapes();
		std::string getOutputName();
		void		SetOutputXform ( Shape* src = 0x0);		
		
		// Output shapes
		Shapes*		getOutShapes();
		void		ClearShapes();
		Shape*		AddShape();				
		Shape*		AddShape(int& i);
		Shape*		AddShapeByCopy (Shape* src);
		bool		CopyShapesFromInput( std::string name );
		void		PlaceShapesEndToEnd();
		void		CreateShapeData(int buf, char* name, int stride);
		void		SetShapeData( int buf, int i, void* dat, int sz );		
		char*		getShapeData( int b, int i=0 );
		uxlong		getShapeDataUXLONG ( int b, int i )		{ return * (uxlong*)	getShapeData(b,i); }
		ushort		getShapeDataUSHORT ( int b, int i )		{ return * (ushort*)	getShapeData(b,i); }
		uchar		getShapeDataUCHAR (int b, int i)		{ return * (uchar*)		getShapeData(b,i); }
		int			getShapeDataI (int b, int i)			{ return * (int*)		getShapeData(b,i); }
		Vec3F	getShapeDataV3 (int b, int i)			{ return * (Vec3F*) getShapeData(b,i); }
		Vec4F	getShapeDataV4 (int b, int i)			{ return * (Vec4F*)getShapeData(b, i); }
		
		void		DeleteShape(int i);
		void		CopyShape (Shape* s, Shapes* src, int i);
		Shape*		getShape(int n); 		
		int			getNumShapes();		

		// Output particles
		void		ClearPoints();
		int			AddPoint();		
		int			getNumPoints ();		
		Points*		getOutputPoints();

		// Internal Parameters
		void		AddParam ( int slot, std::string name, std::string types);
		int			getNumParam ();
		std::string	getParamName (int i);
		std::string getParamValueAsStr (int i );
    int     ParseArgs(std::string value, std::vector<std::string>& args);
    void		FindOrCreateParams(std::string value, uchar sep = ';');
    void		FindOrCreateParam(std::string name, std::string val, uchar vecsep = ',', int k = 0);

		uchar		getParamType(int slot);
		int			getParamByName ( std::string name );
		int			GetParamArray ( std::string base_name, std::vector<int>& list );

		void		SetParam(std::string name, Vec3F val, int k=0);
    void    SetParam(std::string name, float val, int k=0);
    
    void		SetParam(int slot, Vec3F val, int k = 0);
		void		SetParamI(int slot, int i, int val);
		void		SetParamF(int slot, int i, float val);
		void		SetParamI3(int slot, int i, Vec3I val);
		void		SetParamV3(int slot, int i, Vec3F val);
		void		SetParamV4(int slot, int i, Vec4F val);
		void		SetParamStr(int slot, int i, std::string str );
		int			getParamI(int slot,int i = 0);
		float		getParamF(int slot,int i = 0);
		Vec3F	getParamV3(int slot, int i=0);		
		Vec3I	getParamI3(int slot,int i = 0);
		Vec4F	getParamV4(int slot, int i=0);
		std::string getParamStr(int slot, int i=0);
		
		void		SetTimeRange ( Vec3F t )	 { mTimeRange = t; }

		// Object transform
		void		SetTransform ( Vec3F p, Vec3F s, Quaternion r=Quaternion() );
		void		SetPos(Vec3F p);
		void		CopyLocalXform ( Shape* src );	
		void		SetLocalXform ( Matrix4F& xform );
		void		UpdateXform ();
		Shape*		getLocalXform()		{ return mLocalShape; }
		Matrix4F&	getXform ()			{ return mLocalXform; }
		Vec3F	getPos();

		// 3D interaction
		void		AssignShapeToWidget3D ( Shape* s, int w, Vec3I sel );
		void		AssignWidgetToShape3D ( Shape* s, int w, std::string& name);
		bool		IntersectShape3D ( Shape* s, float x, float y );
		Vec3I		GetWidgetSel ( int w );
		Vec4F		getIDS ( int a, int b, int c )		{ return Vec4F(mObjID, a, b, c); }		// picking IDs
		
		//--- asset outputs
		//Object* CreateAssetOut(objType ot);
		//Object* getAssetOut (objType& at) { at = A_NULL; return (mShapes == A_NULL) ? 0x0 : (Object*)gAssets->getAssetObj(mShapes, at); }

	public:
		std::string		mName;
		std::string		mFilename;
		objID			mObjID;				// Location in asset manager			
		Vec3I		mRIDs;		

		Vec3F		mTimeRange;			// object time range

		bool			mVisible;
		bool			mShadows;
		bool			mLoaded;
		bool			mBake;
		char			mMark;

		std::vector<Input>	mInputs;		// Inputs
	
		objID			mOutput;			// Output

		Params*			mParams;			// Params
		
		Shape*			mLocalShape;
		Matrix4F		mLocalXform;
	};


#endif