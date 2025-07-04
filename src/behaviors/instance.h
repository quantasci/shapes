
#ifndef DEF_INSTANCE

	#include "object.h"	
	#include "mersenne.h"
	#include "wang_tiles.h"
	#include <vector>

	class Instance : public Object {
	public:
		Instance();

		virtual objType getType() { return 'inst'; }
		virtual void Define(int x, int y);
		virtual void Generate(int x, int y);
		virtual void Run(float time);		

	private:
		
	};

#endif