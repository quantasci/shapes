
#include "object.h"
#include "object_list.h"
#include "shapes.h"
#include "points.h"
#include "params.h"
#include "quaternion.h"
#include <string>

#include "main.h"		// for dbgprintf

Object::Object ()
{
	mTimeRange = Vec3F(0, 0, 0);			// default time range

	mParams = 0;
	mName = "";
	mFilename = "";
	mVisible = true;
	mShadows = true;
	mBake = false;
	mMark = MARK_DIRTY;
	mObjID = OBJ_NULL;	
	mOutput = OBJ_NULL;
	mRIDs.Set ( NULL_NDX, NULL_NDX, NULL_NDX );

	mLocalShape = new Shape;	
	UpdateXform();
}

Object::~Object()
{
	if ( mLocalShape != 0x0 ) delete mLocalShape;
	Clear();  

	// note: no proper cleanup is done yet. scene must be cleared and then all assets
}


//------------------------------------------------------------------- Input Management

bool Object::AddInput (std::string input_name, objType intype)
{
	Input i;
	i.in_type = intype;
	strncpy( i.in_name, input_name.c_str(), 16);
	i.value = OBJ_NULL;

	mInputs.push_back(i);

	return true;
}

objID Object::getInput (int input_id)
{
	return mInputs[input_id].value;
}
Object* Object::getInput (std::string input_name)
{
	// scan inputs for input_name
	objID oid = OBJ_NULL;
	for (int n = 0; n < mInputs.size(); n++) {
		if (input_name.compare(mInputs[n].in_name) == 0) {
			oid = mInputs[n].value;			
			return (oid==-1) ? 0x0 : gAssets.getObj (oid);
		}
	}	
	return 0x0;	
}

Object* Object::getInputOnObj ( int aid, std::string input_name )
{
	Object* ast = gAssets.getObj ( aid );
	if (ast==0x0) return 0x0;
	return ast->getInput ( input_name);
}

bool Object::hasInputTime()
{
	if (mInputs.size() == 0) return false;
	return (mInputs[0].in_type == 'Atim');
}

std::string Object::getInputAsName (int i )
{
	if ( i >= mInputs.size())  return "";
	
	if ( mInputs[i].value==-1 ) return "";

	Object* obj = gAssets.getObj ( mInputs[i].value );
	if ( obj==0x0 ) return "";

	return obj->getName();		// return name of the input value
}


Object* Object::getInputResult ( std::string input_name )
{	
	Object* obj = getInput( input_name );
	if (obj == 0x0) return 0x0;
	// status = obj->getMark();				// get status
	if (obj->isAsset()) return obj;			// assets have no output
	return obj->getOutput();
}
Shapes*	Object::getInputShapes ( std::string input_name)
{
	return dynamic_cast<Shapes*> (getInputResult(input_name));
}
Vec3F Object::getPos()				{ return mLocalShape->pos; }	

void Object::UpdateXform()
{
	mLocalXform = mLocalShape->getXform();		// evaluate
}

void Object::SetPos(Vec3F p)
{
	mLocalShape->pos = p;
	UpdateXform();
	
	SetOutputXform();		// update output also
}
void Object::SetTransform(Vec3F p, Vec3F s, Quaternion r)
{
	mLocalShape->pos = p;
	mLocalShape->scale = s;
	mLocalShape->rot = r;
	UpdateXform();

	SetOutputXform();		// update output also
}

void Object::CopyLocalXform ( Shape* src )
{
	*mLocalShape = *src;
	UpdateXform();
}
void Object::SetLocalXform ( Matrix4F& xform )
{
	// todo: extract the pos, rot, scale from matrix (assume no shear)
	//mLocalShape->pos = 

	mLocalXform = xform;
}

void Object::SetOutputXform(Shape* src)
{	
	src = (src == 0) ? mLocalShape : src;		// apply behavior transform (default)

	Object* out = gAssets.getObj(mOutput);		// copy to output
	if (out == 0x0) return;
	out->CopyLocalXform ( src );
}


void Object::SetVisible(bool b)
{
	mVisible = b; 
	Object* out = gAssets.getObj(mOutput);
	if (out != 0x0) out->SetVisible( mVisible );
}	


Object* Object::getInputCheckType (std::string input_name, objType chktype )
{
	// scan inputs for input_name
	int oid = OBJ_NULL;
	for (int n = 0; n < mInputs.size(); n++) {
		if (mInputs[n].in_type == chktype)							// faster
			if (input_name.compare(mInputs[n].in_name) == 0) {
				oid = mInputs[n].value;
				return (oid == -1) ? 0x0 : gAssets.getObj(oid);
			}
	}	
	return 0x0;	
}


int Object::getInputNdx(std::string input_name)
{
	// SLOW
	for (int n = 0; n < mInputs.size(); n++) {
		if (input_name.compare(mInputs[n].in_name) == 0)
			return n;
	}
	return OBJ_NULL;
}



void Object::SetInput(std::string input_name, std::string asset_name)
{
	// Identify the input slot
	int ndx = getInputNdx (input_name);
	if (ndx == OBJ_NULL) {
		// dbgprintf("ERROR: Input not found. Input: %s on Obj: %s\n", input_name.c_str(), getName().c_str());
		return;
	}
	if ( asset_name.empty() ) {
		dbgprintf("WARNING: Input for %s is unconnected.\n", input_name.c_str() );
		mInputs[ndx].value = -1;
		return;
	}
	if (input_name.compare("icon")==0) {
		bool stop=true;
	}

	// Get the object to connect
	Object* obj = gAssets.FindOrLoadObject ( asset_name );
	if ( obj==0x0 ) {
		dbgprintf("**** ERROR: Set input on %s. Cannot find or load asset %s\n", input_name.c_str(), asset_name.c_str() );
		mInputs[ndx].value = -1;
		return;
	}
	
	if ( mInputs[ndx].in_type == 'LIST' ) {				// if input is a list, append value
		int listsz = (mInputs.size()-1) - ndx;
		mInputs[ ndx + listsz].value = obj->getID();
		AddInput ( "", 'LIST' );
	} else {											// standard input		
		mInputs[ndx].value = obj->getID();				// set input to specific object
	}
}

void Object::SetInput(std::string input_name, objID obj)
{
	// identify the input slot
	int ndx = getInputNdx(input_name);
	if (ndx == OBJ_NULL) {
		dbgprintf("ERROR: Input not found. Input: %s on Obj: %s\n", input_name.c_str(), getName().c_str());
		return;
	}
	// set input to specific object
	mInputs[ndx].value = obj;	
}

int Object::getInputList(std::string input_name, std::vector<Object*>& list )
{
	int ndx = getInputNdx(input_name);
	if (ndx == OBJ_NULL) return 0;
	
	Object* obj;
	for (int n=ndx; n < mInputs.size()-1; n++) {
		obj = gAssets.getObj (mInputs[n].value );
		list.push_back ( obj );
	}

	return list.size();
}

int	Object::getInputID ( std::string input_name, objType ct )
{
	Object* obj = getInputCheckType ( input_name, ct );
	if ( obj==0x0 ) return OBJ_NULL;
	return obj->getID();
}

Vec8S Object::getInputMat ( std::string input_name )
{
	Object* obj = getInputCheckType ( input_name, 'Amtl');
	if (obj == 0x0) return Vec8S(NULL_NDX);
	int aid = obj->getID();						// asset ID	
	return Vec8S(aid, NULL_NDX, NULL_NDX);
}

Vec8S Object::getInputTex ( std::string input_name )
{
	Object* obj = getInputCheckType ( input_name, 'Aimg');
	if (obj == 0x0) return Vec8S(NULL_NDX);
	int aid = obj->getID();						// asset ID	

	// setup single texture. 
	// x = asset ID, x2 = openGL(unresolved), all others=NULL
	return Vec8S(aid, NULL_NDX, NULL_NDX);
}

void Object::getInputTex ( std::string input_name, Vec8S& texid, int slot )
{
	Object* obj = getInputCheckType ( input_name, 'Aimg');
	if (obj == 0x0) return;
	int aid = obj->getID();				// asset ID
	texid.Set ( slot, aid );			
	texid.Set ( slot+4, NULL_NDX );
}

Object* Object::getInputAsset( std::string input_name, objType ct  )
{
	return getInputCheckType ( input_name, ct );	
}



/*void Object::SetInputFromSource(std::string input_name, std::string srcinput_name)
{
	// get the input slot we wish to set on current object
	int ndx = getInputNdx(input_name);
	if (ndx == OBJ_NULL) {
		dbgprintf("ERROR: Input not found. Input: %s on Obj: %s\n", input_name.c_str(), getName().c_str());
		return;
	}
	// find the asset which is on the source input
	assetID srcid = getInput(srcinput_name);
	if (srcid == A_NULL) {
		dbgprintf("ERROR: Source for SetInputFromSource is not assigned. Src: %s on Obj: %s\n", srcinput_name.c_str(), getName().c_str());
		return;
	}
	if (gAssets->getAssetType(srcid) != A_BEHAVIOR) {
		dbgprintf("ERROR: Source for SetInputFromSource is not a behavior. Src: %s on Obj: %s\n", srcinput_name.c_str(), getName().c_str());
		return;
	}
	// get the asset as a behavior
	Behavior* obj = gAssets->getBehavior(srcid);

	// find the identical named input on the source behavior
	assetID src_input_id = obj->getInput(input_name);

	// set the input slot on current object to the same one on source object
	if (ndx < mInputs.size())
		mInputs[ndx].value = src_input_id;
}*/

//------------------------------------------------------------------- Output Management

void Object::Mark ( char flag, bool on )
{
	if (on)
		mMark |= flag;
	else
		mMark &= ~flag;

	// marking dirty causes output to be dirty
	if ( mOutput != OBJ_NULL && flag==MARK_DIRTY && on)
			gAssets.getObj(mOutput)->Mark (flag, true);
}

Object* Object::ClearOutput()
{
	int id = mOutput;
	Object* out_obj = gAssets.getObj ( id );
	mOutput = OBJ_NULL;
	return out_obj;		// object de-referenced as output but not deleted
} 

Object* Object::CreateOutput ( objType ot )
{	
	// create a shape asset
	std::string obj_name = getName()+"_S";		// source name

	Object* out_obj = gAssets.AddObject (ot, obj_name );
	if (out_obj == 0) {
		dbgprintf("ERROR: Unable to create output on %s.\n", getName().c_str());
	}
	// output of 'this' is the output object
	mOutput = out_obj->getID();

	return out_obj;
}

//----------------------------------------------------- SHAPES
Shapes* Object::getOutShapes()			{ return dynamic_cast<Shapes*> (gAssets.getObj(mOutput)); }		// getObj checks for OBJ_NULL

Shape* Object::AddShape()				{ int i;	return getOutShapes()->Add(i); }
void Object::ClearShapes()				{ Shapes* shapes = getOutShapes(); if (shapes!=0x0) shapes->Clear(); }
void Object::DeleteShape(int i)			{ Shapes* shapes = getOutShapes(); if (shapes != 0x0) shapes->Delete(i); }
Shape* Object::AddShape(int& i)			{ Shapes* shapes = getOutShapes(); return (shapes=0x0) ? 0 : getOutShapes()->Add(i); }
Shape* Object::AddShapeByCopy(Shape* src) { return getOutShapes()->AddShapeByCopy (src); }
int Object::getNumShapes()				{ Shapes* shapes = getOutShapes(); return (shapes==0x0) ? 0 : shapes->getNumShapes(); }
Shape* Object::getShape(int n)			{ Shapes* shapes = getOutShapes(); return (shapes==0x0) ? 0 : shapes->getShape (n); }
char* Object::getShapeData(int b, int i) { Shapes* shapes = getOutShapes(); return (shapes == 0x0) ? 0 : shapes->getElem (b, i); }

// copy all input shapes to output
bool Object::CopyShapesFromInput(std::string input_name )
{
	Shapes* shapes = getInputShapes(input_name);
	if ( shapes == 0x0 ) return false;
	
	for (int n=0; n < shapes->getNumShapes(); n++) 
		AddShapeByCopy ( shapes->getShape(n) );

	return true;
}

void Object::PlaceShapesEndToEnd()
{
	Quaternion rot;
	Shape* s;
	Vec3F dir;
	for (int n = 0; n < getNumShapes() - 1; n++) {
		s = getShape(n);
		dir = (s + 1)->pos - s->pos;
		s->scale.x = dir.Length() * 0.5;									// segment length
		dir.Normalize();
		rot.fromRotationFromTo(Vec3F(1, 0, 0), dir); rot.normalize();	// segment orientation	
		s->rot = rot;
		s->pivot.Set ( 1, 0, 0);		
	}
	s = getShape( getNumShapes()-1 );
	s->scale.x = dir.Length() * 0.5;
	s->rot = rot;
	s->pivot.Set(1, 0, 0);
}

void Object::SetShapeData(int buf, int i, void* src, int sz)
{
	Shapes* shapes = getOutShapes(); 
	//if ( shapes==0x0) return;
	shapes->SetData (buf, i, (char*) src, sz);
}
void Object::CreateShapeData(int buf, char* name, int stride )
{
	Shapes* shapes = getOutShapes();
	shapes->AddBuffer(buf, name, stride, 1 );
}


void Object::CopyShape(Shape* s, Shapes* src, int i)
{
	Shape* ssrc = src->getShape (i);
	*s = *ssrc;			// deep copy
}



//---------------------------------------------------------------- Output points
void Object::ClearPoints()
{
	getOutputPoints()->Clear();
}
int Object::AddPoint()
{
	Points* pnts = getOutputPoints();	
	return (pnts==0x0) ? -1 : pnts->AddPoint();
}
Points* Object::getOutputPoints()
{
	return dynamic_cast<Points*> (gAssets.getObj(mOutput));		// getObj checks for OBJ_NULL
}
int Object::getNumPoints()
{
	Points* pnts = getOutputPoints();	
	return (pnts==0x0) ? 0x0 : pnts->getNumPoints();
}

//-------------------------------------------------------- Outputs (generic)
Object* Object::getOutput()
{	
	return gAssets.getObj(mOutput);			// getObj checks for OBJ_NULL
}
// get output shapes. used by renderers
Shapes* Object::getOutputShapes()
{
	if (!isVisible()) return 0x0;

	if ( useBake() )
		return (Shapes*) getBake();

	Object* output = gAssets.getObj(mOutput);
	if (output==0x0 || output->getType()!='Ashp') return 0x0;

	return (Shapes*) output;
}

std::string Object::getOutputName()
{
	Object* o = gAssets.getObj(mOutput);
	if ( o==0x0 ) return "not found";
	return o->getName();
}
void Object::AddParam(int slot, std::string name, std::string types)
{
	if ( mParams==0x0 )
		mParams = new Params;	

	mParams->AddParam(slot, name, types);
}
void Object::SetParam (std::string name, std::string val, uchar vecsep, int k)
{
	if ( mParams==0x0) return;					// object has no params
	int slot = mParams->FindOrCreateParam( name, val, vecsep );
	if (slot == -1) return;						// named param not found
	mParams->SetParam( slot, val, vecsep, k );
}
void Object::SetParam(std::string name, Vec3F val, int k)
{
	if (mParams == 0x0) return;					// object has no params
	mParams->SetParam(name, val, k );
}
void Object::SetParam( int slot, Vec3F val, int k)
{
	if (mParams == 0x0) return;					// object has no params
	mParams->SetParam(slot, val, k );
}

uchar Object::getParamType(int slot)
{
	return mParams->getType(slot, 0);
}

int	Object::getNumParam ()
{
	if (mParams==0x0) return 0;
	return mParams->getNumParam();
}
std::string	Object::getParamName (int i)
{	
	if (mParams==0x0) return 0;
	if (i >= mParams->getNumParam()) return "";
	return mParams->getParamName(i);
}
std::string Object::getParamValueAsStr (int i )
{
	if (mParams==0x0) return 0;
	if (i >= mParams->getNumParam()) return "";
	return mParams->getParamValueAsStr ( i );
}


int	Object::getParamByName(std::string name)
{
	if (mParams == 0x0) return -1;				// object has no params
	return mParams->FindParam(name);
}
int	Object::GetParamArray ( std::string base_name, std::vector<int>& list )
{
	if (mParams == 0x0) return -1;				// object has no params
	return mParams->GetParamArray( base_name, list );
}

void Object::SetParamI(int slot,int i,int val)		  { mParams->SetParamI(slot,i,val); }
void Object::SetParamF(int slot,int i,float val)      { mParams->SetParamF(slot,i,val); }
void Object::SetParamI3(int slot,int i,Vec3I val) { mParams->SetParamI3(slot,i,val); }
void Object::SetParamV3(int slot,int i,Vec3F val) { mParams->SetParamV3(slot,i,val); }
void Object::SetParamV4(int slot,int i,Vec4F val) { mParams->SetParamV4(slot,i,val); }
void Object::SetParamStr(int slot, int i, std::string val)	{mParams->SetParamStr(slot,i,val); }
int  Object::getParamI(int slot,int i)				{ return mParams->getParamI(slot,i); }
float Object::getParamF(int slot,int i)				{ return mParams->getParamF(slot,i); }
Vec3F Object::getParamV3(int slot,int i)		{ return mParams->getParamV3(slot,i); }
Vec3I Object::getParamI3(int slot,int i)		{ return mParams->getParamI3(slot,i); }
Vec4F Object::getParamV4(int slot,int i)		{ return mParams->getParamV4(slot,i); }
std::string Object::getParamStr(int slot, int i)	{ return mParams->getParamStr(slot, i); }

//-------- 3D interaction

#ifdef USE_WIDGETS
	#include "widget.h"
#endif

void Object::AssignShapeToWidget3D ( Shape* s, int w, Vec3I sel )
{
	#ifdef USE_WIDGETS
		Widget3D* x = gInterface->getWidget3D( w );
		x->sel = sel;
		if ( s==0x0 ) return;
		x->pos = s->pos;
		x->scale = s->scale;
		x->rot = s->rot;
		x->pivot = s->pivot;
	#endif
}

void Object::AssignWidgetToShape3D ( Shape* s, int w, std::string& name )
{
	#ifdef USE_WIDGETS
		Widget3D* x = gInterface->getWidget3D( w );		
		s->pos = x->pos;
		s->scale = x->scale;
		s->rot = x->rot;
		s->pivot = x->pivot;
		name = x->text;
	#endif
}
Vec3I Object::GetWidgetSel ( int w )
{
	#ifdef USE_WIDGETS
		Widget3D* x = gInterface->getWidget3D( w );
		return x->sel;
	#else
		return Vec3I();
	#endif
		
}

bool Object::IntersectShape3D ( Shape* s, float x, float y )
{
	#ifdef USE_WIDGETS
		// get oriented bounding box
		return gInterface->IntersectBox3D( s->pos, s->pivot, s->scale, s->rot, x, y );
	#else
		return false;
	#endif
}


