
#include "camera.h"
#include "quaternion.h"

#include "gxlib.h"
using namespace glib;

#define CH_FROM		0
#define CH_TO		1
#define CH_DOF		2

bool Camera::RunCommand(std::string cmd, vecStrs args)
{
	Vec3F p, t;
	if (cmd.compare("fov") == 0)	{ cam3d.setFov ( strToF(args[0])); return true; }
	if (cmd.compare("orbit") == 0)	{ 
		cam3d.SetOrbit ( strToVec3(args[0], ';'), strToVec3(args[1], ';'), strToF(args[2]), strToF(args[3]) );  
		return true; 
	}
	if (cmd.compare("look") == 0)	{ 
		p = strToVec3(args[0], ';');
		cam3d.setPos ( p.x, p.y, p.z );
		t = strToVec3(args[1], ';');
		cam3d.setToPos ( t.x, t.y, t.z );
		return true; 
	}
	if (cmd.compare("nearfar") == 0) { cam3d.setNearFar ( strToF(args[0]), strToF(args[1]) ); return true; }
	if (cmd.compare("finish") == 0) { return true; }

	return false;
}

void Camera::Define (int x, int y)
{
	AddInput( "time",	 'Atim');	

	AddParam( C_FOV,	 "fov",		"f");	SetParamF( C_FOV, 0, 40.0 );
	AddParam( C_NEARFAR, "nearfar",	"3");	SetParamV3( C_NEARFAR, 0, Vec3F(1, 1000,0 ) );
	AddParam( C_DISTDOL, "distdolly","3" );	SetParamV3( C_DISTDOL, 0, Vec3F(20,1,0) );
	AddParam( C_ANGS,	 "angs",	"3" );	SetParamV3( C_ANGS, 0, Vec3F(0,10,0) );
	AddParam( C_FROM,	 "from",	"3" );	SetParamV3( C_FROM, 0, Vec3F(6,10,12) );
	AddParam( C_TO,		 "to",		"3" );	SetParamV3( C_TO, 0, Vec3F(0,0,0) );	
	AddParam( C_DOF,	 "dof",		"3" );	SetParamV3( C_DOF, 0, Vec3F(0.18, 35, 8) );	

	AddParam( C_KEY,	"key",		"f3333" );	
		SetParamF  ( C_KEY, 0, 0.0 );
		SetParamV3 ( C_KEY, 1, Vec3F(0, 10, 0) );		// angs
		SetParamV3 ( C_KEY, 2, Vec3F(0, 10, 12) );		// from
		SetParamV3 ( C_KEY, 3, Vec3F(0, 0, 0) );		// to
		SetParamV3 ( C_KEY, 4, Vec3F(0.18, 35, 8) );	// dof

	mTimeRange.Set(0, 10000, 0 );
}

void Camera::Generate(int x, int y)
{
	UpdateKeys();

	ReadToCam3D ( getCam3D() );
}


void Camera::UpdateKeys ()
{
	int n;
	float tm;
	Vec3F angs, from, to, dof;
	Quaternion q;

	GetParamArray ( "key", m_Keys );

	// Create spline
	CreateSpline();
	AddChannel ( CH_FROM, 'b', '3' );	// camera from: bspline, vec3
	AddChannel ( CH_TO,   'b', '3' );	// camera to:   bspline, vec3
	AddChannel ( CH_DOF,  'b', '3' );	// camera dof:  bspline, vec3
	for (n=0; n < m_Keys.size(); n++) {
		tm = getParamF( m_Keys[n], 0 );
		angs = getParamV3( m_Keys[n], 1 );
		from = getParamV3( m_Keys[n], 2 );
		to =  getParamV3( m_Keys[n], 3 );
		dof = getParamV3( m_Keys[n], 4 );
		AddKeyToSpline( tm );
		SetKey ( CH_FROM, from );
		SetKey ( CH_TO, to );
		SetKey ( CH_DOF, dof );
	}	
	UpdateSpline ();

	n = m_Keys.size()-1;
	m_EndTime = getParamF( m_Keys[n], 0 );	
}

void Camera::WriteFromCam3D ( Camera3D* cam)
{
	// Update the parameters from the Camera3D
	SetParamF ( C_FOV, 0, cam->getFov() );
	SetParamV3 ( C_NEARFAR, 0, Vec3F( cam->getNear(), cam->getFar(), 0 ) );
	SetParamV3 ( C_ANGS, 0, cam->getAng() );
	SetParamV3 ( C_FROM, 0, cam->getPos() );
	SetParamV3 ( C_TO,	 0, cam->getToPos() );
	SetParamV3 ( C_DOF,	 0, cam->getDOF() );
	SetParamV3 ( C_DISTDOL, 0, Vec3F( cam->getOrbitDist(), cam->getDolly(), 0 ) );
}

void Camera::ReadToCam3D ( Camera3D* cam )
{
	float fov = getParamF ( C_FOV );
	
	Vec3F nf = getParamV3(C_NEARFAR);
	Vec3F distdol = getParamV3(C_DISTDOL);	
	Vec3F angs = getParamV3(C_ANGS);
	Vec3F from = getParamV3(C_FROM);	
	Vec3F dof = getParamV3(C_DOF);
	Vec3F to = getParamV3(C_TO);

	cam->setFov ( fov );
	cam->setNearFar ( nf.x, nf.y );		
	cam->setDOF (dof);
	cam->setDirection ( from, to );				// updates from, to, angles, dist
	SetParamV3 (C_ANGS, 0, cam->getAng() );		// get new angles
	distdol.x = cam->getOrbitDist();
	distdol.y = cam->getDolly();
	SetParamV3(C_DISTDOL, 0, distdol);

	if ( m_Keys.size() > 1 ) {
		angs = getParamV3( m_Keys[0], 1 );
		from = getParamV3( m_Keys[0], 2 );
		to =  getParamV3( m_Keys[0], 3 );
		dof = getParamV3( m_Keys[0], 4 );			
		cam->setPos ( from.x, from.y, from.z );	
		cam->setToPos ( to.x, to.y, to.z );
		cam->setDOF ( dof );	
	}

	cam->updateAll();
}

void Camera::AddKey (float time)
{
	// Add key from self
	int kn = m_Keys.size();
	std::string keyname = "key[" + iToStr(kn) + "]";	

	Vec3F angs, from, to, dof;
	angs = cam3d.getAng ();
	from = cam3d.getPos ();
	to = cam3d.getToPos ();
	dof = cam3d.getDOF ();

	SetParam ( keyname, fToStr(time), ',', 0 );			// creates a new param
	SetParam ( keyname, angs, 1 );
	SetParam ( keyname, from, 2 );
	SetParam ( keyname, to, 3 );
	SetParam ( keyname, dof, 4 );

	UpdateKeys ();
}


void Camera::Run(float time)
{
	if ( m_Keys.size() > 1 && time < m_EndTime) {

		Vec3F from, to, dof;
		EvaluateSpline( time );		// evaluate camera curve
		
		from = getSplineV3(CH_FROM);
		to = getSplineV3(CH_TO);
		dof = getSplineV3(CH_DOF);
		cam3d.setPos ( from.x, from.y, from.z );
		cam3d.setToPos ( to.x, to.y, to.z );
		cam3d.setDOF ( dof );		
		cam3d.setDirection ( from, to );		// set angles given the new from and to		
		
		MarkDirty();
	}
}

void Camera::Sketch ( int w, int h, Camera3D* cam )
{
	Vec3F al, bl;
	Vec3F a, b;

	float dt = m_EndTime/80.0;
	for (float t=0; t < m_EndTime; t += dt ) {
		EvaluateSpline ( t );
		a = getSplineV3(CH_FROM);
		b = getSplineV3(CH_TO);
		drawLine3D ( a, al, Vec4F(1,1,0,1) );
		drawLine3D ( a, b, Vec4F(.6,.6,.6,1) );
		al = a;
		bl = b;
	}	
}