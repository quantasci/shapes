

#ifndef DEF_OBJ_MESH
	#define DEF_OBJ_MESH

	#include <string>
	#include <vector>
	#include <map>	

	#include "meshx.h"	
  #include "object.h"
	
	class Mesh : public MeshX, public Object {
	public:
		Mesh () : MeshX() { 
			AddParam(0,"tex","i"); 
			SetTexture(-1); 
		}
			
		virtual objType getType()							{ return 'Amsh'; }		
		virtual Vec4F GetStats()							{ 
			return MeshX::GetStats(); 
		}
		virtual bool Load(std::string fname)	{ return MeshX::Load(fname); }

		void SetTexture(int id)  { SetParamI(0, 0, id); }
	
	};


#endif

