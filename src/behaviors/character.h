

#ifndef DEF_CHARACTER
	#define DEF_CHARACTER

	#include "object.h"	
	#include <vector>
	#include <queue>
	#include "quaternion.h"
	#include "timex.h"

	#define MT_CYCLE		0				// motion types
	#define MT_SUBCYCLE		1
	#define MT_IK			2
	#define MT_FOLLOW		3

	#define M_UNKNOWN		0				// motion states
	#define	M_BASIC			0
	#define M_STAND			1	
	#define M_WALK			2
	#define M_RUN			4
	#define M_ACT			8
	#define M_TURN			16
	#define M_TARGET		32		
	#define M_PATH			64
	#define M_NOW			128	
	#define M_ASAP			256
	#define M_RESET_ROT		512
	#define M_RESET_POS		1024

	#define EVAL_IDENTITY		0			// no world transform (testing)
	#define EVAL_CHARACTER		1			// current character orientation
	#define EVAL_ORIENT			2			// joint set orientation
	#define EVAL_CYCLE			3			// cycle orientation
	#define EVAL_POSE			4			// cycle pose (no translation)
	
	#define JNTS_A				0
	#define JNTS_B				1	
	#define JNTS_C				2		
	#define JNTS_MAX			3

	// ***NOTE***
	// In the future, all joint orient & pos should be dualquats!! 
    // Get rid of Mworld, rename pos=>Wpos, Morient=>Worient, orient=>Lorient, bonepiv=>Lpos, bonesize=>Lsize  (W=world, L=local)

	struct Joint {
		char		name[64];
		char		lev;
		uint		clr;
		float		length;		// bone length, v''
		Vec3F	pos;		// world bone position, T
		Quaternion	orient;		// local orientation angles, Ri (converted to ideal format, Mv = T Ri' v'', see Meredith & Maddock paper)
		Quaternion  Morient;	// world orientation
		Matrix4F	Mworld;		// world transform (computed on-the-fly)
		Vec3F	bonepiv;	// bone shape pivot point in local coordinate space
		Vec4F	bonesize;	// bone bounding box in local coordinate space		
		Vec4F	bonetexsub; // bone texture UV sub-domain
		int			parent;
		int			child;
		int			next;
		//-- debugging
		Vec3F	angs;		// BVH angles (debug purposes, will be removed in future)
		Vec3F	bonevec;	// BVH bone vec
	};
	typedef std::vector<Joint>		JointSet;

	struct Orientation {
		Vec3F		pos;
		Vec3F		vel;
		Quaternion		rot;
		Quaternion		rotvel;
		float			scale;	
	};

	struct Motion {	
		Motion()		{ states = M_UNKNOWN; cycle = OBJ_NULL;  }		
		
		int				id;		
		unsigned char	mtype;			// motion type
		unsigned int	states;			// state flags
		int				tslot;			// timeline slot
		bool			active;		
		float			speed;			// speed of motion (frames/sec)
		Vec3F		duration;		// time of motion

		int				cycle;			// cycle for this motion
		float			frame;			// current frame (fractional)
		float			frame_last;
		Vec3I		range;			// start & end of motion (in frames)
		Vec3I		interp;			// start & end of interpolation (in frames)
		Orientation		base_orient;	// base orientation (at start)		
	};	

	struct BindInfo {
		Dualquat	orig;			// bone original pos & rot (in T-pose)
		Vec3F	pos, norm;
		Dualquat	localrel[4];	// bone local relative pos & rot, in local space of the bound joint
		int			bind[4];
		float		wgts[4];		
		Vec3F	specs;			// muscle dimensions
		Vec3F	p1, p2;
		Vec3I	b1, b2;			// muscle to bone connections
	};
		
	class MotionCycles;
	class Scene;
	class Curve;

	class Character : public Object {
	public:
		Character () ;

		virtual objType getType()	{ return 'char'; }
		virtual void Define (int x, int y);
		virtual bool RunCommand(std::string cmd, vecStrs args);
		virtual void Generate (int x, int y);
		virtual void Run ( float time );
		virtual void Render ();
		virtual void Sketch ( int w, int h, Camera3D* cam );
		virtual Vec3F getPosition ();
		Vec3F getDir ();

		void DrawJoints ( int jset, int mode, bool boxes, bool axes, Camera3D* cam );
		void DrawCycle ( Motion& m );

		void LoadBVH ( std::string fname );							// Load joint hierarchy
		void LoadBoneInfo (std::string  fname );						// Load bone info file
		Vec3F ComputeBoneAngs ( Vec3F v );
				
		void SetHomePos ( Vec3F p )			{ m_HomePos = p; }
		void SetPos ( Vec3F p )				{ m_Orient[JNTS_A].pos = p; }
		void SetTarget (Vec3F h);		
			
		// Joints
		int AddJoint ( int s, int lev, int p );
		void CopyJoints( int src, int dst );
		void EvaluateCycleDerivatives ( int jset, int cyid, float frame, float df, Vec3F cp, Vec3F cpl, 
									Quaternion co, Quaternion col, Vec3F& cp_delta, Quaternion& co_delta );
		void EvaluateCycleInterpolation ( float t );
		void EvaluatePoseChange ( int frame, int cyid, Quaternion co, Quaternion& da );
		void EvaluateJoints ( int jset, int method );
		void EvaluateJoints ( JointSet& joints, Matrix4F& world, Quaternion& orient );						// Evaluate cycle to joints
		void EvaluateJointsRecurse ( JointSet& joints, int curr_jnt, Matrix4F tform, Quaternion orient );	// Update joint transforms		

		Joint* FindJoint (int jset, std::string name, int& j );
		Joint* getJoint (int s, int i)	{ return &m_Joints[s][i]; }
		JointSet& getJoints(int s)		{ return m_Joints[s]; }
		int getNumJoints (int s)		{ return (int) m_Joints[s].size(); }							
		int getLastChild ( int p );
		int getNumChannels ()			{ return (int) m_Channels.size();}
		Vec3I getChannel(int i)		{ return m_Channels[i]; }
		Vec3I FindChannel (int jnt, int typ);

		// Motions
		void AddMotion ( std::string name )		{ AddMotion ( name, 0, 2.0f ); }
		void AddMotion ( std::string name, unsigned int flags, float speed = 1.0f );
		void ResetMotions ( float t, bool tpose=false );
		void ProcessMotions ( float t );
		void PruneMotions ( float t, int dir=-1 );
		void EvaluateMotion ( float t, Motion& m );	
		void EvaluateCycleMotion ( float t, Motion& m, bool bPoseChange = false );
		void EvaluatePathMotion ( float t, Motion& m );
		void EvaluateTargetingMotion ( float t, Motion& m );
		void getTargetDirection ( float speed );		

		void PlanPath ( Vec3F target );

		float getStartTime ( unsigned int flags, int new_cycle, float new_duration );
		int getActiveCycle ();
		int getTimelineSlot ( float t );
		bool UpdateActiveMotion ( float t, Motion& m );		
		void DecideNewMotion ();		

		// Bindings
		void ResetTPose();
		void BindBones ();
		void EvaluateBones ();		
		void ResetBones();		
		void BindMuscles ();
		Quaternion getMuscleRotation(Vec3F muscle_dir, Quaternion& bone_rot, float angle=0);
		void EvaluateMuscles();
		void ResetMuscles();


		void SetScene ( Scene* s )	{ m_Scene = s; }
		void SetLead ( int l )		{ m_Lead = l; }
		std::string getStyle ()	{ return m_Style; }
	
		unsigned int getStates ( Motion& m )	{ return m.states; }			// member var		
		bool hasState ( Motion& m, unsigned int f )	{ return (m.states & (unsigned int) f)==0 ? false : true; }		
		bool hasState ( unsigned int flags, unsigned int f )	{ return (flags & (unsigned int) f)==0 ? false : true; }		
		void SetState ( Motion& m, unsigned int f, bool val)   {if (val) m.states |= (unsigned int) f; else m.states &= ~(unsigned int) f; }

		Vec3F getTargetDir ();
		void setPause(bool p)	{m_Pause=p;}
		bool getPause()	{ return m_Pause; }

		void ToggleBones()	{ m_bBones = !m_bBones; }

	private:
		int						m_State;		
		bool					m_Sync;
		std::string				m_Style;

		Orientation				m_Orient[JNTS_MAX];		
		
		JointSet				m_Joints[JNTS_MAX];		// joint state
		std::vector<Vec3I>	m_Channels;				

		std::vector<Motion>		m_Motions;				// motion events
		int						m_PrimaryMotion;
		int						m_SecondaryMotion;

		objID					m_PathID;		
		float					m_PathU;
		int						m_PathStatus;
		Vec3F				m_Target;				
		Vec3F				mTargetDir;
		float					mTargetDist, mTargetAng, mTargetDelta;
		Quaternion				mTargetDA;
		Vec3F				m_HomePos;
		Vec3F				m_Dir;

		int						m_Lead;
		bool					m_Pause;
		float					m_Alpha;
		float					m_InterpFrames;
		float					m_TimelineStart;

		Scene*					m_Scene;
		
		float					m_FPS;
		bool					m_bBones;

		Vec3F				mP[5];

		std::vector<BindInfo>	m_BoneBinds;
		std::vector<BindInfo>	m_MuscleBinds;

	};

#endif