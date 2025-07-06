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

#include "set.h"
#include "common_defs.h"

#include <string.h>

Set::Set()
{ 
	mData = 0; 
	mNum=0; mMax=0; mStride = 0; 
}

Set::~Set()
{
	Clear();	
}

void Set::Clear ()
{
	if ( mData != 0x0 ) free ( mData );	
	mData = 0x0;
	mNum = 0;
	mMax = 0;
	if ( mStride != 0 ) Prepare ( 8, mStride );
}

void Set::Prepare ( int initmax, int stride, bool owner, bool preserve )
{
	if ( mData != 0x0 ) free ( mData );

	mNum = 0;
	mMax = initmax;
	mStride = stride;
	bOwner = owner;
	bPreserveOrder = preserve;

	mData = (char*) malloc ( mMax * mStride );
}

void Set::Empty ()
{
	mNum = 0;
}

int Set::Add ()
{
	if ( mNum >= mMax || mMax==0) {
		mMax = mMax*2 + 1;
		char* newdata = (char*) malloc ( mMax * mStride );
		if ( mData != 0x0 ) {
			memcpy ( newdata, mData, mNum*mStride );
			free ( mData );
		}
		mData = newdata;		
	}
	mNum++;
	return (mNum-1);
}

int Set::Add ( void* obj )
{
	int i = Add ();
	memcpy ( (char*) mData + i*mStride, (char*) obj, mStride );	
	return i;
}

void Set::Delete ( int i )
{
	if ( i < 0 || i >= mNum ) return;

	if ( bPreserveOrder ) {
		memmove ( (char*) mData + i*mStride, (char*) mData + (i+1)*mStride, (mNum-i-1)*mStride );
	} else {
		memcpy ( (char*) mData + i*mStride, (char*) mData + (mNum-1)*mStride, mStride );
	}
	mNum--;
}

void Set::CopyFrom ( Set* src )
{
	uxlong num = src->getNum();
	uxlong stride = src->getStride();

	Prepare ( num, stride );						// allocate memory (zeros num, sets max)
	mNum = num;

	memcpy ( mData, src->mData, num*stride );		// deep copy from source
}
