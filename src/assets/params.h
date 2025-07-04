
#ifndef DEF_PARAMS
	#define DEF_PARAMS

	#include <string>		
	#include "vec.h"
	#include "object.h"

	struct Param {		
		Param() { sz = 0; cnt = 0; val = 0; }
		char	key[64];		
		ushort	sz, cnt;
		uchar*	val;
	};	

	class Params : public Object {
	public:
		virtual objType	getType()		{ return 'Apms'; }							
		
		int			FindOrCreateParam (std::string name, std::string val, uchar vecsep );	
		int			AddParam ( std::string name, std::string types );					// Add param by name
		void		AddParam (int slot, std::string name, std::string types );			// Add param at specific slot		
		
		int			getNumParam()	{ return mPM.size(); }		
		int			FindParam(std::string name);		
		void		ParamError(int slot, int i);
		int			getCount ( int slot );
		uchar		getType ( int slot, int i );
		std::string getParamName(int i)	{ return (i <mPM.size() ? mPM[i].key : ""); }
		
		// Typed get/set
		void		SetParamI (int slot, int i, int val);
		void		SetParamF (int slot, int i, float val);
		void		SetParamI3(int slot, int i, Vec3I val);
		void		SetParamV3(int slot, int i, Vec3F val);
		void		SetParamV4(int slot, int i, Vec4F val);	
		int			getParamI(int slot,int i = 0);
		float		getParamF(int slot,int i = 0);
		Vec3F	getParamV3(int slot,int i = 0);
		Vec3I	getParamI3(int slot,int i = 0);
		Vec4F	getParamV4(int slot,int i = 0);
		std::string getParamStr(int slot, int i=0);

		void		SetParamStr(int slot, int i, std::string str);

		// Typeless get/set
		void		SetParam ( int slot, std::string val, uchar vecsep = ',', int k=0 );	// typeless set
		void		SetParam ( int slot,		 Vec3F val, int k=0 );						// typeless set
		void		SetParam ( std::string name, Vec3F val, int k=0 );
		std::string getParamValueAsStr ( int slot );
		uchar		getTypeFromValue ( std::string val, uchar vecsep );

		// Parameter arrays
		std::string getArrayBase ( std::string name, int& i );		
		int			AddParamByCopy( int baseslot, std::string name);
		int			GetParamArray ( std::string base_name, std::vector<int>& list );

	public:

		std::vector<Param>	mPM;

	};

#endif


