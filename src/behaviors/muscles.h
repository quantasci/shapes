

#ifndef DEF_MUSCLES
	#define DEF_MUSCLES

	#include "object.h"	
	#include <vector>

	#define BMUSCLE		10		// muscle shape extra data

	struct Muscle {		
		char		name[128];
		Vec3F	p1, p2;		// attachment points (world units)
		Vec3I	b1, b2;		// bones connected
		Vec3F	specs;		// muscle dimensions (width, length, hgt)
		Vec3F	norm;		// up direction
		float		rest_len;	// rest length
		float		angle;		// up orientation
		float		curve;		// muscle curvature
	};

	class Muscles : public Object {
	public:
		Muscles() : Object() { }

		virtual objType getType()	{ return 'musl'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);	
		virtual void Run ( float time );
		virtual void Sketch ( int w, int h, Camera3D* cam );
		virtual bool Select3D(int w, Vec4F& sel, std::string& name, Vec3F orig, Vec3F dir);
		virtual void Adjust3D(int w, Vec4F& sel);
		
		void Clear();
		void Save(std::string fname);
		bool Load(std::string fname);

		int AddMuscle(Vec3F p1, Vec3F p2, Vec3I m1, Vec3I m2, Vec3F norm);
		void DeleteMuscle(int i);
		Muscle* getMuscle(int n) { return &mPartList[n]; }


	private:		

		std::vector< Muscle >		mPartList;
		
		Vec3I					mSel;
	};

#endif