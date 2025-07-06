//-------------------------
// Copyright 2020-2025 (c) Quanta Sciences, Rama Hoetzlein
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//--------------------------


#include "motioncycles.h"
#include "character.h"
#include "shapes.h"
#include "string_helper.h"
#include "camera.h"
#include "curve.h"
#include "main.h"
#include "parts.h"
#include "muscles.h"
#include "scene.h"

#include "gxlib.h"
using namespace glib;

#include <stack>
#include <vector>

#define BVH_START		0
#define BVH_HIER		1
#define BVH_MOTION		2
#define BVH_TIMEDATA	3
#define BVH_END		4

#define CHAN_XYZ_POS		0
#define CHAN_ZYX_ROT		1

#define THIT	1.0				// hit range in meters
#define TWALK	25

#define JUNDEF		0xFFFF


Character::Character () : Object()
{
	m_Pause = false;
	m_Orient[JNTS_A].pos.Set(0,0,0);
	m_Orient[JNTS_A].rot.set ( Vec3F(0,0,0) );
	m_Orient[JNTS_A].scale = 1.0f;

	m_PathID = -1;
	m_PathU = 0;
	m_PathStatus = 0;
	mTargetDir.Set(0,0,0);
	mTargetDist = 0;
	
	// underlying frame rate of motion data (should be in motioncycles)
	m_FPS = 120.0f;
	
	m_InterpFrames = 20.f;
	m_TimelineStart = 0.f;
	m_PrimaryMotion = -1;
	m_SecondaryMotion = -1;
	m_bBones = true;
	
	m_Target = Vec3F(0,0,0);	
	m_Style = "home";
}

bool Character::RunCommand(std::string cmd, vecStrs args)
{
	if (cmd.compare("skeleton") == 0)	{ LoadBVH(args[0]);			return true; }
	if (cmd.compare("boneinfo") == 0)	{ LoadBoneInfo(args[0]);	return true; }
	if (cmd.compare("home") == 0)		{ SetHomePos ( strToVec3(args[0], ';') );	return true; }

	if (cmd.compare("finish") == 0) {
		SetScene ( gScene );
		BindBones();
		BindMuscles();
		return true;
	}
	return false;
}

void Character::Define (int x, int y)
{
	AddInput ("time",	'Atim' );		// time must come first
	AddInput ("shader", 'Ashd' );
	AddInput ("tex",	'Aimg' );
	AddInput ("mesh",	'Amsh' );
	AddInput ("clr",	T_VEC4 );	
	AddInput ("path",	'Acrv' );
	AddInput ("bones",  'Ashp' );
	AddInput ("muscles", 'Ashp');
	AddInput ("motions", 'mcyc');		// library of motion cycles

	//-- future stuff
	// AddInput ("path", 
	// AddInput ("style", T_STR );
	// AddInput ("home", T_STR );

	SetInput ( "shader",	"shade_mesh" );
	SetInput ( "mesh",		"model_cube");		// defaults
	SetInput ( "tex",		"color_white" );
}

void Character::Generate (int x, int y)
{
	if ( mOutput != OBJ_NULL ) {
		CopyJoints( JNTS_A, JNTS_B );		// copy once loaded from bvh
		return;
	}

	CreateOutput ( 'Ashp' );	

	//SetShapeShaderFromInput ("shader");		-- obsolete
	//SetShapeMeshFromInput ( "mesh" );			-- obsolete
	//SetShapeTex ( G_TEX_UNI, "tex", "" );			-- obsolete
	SetOutputXform ();
}

void Character::ResetTPose()
{
	ResetMotions (gScene->getTime(), true);			// Goto bind pose. true = T-pose

	ResetBones()			;						// Return bones to default locations

	ResetMuscles();									// Return muscles to default locations
}

void Character::BindBones ()
{
	Parts* parts = (Parts*) getInput( "bones" );
	Shapes* bones = getInputShapes ( "bones" );
	if ( bones== 0x0 ) return;		// nothing to bind

	// Clear bind info
	m_BoneBinds.clear ();
	
	Vec3F bpos, bdir;
	Vec3F dvec, jp;
	//Matrix4F jorient;
	Quaternion jorient;
	int k;
	float dst;
	Dualquat brel, j, jinv;
	BindInfo bi;
	Shape* s;
	std::string pbind, jname;

	// For each bone, record the local relative relation of the bone to the character skeleton	
	for (int n=0; n < bones->getNumShapes(); n++ ) {
		s = bones->getShape(n);
		
		// save original bone orientation	
		bi.orig = Dualquat(s->rot, s->pos);	bi.orig.normalize();
		bi.specs = s->pos;
		
		if (parts->getExtra(n).length() > 0) {
			// part provides an override for the joint to use
			pbind = parts->getExtra(n);
			for (k = 0; k < getNumJoints(JNTS_A); k++) {
				if (pbind.compare(m_Joints[JNTS_A][k].name) == 0) {
					bi.bind[0] = k; bi.wgts[0] = 1.0;
				}
			}
		} else {
			// find nearest joint in hierarchy
			bi.wgts[0] = 1.0e10;
			bi.bind[0] = 0;
			for (k = 0; k < getNumJoints(JNTS_A); k++) {
				dvec = m_Joints[JNTS_A][k].pos - s->pos;					// joint-to-bone distance
				dst = sqrt(dvec.x * dvec.x + dvec.y * dvec.y + dvec.z * dvec.z);
				if (dst < bi.wgts[0]) { bi.bind[0] = k; bi.wgts[0] = dst; }

			}
			bi.wgts[0] = 1.0;
		}

		/*
		// find first two nearest joints in hierarchy 
		bi.wgts[0] = 1.0e10; 
		bi.bind[0] = 0;			
		for (k=0; k < getNumJoints(JNTS_A); k++ ) {			
			dvec = m_Joints[JNTS_A][k].pos - s->pos;					// joint-to-bone distance
			dst = sqrt(dvec.x*dvec.x + dvec.y*dvec.y + dvec.z*dvec.z);
			if ( dst < bi.wgts[0] ) { bi.bind[0] = k; bi.wgts[0] = dst; }	
		}
		bi.wgts[1] = 1.0e10; 
		bi.bind[1] = 0;			
		for (k=0; k < getNumJoints(JNTS_A); k++ ) {			
			dvec = m_Joints[JNTS_A][k].pos - s->pos;					// joint-to-bone distance
			dst = sqrt(dvec.x*dvec.x + dvec.y*dvec.y + dvec.z*dvec.z);
			if ( dst < bi.wgts[1] && k != bi.bind[0] ) { bi.bind[1] = k; bi.wgts[1] = dst; }		// find 2nd nearest, skip the already found bind[0] joint
		}
		// inverse distance weighting
		a = 1.0 / pow(bi.wgts[0], 2.0); b = 1.0 / pow(bi.wgts[1], 2.0);
		bi.wgts[0] = a / (b+a);
		bi.wgts[1] = b / (b+a);*/
			
		// get local relative joint orientations
		for (int i=0; i < 1; i++) {
			k = bi.bind[i];
			jorient = m_Joints[JNTS_A][k].Morient;						// world orientation of joint
			j = Dualquat ( jorient, m_Joints[JNTS_A][k].pos );			// dual-quaternion for world joint orientation & pos
			jinv = j.inverse ();										// inverse joint transform

			bi.localrel[i] = Dualquat ( s->rot, s->pos ) * jinv;		// local relative orientation => bone shape 's' in the local space of the joint 'jinv'
			bi.localrel[i].normalize ();
		}		

		m_BoneBinds.push_back ( bi );
	}	
}

void Character::EvaluateBones ()
{
	if ( m_BoneBinds.size() == 0 ) return;		// nothing bound
	
	Shapes* bones = getInputShapes ( "bones" );
	Shape* s; 
	Quaternion jorient;
	int jbound;
	Dualquat b, bsum, beval, j, jinv;

	// Full equation: B(t) = B0 J0inv J(t)				// bone at time t = the bone in base pose, in the space of joint in base pose, transformed to space of joint at current time

	// For each bone, apply the local relative bone orientation to the *current* joint orientation
	for (int n=0; n < m_BoneBinds.size(); n++ ) {
		s = bones->getShape(n);

		jbound = m_BoneBinds[n].bind[0];
		jorient = m_Joints[JNTS_A][ jbound ].Morient;
		j = Dualquat ( jorient, m_Joints[JNTS_A][ jbound ].pos );				// dual-quaternion for *current* joint orientation
		bsum = m_BoneBinds[n].localrel[0] * j;
		//bsum.normalize();

		/*bsum.zero();
		for (int i=0; i < 1; i++) {
			jbound = m_BindInfo[n].bind[i];
			jorient = m_Joints[JNTS_A][ jbound ].Morient;
			j = dualquat ( jorient, m_Joints[JNTS_A][ jbound ].pos );				// dual-quaternion for joint orientation			
			beval = m_BindInfo[n].localrel[i] * j;		
			beval.normalize();
			bsum = bsum + (beval * m_BindInfo[n].wgts[i]);	
		}*/		

		s->pos =		bsum.getTranslate();
		s->rot =		bsum.getRotate();
		m_BoneBinds[n].pos = s->pos;

		if (n==0) {
			Vec3F t = j.getTranslate();
			dbgprintf ( "%d %f,%f,%f\n", n, t.x, t.y, t.z );
		}

	}

	bones->MarkDirty();
}

void Character::ResetBones()
{
	if (m_BoneBinds.size() == 0) return;		// nothing bound

	// Reset to original locations
	Shapes* bones = getInputShapes("bones");
	Shape* s;
	for (int n = 0; n < m_BoneBinds.size(); n++) {
		s = bones->getShape(n);
		s->pos = m_BoneBinds[n].specs;
		s->rot = m_BoneBinds[n].orig.getRotate();
	}
	m_BoneBinds.clear();
}

void Character::BindMuscles()
{	
	BindInfo bi;
	Muscle* m;	
	Dualquat j, jinv;

	Muscles* muscles = (Muscles*) getInput( "muscles" );			if (muscles == 0x0) return;
	Shapes* mush = getInputShapes( "muscles");			if (mush == 0x0) return;
	Shapes* bones = getInputShapes( "bones");
	Shape* sm, * sb;

	m_MuscleBinds.clear();
	
	for (int n = 0; n < mush->getNumShapes(); n++) {
		sm = mush->getShape(n);						// get the muscle shape
		m = muscles->getMuscle(n);

		bi.orig = Dualquat(sm->rot, sm->pos);
				
		sb = bones->getShape( m->b1.x );			// get the bone #1 shape ID.  b1.x = shape #, b1.y = vertex #.. of the bones Parts object
		j = Dualquat( sb->rot, sb->pos );			// dual-quaternion for the bone
		jinv = j.inverse();							// inverse bone transform
		bi.localrel[0] = Dualquat(m->p1) * jinv;	// position #1 (start) of muscle in the local space of bone #1
		bi.localrel[0].normalize();

		sb = bones->getShape(m->b2.x);				// get the bone #2 shape ID.  b1.x = shape #, b1.y = vertex #.. of the bones Parts object
		j = Dualquat( sb->rot, sb->pos );			// dual-quaternion for the bone
		jinv = j.inverse();							// inverse bone transform
		bi.localrel[1] = Dualquat(m->p2) * jinv;	// position #2 (end) of muscle in the local space of bone #2
		bi.localrel[1].normalize();

		bi.b1 = m->b1;								// muscle bind bones
		bi.b2 = m->b2;
		bi.p1 = m->p1;								// muscle in rest pose
		bi.p2 = m->p2;
		bi.norm = m->norm;
		bi.specs = m->specs;
		
		m_MuscleBinds.push_back( bi );
	}
}

Quaternion Character::getMuscleRotation(Vec3F muscle_dir, Quaternion& bone_rotate, float angle)
{
	Vec3F p2, p3, p4;

	// align shape to muscle direction
	Quaternion c1, c2;
	c1.fromRotationFromTo(Vec3F(0, 1, 0), muscle_dir);			// rotate the shape Y-axis along the muscle direction
	
	// twist fix
	p2 = c1.rotateVec(Vec3F(0, 1, 0)); p2.Normalize();	// muscle shape, Y-axis before twist fix ('norm' of muscle)
	p3 = c1.rotateVec(Vec3F(0, 0, 1)); p3.Normalize();	// muscle shape, Z-axis before twist fix 
	p4 = Vec3F(0, 0, 1); p4 *= bone_rotate; 			// Z-axis of bone 
	p4 -= p2 * (float) p2.Dot(p4); p4.Normalize();					// [optional step] project the bone Z-axis into the new muscle shape base plane (XZ)
	/* //-- debugging
	mP[0] = p1;
	mP[2] = p1 + p2;	// muscle shape, Y-axis before twist fix (blue)
	mP[3] = p1 + p3;	// muscle shape, Z-axis before twist fix (red)
	mP[4] = p1 + p4;	// bone orientation, Z-axis (yellow)
	*/
	// see: https://stackoverflow.com/questions/5188561/signed-angle-between-two-3d-vectors-with-same-origin-within-the-same-plane
	float twist = acos(p3.Dot(p4));							// angle between muscle Z-axis and bone Z-axis
	float cross = p2.Dot(p3.Cross(p4));						// cross between them is compared to muscle norm for *signed* in-plane rotation. (note: p3 is destroyed here)
	if (cross < 0) twist = -twist;
	c2.fromAngleAxis ( twist + angle*DEGtoRAD, muscle_dir);
	c2.normalize();

	return (c2 * c1);			// combine muscle alignment and twist fix
}

void Character::EvaluateMuscles ()
{
	Muscles* muscles = (Muscles*)getInput("muscles");			if (muscles == 0x0) return;
	Shapes* mush  = getInputShapes( "muscles");		if (mush == 0x0) return;
	Shapes* bones = getInputShapes( "bones");
	Shape *sb, *sh;
	Muscle* m;
	if (mush == 0x0) return;
	BindInfo* bi;
	Vec3F p1, p2, p3, p4;
	Vec3F mdir;
	Dualquat j;
	Quaternion c;
	float bulge, muscle_len;

	// Full equation: M(t) = M0 B0inv B(t)				// muscle pnt at time t = the muscle pnt in base pose, in the space of bone in base pose, transformed to space of bone at current time

	Vec3F* sh_muscle = (Vec3F*) mush->getData (BMUSCLE);		// shape muscle data

	for (int n = 0; n < m_MuscleBinds.size(); n++) {
		sh = mush->getShape(n);		
		bi = &m_MuscleBinds[n];							// bind info (static since bind time)
		m = muscles->getMuscle(n);						// get muscle for dynamic params (e.g. size, curve, flat)

		sb = bones->getShape(bi->b2.x);					// get the bone #2 shape ID
		j = Dualquat(sb->rot, sb->pos);					// dual-quaternion for *current* bone orientation
		p2 = (bi->localrel[1] * j).getTranslate();		// map muscle point into bone space

		sb = bones->getShape(bi->b1.x);					// get the bone #1 shape ID
		j = Dualquat(sb->rot, sb->pos);					// dual-quaternion for *current* bone orientation
		p1 = (bi->localrel[0] * j).getTranslate();		// map muscle point into bone space
			
		sh->pos = p1;
		mdir = p2 - p1; 
		muscle_len = mdir.Length();							// muscle current length
		mdir.Normalize();									// muscle dir
		bulge = m->rest_len / muscle_len;
		bulge *= bulge;
		if (bulge < 0.3) bulge = 0.3;
		if (bulge > 2.0) bulge = 2.0;
			
		// final shape orientation
		sh->rot = getMuscleRotation(mdir, sb->rot, m->angle);		// shape orientation. c1 = align shape along muscle dir, c2 = twist fix. (right-to-left evaluation, first c1, then c2)
		sh->scale = m->specs * muscle_len * Vec3F(bulge, 1.0, bulge);
		sh_muscle[n] = Vec3F(m->angle, m->curve, 0.f);
	}

	mush->MarkDirty();
}

void Character::ResetMuscles()
{
	if (m_MuscleBinds.size() == 0) return;		// nothing bound

	// Reset to original locations
	Muscles* muscles = (Muscles*)getInput("muscles");			if (muscles == 0x0) return;
	Shapes* mush = getInputShapes("muscles");
	Shapes* bones = getInputShapes("bones");
	Shape* sb;
	Shape* sh;
	Muscle* m;
	Vec3F mdir;
	BindInfo* bi;
	Quaternion c;
	float bulge; 

	Vec3F* sh_muscle = (Vec3F*)mush->getData (BMUSCLE);		// shape muscle data

	for (int n = 0; n < m_MuscleBinds.size(); n++) {
		sh = mush->getShape(n);
		m = muscles->getMuscle(n);						// get muscle for dynamic params (e.g. size, curve, flat)
		bi = &m_MuscleBinds[n];

		sh->pos = bi->p1;								// reset to bind positions
		mdir = bi->p2 - bi->p1;
		float muscle_len = mdir.Length();				// reset muscle length
		bulge = 1.0;									// no bulge, rest_len/muscle_len = 1.0 when muscle is reset
		
		sb = bones->getShape(bi->b1.x);					// get the bone #1 shape ID

		sh->rot = getMuscleRotation(mdir, sb->rot, m->angle);			// base-pose orientation
		sh->scale = m->specs * muscle_len * bulge;		// rest length of muscle
		sh_muscle[n] = Vec3F(m->angle, m->curve, 0.f);
	}
	m_MuscleBinds.clear();
}



void Character::PlanPath ( Vec3F target )
{
}

Vec3F Character::getPosition () 
{ 
	if ( m_PrimaryMotion != -1 && m_SecondaryMotion != -1 ) 
		return m_Orient[JNTS_C].pos;
	return m_Orient[JNTS_A].pos;
}
Vec3F Character::getDir ()
{
	return m_Dir;				// direction of motion
}

float circleDelta ( float a, float b )
{
	a = (float) fmod (a, 360 ); if ( a < 0 ) a += 360;

	float d = fabs( b - a);
	float r = d > 180 ? 360 - d : d;
	int sign = (b - a >= 0 && b - a <= 180) || (b - a <=-180 && b - a>= -360) ? 1 : -1; 
	return r * sign;
}

/*
void Character::NextAction ()
{
	m_CPrev = m_CCurr;				// save previous cycle	
	if (m_CNext.state!=STATE_UNKNOWN) m_CCurr = m_CNext; 			// get next cycle, if it was set
	m_CEnd = m_CCurr.length-1;		// new end of cycle
	m_CNext.state = STATE_UNKNOWN;	
	m_CFrame = 0;	
}


void Character::ChangeAction ( int state, std::string name, bool target, bool force )
{
	if ( m_CNext.state == state ) return;

	Action a;
	char buf[256];
	strncpy ( buf, name.c_str(), 256 );
	a.cycle = cycleset->FindCycle ( buf );
	if ( a.cycle == A_NULL ) return;
	//a.drift = cycleset->getCycleDrift ( a.cycle );
	a.gotarget = target;
	a.length = cycleset->getCycleEnd( a.cycle );
	a.state = state;

	bool bUnknown = (m_CCurr.state==STATE_UNKNOWN);
	bool bChanged = (a.state != m_CCurr.state || a.cycle != m_CCurr.cycle) && (a.state != STATE_ACT);
	bool bAct = (a.state == STATE_ACT && m_CCurr.state != STATE_ACT );

	if (bAct)
		bool stop = true;

	if ( force && (bUnknown || bChanged || bAct) ) {
		// interrupt current state
		m_CNext = a;	
		m_CEnd = m_CFrame + 5;
		if ( m_CEnd >= m_CCurr.length ) m_CEnd = m_CCurr.length-1;	
	}
}
*/

float Character::getStartTime ( unsigned int flags, int new_cycle, float new_duration )
{
	Vec3I transition (-1,0,0);
	float time = gScene->getTime();
	float t = time;
	
	if ( m_Motions.size() == 0 || hasState (flags, M_NOW))
		return time;									// now

	// get last cycle motion	
	int prev_cycle_id = 0;										// find the last cycle motion
	for (int n=0; n < m_Motions.size(); n++ ) {
		if ( m_Motions[n].mtype == MT_CYCLE && m_Motions[n].duration.y > m_Motions[prev_cycle_id].duration.y ) 
			prev_cycle_id = n;		
	}
	
	Motion& mprev = m_Motions[ prev_cycle_id ];
	
	//if ( mprev.cycle != new_cycle ) 
	//	transition = cycleset->FindTransition ( mprev.cycle, new_cycle, mprev.frame );
	transition.x = -1;

	float FRAMEtoTIME = 1.0f / (m_FPS*mprev.speed);
	float INTERP_TIME = m_InterpFrames * FRAMEtoTIME;

	if (transition.x == -1) {							
		// no transition found, smooth interpolate from end of last motion
		if (mprev.duration.y - mprev.duration.x <= INTERP_TIME || new_duration <= INTERP_TIME) {
			t = mprev.duration.y;						// tack on end			
		}
		else {
			t = mprev.duration.y - INTERP_TIME;			// blend motions			
		}
	} else {
		// transition from the previous cycle to the new cycle at a good frame (pose matching)

		// left offseting for interpolation
		//   0 = new time matches transition frame
		// 0.5 = split interpolation on either side
		//   1 = new time will allow interpolation to end of the transition frame
		float ishift = 0.5f;	
	
		// convert from transition frame in i-cycle to time
		t = mprev.duration.x + float(transition.y - m_InterpFrames)*FRAMEtoTIME;	// time units

		// truncate the previous motion
		mprev.range.y = transition.y + int( m_InterpFrames*(1.0f-ishift));			// frame units
		mprev.range.z = (mprev.range.y-mprev.range.x);		
		mprev.duration.y = mprev.duration.x + mprev.range.z*FRAMEtoTIME;			// time units
	} 	
	if ( t < time ) t = time;

	return t;
}
int Character::getActiveCycle ()
{
	// find the next active cycle that is in range, 
	// but is neither primary or secondary
	for (int n=0; n < m_Motions.size(); n++ ) {
		if ( m_Motions[n].mtype == MT_CYCLE && m_Motions[n].active == true && n != m_PrimaryMotion && n != m_SecondaryMotion )
			return n;
	}
	return -1;
}

int Character::getTimelineSlot ( float t )
{
	bool collide = true;
	int slot = 0;

	while (collide) {		
		collide = false;
		for (int n=0; n < m_Motions.size(); n++ ) {
			if ( m_Motions[n].tslot == slot && t <= m_Motions[n].duration.y)
				collide = true;
		}
		if ( collide ) slot++;
	}
	return slot;
}

void Character::AddMotion ( std::string name, unsigned int flags, float speed)
{
	MotionCycles* cycleset = dynamic_cast<MotionCycles*> (getInput("motions"));
	if (cycleset == 0x0) { dbgprintf("ERROR: Character does not have motions (cycleset).\n"); exit(-2); }

	char buf[256];
	Motion m;
	float time = gScene->getTime ();

	if ( hasState (flags, M_NOW) )
		PruneMotions ( time, 1 );		// if now, remove all future motions 
	
	m.mtype = MT_CYCLE;			// any named motion defaults to being a cycle
	m.states = flags;
	m.id = (int) m_Motions.size();
	
	if (m.id==0) SetPos ( m_HomePos );				// reset position on first motion
	
	strncpy ( buf, name.c_str(), 256 );
	m.cycle = cycleset->FindCycle ( buf );		// find cycle by name
	if ( m.cycle == OBJ_NULL ) return;	

	m.active = false;
	m.frame = 0;
	m.frame_last = 0;
	m.speed = (speed==0) ? 0.1 : speed;
	
	m.range.x = 0;
	m.range.y = cycleset->getCycleEnd( m.cycle ) - 1;
	m.range.z = (m.range.y-m.range.x);	

	m.duration.y = m.range.z / (m_FPS*m.speed);						// cycle duration in time units
	m.duration.x = getStartTime ( flags, m.cycle, m.duration.y );	// motion start time
	m.duration.y += m.duration.x;									// motion end time	

	m.tslot = getTimelineSlot ( m.duration.x );

/*	if ( force && (bUnknown || bChanged || bAct) ) {
		// interrupt current state
		m_CNext = a;	
		m_CEnd = m_CFrame + 5;
		if ( m_CEnd >= m_CCurr.length ) m_CEnd = m_CCurr.length-1;	
	} */

	m_Motions.push_back ( m );
}

void Character::ResetMotions ( float t, bool tpose )
{
	// clear all motions
	m_Motions.clear ();
	m_PrimaryMotion = -1;
	m_SecondaryMotion = -1;

	// goto T-pose
	if ( tpose ) {
		AddMotion ( "stand", M_NOW | M_BASIC | M_RESET_POS | M_RESET_ROT , 1.0f );		
		m_Orient[JNTS_A].rot.set ( Vec3F(0,0,0) );
		m_Orient[JNTS_A].pos = m_HomePos;
		if ( m_Motions.size() > 0) EvaluateMotion ( t, m_Motions[0] );		
	}
}

void Character::PruneMotions ( float t, int dir )
{
	int save1 = m_PrimaryMotion;
	int save2 = m_SecondaryMotion;

	for (int n=0; n < m_Motions.size(); ) {
		if ( dir==-1 && m_Motions[n].duration.y <= t  && m_Motions[n].active==false ) {		// prune before t
			m_Motions.erase ( m_Motions.begin() + n );
		} else if ( dir==1 && m_Motions[n].id != save1 ) {		// prune all except primary
			m_Motions.erase ( m_Motions.begin() + n );
		} else {
			n++;
		}
	}
	m_PrimaryMotion = -1;
	m_SecondaryMotion = -1;
	for (int n=0; n < m_Motions.size(); n++ ) {						// fix motion ids
		if ( m_Motions[n].id == save1 ) m_PrimaryMotion = n;
		if ( m_Motions[n].id == save2 ) m_SecondaryMotion = n;
		m_Motions[n].id = n;				
	}
}

bool Character::UpdateActiveMotion ( float t, Motion& m)
{
	bool changed = false;
	bool activated = false;
	
	m.frame_last = m.frame;
	m.frame = (t - m.duration.x) * (m_FPS*m.speed);				// frame units

	// make motion active or inactive
	if ( m.active == true ) {		
		if ( m.frame < m.range.x || m.frame > m.range.y ) {
			m.active = false;									// make inactive
			changed = true;
		}
	} else { 
		if ( m.frame >= m.range.x && m.frame <= m.range.y ) {	
			m.active = true;									// make active		
			changed = true;
			activated = true;
		}
	}
	if ( !changed ) return false;

	// update primary & secondary
	if ( !activated ) {
		if ( m.id == m_PrimaryMotion ) m_PrimaryMotion = -1;			// deactivate primary or secondary
 		if ( m.id == m_SecondaryMotion ) m_SecondaryMotion = -1;
		PruneMotions ( t );												// remove outdated motions
	}	
	if ( m_PrimaryMotion == -1 ) {		
		if ( m_SecondaryMotion != -1 ) {
			m_PrimaryMotion = m_SecondaryMotion;						// switch primary to secondary
			m_Orient[JNTS_A] = m_Orient[JNTS_B];						// <-- orientation moves to 2nd
			m_SecondaryMotion = -1;
		} else {
			if (activated) m_PrimaryMotion = m.id;
		}
	}
	if ( m_SecondaryMotion == -1 ) {
		if (activated && m.id != m_PrimaryMotion ) {
			m_SecondaryMotion = m.id;									// secondary just activated
			m_Orient[JNTS_B] = m_Orient[JNTS_A];						// <-- orientation starts at 1st
		} else {
			m_SecondaryMotion = getActiveCycle();						// get an unused active cycle
		}
	}		

	return activated;
}

void Character::SetTarget  (Vec3F h)
{
	Vec3F dir = h - m_Orient[JNTS_A].pos;
	float dist=dir.Length();
	if ( dist > THIT ) {
		m_Target = h; 		
	}
}

void Character::getTargetDirection ( float speed )
{
	mTargetDir = Vec3F(m_Target.x - m_Orient[JNTS_A].pos.x, 0.f, m_Target.z - m_Orient[JNTS_A].pos.z); 
	mTargetDist = mTargetDir.Length();
	mTargetDir.Normalize();
	mTargetAng = atan2( mTargetDir.x, mTargetDir.z)/DEGtoRAD; if ( mTargetAng < 0 ) mTargetAng += 360;

	// Current forward direction & angle
	Vec3F fwd (0,0,1);
	Matrix4F mtx;
	Quaternion roty;
	m_Orient[JNTS_A].rot.getMatrix ( mtx );
	fwd *= mtx;	
	float ang = atan2(fwd.x, fwd.z)/DEGtoRAD; if ( ang < 0 ) ang += 360;	

	// Circular difference to target	

	float ang_momentum = std::max(5.0-speed, .2) / m_FPS;			// speed of turning
	
	Quaternion tdang;
	mTargetDelta = circleDelta( ang, mTargetAng );
	mTargetDA.set ( 0, 1, 0, 0);	
	mTargetDA.fromAngleAxis (  mTargetDelta * ang_momentum * m_Orient[JNTS_A].vel.Length(), Vec3F(0,1,0) );	 
}

void Character::EvaluatePathMotion ( float t, Motion& m )
{
	if (m.id != m_PrimaryMotion) return;	
	if (m_PathStatus==1) return;

	Curve* curv = dynamic_cast<Curve*> ( getInput ( "path" ) );
	if ( curv== 0x0 ) return;

	// Advance motion along path
	float curvelen = curv->getCurveLength();	
	m_PathU += 0.1f * (1.f/curvelen) / (mTargetDist + 2);
	
	if ( m_PathU > 0.98 ) {
		if (m_PathStatus==0) {
			m_PathStatus = 1;
			m_PathU = 1;
			AddMotion ( "runstand", M_TARGET | M_NOW );	
		}
	}

	// Set target to path 
	m_Target = curv->getPointOnCurve ( m_PathU );
}


void Character::EvaluateTargetingMotion ( float t, Motion& m )
{
	int jset = (m.id == m_PrimaryMotion) ? JNTS_A : (m.id==m_SecondaryMotion) ? JNTS_B : -1;
	if ( jset == -1 ) return;

	// Determine orientation to target
	getTargetDirection( m.speed );	

	// Rotate character	
	m_Orient[jset].rot = mTargetDA * m_Orient[jset].rot;	// adjust orientation (world y-axis)
	m_Orient[jset].rot.normalize();		

}



/*if ( mTargetDist  < THIT && mnext.state != STATE_STAND ) {
		AddMotion ( STATE_STAND, "stand", true, true );	
	} else if ( mTargetDist > THIT ) {		
		if ( mTargetDist < TWALK && mnext.state != STATE_WALK ) {			// change to walk if nearby
			AddMotion ( STATE_WALK, "walk", true, true);
		} else if ( mTargetDist > TWALK  && mnext.state != STATE_RUN ) {	// change to run if far away	
			AddMotion ( STATE_RUN, "run", true, true );	
		}		
	}	*/


void Character::DecideNewMotion ()
{
	getTargetDirection( 1.0 );	

	// Go home	
	/*if ( mTargetDist < THIT ) {					
		AddMotion ( STATE_STAND, "stand", false, true );		// at home, stand
		return;
	} 
	if ( mTargetDist < TWALK ) {								// near to home
		if ( fabs(mTargetDelta) < 6 ) {							// check if lined up 
			AddMotion ( STATE_WALK, "walk", true, false );	
		} else {
			if ( mTargetDelta < 0 ) {							// not lined up, turn
				AddMotion ( STATE_TURN, "turnR", true, false  );
			} else	{
				AddMotion ( STATE_TURN, "turnL", true, false  );
			}
		}
	} else {													// far away, run /w targeting
		// run if far away
		AddMotion ( STATE_RUN, "run", true, true  );
	}*/
}

void Character::EvaluateCycleMotion ( float t, Motion& m, bool bPoseChange )
{
	MotionCycles* cycleset = dynamic_cast<MotionCycles*> (getInput("motions"));
	if (cycleset == 0x0) { dbgprintf("ERROR: Character does not have motions (cycleset).\n"); exit(-2); }

	Vec3F cp, cpl, dp;	
	Quaternion co, col, da;

	int jset = (m.id == m_PrimaryMotion) ? JNTS_A : (m.id==m_SecondaryMotion) ? JNTS_B : -1;
	if ( jset == -1 ) return;

	// Update joints
	float df = m.frame - m.frame_last;
	
	// Get interpolated motion poses for the current and previous frames (fractional)
	cycleset->AssignCycleToJoints ( m.cycle, m_Joints[jset], m.frame, m.frame_last, cp, cpl, co, col );

	// Determine motion delta from cycles
	m_Orient[jset].vel = 0; 
	m_Orient[jset].rotvel.set (0,0,0,1);
	if ( bPoseChange ) {
		m.frame_last = m.frame-1;
		EvaluatePoseChange ( m.frame, m.cycle, co, m_Orient[jset].rotvel );
	} else {
		// compute motion velocity and rotational velocity from prev to current frame (returns m_Orient[jset].vel and .rotvel)
		EvaluateCycleDerivatives ( jset, m.cycle, m.frame, df, cp, cpl, co, col, m_Orient[jset].vel, m_Orient[jset].rotvel );
	}

	// Move character		
	m_Orient[jset].pos += m_Orient[jset].vel;	// m.speed;
	m_Orient[jset].rot = m_Orient[jset].rot * m_Orient[jset].rotvel;
	m_Orient[jset].rot.normalize();
	m_Dir = m_Dir * 0.96f + m_Orient[jset].vel * 0.04f;  m_Dir.Normalize();	  // smoothed direction

	// Evaluate joints
	Matrix4F world;
	m_Orient[jset].rot.getMatrix ( world );
	world.PostTranslate ( m_Orient[jset].pos );	
	EvaluateJoints ( m_Joints[jset], world, m_Orient[jset].rot );
}

void Character::EvaluateCycleInterpolation ( float t )
{
	Motion& m1 = m_Motions[m_PrimaryMotion];
	Motion& m2 = m_Motions[m_SecondaryMotion];
	float t1 = m2.duration.x;			// start of secondary
	float t2 = m1.duration.y;			// end of primary
	Quaternion o1, o2;
	Vec3F p1, p2;

	float u = (t - t1) / (t2 - t1);		// blend value

	// blend joint orientations
	for (int n=0; n < m_Joints[JNTS_A].size(); n++) {		
		o1 = m_Joints[JNTS_A][n].orient;
		o2 = m_Joints[JNTS_B][n].orient;
		o1.slerp ( o1, o2, u );
		m_Joints[JNTS_A][n].orient = o1;		
	}
	// use second cycle orientation
	o1 = m_Orient[JNTS_A].rot;
	o2 = m_Orient[JNTS_B].rot;
	o1.slerp ( o1, o2, u );
	m_Orient[JNTS_C].rot = o1;							// entire character orientation is interpolated (Crot = A & B)
	m_Orient[JNTS_C].pos = m_Orient[JNTS_B].pos;		// entire character position tracks with secondary motion (Cpos = B)

	// re-evaluate the joint set with new orientations
	EvaluateJoints ( JNTS_A, EVAL_CHARACTER );	
}

void Character::EvaluateMotion ( float t, Motion& m )
{
	bool started  = false;

	// update activity
	started = UpdateActiveMotion ( t, m );

	if ( m.active ) {				
		switch (m.mtype) {
		case MT_CYCLE:
			EvaluateCycleMotion ( t, m, started );			
			break;
		};
		// Keep grounded
		//float dy = m_HomePos.y - m_Orient[JNTS_A].pos.y;		
		//m_Orient[JNTS_A].pos.y += dy * 0.005; // * m_Orient[jset].vel.Length();

		if ( hasState(m, M_PATH) )
			EvaluatePathMotion ( t, m );
		if ( hasState(m, M_TARGET) )
			EvaluateTargetingMotion ( t, m );		
		if ( hasState(m, M_RESET_ROT) )
			m_Orient[JNTS_A].rot.set ( Vec3F(0,0,0) );
		if ( hasState(m, M_RESET_POS) )
			m_Orient[JNTS_A].pos = m_HomePos;
	}	
}

void Character::ProcessMotions ( float t )
{
	for (int n=0; n < m_Motions.size(); n++) {	
		EvaluateMotion ( t, m_Motions[n] );
	}
	
	if ( m_PrimaryMotion != -1 && m_SecondaryMotion != -1 )
		EvaluateCycleInterpolation ( t );
}

void Character::Run ( float time )
{
	if ( time >= 0 )
		ProcessMotions ( time );

	// Decide next action before it comes	
	// if ( m_CFrame > m_CEnd - 5 && m_CNext.state == STATE_UNKNOWN )
	//   DecideNextAction();			

	EvaluateBones ();
	EvaluateMuscles();
	
	MarkClean();
}

void Character::DrawCycle ( Motion& m )
{
	MotionCycles* cycleset = dynamic_cast<MotionCycles*> (getInput("motions"));
	if (cycleset == 0x0) { dbgprintf("ERROR: Character does not have motions (cycleset).\n"); exit(-2); }

	Vec3F a, b;
	Vec3F* pnt;
	Matrix4F mtx, cycle_mtx;
	Cycle* cy = cycleset->getCycle( m.cycle );

	int nframes = cy->frames;
	int njoints = cy->joints;
	
	// transform to current character 
	m_Orient[JNTS_A].rot.getMatrix ( mtx );				// add current character orientation
	mtx.PostTranslate ( m_Orient[JNTS_A].pos );

	Quaternion cq = cycleset->getCycleFrameOrientation ( m.cycle, m.frame );
	Vec3F cq_angs;
	Vec3F cp = cycleset->getCycleFramePos ( m.cycle, m.frame );
	cq = cq.inverse();
	cq.toEuler ( cq_angs );	
	cycle_mtx.RotateZYX ( cq_angs );		// remove cycle orientation
	cycle_mtx.PreTranslate ( cp * -1.0f );	// remove cycle translation
	mtx *= cycle_mtx;

	float alpha;


	// draw multiple frames of cycle
	for (int f = m.frame; f < nframes; f += 5 ) {

		alpha = 1.0f - (float(f) / float(nframes))*0.5f;
		pnt = cy->vis + (f * njoints * 2 );

		for (int j=1; j < njoints; j++) {			// draw each joint
			a = *pnt++;	a *= mtx;
			b = *pnt++; b *= mtx;
			drawLine3D ( Vec3F(a.x, a.y, a.z), Vec3F(b.x, b.y, b.z), Vec4F(1,1,1, alpha) );
		}
	}
}

Vec3F Character::getTargetDir ()
{
	Vec3F dir = m_Target - m_Orient[JNTS_A].pos;
	float ang=atan2(dir.x, dir.z)/DEGtoRAD; if ( ang < 0 ) ang += 360;
	return Vec3F(sin(ang*DEGtoRAD), 0.f, cos(ang*DEGtoRAD)); 
}

void Character::DrawJoints ( int jset, int mode, bool boxes, bool axes, Camera3D* cam)
{
	Vec3F a,b,c, p, p2;
	Vec4F clr;
	int child;

	float s = 0.006;			// joint size (meters)

	clr.Set(1,0,1,1);	

	//------ draw joint set world orientation
	//	
	Matrix4F rotation;
	m_Orient[jset].rot.getMatrix ( rotation );			// local orientation	
	a = Vec3F(1,0,0); a *= rotation; a *= s;
	b = Vec3F(0,1,0); b *= rotation; b *= s;
	c = Vec3F(0,0,1); c *= rotation; c *= s;
	p = m_Joints[jset][0].pos;
	drawLine3D ( p, p+a, Vec4F(1, 0, 0, 1) );
	drawLine3D ( p, p+b, Vec4F(0, 1, 0, 1) );
	drawLine3D ( p, p+c, Vec4F(0, 0, 1, 1) );
	
	//------ draw joints
	for (int n=1; n < m_Joints[jset].size(); n++ ) {

		p = m_Joints[jset][n].pos;

		if ( boxes ) {
			// draw joint box			
			drawBox3D ( p-s, p+s, clr );	
		}		
		if ( axes ) {
			// draw joint axis
			rotation = m_Joints[jset][n].Mworld;
			rotation.PostTranslate ( p * -1.0f );
			a = Vec3F(1,0,0); a *= rotation; a *= s;
			b = Vec3F(0,1,0); b *= rotation; b *= s;
			c = Vec3F(0,0,1); c *= rotation; c *= s;
			drawLine3D ( p, p+a, Vec4F(1, 0, 0, 1) );	// red, x-axis
			drawLine3D ( p, p+b, Vec4F(0, 1, 0, 1) );	// green, y-axis
			drawLine3D ( p, p+c, Vec4F(0, 0, 1, 1) );	// blue, z-axis		
		}
		// draw joint line (to child)
		child = m_Joints[jset][n].child;
		if (child != JUNDEF) {
			p2 = m_Joints[jset][child].pos;
			drawLine3D( p, p2, clr );	// red, x-axis	
			p = p*0.5f + p2*0.5f;			
		}
		//drawText3D ( p, cam, m_Joints[jset][n].name, clr );

		clr.y = 1 - clr.y;
	}
}

void Character::Sketch ( int w, int h, Camera3D* cam )
{
	MotionCycles* cycleset = dynamic_cast<MotionCycles*> (getInput("motions"));
	if (cycleset == 0x0) { dbgprintf("ERROR: Character does not have motions (cycleset).\n"); exit(-2); }

	float cs	= 1.0;			// character scale in meters (at hips)

	char msg[512];
	Vec3F targetdir = m_Target - m_Orient[JNTS_A].pos; targetdir.Normalize();
	Vec3F dir, ang;
	Vec3F a,b,c, p;
	float cz, mx1, mx2, my;
	Matrix4F local;	
	Motion m;

	bool timeline = false;

	drawLine3D(mP[0], mP[2], Vec4F(0, 0, 1, 1));		
	drawLine3D(mP[0], mP[3], Vec4F(1, 0, 0, 1));
	drawLine3D(mP[0], mP[4], Vec4F(1, 1, 0, 1));

	//* -- need to setup 2D sketching
	if ( timeline && m_bBones) {
		//--------------- draw motion event timeline
		end3D();
		
		start2D( w, h );		

		float time = gScene->getTime ();

		float t = (time-m_TimelineStart)*m_FPS;			// time in frames
		if ( t > w ) m_TimelineStart = time;			// adjust timeline window

		drawFill ( Vec2F(0, 0), Vec2F(w, 100), Vec4F(0,0,0, 0.4) );			// background
		
		for (int x=0; x < w; x += m_FPS )				// lines every second
			drawLine ( Vec2F(x, 0), Vec2F(x , 100), Vec4F(.2,.2,.2,1) );		

		// motion timeline
		for (int n=0; n < m_Motions.size(); n++ ) {
			m = m_Motions[n];
			mx1 = (m.duration.x-m_TimelineStart)*m_FPS;	// motion start & end
			mx2 = (m.duration.y-m_TimelineStart)*m_FPS;
			my = m.tslot * 20 + 20; cz = (m.active) ? 1.0 : 0.6;
			sprintf ( msg, "%s", cycleset->getName(m.cycle) );	// motion name			
			drawText ( Vec2F(mx1, my), msg, Vec4F(1,1,1, cz) );			
			drawLine ( Vec2F(mx1, my), Vec2F(mx2, my), Vec4F(1, 0, 0, cz) );
		}
		// debugging
		if ( m_PrimaryMotion >= 0 ) {
			m = m_Motions[m_PrimaryMotion];
			mx1 = (m.duration.x-m_TimelineStart)*m_FPS;	// motion start & end
			mx2 = (m.duration.y-m_TimelineStart)*m_FPS;
			drawFill ( Vec2F(mx1, my), Vec2F(mx2, my), Vec4F(1, 1, 1, cz) );
			/*sprintf ( msg, "motion: %s, start %4.2f, end %4.2f, frame %d\n", cycleset->getName(m.cycle), m.duration.x, m.duration.y, (int) m.frame );
			drawText ( 10, 100, msg, 0,0,0,1 );
			sprintf ( msg, "time: %4.2f,  frame: %4.2f\n", m_Time, m_Frame );
			drawText ( 10, 130, msg, 0,0,0,1 );		 */
		}
		drawLine ( Vec2F(t, 0), Vec2F(t, 100), Vec4F(1, 1, 1, 1) );
		end2D();
		start3D( cam );
	}

	//-------------- draw joint sets		
	bool joints = true;
	DrawJoints ( JNTS_A, EVAL_CHARACTER, joints, joints, cam );	

	//-------------- bone binding debug
	/*int j;
	for (int n=0; n < m_BindInfo.size(); n++) {
		j = m_BindInfo[n].bind[0];
		drawLine3D ( m_BindInfo[n].pos, m_Joints[JNTS_A][j].pos, Vec4F(1,1,0,1) );
	}*/
	
	//------------ draw target icon
	//drawLine3D( p.x, 0, p.z, p.x+targetdir.x*3, 0, p.z+targetdir.z*3, 1, 1, 0, 1);	
	float t2 = THIT/2.f;
	drawBox3D ( Vec3F(m_Target.x-t2, 0.05f, m_Target.z-t2), Vec3F(m_Target.x+t2, 0.05f, m_Target.z+t2), Vec4F(1, 1, 0, 1) );
}

void Character::Render ()
{	
	if (!m_bBones) {
		for (int n=0; n < getNumShapes(); n++ ) 
			getShape(n)->scale = Vec3F(0.0001f,0.0001f,0.0001f);
		return;
	}

	// Evaluate shapes
	//
	Matrix4F offs;
	offs.SRT ( Vec3F(1,0,0),Vec3F(0,1,0),Vec3F(0,0,1), Vec3F(0,0,0), 1.0f );

	float z=0;	
	
	Shape* s;

	for (int n=0; n < getNumShapes(); n++ ) {		
		s = getShape ( n );			

		s->pos = m_Joints[JNTS_A][n].pos;				
		s->pos *= offs;		
		s->ids = getIDS ( n, 0, 0 );
				
		Matrix4F w = m_Joints[JNTS_A][n].Mworld;
		w.InvTranslate(m_Joints[JNTS_A][n].pos);		// remove translations from the rotation matrix
		s->rot = w;
		
		//Vec3F(-0.5f * m_Joints[JNTS_A][n].bonesize.x,0.f,-0.5f * m_Joints[JNTS_A][n].bonesize.z)
		s->scale = m_Joints[JNTS_A][n].bonesize;
		s->pivot = Vec3F(-.5f, 0.0f, -0.5f) + m_Joints[JNTS_A][n].bonepiv;
		s->texsub = Vec4F(0,0,1,1);			//m_Joints[n].bonetexsub;
		s->clr = COLORA ( 1, 1, 1, 1 );
		
		//s->clr = VECCLR ( mClr.x, mClr.y, mClr.z, mClr.w );	
		//s->texpiv = Vec3F(0.f, m_Joints[JNTS_A][n].bonesize.w, z);	// bonesize.w = local y translation 
	}
}

int Character::getLastChild ( int p )
{
	int child = m_Joints[JNTS_A][p].child;
	if ( child == JUNDEF ) return JUNDEF;
	while ( m_Joints[JNTS_A][child].next != JUNDEF ) {
		child = m_Joints[JNTS_A][child].next;
	}
	return child;
}


int Character::AddJoint ( int s, int lev, int p )
{
	Joint jnt;
	jnt.parent = p;
	jnt.lev = lev;
	jnt.pos = Vec3F(0,0,0);
	jnt.angs = Vec3F(0,0,0);
	jnt.clr = 0;	
	jnt.child = JUNDEF;
	jnt.next = JUNDEF;
	
	int curr = (int) m_Joints[s].size();

	if ( p != JUNDEF ) {
		Joint* parnt = getJoint (s, p);
		int plastchild = getLastChild ( p );	// last child of parent
		if ( plastchild != JUNDEF ) {
			getJoint(s, plastchild)->next = curr;			
		} else {
			parnt->child = curr;			
		}
		//parnt->child = curr;					// new child of parent
		//jnt.next = child_of_parnt;				// next of current
	}

	m_Joints[s].push_back ( jnt );					// add joint

	return (int) m_Joints[s].size()-1;
}

void Character::LoadBoneInfo (std::string  fname )
{
	FILE* fp;
	char buf[16384];
	char filepath[1024];
	int mode = 0, cnt=0;	
	std::string word, tag, val, name;
	Vec3F pivot;
	Vec4F sizevec;
	Vec4F texsub;
	int j;
	Joint* jnt;

	int jset = JNTS_A;

	if ( !getFileLocation ( fname.c_str(), filepath ) ) {
		dbgprintf ( "ERROR: Cannot find file. %s\n", fname );		
		return;
	}
	fp = fopen ( filepath, "rt" );	


	while ( feof( fp ) == 0 ) {
		fgets ( buf, 16384, fp );
		word = buf;

		switch ( mode ) {
		case 0:
			word = strSplitLeft ( word, " \t\n" );
			if (word.compare("BONES")==0) mode = 1;
			break;
		case 1:
			pivot.Set(0,0,0);
			sizevec.Set(.01, .01, .01,0);
			texsub.Set(0,0,1,1);

			while ( !word.empty() ) {
				val = strParseArg ( tag, ":",  ";\n", word );
				if (tag.compare("name")==0)		name = val;
				if (tag.compare("pivot")==0)	strToVec4( val, '<', ',', '>', &pivot.x );
				if (tag.compare("size")==0)		strToVec4( val, '<', ',', '>', &sizevec.x );
				if (tag.compare("texsub")==0)	strToVec4( val, '<', ',', '>', &texsub.x );
			}			

			jnt = FindJoint ( jset, name, j);
			if ( jnt != 0x0 ) {
				jnt->bonepiv = pivot;
				jnt->bonesize = sizevec / 100.0f;		// sizevec is in mm. convert mm to meters.
				jnt->bonetexsub = texsub;
			}
			
			//printf ( "%s: %f, %f, %f\n", name.c_str(), sz.x, sz.y, sz.z );
		};
	}	

	fclose (fp );
}

Joint* Character::FindJoint ( int s, std::string name, int& j )
{
	for (j=0; j < getNumJoints(s); j++ ) {
		if ( name.compare(m_Joints[s][j].name) == 0 )
			return &m_Joints[s][j];
	}
	j = -1;
	return 0x0;
}

Vec3I Character::FindChannel(int jnt, int typ)
{
	for (int c = 0; c < getNumChannels(); c++) {
		if (m_Channels[c].x == jnt && m_Channels[c].y == typ)
			return m_Channels[c];
	}
	return Vec3I(-1,-1,-1);
}

// this is an older technique to compute the bone angs (obsolete, see ComputeOrientAngs)
Vec3F Character::ComputeBoneAngs ( Vec3F v )
{
	Vec3F r (0,0,0);	
	float len = v.Length();
	
	r.z = (fabs(v.y - v.x) < 0.0001) ? 0.0 : 90+atan2( v.y, v.x ) / DEGtoRAD;
	r.x = (fabs(v.z - len ) < 0.0001) ? 0.0 : 90+acos( v.z / len) / DEGtoRAD;
	r.y = 0.0;
	if ( fabs(v.z)>fabs(v.x) && fabs(v.z)>fabs(v.y) ) r.x+=90;

	// if ( j==20 ) r.z-=90;
	// if ( j==27 ) r.z+=90;
	return r;
}

void Character::LoadBVH ( std::string fname )
{
	FILE* fp;
	char buf[16384];
	char filepath[1024];

	std::string word, word1, word2, word3, name;
	int mode = 0;
	std::stack<int> jtree;
	int jnt = 0, jlev = 0, jparent = 0;
	int bytes = 0, cnt = 0, num_chan = 0;
	int frame = 0;
	char args[10][1024];
	Vec3F vec;	

	int jset = JNTS_A;

	if ( !getFileLocation ( fname.c_str(), filepath ) ) {
		dbgprintf ( "ERROR: Cannot find file. %s\n", fname );		
		return;
	}
	fp = fopen ( filepath, "rt" );	

	jnt = 0;
	jparent = 0;
	
	int line = 0;
	while ( feof( fp ) == 0 ) {
		fgets ( buf, 16384, fp );	

		if ( buf[0] == '#' )				// Comments
			continue;

		switch ( mode ) {
		case BVH_START:				// Start of file
			word = readword ( buf, ' ' );
			if ( word.compare ( "HIERARCHY" ) == 0 ) {
				mode = BVH_HIER;		// Goto mode Hierarchy section
			}
			break;
		case BVH_HIER:											// Hierarchy section
			word = readword ( buf, ' ' );
			
			if ( word.compare ( "ROOT" ) == 0 ) {				// Root joint
				name = readword ( buf, ' ' );					// name of joint
				fgets ( buf, 1024, fp );						// open bracket
				if ( jtree.size() != 0 ) {
					dbgprintf (  "ERROR: Improperly formatted BVH file. Too many ROOT entries? %s\n", fname );
				}
				jparent = JUNDEF; 								

				// Add joint				
				jparent = jnt;								
				jnt = AddJoint ( jset, jlev, JUNDEF );
				jtree.push ( jnt );	

				strncpy ( getJoint(jset, jnt)->name, name.c_str(), 64 );

			} else if ( word.compare ( "JOINT" ) == 0 ) {		// Tree joint		
				name = readword ( buf, ' ' );					// name of joint
				
				fgets ( buf, 1024, fp );						// read open bracket '{'
				if ( jtree.size()==0) {
					dbgprintf ( "ERROR: Improperly formatted BVH file. Missing bracket? %s\n", fname );
				}				
				jparent = jtree.top();	
				jlev++;

				// Add joint	
				int jprev = jnt;				
				jnt = AddJoint ( jset, jlev, jparent );				
				jtree.push ( jnt ); 
				jparent = jprev;		

				strncpy ( getJoint(jset, jnt)->name, name.c_str(), 64 );
				

			} else if ( word.compare ( "OFFSET" ) == 0 ) {		// Joint offset data
			
				cnt = sscanf ( buf, "%s %s %s", args[0], args[1], args[2] );
			
				vec.Set ( (float) atof(args[0]), (float)atof(args[1]), (float) atof(args[2]) );	
				
				//---- seems like it should work with this. (apply offset to current joint instead of parent)
				/*if (jnt != JUNDEF) {
					m_Joints[jset][jnt].bonevec = vec;				// Store bone vector for debugging (removed later)
					m_Joints[jset][jnt].bonepiv = vec;				// Store bone vector for refactoring
					m_Joints[jset][jnt].length = vec.Length();		// Bone length
				}*/

				if ( jparent != JUNDEF ) {
					m_Joints[jset][ jparent ].bonevec = vec;				// Store bone vector for debugging (removed later)
					m_Joints[jset][ jparent ].bonepiv = vec;				// Store bone vector for refactoring				
					m_Joints[jset][ jparent ].length = vec.Length();		// Bone length
				}

			} else if ( word.compare ( "CHANNELS" ) == 0 ) {	// Joint channel info
				
				cnt = sscanf ( buf, "%s %s %s %s %s %s %s %s %s %s", args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9] );
				int c = 0, ccnt = atoi ( args[0] );
				
				for ( c = 0; c < ccnt; c += 3 ) { 		
					word1 = args[c+1]; word2 = args[c+2]; word3 = args[c+3]; 
					if ( word1.compare("Xposition")==0 && word2.compare("Yposition")==0 && word3.compare("Zposition")==0 ) {
						// record joint which has position frames
						m_Channels.push_back ( Vec3I ( jnt, CHAN_XYZ_POS, 0 ) );
					}
					if ( word1.compare("Zrotation")==0 && word2.compare("Yrotation")==0 && word3.compare("Xrotation")==0 ) {
						// record joint which has rotation frames
						m_Channels.push_back ( Vec3I ( jnt, CHAN_ZYX_ROT, 0 ) );
					}
				}

			} else if ( word.compare ( "End" ) == 0 ) {			// End effector
				fgets ( buf, 1024, fp );						// open bracket
				jtree.push( jnt );
				jlev++;
			} else if ( word.compare ( "}" ) == 0 ) {			// Close joint bracket
				jparent = jtree.top ();							// pop to parent
				jtree.pop ();
				jlev--;
				if ( jtree.size() == 0 ) mode = BVH_MOTION;
			}
			break;

		case BVH_END: case BVH_MOTION:
			break;
		}; 
	}

	// Create shapes	

	if ( getOutShapes()==0x0 ) 
		Generate(0,0);		// generate output shapes if not done yet

	Shape* s;	
	int mesh = getInputID ( "mesh",	'Amsh' );

	for (int n=0; n < getNumJoints(jset); n++ ) {
		s = AddShape ();
		s->type = S_MESH;		
		s->meshids.x = mesh;
		s->pos.Set(0,0,0);
		s->scale.Set(1,1,1);
	}	

	char indent[16];
	memset ( indent, ' ', 16 ); indent[15]='\0';
	Joint* j;
	for (int n=0; n < getNumJoints(jset); n++ ) {
		j = getJoint(jset,n);	
		
		indent[ j->lev ] = '\0';
		//printf ( "   Joint: %2d, |%2d c%2d p%2d, %s (%.3f,%.3f,%.3f) \n", n, (j->next==JUNDEF) ? -1 : j->next, (j->child==JUNDEF) ? -1 : j->child, (j->parent==JUNDEF) ? -1 : j->parent, j->name, j->bonevec.x, j->bonevec.y, j->bonevec.z);
		indent[ j->lev ] = ' ';
	}
}

void Character::CopyJoints ( int src, int dst )
{
	while ( m_Joints[dst].size() < m_Joints[src].size() )
		m_Joints[dst].push_back ( Joint() );
	
	for (int n=0; n < getNumJoints(src); n++) {		
		memcpy ( &m_Joints[dst][n], &m_Joints[src][n], sizeof(Joint) );
	}
}

/* void Character::UpdateBlendJoints ()
{
	Vec3F dangs;
	
	float amt;
	Quaternion r, q1, q2, qd;

	for (int n=0; n < getNumJoints(); n++) {
		q1 = m_BlendJoints[n].orient;
		q2 = m_Joints[n].orient;		
		qd = q2.inverse() * q1;
		amt = qd.getAngle() / 40.0f;
		if ( amt < 0.1f ) amt = 0.1f;
		if ( amt > 1.0f ) amt = 1.0f;
		r.slerp ( q1, q2, amt );		
		m_BlendJoints[n].orient = r;
	}
	//m_BlendJoints[0].orient = m_Joints[0].orient;
} */


// 
void Character::EvaluatePoseChange ( int frame, int cyid, Quaternion co, Quaternion& co_delta )
{
	// Get rotational change in pose

	Vec3F fwd (0, 0, 1), cofwd(0,0,1);
	Matrix4F mtx;
	Quaternion roty;

	// forward direction of the new pose
	co.getMatrix ( mtx );			
	cofwd *= mtx;
	cofwd.y = 0;

	// current forward direction
	m_Orient[JNTS_A].rot.getMatrix ( mtx );
	fwd *= mtx;
	fwd.y = 0;	

	// align direction of motion with new pose
	roty.fromRotationFromTo ( cofwd, fwd );		
	
	// change in pose:
	// Oinv = returns to identity matrix (Orient * Oinv = I)
	// roty = orient the pose along current forward direction (2D in-plane)
	// co   = adjust the orientation to account for 3D articulation of new base pose
	co_delta = m_Orient[JNTS_A].rot.inverse() *  roty * co;
}

// EvaluateCycleDerivatives
// - determine the *change* in character orientation (cpd & cod) 
void Character::EvaluateCycleDerivatives ( int jset, int cyid, float frame, float df, Vec3F cp, Vec3F cp_last,
					Quaternion co, Quaternion col, 
					Vec3F& cp_delta, Quaternion& co_delta )
{
	co.normalize();
	col.normalize();

	// Get positional derivative		
	Quaternion r = m_Orient[jset].rot * co.inverse();		// remove cycle orientation, orient to current figure	
	Matrix4F mtx;	
	r.normalize();
	r.getMatrix ( mtx );
	cp_delta = (cp - cp_last);					// change in cycle pos (cyc_pos - cyc_posL)	
	cp_delta *= mtx;	

	// Get rotational derivative
	co_delta  = col.inverse() * co;				// c = a - b, finds the difference in orientation (rotational velocity)			
	co_delta.normalize();
}

void Character::EvaluateJoints (int jset, int method )
{
	// choose world in which to evaluate joints
	Matrix4F world;
	Quaternion orient;
	switch (method) {
	case EVAL_IDENTITY:		//--- no orientation (testing)		
		world.Identity ();				
		break;
	case EVAL_CHARACTER: {	//--- current character orientation		
		int jorient = 0;
		if ( m_PrimaryMotion != -1 && m_SecondaryMotion != -1 )	jorient = 2; 
		orient = m_Orient[jorient].rot;
		orient.getMatrix ( world );
		world.PostTranslate ( m_Orient[jorient].pos );				
		} break;	
	case EVAL_ORIENT: 		//--- joint set orientation		
		orient = m_Orient[jset].rot;
		orient.getMatrix ( world );
		world.PostTranslate ( m_Orient[jset].pos );			
		break;	
	case EVAL_CYCLE: 		//--- cycle motion
		orient = m_Joints[jset][0].orient;
		orient.getMatrix ( world );		
		world.PostTranslate ( m_Joints[jset][0].pos );
		break;
	case EVAL_POSE: 		//--- orientation from cycle (no translation)
		orient = m_Joints[jset][0].orient;
		orient.getMatrix ( world );				
		break;
	};
	// evaluate joints
	EvaluateJoints ( m_Joints[jset], world, orient );	
}

void Character::EvaluateJoints ( std::vector<Joint>& joints, Matrix4F& world, Quaternion& orient )
{
	EvaluateJointsRecurse ( joints, 0, world, orient );
}

// recursive funcs
void Character::EvaluateJointsRecurse ( std::vector<Joint>& joints, int curr_jnt, Matrix4F world, Quaternion morient )
{
	// Evaluation of joint chain
	//
	// local orientation
	Matrix4F orient_mtx;	
	Vec3F a; 
	
	// joints[curr_jnt].orient.toEuler ( a );			// cast to Euler ZYX angles first
	// orient.RotateZYX ( a );						
	joints[curr_jnt].orient.getMatrix ( orient_mtx );	// Ri' = orientation angles (animated)			
		
	// rotate joint
	if ( curr_jnt > 0 ) {
		world *= orient_mtx;							// Mw = M(w-1) Ri' v''
		morient = morient * joints[curr_jnt].orient;
	}

	joints[curr_jnt].Mworld = world;
	joints[curr_jnt].Morient = morient;
	joints[curr_jnt].pos = world.getTrans();			// Tworld

	// translate child to end of bone
	world.PreTranslate(Vec3F(0.f, joints[curr_jnt].length, 0.f));		// v'' = bone length
		
	// recurse	
	int child_jnt = joints[curr_jnt].child;
	while ( child_jnt != JUNDEF ) {
		EvaluateJointsRecurse ( joints, child_jnt, world, morient );
		child_jnt = joints[child_jnt].next;
	}
}




/*if ( n==1 && m_Joints[n].parent != 65535 ) {
			
			q = m_Joints[ n ].pos;			
			
			// the non-oriented frame of a new joint is the identity matrix			
			pmtx.Identity();

			// draw the bone vector, v = Rj0 v''
			u = m_Joints[n].bonevec*0.5f; u *= pmtx;
			drawLine3D ( q.x, q.y, q.z, q.x+u.x, q.y+u.y, q.z+u.z, 1, 1, 0, 1 );	// yellow

			// draw animated joint, Ri v'', WITHOUT the bone rotation
			Ri.RotateZYX ( m_Joints[n].angs );			// Ri    = joint rotation (animated)
			Mv = pmtx; 
			Mv *= Ri;
			u = Vec3F(0.f, m_Joints[ n ].length*0.5f, 0.f );   // v'' = <0, w, 0>
			u *= Mv;					
			drawLine3D ( q.x, q.y, q.z, q.x+u.x, q.y+u.y, q.z+u.z, 1, 0, 1, 1 );	// purple

			Vec3F boneangs = ComputeBoneAngs ( m_Joints[n].bonevec );

			// draw animated joint, Ri Rj0 v'' /w normalized bone rotation
			Rj0.RotateZYX ( boneangs );		// Rj0   = bone orientation refactoring (for BVH)			
			Mv = pmtx;
			Mv *= Ri;
			Mv *= Rj0;			
			u = Vec3F(0.f, m_Joints[ n ].length*0.5f, 0.f );   // v'' = <0, w, 0>
			u *= Mv;					
			drawLine3D ( q.x, q.y, q.z, q.x+u.x, q.y+u.y, q.z+u.z, 1, 0.5, 0, 1 );	// orange 

			// draw the concatenated orientation angles, Ri'
			Quaternion a, b, c;
			a.set ( m_Joints[n].angs );
			b.set ( boneangs );
			c = a * b;
			c.toEuler ( xangs );				// fixed Euler angles
			Ri2.RotateZYX ( xangs );			// rotation matrix from angles			
			// draw the joint using the more conventient Ri'			
			u = Vec3F(0.f, m_Joints[ n ].length*0.75f, 0.f );   // v'' = <0, w, 0>
			u *= Ri2;
			drawLine3D ( q.x, q.y, q.z, q.x+u.x, q.y+u.y, q.z+u.z, 0, 1, 1, 1 );	// cyan 
		}*/