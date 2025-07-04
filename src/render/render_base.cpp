
#include "render_base.h"

#include "timex.h"
#include "render.h"
#include "scene.h"
#include "material.h"

void RenderBase::SetStateDebug ( int grp, int& x, int& y, uint64_t key )
{
	if (x > 256 || y > 256) return;

	uchar v = 0;
	for (int n=0; n<8; n++)		// XOR 8-bytes into 1-byte
		v ^= ((uchar*)&key)[n];

	mState[grp][x][y] = mPalette[ v ];

	if (grp==0)
		if (++x > 256 ) { x = 0; y++; }
}

// Shape key = 8-byte hash from 16-byte state
uint64_t RenderBase::getShapeKey ( float s1, float s2, float s3, float s4 )
{
	// construct an 8-byte (64-bit int) key from the 16-byte (4 floats) state
	uint64_t key;									// 8 byte key
	key = uint64_t(s1);						// s1 = 16-bit (65536)		- typically material (low bits)
	key ^= uint64_t(s2) << 16;		// s2 = 8-bit  (256)			- typically shader	
	key ^= uint64_t(s3) << 24;		// s3 = 16-bit (65536)		- typically mesh	(high bits)
	//key ^= uint64_t(s4) << 40;		// s4 = 24-bit (16777216)	- not yet used
	return key;
}


void RenderBase::InitializeStateSort ()
{
	mSGCnt = 0;
	mSGMax = 512;	
	mSG = (ShapeGroup*) malloc ( mSGMax * sizeof(ShapeGroup));

	mSB.AddBuffer ( BSHAPES,	"shps",		sizeof(Shape), 512 );
	mSB.AddBuffer ( BXFORMS,	"xforms",	16*sizeof(float), 512);
	mSB.AddBuffer ( BBINS,		"bins",		sizeof(int), 512 );
	mSB.AddBuffer ( BOFFSETS,	"offs",		sizeof(int), 512 );
}

void RenderBase::ResetNodes()
{
	mSGCnt = 0;
	mSGRoot = NODE_NULL;
	mNode = NODE_NULL;
	mSG[0].key = KEY_NULL;
	mSG[0].left = NODE_NULL;
	mSG[0].right = NODE_NULL;
	mSG[0].count = 0;
}

int RenderBase::AddNode ()
{
	if ( ++mSGCnt > mSGMax ) {
		int prevcnt = mSGMax;
		mSGMax *= 2;											// dynamic power-of-two allocation
		ShapeGroup* newbuf = (ShapeGroup*) malloc ( mSGMax * sizeof(ShapeGroup) );		
		memcpy ( newbuf, mSG, prevcnt * sizeof(ShapeGroup));
		free( mSG );
		mSG = newbuf;
	}
	int node = mSGCnt - 1;
	mSG[node].key = KEY_NULL;
	mSG[node].left = NODE_NULL;
	mSG[node].right = NODE_NULL;
	mSG[node].count = 0;
	return node;
}

bool RenderBase::FindNode(uint64_t key, int& node)
{
	if ( node != NODE_NULL )
		if (mSG[node].key==key ) return true;			// mNode is preserved, so its previous value is cached for performance!

	int next = mSGRoot;									// not in duplicate group. start from root

	for (; next != NODE_NULL; ) {
		node = next;
		if (mSG[node].key == KEY_NULL) return false;	// child is empty, pre-allocated (return last parent)
		if (key == mSG[node].key) return true;			// found matching node		
		if (key < mSG[node].key) {
			next = mSG[node].left;
		} else {
			next = mSG[node].right;
		}		
	}
	return false;		// not found in tree (return last parent)
}




int RenderBase::InsertNode (uint64_t key, Vec4F state, int shader, std::string name, int& parent)
{
	int node = AddNode();
	mSG[node].key = key;
	mSG[node].meshids = state;
	mSG[node].shader = shader;

	strncpy ( mSG[node].name, name.c_str(), 16 );		// for debugging

	if ( parent == NODE_NULL ) {			
		mSGRoot = node;						// new node is root
	} else if ( key < mSG[parent].key ) {
		mSG[parent].left = node;
	} else {
		mSG[parent].right = node;
	}
	return node;
}

int RenderBase::IncrementNode ( int node )
{
	return mSG[node].count++;
}


int RenderBase::ExpandShapeBuffers(int est_added, int*& bins, int*& offs)
{
	if (mID + est_added > mShapeCnt) {
		mShapeCnt = mID + est_added;						// new shape count (upper bound using estimated added)
		mSB.SetNum(mID);
		mSB.ResizeBuffer(BOFFSETS, mShapeCnt, true);	// shape offsets (safe expansion)
		mSB.ResizeBuffer(BBINS, mShapeCnt, true);		// shape bins (safe expansion)
		
	}
	bins = (int*) mSB.GetStart(BBINS);
	offs = (int*) mSB.GetStart(BOFFSETS);
	return mSB.GetMaxElem(BBINS);
}

void RenderBase::ResolveTexture ( Vec8S* texids )
{
	if ( texids == 0x0 ) return;
	if ( texids->w2 == 4 ) return;		// all 4x texture slots ready (some may be null)

	Object* obj;
	unsigned short tid, gl;

	texids->w2 = 0;
	for (int i=0; i < 4; i++ ) {
		tid = texids->get(i);			// texture asset id
		gl = texids->get(i+4);			// GLID
		
		if ( tid != NULL_NDX ) {
			// texture requested. check if unresolved with opengl
			if ( gl == NULL_NDX ) {		
				obj = gAssets.getObj( tid );
				gl = obj->getRIDs().x;
				if (obj != 0x0) {
					// resolve GLID
					texids->Set( i+4, gl );
					texids->w2++;			
				} else {
					// not yet resolved
					// w2 counter not incremented
				}
			}
		} else {
			// no texture specified
			texids->w2++;
		}
	}	
}

bool RenderBase::getMaterialObj ( Vec8S* matids, Material*& obj )
{
	int mid = matids->get(0);
	if (mid==NULL_NDX) return false;
	obj = dynamic_cast<Material*>( gAssets.getObj( mid ) );
	return (obj != 0x0);
}



void RenderBase::ResolveMaterial ( Vec8S* matids, int& shader )
{
	Material* obj;	

	if ( matids == 0x0 ) return;
	
	if ( matids->get(4)==NULL_NDX ) {		
		if (!getMaterialObj (matids, obj)) return;
		matids->Set( 4, obj->getRIDs().x );					// opengl material id
	}
	if ( matids->get(5)==NULL_NDX ) {		
		if (!getMaterialObj (matids, obj)) return;		
		matids->Set( 5, obj->getRIDs().y );					// optix material id		
	}	
	if ( matids->get(6)==NULL_NDX ) {
		if (!getMaterialObj (matids, obj)) return;
		shader = obj->getParamI ( M_SHADER );			// shader
		matids->Set( 6, shader );
	} else {
		shader = matids->get(6);
	}
}


// Insert Shapes
// - insert shapes into an array-based balanced binary search tree 
// - taking advantage of strong coherency /w many duplicate keys
// 
void RenderBase::InsertShapes ( Shapes* shapes, int& x, int& y )
{
	if (shapes==0x0 ) return;

	uint64_t key;
	Shape* s;
	int shader = NULL_NDX;
	int* bins, *offs;										// traversal order bins & offsets
	int max = ExpandShapeBuffers( shapes->getNumShapes(), bins, offs );		// expand dynamically as needed
	
	for (int i = 0; i < shapes->getNumShapes(); i++) {
		s = shapes->getShape(i);		
		if ( s->isInvisible() || s->meshids.x==MESH_MARK) continue;
		if (s->meshids.x < 0 || s->meshids.x >= MESH_NULL) {
			dbgprintf("ERROR: Mesh id invalid. %d\n", s->meshids.x);
			s->invisible = 1; continue;
		}

		if ( s->type == S_SHAPEGRP) {
			// Shape group
			InsertShapes ( (Shapes*) gAssets.getObj ( s->meshids.x ), x, y );
			continue;
		}

		// Resolve textures
		// *NOTE* This happens in RenderBase because renderers get only the final SHAPE buffers 
		// which are not stateful. The original shapes must be resolved to cache material id.		
		ResolveMaterial( &s->matids, shader );

		// State sorting		
		key = getShapeKey (s->matids.x, shader, s->meshids.x, 0 );		// get key (group)
		
		//-- debug instance groups (keys)
		// dbgprintf ( "%d: k:%012lld, %d %d %d\n", i, key, (int) s->matids.x, shader, (int) s->meshids.x );

		// Insert shape into BST tree
		if ( !FindNode ( key, mNode ) ) {		
			
			std::string name = gAssets.getObj ( s->meshids.x )->getName();

			mNode = InsertNode ( key, s->meshids, shader, name, mNode );
		}		
		// Assign shape a bin & index
		if ( mID > max ) {
			dbgprintf ( "ERROR: Insert ID exceeds max.\n");
		}
		bins[mID] = mNode;
		offs[mID] = IncrementNode ( mNode );
		mID++;

		#ifdef DEBUG_STATE
			int c = mSG[mNode].count - 1;
			SetStateDebug ( 0, x, y, key );					// unsorted state
			SetStateDebug ( 1, c, mNode, key );				// sorted state
		#endif	

		// Binary Tree Insertion
		// *NOTE* DO NOT use radix or quicksort, SINCE THE DATA IS HIGHLY COHERENT. Already many duplicate, adjacent shapes.		
		// for each shape in master list
		//   if key of prev shape == key of current shape.. then re-use the group index
		//   else  find or create group
		//   group size++
		//   shape group = group			(need an independent buffer for this data)
		//   shape  indx = groupindex++		
	}
}


int RenderBase::PrefixScanShapes()
{
	int sum = 0;
	for (int n=0; n < mSGCnt; n++) {
		mSG[n].offset = sum;
		sum += mSG[n].count;
	}
	return sum;
}

void RenderBase::SortShapes ( Shapes* shapes, Matrix4F& shapes_xform )
{
	if (shapes==0x0 ) return;

	int bin, ndx;
	Shape *src, *dest;
	Shape* out_shapebuf = (Shape*) mSB.GetStart ( BSHAPES );		// curret node for new insertions
	Matrix4F* out_xforms = (Matrix4F*) mSB.GetStart ( BXFORMS );
	int* bins = (int*) mSB.GetStart (BBINS);						// traversal order bins
	int* offs = (int*) mSB.GetStart (BOFFSETS);						// traversal order offsets
	Matrix4F* xform;

	for (int x = 0; x < shapes->getNumShapes(); x++) {
		src = shapes->getShape(x);
		if (src->isInvisible() || src->meshids.x == MESH_MARK) continue;
		if (src->type == S_SHAPEGRP) {
			// shape sub-group
			Object* obj = (Shapes*) gAssets.getObj ( src->meshids.x ) ;
			SortShapes( obj->getOutputShapes(), obj->getXform() );
			continue;
		}
		mChkSum = mChkSum + (uint64_t(src->meshids.x) + uint64_t(src->pos.x*src->pos.y*src->pos.z) + uint64_t(src->scale.x * 100.0));
	
		// deep copy shape into shape buffer
		bin = bins[mID];
		ndx = offs[mID];
		dest = out_shapebuf + (mSG[bin].offset + ndx);
		memcpy ( dest, src, sizeof(Shape) );

		// construct shape transform with object xform
		// Matrix4F& xf = src->getXform();

		xform = out_xforms + (mSG[bin].offset + ndx);
		xform->Multiply (shapes_xform, src->getXform() );			// shape transform * object transform

		mID++;		// traversal order ID
	}
}
 

void RenderBase::InsertAndSortShapes ()
{
	Scene* scn = getRenderMgr()->getScene ();
	Object *obj;

	// Step 1. Insert all shapes
	PERF_PUSH ("  Insert");
	int x = 0;
	int y = 0;	
	int maxadd = 0;
	ResetNodes ();		// clear BST	
	mShapeCnt = 0;
	mID = 0; 

	// traverse scene graph	
	for (int n = 0; n < scn->getNumScene(); n++) {					
		obj = scn->getSceneObj(n);		
		InsertShapes ( obj->getOutputShapes(), x, y );
		x=0; y++;		
	}
	PERF_POP(); 
	
	// Step 2. Prefix scan shape groups
	PERF_PUSH("  Scan");
	mShapeCnt = PrefixScanShapes();
	PERF_POP();

	// Step 3. Sort shapes into bins
	PERF_PUSH("  Sort");
	mSB.ResizeBuffer(BSHAPES, mShapeCnt );		// resize the shape buffer (destructively)
	mSB.ResizeBuffer(BXFORMS, mShapeCnt );
	mSB.SetNum(mShapeCnt);
	mChkSumL = mChkSum;
	mChkSum = 0;
	mID = 0;
	for (int n = 0; n < scn->getNumScene(); n++) {
		obj = scn->getSceneObj(n);				
		SortShapes ( obj->getOutputShapes(), obj->getXform() );
	}
	PERF_POP();

}


		