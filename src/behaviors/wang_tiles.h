
#ifndef DEF_WANGTILES
	#define DEF_WANGTILES

	#include "vec.h"

	struct WangTile
	{
		int n, e, s, w;
		int numSubtiles, numSubdivs, numPoints, numSubPoints;
		int ** subdivs;
		Vec2F *points;
		Vec2F *subPoints;
	};
	
	class Camera3D;
	class Image;

	class WangTiles {
	public:
		WangTiles ();
		
		bool	LoadTileSet ();
		void	SetDensityFunc ( uchar* density, int xres, int yres, Vec3F sz );
		void	SetDensityImg ( Image* img, Vec3F sz);
		void	SetMaxPoints (int m );
		bool	isReady() { return mDensityFunc != 0x0; }

		int		RecurseTileImage ( Vec2F cmin, Vec2F cmax, float zm, float ts );
		void	RecurseTileImage ( WangTile & t, float x, float y, int level);

		int		Recurse3D ( Camera3D* cam, float zm, float ts, float dst );
		void	Recurse3D ( WangTile& t, float x, float y, int level);
		//void	Recurse3D ( Camera3D* cam, float ts );

		int numPnts ()				{ return mNumPnts; }
		Vec3F getPnt ( int n )	{ return mPoints[n]; }

	private:
		
		float		mZoom, mDist, mDSQ2;
		Vec2F	mClipMin, mClipMax;
		Camera3D*	mCam;

		uchar*		mDensityFunc;				// input density function
		int			mStride;
		Vec3F	mRes;
		Vec3F	mSize;
	
		int			mNumPnts, mMaxPnts;		// output points 
		Vec3F*	mPoints;
		Vec3F*  mCurrPnt;

		WangTile*	mTiles;					// wang tile data
		int			numTiles;
		int			numSubtiles;
		int			numSubdivs;

		float		toneScale;
	};

#endif


