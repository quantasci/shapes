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

#include "common_defs.h"

#include "wang_tiles.h"
#include "image.h"
#include "camera3d.h"

#include <stdio.h>
#include <stdlib.h>

/*
	Copyright 2006 Johannes Kopf (kopf@inf.uni-konstanz.de)
	Implementation of the algorithms described in the paper

	Recursive Wang Tiles for Real-Time Blue Noise
	Johannes Kopf, Daniel Cohen-Or, Oliver Deussen, Dani Lischinski
	In ACM Transactions on Graphics 25, 3 (Proc. SIGGRAPH 2006)
*/

WangTiles::WangTiles ()
{		
	mTiles = 0x0;
	mDensityFunc = 0x0;
	mPoints = 0x0;
	toneScale = 1.0;

	SetMaxPoints ( 1000000 );		// allocate output points
}

int freadi(FILE* fp)
{
	int v;
	fread(&v, sizeof(int), 1, fp );
	return v;	
}
float freadf(FILE* fp)
{
	float v;
	fread(&v, sizeof(float), 1, fp);
	return v;
}
int isqrt(int val)
{
	unsigned long temp, g = 0, b = 0x8000, bshft = 15;
	do {
		if (val >= int(temp = (((g << 1) + b) << bshft--)) ) {
			g += b;
			val -= temp;
		}
	} while (b >>= 1);
	return g;
}

bool WangTiles::LoadTileSet()
{
	std::string filename = "wang_tileset.dat";
	std::string filepath;

	if ( !getFileLocation ( filename, filepath ) ) {
		dbgprintf ( "ERROR: Unable to locate %s\n", filename.c_str() ); 
		return false;
	}

	FILE * fin = fopen(filepath.c_str(), "rb");
	if ( fin==0x0 ) return false;
	
	numTiles = freadi(fin);
	numSubtiles = freadi(fin);
	numSubdivs = freadi(fin);

	mTiles = new WangTile[numTiles];

	for (int i = 0; i < numTiles; i++)
	{
		mTiles[i].n = freadi(fin);
		mTiles[i].e = freadi(fin);
		mTiles[i].s = freadi(fin);
		mTiles[i].w = freadi(fin);

		mTiles[i].subdivs = new int * [numSubdivs];
		for (int j = 0; j < numSubdivs; j++) {
			int* subdiv = new int[numSubtiles * numSubtiles];
			for (int k = 0; k < numSubtiles * numSubtiles; k++)
				subdiv[k] = freadi(fin);
			mTiles[i].subdivs[j] = subdiv;
		}

		mTiles[i].numPoints = freadi(fin);		
		mTiles[i].points = new Vec2F [mTiles[i].numPoints];
		for (int j = 0; j < mTiles[i].numPoints; j++)
		{
			mTiles[i].points[j].x = freadf(fin);
			mTiles[i].points[j].y = freadf(fin);
			freadi(fin); freadi(fin); freadi(fin); freadi(fin);
		}

		mTiles[i].numSubPoints = freadi(fin);
		mTiles[i].subPoints = new Vec2F[mTiles[i].numSubPoints];
		for (int j = 0; j < mTiles[i].numSubPoints; j++)
		{
			mTiles[i].subPoints[j].x = freadf(fin);
			mTiles[i].subPoints[j].y = freadf(fin);
			freadi(fin); freadi(fin); freadi(fin); freadi(fin);
		}
		dbgprintf ( "Wang Tile: %d, subt: %d, pnts:%d, subpnt:%d\n", i, numSubtiles, mTiles[i].numPoints, mTiles[i].numSubPoints );
	}	
	fclose(fin);
	
	return true;
}

void WangTiles::SetMaxPoints ( int p )
{
	mMaxPnts = p;
	if (mPoints != 0) delete mPoints;
	mPoints = new Vec3F[mMaxPnts];
}

void WangTiles::SetDensityFunc(uchar* density, int xres, int yres, Vec3F sz)
{
	mDensityFunc = density;
	mStride = sizeof(uchar);
	mRes = Vec3F(xres, yres, 0);
	mSize = sz;
}
void WangTiles::SetDensityImg ( Image* img, Vec3F sz )
{
	mDensityFunc = (uchar*) img->GetData();
	mStride = img->GetBytesPerPix();
	mRes = Vec3F(img->GetWidth(), img->GetHeight(), 0);
	mSize = sz;
}

int WangTiles::RecurseTileImage ( Vec2F cmin, Vec2F cmax, float zm, float ts )
{
	if (mDensityFunc==0x0) dbgprintf ( "ERROR: Wang tiles has no density image.\n" );
	if (mMaxPnts== 0) dbgprintf ("ERROR: Wang tiles has no max points.\n");	
	
	mClipMin = cmin;
	mClipMax = cmax;
	mZoom = 1.0 / zm;
	toneScale = ts * 256.0;

	mNumPnts = 0;
	mCurrPnt = mPoints;

	RecurseTileImage ( mTiles[0], 0, 0, 0);

	return mNumPnts;
}

void WangTiles::RecurseTileImage (WangTile & t, float x, float y, int level )
{
	float tileSize = 1.f / powf(float(numSubtiles), float(level));
	
	if ((x+tileSize < mClipMin.x) || (x > mClipMax.x) || (y+tileSize < mClipMin.y) || (y > mClipMax.y))
		return;

	float factor = toneScale * tileSize*tileSize / mZoom;
	int tilePnts = imin(t.numSubPoints, int( factor - t.numPoints ) );
		
	if ( mNumPnts + tilePnts > mMaxPnts )
		return;

	// generate points
	float px, py, v;
	Vec3F* basepnt = mCurrPnt;

	for (int i = 0; i < tilePnts; i++)
	{
		px = (x + t.subPoints[i].x*tileSize) * mSize.x;
		py = (y + t.subPoints[i].y*tileSize) * mSize.z;
		// skip point if it lies outside of the clipping window
		if ((px < mClipMin.x) || (px > mClipMax.x) || (py < mClipMin.y) || (py > mClipMax.y))
			continue;

		// evaluate density function
		v = *(mDensityFunc + (int(py * (mRes.y/mSize.z)) * int(mRes.x) + int(px * (mRes.x/mSize.x)))*mStride) / 255.0f;
		if ( v*v <= i / factor )
			continue;

		// output point			
		mCurrPnt->Set ( px, py, level );
		mCurrPnt++;
	}
	mNumPnts += (mCurrPnt - basepnt);

	// recursion
	if (factor - t.numPoints > t.numSubPoints) {
		for (int ty = 0; ty < numSubtiles; ty++)
			for (int tx = 0; tx < numSubtiles; tx++)
				RecurseTileImage(mTiles[t.subdivs[0][ty * numSubtiles + tx]], x + tx * tileSize / numSubtiles, y + ty * tileSize / numSubtiles, level + 1);
	}
}


int WangTiles::Recurse3D(Camera3D* cam, float zm, float ts, float dst )
{
	// setup clipping to camera bounding box
	Vec3F ca, cb;
	mCam = cam;
	mCam->getBounds ( Vec2F(0,0.5), Vec2F(1,0.5), dst, ca, cb );
	mClipMin = Vec3F(ca.x, ca.z, 0);
	mClipMax = Vec3F(cb.x, cb.z, 0);

	mZoom = zm;
	mDist = dst;								// maximum distance for point generation
	mDSQ2 = mDist * mDist * 0.25 * 0.25;		// fractional distance (25%) for full density
	toneScale = ts * 256.0;

	mNumPnts = 0;
	mCurrPnt = mPoints;

	Recurse3D(mTiles[0], 0, 0, 0);

	return mNumPnts;
}

void WangTiles::Recurse3D(WangTile& t, float x, float y, int level)
{
	float tileSize = 1.f / powf(float(numSubtiles), float(level));

	// transform tile to world space
	Vec3F a(x * mSize.x, 0, y * mSize.z);
	Vec3F b = a + Vec3F(tileSize * mSize.x, 1000, tileSize * mSize.z);

	// check if tile is outside the camera bounding box
	if ((b.x < mClipMin.x) || (a.x > mClipMax.x) || (b.y < mClipMin.y) || (a.y > mClipMax.y))		
		return;
	
	// check if tile is outside the camera frustum
	if (!mCam->boxInFrustum(a, b))
		return;

	float factor = toneScale * tileSize * tileSize / mZoom;
	int tilePnts = imin(t.numSubPoints, int(factor - t.numPoints));

	if (mNumPnts + tilePnts > mMaxPnts)
		return;

	// generate points
	float px, py;
	float dist, distfactor = 1.0;
	Vec3F* basepnt = mCurrPnt;

	float v;
	for (int i = 0; i < tilePnts; i++)
	{
		// candidate point, transformed to world space
		px = (x + t.subPoints[i].x * tileSize) * mSize.x;
		py = (y + t.subPoints[i].y * tileSize) * mSize.z;

		// skip point if it lies outside of camera bounding box
		if ((px < mClipMin.x) || (px > mClipMax.x) || (py < mClipMin.y) || (py > mClipMax.y))
			continue;

		// distance to candidate
		dist = (Vec3F(px, 0, py) - Vec3F(mCam->from_pos.x, 0, mCam->from_pos.z) ).LengthSq();
		if ( dist >= mDist*mDist) 
			continue;
		
		// full density below threshold fractional distance,
		// otherwise falloff density with distance
		distfactor = (dist < mDSQ2) ? 1.0 : 1.0 + (distfactor / mDSQ2);

		// evaluate density function
		v = * (mDensityFunc + (int(py * (mRes.y/mSize.z)) * int(mRes.x) + int(px * (mRes.x / mSize.x))) * mStride) / 255.0f;
		if (v * v <= distfactor * i / factor)
			continue;

		// output point			
		mCurrPnt->Set(px, py, dist );
		mCurrPnt++;
	}
	mNumPnts += (mCurrPnt - basepnt);

	// recursion
	if (factor - t.numPoints > t.numSubPoints) {
		for (int ty = 0; ty < numSubtiles; ty++)
			for (int tx = 0; tx < numSubtiles; tx++)
				Recurse3D (mTiles[t.subdivs[0][ty * numSubtiles + tx]], x + tx * tileSize / numSubtiles, y + ty * tileSize / numSubtiles, level + 1);
	}
}
