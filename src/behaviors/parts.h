

#ifndef DEF_PARTS
	#define DEF_PARTS

	#include "object.h"	
	#include <vector>


	struct Part {
		int				asset_id;			// asset ID
		char			asset_name[256];	// asset name
		char			append[256];		// appended to name
		char			extra[256];			// extra info
		uchar			inst;				// instance #
		Vec3F		mirror;				// mirroring
	};

	class Parts : public Object {
	public:
		Parts() : Object() { }

		virtual objType getType()	{ return 'prts'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);	
		virtual void Sketch ( int w, int h, Camera3D* cam );
		virtual bool Select3D (int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir);
		virtual void Adjust3D (int w, Vec4F& sel);

		void Clear();
		void Save ( std::string fname );
		bool Load ( std::string fname, float scale);
		void LoadFolder ( std::string path );

		bool Raytrace(Vec3F pos, Vec3F dir, Vec3I& sel, Vec3F& hit, Vec3F& norm);

		void RescaleAll ( Vec3F s );
		void PlaceParts ( Vec3F vmin, Vec3F vmax, float spacing );

		void AddPart(std::string mesh_name, int mid, std::string append, std::string extra, int inst);

		/*void Load ( char* fname );
		void AddPart ( char* mesh, Vec3F pos, Vec3F scale, Vec4F texsub );
		void AssignTexture ( char* name );*/
		std::string getMeshName(int n)		{ return mPartList[n].asset_name; }
		std::string getExtra(int n)			{ return mPartList[n].extra; }
		
	private:		

		std::vector< Part >			mPartList;
		float						mUniformSz;

		Vec3I					mSel;
	};

#endif