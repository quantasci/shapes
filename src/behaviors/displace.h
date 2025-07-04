

#ifndef DEF_DISPLACE
	#define DEF_DISPLACE

	#include "object.h"	
	#include <vector>

	class Displace : public Object {
	public:
		Displace();

		virtual objType getType()	{ return 'disp'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);

		void BuildSubdiv ();

	private:		
	
	};

#endif