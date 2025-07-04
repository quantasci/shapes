

#ifndef DEF_MOTIONCYCLES
	#define DEF_MOTIONCYCLES

	#include "object.h"
	#include "character.h"			
	#include "vec.h"
	#include <vector>

	class ImageX;
	
	typedef std::vector<Vec3I>	TransitionList;

	struct Cycle {
		char		name[64];
		char		group[64];
		int			frames;
		int			joints;
		int			frameskip;

		// tables: col = frame#, row = jointID
		Vec3F*	pos_table;		// joint pos, 		
		Vec3F*	ang_table;		// joint angles, BVH format (deleted after loading)		
		Quaternion*	orient_table;	// joint orientation, Ri'

		Vec3F*	vis;

		TransitionList	transitions;
	};		

	class MotionCycles : public Object {
	public:
		MotionCycles ();
		~MotionCycles();
		void Clear();

		virtual objType getType()	{ return 'mcyc'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand (std::string cmd, vecStrs args);
		
		// BVH Loading
		void LoadTPose (std::string fname);
		int LoadBVH (std::string fname);
		void RefactorBVH(std::vector<Joint>& joints, Cycle* cy, int curr_jnt, int frame, Quaternion Oparent);
		void RemoveAllBVH();

		// Cycle operations
		int AddCycle ( std::string name, int nframes, int njoints, int frameskip );
		int SplitCycle ( std::string name, int src, int fin, int fout );
		void DeleteCycle ( int i );		
		void ListCycles ();
		int FindCycle ( std::string name );		
		float getHipAngle ( Matrix4F& mtx );		
		void getInterpolatedJoint ( float t, Cycle* cy, Vec3F& p, Quaternion& q );
		void AssignCycleToJoints ( int cyc, JointSet& joints, float t );
		void AssignCycleToJoints ( int cyc, JointSet& joints, float t, float dt,Vec3F& cp, Vec3F& cpl, Quaternion& cov, Quaternion& covl );
		Vec3I FindTransition(int ci, int cj, int frame);

		Quaternion ComputeOrientFromBoneVec(Vec3F v);
		
		//---------- Advanced cycle operations
		//void CreateTempChars(char* bvh1, char* bvh2);
		//void FinishTempChar ();
		//float ComparePoses(int i, int iframe, int j, int jframe);
		//void CompareCycles(int i, int j, std::string fname);
		//void CompareCycles(std::string iname, std::string jname);
		//void IdentifyTransitions(int ci, int cj, ImageX& img);

		void GenerateVis ( int cyid );
		
		void SetGroup ( std::string name );
		int RandomCycleInGroup ( std::string name );		
		
		int getNumCycles ()			{ return (int) m_Cycles.size(); }
		Cycle* getCycle ( int i )	{ return (i==OBJ_NULL) ? 0 : &m_Cycles[i]; }
		int getCycleEnd ( int i )	{ return (i==OBJ_NULL) ? 0 : m_Cycles[i].frames; }
		char* getName ( int i )		{ return m_Cycles[i].name; }
		Quaternion getCycleFrameOrientation ( int cid, int f );
		Vec3F getCycleFramePos ( int cid, int f );

	private:

		std::vector<Cycle>		m_Cycles;
		std::string				m_CurrGroup;

		Character*				m_TPose;

		//Character*			m_Char1;			// template characters
		//Character*			m_Char2;
	};

#endif