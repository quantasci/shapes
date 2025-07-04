

#ifndef DEF_LOFT
	#define DEF_LOFT

	#include "object.h"	
	#include "mersenne.h"
	#include <vector>

	class Mesh;

	struct LoftGroup {
		LoftGroup()	{ parent=OBJ_NULL; }
		int			parent;			// parent shape 		
		Vec3I	variant;		// variant #
		Vec3F	scaling;
		
		std::vector<int>	links;		// segment IDs		
	};

	class Loft : public Object {
	public:
		Loft();

		virtual objType getType()	{ return 'loft'; }
		virtual void Define (int x, int y);
		virtual void Generate (int x, int y);
		virtual void Run ( float time );		
		virtual void Render ();
		virtual void Sketch ( int w, int h, Camera3D* cam );
		virtual bool Select3D(int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir);
		virtual Vec3F getStats();

		void ConstructSection(int ures, Vec3F norm);
		void ConstructMeshes (Vec3I res, std::string srcname, std::string name);

		void GenerateIndividual ();			// generate a loft profile for each individual shape
		void GenerateGrowth (float time);	// generate a segmented profile that grows over time
		int  GenerateComplete (int w);		// generate a final segmented profile (no animation)

		void CountChainsAndLengths();
		void getLoftSection(float v, int w, Matrix4F& xform, Quaternion& xrot, float& sz, uint& clr);
		Vec3I getMeshRes();

	private:		

		bool			m_bSegments;
		Vec3F*		m_Section;
		Vec3F*		m_SectionNorm;

		std::vector<LoftGroup>	m_lofts;
		std::vector<Mesh*>		m_meshes;			// performance pointer		

		Shapes*			m_shapes;		

		Mersenne		m_rand;
	};

#endif