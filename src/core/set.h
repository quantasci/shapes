

#ifndef DEF_SET
	#define DEF_SET

	#ifdef DEBUG_HEAP
            #ifdef _WIN32
		#ifndef _CRTDBG_MAP_ALLOC  
			#define _CRTDBG_MAP_ALLOC  
		#endif	
		#include <stdlib.h>  
		#include <crtdbg.h> 
            #endif
	#else
		#include <stdlib.h>  
	#endif
	#include "vec.h"

	class Set {
	public:
		Set();
		~Set();

		void		Prepare ( int initmax, int stride, bool owner=true, bool preserve=false );
		void		Clear ();		// frees the memory pool
		void		Empty ();
		int			Add ();
		int			Add ( void* obj );
		void		Delete ( int i );
		void		CopyFrom ( Set* src );

		int			getNum ()			{ return mNum; }
		int			getMax ()			{ return mMax; }
		int			getStride ()		{ return mStride; }
		char*		getElem (int i )	{ return mData + i*mStride; }
		char*		getData()			{ return mData; }
		int			getSize()			{ return mNum*mStride; }

	public:

		int			mNum, mMax;
		int			mStride;
		char*		mData;

		bool		bOwner;
		bool		bPreserveOrder;

	};

#endif
