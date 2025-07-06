
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
