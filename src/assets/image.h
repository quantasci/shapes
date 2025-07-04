
#ifndef DEF_IMAGEX
	#define DEF_IMAGEX

	#include <string>	
	#include "imagex.h"	
	#include "object.h"
	#include "dataptr.h"
	

	class Image : public ImageX, public Object {
	public:
		virtual objType getType() { return 'Aimg'; }

		Image () : ImageX() {};
		Image ( int xr, int yr, ImageOp::Format fmt, uchar use_flags=DT_CPU ) : ImageX(xr,yr,fmt,use_flags) {};
		Image ( std::string name, int xr, int yr, ImageOp::Format fmt ) : ImageX(name,xr,yr,fmt) {};

		virtual bool Load(std::string fname) { 
			bool result = ImageX::Load(fname);				// load image
			if (result) {
				SetFilter ( ImageOp::Filter::Linear );		// default filtering
				
				Commit ( DT_CPU | DT_GLTEX );
			}
			return result;
		}
	};


#endif




