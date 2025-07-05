
#ifndef DEF_VOLUME_H
	#define DEF_VOLUME_H

	#include "object.h"
	#ifdef USE_GVDB	
		#include "gvdb.h"
	#endif

	class LightSet;

	class Volume : public Object {
	public:
		Volume () { 
			#ifdef USE_GVDB
				gvdb = 0x0; 
			#endif	
		}
		~Volume () {};	
		virtual objType getType()		{ return 'Avol'; }		
		virtual void Clear() {};
		//virtual void Sketch (int w, int h, Camera3D* cam);		
		void Sketch3D (int w, int h, Camera3D* cam);

		void Initialize (int w, int h);
		void SetTransferFunc ();
		void Render (Camera3D* cam, LightSet* lset, int w, int h, int out_tex, int depth_tex=-1);

		#ifdef USE_GVDB
			VolumeGVDB* getGVDB() {return gvdb;}
		#endif	
	
	public:
		#ifdef USE_GVDB
			VolumeGVDB*		gvdb;
		#endif

		int				mTexRID;			// texture applied

		int				mRX, mRY;
		int				mRenderShade;
		int				mRenderChan;
	};

#endif