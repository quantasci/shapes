

#include "params.h"

// AddParam
// add parameter by name, using the last slot
// 
int Params::AddParam (std::string name, std::string types)
{
	int slot = mPM.size();

	AddParam ( slot, name, types);	
	return slot;
}

// AddParam
// add parameter at specific slot
// - slot  - index of param
// - name  - name of the param entry
// - types - create storage for sub-param types. eg. 'i3', one int and one vec3f
// 
void Params::AddParam(int slot, std::string name, std::string types)
{
	Param pm;
	strncpy (pm.key, name.c_str(), 64);

	if ( slot < mPM.size() ) {
		dbgprintf ( "ERROR: AddParam slot already used. slot %d, new %s\n", slot, name.c_str() );
		exit(-76);
	}

	// param storage
	// struct param { char	key[64]; ushort	sz, cnt; uchar* val;}  -- see params.h
	// 
	// param members:   key   |  sz   |  cnt  |  val*
	// param sizes:     64    |   2   |   2   |
	// val indexing:                          |  OFFSETS 0-3  |  TYPES 4-7    |                      VALUES                                |
 	// val sizes:                             | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |      4     |    4       |      11       |        12        |
	// val entries:                           |o0 |o1 |o2 |o3 |t0 |t1 |t2 |t3 |     int    |    int     |      str      |      vec3        |

	int cnt = types.length();
	char t[32];
	char o[32];

	// reserve space for the offsets (1 byte) & types (1 byte)
	int offset = cnt * 2;			

	// prefix scan for offsets
	for (int n = 0; n < cnt; n++) {
		o[n] = offset;				// record offsets
		t[n] = types.at(n);			// record types
		if (types.at(n) == 'f') offset += sizeof(float);
		if (types.at(n) == 'i') offset += sizeof(int);
		if (types.at(n) == '3') offset += sizeof(Vec3F);
		if (types.at(n) == '4') offset += sizeof(Vec4F);
		if (types.at(n) == 'I') offset += sizeof(Vec3I);
		if (types.at(n) == 's') offset += 1;
	}
	pm.sz = offset;						// total accumulated size (prefix sum)
	pm.cnt = cnt;

	// allocate param data here
	pm.val = (uchar*) malloc (pm.sz);

	// insert types and offsets into storage
	memset(pm.val, 0, pm.sz);
	for (int n = 0; n < cnt; n++) {
		pm.val[n] = o[n];			// offsets
		pm.val[n+cnt] = t[n];		// types
	}

	while (slot >= mPM.size())
		mPM.push_back(pm);

	mPM[slot] = pm;

}

int Params::AddParamByCopy( int baseslot, std::string name)
{
	Param base = mPM[baseslot]; 

	// reconstruct types	
	std::string types;
	for (int n=0; n < base.cnt; n++) {
		types += base.val[n + base.cnt];
	}
	// slot for new param
	int slot = mPM.size();		

	AddParam ( slot, name, types );

	return slot;
}

// Parameter arrays
std::string Params::getArrayBase ( std::string name, int& i )
{
	i = -1;
	std::size_t pos = name.find_first_of ( "[" );
	if ( pos==std::string::npos ) return name;
	i = strToI ( name.substr(pos+1) );
	return name.substr ( 0, pos );
}

int Params::FindOrCreateParam (std::string array_name, std::string val, uchar vecsep) 
{
	int slot = -1, arrayndx;	
	std::string types;
	std::string name = getArrayBase (array_name, arrayndx);				// name w/o array operator

	for (int n=0; n < mPM.size(); n++) {
		if ( array_name.compare ( mPM[n].key )==0 ) return n; 		// exact match to array name
		if ( name.compare ( mPM[n].key )==0 ) slot = n;				// name match 
	}
	// name match 
	if ( slot != -1 ) {										// slot for base name
		if ( arrayndx != 0 )								// if arrayndx=0 use slot directly (eg. light == light[00])
			slot = AddParamByCopy ( slot, array_name );		// add array entry by copy (eg. light[01] copy type from light)
	} else {
		types = std::string(1, getTypeFromValue(val, vecsep) );		// determine type from value
		slot = AddParam ( name, types );					// add new param
	}
	return slot;
}

int	Params::GetParamArray ( std::string base_name, std::vector<int>& list )
{
	int i;	
	std::string base;
	
	list.clear ();
	for (int n=0; n < mPM.size(); n++) {
		base = getArrayBase(mPM[n].key, i );
		if ( base.compare ( base_name )==0 ) {	// base names match
			list.push_back ( n );
		}
	}
	return list.size();	
}

int Params::FindParam(std::string name)
{
	for (int n=0; n < mPM.size(); n++) {
		if ( name.compare ( mPM[n].key )==0)
			return n;
	}
	return -1;
}	


void Params::SetParamStr ( int slot, int i, std::string str )
{
	Param& pm = mPM[slot];
	int off1 = pm.val[i];			
	int off2 = (i==pm.cnt-1) ? pm.sz-1 : pm.val[i+1];
	int currlen = off2 - off1;
	if ( currlen < (int) str.length() ) {
		// reallocate param data 
		int incr = str.length() - currlen + 1;		
		uchar* newdata = (uchar*) malloc( pm.sz + incr );
		memcpy ( newdata, pm.val, off1 );							// copy original params
		if ( i < pm.cnt-1 ) {
			memcpy ( newdata + off2, &pm.val[off2], pm.sz - off2 );	// copy remaining params (shifted to make room)
			for (int j=i+1; j < pm.cnt; j++) {
				newdata[j] += incr;									// adjust offsets (excluding current one)
			}
		}
		free ( pm.val );
		pm.val = newdata;
		pm.sz += incr;
	}
	strcpy ( (char*)&pm.val[off1], str.c_str() );
}

std::string Params::getParamStr(int slot, int i)
{
	Param& pm = mPM[slot];
	return std::string ( (char*) &pm.val[pm.val[i]] );
}

std::string fStr ( float f ) 
{
	if ( int(f) == f ) return iToStr( f );
	return fToStr( f );
}

uchar Params::getTypeFromValue ( std::string val, uchar vecsep)
{
	std::string v = strTrim( val );

	if ( v[0]=='<' ) {
		int seps = 0;
		for (int i = 0; i < v.size(); i++)
			if ( v[i] == vecsep ) seps++;
		if ( seps==2 ) return '3';		// vec3
		if ( seps==3 ) return '4';		// vec4
	} else {
		size_t p = v.find_first_not_of ( ".0123456789+-" ); 
		if ( p != std::string::npos ) {	
			return 's';			// string
		} else {
			return 'f';			// float
		}
	}
	return '?';
}

std::string Params::getParamValueAsStr ( int slot )
{
	Vec3F v;	
	Vec3I vi;
	Vec4F w;

	if ( slot >= mPM.size()) return "";

	Param& pm = mPM[slot];

	// concatenate sub-params
	std::string result="", s;

	for (int k=0; k < mPM[slot].cnt; k++) {
		int o = pm.val[k];
		uchar t = getType(slot, k);

		switch (t) {
		case 'f':	s = fStr ( *((float*)&pm.val[o]) );	break;
		case 'i':	s = iToStr ( *((int*)&pm.val[o]) );		break;
		case '3':	v = *((Vec3F*) &pm.val[o]);	s = "<"+ fStr(v.x) + ","+fStr(v.y)+","+fStr(v.z)+">";	break;
		case '4':	w = *((Vec4F*) &pm.val[o]); s = "<"+ fStr(w.x) + ","+fStr(w.y)+","+fStr(w.z)+","+fStr(w.w)+">";		break;
		case 'I':	vi  = *((Vec3I*) &pm.val[o]); s = "<" +iToStr(vi.x) + ","+iToStr(vi.y)+","+iToStr(vi.z)+">";   break;
		case 's':	s = getParamStr(slot, k);	break;
		};

		result = (k==0) ? s : result + ", " + s;
	}
	return result;
}

void Params::SetParam( int slot, std::string val, uchar vecsep, int k )
{
	uchar t = getType(slot, k);		// k = sub-param
	Param& pm = mPM[slot];
	int o = pm.val[k];			// offset
	switch (t) {
	case 'f': *((float*)&pm.val[o]) = strToF(val);	break;
	case 'i': *((int*)&pm.val[o]) = strToI(val);	break;
	case '3': *((Vec3F*) &pm.val[o]) = strToVec3(val, vecsep);	break;
	case '4': *((Vec4F*) &pm.val[o]) = strToVec4(val, vecsep);	break;
	case 'I': *((Vec3I*) &pm.val[o]) = (Vec3I) strToVec3(val, vecsep);	break;
	case 's': SetParamStr ( slot, k, val );			break;
	}
}

void Params::SetParam(int slot, Vec3F val, int k)
{
	uchar t = getType(slot, k );
	Param& pm = mPM[slot];
	int o = pm.val[k];
	switch (t) {
	case 'f': *((float*)&pm.val[o]) = val.x;	break;
	case 'i': *((int*)&pm.val[o]) = int(val.x);	break;
	case '3': *((Vec3F*)&pm.val[o]) = val;	break;
	case '4': *((Vec4F*)&pm.val[o]) = Vec4F(val, 1);	break;
	case 'I': *((Vec3I*)&pm.val[o]) = (Vec3I) val;	break;
	}
}

void Params::SetParam( std::string name, Vec3F val, int k )
{
	int slot = FindParam(name);
	if ( slot==-1 ) return;
	SetParam ( slot, val, k );
}

int Params::getCount(int slot)
{
	return mPM[slot].cnt;
}
uchar Params::getType(int slot, int i)
{
	Param& pm = mPM[slot];
	return pm.val[i + pm.cnt];
}
void Params::ParamError ( int slot, int i) 
{
	dbgprintf ( "ERROR: Parameter error. %s, slot %d, sub %d\n", mPM[slot].key, slot, i );
	exit(-77);
}

void Params::SetParamI(int slot, int i, int val)
{
	if ( getType(slot,i) != 'i' ) ParamError(slot, i);
	Param& pm = mPM[slot];
	int p = pm.val[i];			// offset
	*((int*)&pm.val[p]) = val;
}

void Params::SetParamF(int slot, int i, float val)
{
	if (getType(slot, i) != 'f') ParamError(slot, i);
	Param& pm = mPM[slot];
	int p = pm.val[i];			// offset
	*((float*)&pm.val[p]) = val;
}
void Params::SetParamI3 (int slot, int i, Vec3I val)
{
	if (getType(slot, i) != 'I') ParamError(slot, i);
	Param& pm = mPM[slot];
	int p = pm.val[i];			// offset
	*((Vec3I*)&pm.val[p]) = val;
}

void Params::SetParamV3 (int slot, int i, Vec3F val)
{
	if (getType(slot, i) != '3') ParamError(slot, i);
	Param& pm = mPM[slot];
	int p = pm.val[i];			// offset
	*((Vec3F*)&pm.val[p]) = val;
}

void Params::SetParamV4 (int slot, int i, Vec4F val)
{
	if (getType(slot, i) != '4') ParamError(slot, i);
	Param& pm = mPM[slot];
	int p = pm.val[i];			// offset
	*((Vec4F*)&pm.val[p]) = val;
}

int Params::getParamI(int slot,int i)
{
	if (getType(slot, i) != 'i') ParamError(slot, i);
	Param& pm = mPM[slot];
	return *((int*) &pm.val[ pm.val[i] ]);
}
float Params::getParamF(int slot,int i)
{
	if (getType(slot, i) != 'f') ParamError(slot, i);
	Param& pm = mPM[slot];
	return *((float*) &pm.val[ pm.val[i] ]);
}
Vec3I Params::getParamI3(int slot,int i)
{
	if (getType(slot, i) != 'I') ParamError(slot, i);
	Param& pm = mPM[slot];
	return *((Vec3I*)&pm.val[pm.val[i]]);
}
Vec3F Params::getParamV3(int slot,int i)
{
	if (getType(slot, i) != '3') ParamError(slot, i);
	Param& pm = mPM[slot];
	return *((Vec3F*)&pm.val[pm.val[i]]);
}
Vec4F Params::getParamV4(int slot,int i)
{
	if (getType(slot, i) != '4') ParamError(slot, i);
	Param& pm = mPM[slot];
	return *((Vec4F*)&pm.val[pm.val[i]]);
}

