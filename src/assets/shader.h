
#ifndef DEF_SHADER
	#define DEF_SHADER

	#include <string>		
	#include "vec.h"
	#include "object.h"

	class Shader : public Object {
	public:
		Shader ();
		virtual objType	getType()		{ return 'Ashd'; }		
		virtual void	Clear ();		
		virtual bool	Load ( std::string fname )	{ return true; }			// shaders are defer loaded for when there is a GL context

		//-- member vars

		// mFilename contains shader name, inherited from Object

	};

#endif


