
//#define DEBUG_MEM
#define DEBUG_GL	false
#define PROFILE		false
#define STATS			false


#include <time.h>
#include "timex.h"

#include "main.h"			// window system 
#include "gxlib.h"		// graphics lib
using namespace glib;

#include "scene.h"
#include "render.h"
#include "render_gl.h"
#include "render_optix.h"
#include "object_list.h"
#include "lightset.h"
#include "image.h"

#ifdef BUILD_CUDA
	#include "common_cuda.h"
	#ifdef BUILD_OPTIX
		#include "optix_scene.h"
	#endif
#endif

#define MODE_NONE				0
#define MODE_FLY				0
#define MODE_ORBIT			1
#define MODE_AUTO_ORBIT	2

class Sample : public Application {
public:
	virtual bool init();
	virtual void on_arg(int i, std::string arg, std::string val);
	virtual void startup ();
	virtual void display();
	virtual void reshape(int w, int h);
	virtual void motion (AppEnum button, int x, int y, int dx, int dy);	
	virtual void mousewheel(int delta);
	virtual void keyboard(int keycode, AppEnum action, int mods, int x, int y);
	//virtual void keyboardchar(unsigned char key, int mods, int x, int y);
	virtual void mouse (AppEnum button, AppEnum state, int mods, int x, int y);	
	virtual void shutdown();
	
	void		start_guis ( int w, int h );
	void		handle_events ();
	void		MoveCamera(char how, Vec3F amt);
	void		UpdateCamera ();
	void		ResetScene ( bool newseed );
	void		DrawTimeline();
	void		DrawInfo ();

	int			m_rendergl_tex, m_renderoptix_tex;
	int			m_rendergl_pick;	
	int			m_section_tex;
	bool		m_record;
	int			m_record_start;
	float		mouse_plane;
	int			mouse_down;
	
	int			m_cam_mode;
	bool		m_overlay;
	bool		m_showeval;	
	bool		m_show_info;
	bool		m_display, m_stats;
	int			m_select;
	
	int			mFirstChr;
	int			mFirstTForm;	
	TimeX		m_clock, m_lastclock;			// timex
	float		m_time;
	int			m_lastframe;
	float		m_tave, m_tcnt;

	int				m_mode;	
	Vec3F			m_M1, m_M2, m_M3, m_N;
	Vec3I			m_S1, m_S2;

	std::string			m_SceneFile;

	Scene					mScene;
	RenderMgr			mRenderMgr;
	std::vector<Object*>	mTimeObjects;

	RenderGL*			m_RendGL;

	#ifdef BUILD_OPTIX
		RenderOptiX*	m_RendOptiX;
		OptixScene		mOptix;	
	#endif
	#ifdef BUILD_PORTAUDIO
		SoundMgr		mSoundMgr;
	#endif
	bool				mbSoundOn;	

	float		mAng;

};
Sample obj;

void Sample::start_guis (int w, int h)
{
	//setview2D ( w, h );
	//guiSetCallback ( 0x0 );		
	//addGui ( 10, h-30, 130, 20, "Overlay", GUI_CHECK, GUI_BOOL, &m_overlay, 0.f, 1.f );		
}

void Sample::on_arg(int i, std::string arg, std::string val)
{
	if (i==1 && val.empty()) {
		m_SceneFile = arg;
	}
}

bool Sample::init ()
{
	mFirstTForm = -1;	
	m_clock.SetTimeNSec ();
	m_lastclock = m_clock;
	m_time = 0;
	m_lastframe = 0;
	m_select = -1;
	m_tave = 0;
	m_tcnt = 0;
	
	m_show_info = false;
	m_cam_mode = MODE_FLY;	
	m_stats = STATS;	
	m_showeval = false;

	m_M1.Set(-1000,0,0);
	m_M2.Set(-1000,0,0);

	// Default resolution
	m_display = false;
	int w = getWidth(), h = getHeight();			// window width & height
	mouse_down = -1;
	m_rendergl_tex = -1;	
	m_renderoptix_tex = -1;	
	m_rendergl_pick = -1;
	
	mAng = 0;
   
  int s = sizeof(Shape);

	mouse_plane = 3;			// height


	// Usage - no scene file
	if (m_SceneFile.empty()) {
    dbgprintf("\nNO SCENE FILE FOUND\n\n");
    dbgprintf ("Usage: shapes {scene_file}\n\n");
    dbgprintf ("{scene_file}   Scene file to render, txt or gltf.\n\n");
    dbgprintf ("Data Path: %s  <-- searching for scenes here\n", ASSET_PATH );
    dbgprintf ("Shader Path: %s\n", SHADER_PATH );
    dbgprintf ("\n");		  
    exit(-1);
	}

	// Instrumentation
  //dbgprintf ( "Perf profiling:\n");
  bool cpu_perf = PROFILE;	
  bool gpu_perf = PROFILE;
  PERF_INIT ( 64, cpu_perf, gpu_perf, true, 0, "" );		// cpu, gpu, cons

	#ifdef BUILD_CUDA
		// Start CUDA	
		dbgprintf("Starting CUDA.\n");
		CUdevice dev; 
		CUcontext ctx;
		cuStart ( DEV_FIRST, 0, dev, ctx, 0, true );
	#endif


	// Search paths
 // dbgprintf ( "ASSET PATH: %s\n", ASSET_PATH );
  //dbgprintf ( "SHADER PATH: %s\n", SHADER_PATH );
  addSearchPath( "D:\\assets" );
  addSearchPath ( ASSET_PATH );	
  addSearchPath ( SHADER_PATH );	
  addSearchPath ( "." );	
	
	// Delay-Loaded Assets 
  gAssets.AddAssetPath( "D:\\assets" );
  gAssets.AddAssetPath ( ASSET_PATH );
  gAssets.AddAssetPath ( SHADER_PATH );
	
	// GUI interface
	dbgprintf("Starting GUI.\n");
	init2D ( "arial_24" );
	setview2D ( w, h );	
	setTextSz ( 16, 1 );		
	start_guis ( w, h );

	#ifdef BUILD_OPTIX
		mOptix.InitializeOptix ( w, h );	
	#endif

	// Load Scene 			
  std::string filepath;
  if (getFileLocation(m_SceneFile, filepath)) {
    dbgprintf("\nLOADING SCENE: %s\n", filepath.c_str());
	  mScene.Load ( m_SceneFile, w, h);	// load scene file	
  } else {
    dbgprintf("\n**** ERROR: Unable to find file %s\n", m_SceneFile.c_str());
    exit(-1);
  }

	// load anatomy models
	//gAssets.LoadDir ( "F:\\anatomy_data\\obj\\", "*.*", failed );
	//mScene.ShowAssets ();

	// Render output texture
	glViewport ( 0, 0, w, h );
	createTexGL ( m_rendergl_tex, w, h );		
	createTexGL ( m_rendergl_pick, w/2, h/2 );		

	// Rendering
	dbgprintf("\nINITIALIZING RENDERERS.\n");
	m_RendGL = new RenderGL;
	m_RendGL->SetDebug ( DEBUG_GL );
	mRenderMgr.AddRenderer ( m_RendGL, m_rendergl_tex );

	#ifdef BUILD_OPTIX
		createTexGL(m_renderoptix_tex, w, h);
		m_RendOptiX = new RenderOptiX;
		m_RendOptiX->SetOptix ( &mOptix );	
		mRenderMgr.AddRenderer ( m_RendOptiX, m_renderoptix_tex );
	#endif
	
	// Initialize
	mRenderMgr.Initialize ( &mScene );		

	// Generate
	dbgprintf("\nGENERATING SCENE.\n");
	mScene.Generate ( w, h );				// create scene outputs & RIDs

	// Get list of temporal (keyframed) objects
	mScene.getTimeObjects( mTimeObjects );

	// Start renderer		
	mRenderMgr.SetRecording ( mScene.getGlobals()->getParamI(G_RECORD ) );
	mRenderMgr.SetFrameRange ( Vec3I( mScene.getGlobals()->getParamI(G_RECORD_START), 1000000, 0) );
	mRenderMgr.SetRendererForRecording ();

	if ( mRenderMgr.isRecording() )
		mRenderMgr.SetAnimation ( true );	
	
	// Set ending time
	Camera* camobj = mScene.getCameraObj();	
	float endtime = camobj->getEndTime();
	Vec3F range = mScene.getGlobals()->getParamV3 ( G_TIMERANGE );
	if (range.y==0) mScene.getGlobals()->SetParamV3 ( G_TIMERANGE, 0, Vec3F(0, endtime, 0) );

	/* // [debugging]
	printf ( "CUdeviceptr: %d\n", sizeof(CUdeviceptr) );
	printf ( "CUgraphicsResource: %d\n", sizeof(CUgraphicsResource) );
	printf ( "CUarray: %d\n", sizeof(CUarray) );
	printf ( "CUtexObject: %d\n", sizeof(CUtexObject) );
	printf ( "CUsurfObject: %d\n", sizeof(CUsurfObject) );
	printf ( "std::string: %d\n", sizeof(std::string) );
	printf ( "cuDataX: %d\n", sizeof(cuDataX) );
	printf ( "char*: %d\n", sizeof(char*) );
	*/

	// Set inital window resolution
	Vec3F res = mRenderMgr.GetFrameRes();
	appResizeWindow ( res.x, res.y );			
	reshape(res.x, res.y);
	UpdateCamera(); 

	mouse_down = BUTTON_NONE;
	m_display = true;						// start display loop
	return true;
}


#define UI_CMDS			0
#define UI_JOINTS		1
#define UI_CENTER		2

#define UI_ANIM			3		
#define UI_STRETCH		4
#define UI_WALK			5
#define UI_JUMP			6

void Sample::handle_events()
{

}

void Sample::DrawTimeline () 
{
	int w = getWidth(), h = getHeight();
	
	float fps = 30.0;
	float spacing = 4;					// pixels per frame (timeline scale)
	float pix_per_sec = fps*spacing;	// pixels per second 

	setTextSz(16, 1);

	start2D ( w, h );
	
	// timeline tick marks
	drawFill ( Vec2F(0, h-40), Vec2F(w, h), Vec4F(0,0,0,.5) );
	for (int n=0; n < w; n+= spacing) {
		drawLine ( Vec2F(n, h-40), Vec2F(n, h), Vec4F(1,1,1,.15) );
		if ( (n % int(pix_per_sec))==0 ) drawLine( Vec2F(n, h - 40), Vec2F(n, h), Vec4F(1, 1, 1, 1) );
	}
	// object ranges
	int y = h-40+2;
	Vec3F range;
	for (int n = 0; n < mTimeObjects.size(); n++) {
		range = mTimeObjects[n]->mTimeRange * pix_per_sec;
		drawFill ( Vec2F(range.x, y), Vec2F(range.y, y+3), Vec4F(0, 1, 0, 1) );
		y += 4;
	}
	// current time
	float t = mScene.getTime() ;
	drawLine( Vec2F(t * pix_per_sec, h - 40), Vec2F(t * pix_per_sec, h), Vec4F(1, 0, 0, 1) );

	// print time 
	char msg[512];
	sprintf ( msg, "%4.2f  %d", t, int(t*fps) );
	drawText( Vec2F(2, h-38), msg, Vec4F(1,1,1,1) );


	end2D();
}

void Sample::ResetScene ( bool newseed )
{
	m_time = 0;
	m_lastframe = 0;

	gScene->RegenerateScene( newseed );		// regenerate scene
	gScene->Execute ( false, 0, 0, false );	
}

void Sample::DrawInfo ()
{
	if (!m_show_info) return;

	setTextSz (16, 1);
	start2D( getWidth(), getHeight() );
	char msg[512];
	Vec3F dof = mScene.getCamera3D()->getDOF();
	sprintf ( msg, "time: %4.2f  frame: %d  dof: %4.3f, %4.3f", mScene.getTime(), mRenderMgr.getFrame(), dof.x, dof.z );
	drawText ( Vec2F(5, 5), msg, Vec4F(1,1,1,1) );
	end2D();
}

void Sample::display ()
{	
	int frame, frame_adv = 0;

	if (!m_display) return;

	int w = getWidth();
	int h = getHeight();
	Camera3D* cam = mScene.getCamera3D();

	if (m_showeval) 
		dbgprintf ("--- FRAME\n");

	if (m_stats) mScene.Stats();		// print stats

	handle_events ();

	//------------- Advance or Execute
	if ( mRenderMgr.DoAdvance() ) {

		// Advance, run time forward
		m_clock.SetTimeNSec();
		float elapsed = m_clock.GetElapsedMSec(m_lastclock) / 1000.f;	// time since we were last here
		m_time += elapsed;												// advance simulation time by elapsed real time
		m_lastclock = m_clock;
		
		// Advance frame
		float fps = mScene.getFPS();		
		frame = mScene.getRate() * fps * m_time;		// current frame
		frame_adv = frame - m_lastframe;				// number of frames elapsed		
		if (frame_adv > 0) {
			frame_adv = 1;		// at most 1 frame (optional)
			
			for (int n = 0; n < frame_adv; n++)								// run the scene for N frames
				mScene.Execute (true, mScene.getTime() + (1.0 / fps), (1.0 / fps), m_showeval);		// true = run (advance time)
	
			UpdateCamera();
			m_lastframe = frame;
		}
		mRenderMgr.SetFrameAdvance ( frame_adv );

	} else {
		// Execute, no time advance
		mScene.Execute (false, mScene.getTime(), 0, m_showeval);
		frame = mScene.getRate() * mScene.getFPS() * m_time;
	}

	// dbgprintf ( "Time: %f   Frame: %d\n", m_time, frame );
	
	// Move camera automatically
	if (m_cam_mode==MODE_AUTO_ORBIT) {		
		MoveCamera('o', Vec3F(0.04f, 0, 0));
	}

	//------------- Render	
	if (m_showeval) {
		PERF_PUSH("RENDER");
		dbgprintf ( "  RENDER\n" );		
	}	
	// render timing
	TimeX clk1, clk2;
	clk1.SetTimeNSec();			

	// Render 3D scene
	clearGL();
	mRenderMgr.Render (w, h, m_rendergl_pick);

	if ( mRenderMgr.getCurrentRenderer()==0) {						// Render GL		
		renderTexGL ( w, h, m_rendergl_tex, 1 );						// invert opengl fbo output
		if (mRenderMgr.getOptGL(OPT_SKETCH_PICK)) renderTexGL ( w/2, h/2, m_rendergl_pick, 1 );		// debug - picking buffer
	} else {
		glBindFramebuffer ( GL_FRAMEBUFFER, 0 );						// Render OptiX
		renderTexGL ( w, h, m_renderoptix_tex, 0 );
	}

	clk2.SetTimeNSec();
	float t = clk2.GetElapsedMSec(clk1);	
	m_tave += t;
	m_tcnt ++;

	if (m_showeval) {
		PERF_POP();		
		dbgprintf("    %f msec\n", t);
	}	
	
	//------------ Record completed frame
	if ( mRenderMgr.RecordFrame() ) {
		if ( mRenderMgr.isAnimating() ) {
			// If recorded last frame at end of animation range.. Quit app!		
			Vec3F range = mScene.getGlobals()->getParamV3 ( G_TIMERANGE );
			if ( mScene.getTime() > range.y ) {
				dbgprintf ( "END OF ANIMATION. Terminating.\n");
				appQuit();		// Done. Quit!
			}
		} else {
			// Stop recording (single frame)
			mRenderMgr.SetRecording(false);
		}
	}


	//------------------ Overlay Render 

	// 2D GUI
	PERF_PUSH(" DRAW2D");
	  mRenderMgr.Sketch2D();
	DrawInfo ();

	PERF_POP();

	// 2D Overlay
	start2D(w, h);
		
		// draw fps
		char msg[256];		
		sprintf ( msg, "%4.2f ms (%4.2f fps)", m_tave/m_tcnt, 1000.0/(m_tave/m_tcnt) );
		drawText ( Vec2F(10,10), msg, Vec4F(1,1,1,1) );
		
	end2D();  
	
	drawAll ();

  // CSM debugging - draw cascade maps
  /* int csm_tex = mRenderMgr.getGL()->getCSMTex();
  renderTexArrayGL(0, 0, 200, 200, csm_tex, 0);
  renderTexArrayGL(300, 0, 500, 200, csm_tex, 1);
  renderTexArrayGL(600, 0, 800, 200, csm_tex, 2);
  renderTexArrayGL(900, 0, 1100, 200, csm_tex, 3); */

	appPostRedisplay();				// refresh display
}

void Sample::UpdateCamera ()
{
	Camera3D* cam = mScene.getCamera3D();
	if (cam==0) return;

	// update camera res & aspect	
	int w = getWidth(), h = getHeight();	
	cam->setSize ( w, h );
	cam->setAspect( float(w)/float(h) );	
	cam->updateAll ();

	Camera* camobj = mScene.getCameraObj();
	camobj->WriteFromCam3D ( cam );

	// update renderers	
	mRenderMgr.UpdateCamera ();			
	
	appPostRedisplay();				// update display
}

void Sample::MoveCamera ( char how, Vec3F amt )
{
	Camera3D* cam = mScene.getCamera3D();

	switch (how) {
	case 'a': {														// Fly Angles (view direction)
		Vec3F angs = cam->getAng();
		angs += amt;
		cam->setAngles (angs.x, angs.y, angs.z);
	} break;
	case 'f': {														// Fly Forward 		
		float zoom = (cam->getOrbitDist() - cam->getDolly());
		cam->moveRelative(0, 0, -amt.z * zoom * 0.0001f);
		//cam->setDist ( (cam->getToPos() - cam->getPos()).Length() );
		} break;	
	case 'o': {														// Orbit
		Vec3F angs = cam->getAng();
		angs += amt;
		//if ( angs.x >= 80 ) angs.x = 0;
		//dbgprintf ( "angs: %f %f \n", angs.x, angs.y );
		cam->SetOrbit(angs, cam->getToPos(), cam->getOrbitDist(), cam->getDolly());
		} break;
	case 'z': {														// Zoom
		float dist = cam->getOrbitDist(); 
		float zoom = (dist - cam->getDolly()) * 0.001f;
		dist -= amt.z * zoom;		
		cam->SetOrbit(cam->getAng(), cam->getToPos(), dist, cam->getDolly() );
		} break;
	case 't': {														// Track
		float zoom = (cam->getOrbitDist() - cam->getDolly()) * 0.001f;
		cam->moveRelative ( amt.x * zoom, amt.y * zoom, amt.z * zoom);
		//Vec3F hit = intersectLinePlane(cam->getPos(), cam->getToPos(), Vec3F(0, mouse_plane, 0), Vec3F(0, 1, 0));		// hit plane	
		//cam->setOrbit(cam->getAng(), hit, cam->getOrbitDist(), cam->getDolly());
		} break;
	case 'p': {														// Dolly 
		float orbit = cam->getOrbitDist() - amt.z * 0.01f;
		if (orbit < 0.01) orbit = 0.01;
		cam->SetOrbit( cam->getAng(), cam->getToPos(), orbit, cam->getDolly());

		dbgprintf( "%f %f\n", orbit, cam->getDolly() );

		/*float zoom = (cam->getOrbitDist() - cam->getDolly()) * 0.001f;
		mouse_plane += amt.y*zoom;		
		Vec3F to = cam->getToPos()  + amt*zoom;
		cam->setToPos ( to.x, to.y, to.z );
		cam->setOrbit(cam->getAng(), to, cam->getOrbitDist(), cam->getDolly());*/
		} break;
	case 'l': {			// Move light
		LightSet* lgts = dynamic_cast<LightSet*>(gScene->FindByType('lgts'));
		if (lgts != 0x0) {
			if ( lgts->getNumLights() > 0 ) {
				RLight* lgt = lgts->getLight(0);
				/* Quaternion q, t;
				q.fromAngleAxis( amt.x, Vec3F(0, 0, 1));
				t.fromAngleAxis( amt.y, Vec3F(1, 0, 1)); */
				lgt->pos.x += -amt.x * 200.0;
				lgt->pos.z += -amt.y * 200.0;
				mRenderMgr.UpdateLights();
			}
			lgts->Update();		// update lights
		}
		} break;	
	case 'd': {								// Depth-of-field
		Vec3F dof = cam->getDOF ();
		dof += amt * Vec3F( -0.005, 0, 0.01);
		if (dof.x < 0.001) dof.x = 0.001;
		if (dof.z < 0.1) dof.z = 0.1;
		cam->setDOF ( dof );		
		} break;
	};
	UpdateCamera();
}

void Sample::mousewheel (int delta)
{
	// wheel - zoom in/out
	MoveCamera ( 'z', Vec3F(0,0,float(delta)) );
}


void Sample::motion (AppEnum button, int x, int y, int dx, int dy) 
{
	// Get camera for scene	
	bool shift = (getMods() & KMOD_SHIFT);		// Shift-key to modify light
	bool ctrl = (getMods() & KMOD_CONTROL);
	bool alt = (getMods() & KMOD_ALT);
	float fine = 0.5f;	
		
	// Camera navigation
	switch ( button ) {	
	case AppEnum::BUTTON_LEFT: 
		if ( alt ) {
			if (shift) MoveCamera ( 't', Vec3F(dx, -dy, 0 ) );			// pan (alt+shift like Blender)
			else  	   MoveCamera ( 'o', Vec3F(dx*0.1f, -dy*0.1f, 0));  // orbit (alt like Blender)		
		} else {
			if ( shift ) MoveCamera('l', Vec3F(dx*0.2*DEGtoRAD, dy*0.2*DEGtoRAD, 0));
			//if ( ctrl ) MoveCamera('d', Vec3F(dx, 0, dy) );
		}		 
		break;	
	case AppEnum::BUTTON_MIDDLE: 
		MoveCamera ( 't', Vec3F(dx, -dy, 0 ) );
		break; 
	case AppEnum::BUTTON_RIGHT: 
		if ( m_cam_mode == MODE_FLY ) 
			MoveCamera ( 'a', Vec3F(dx*0.02f, -dy*0.02f, 0 ) );		
		else 
			MoveCamera ( 'o', Vec3F(dx*0.02f, -dy*0.02f, 0));
		break;	
	};

}

void Sample::mouse ( AppEnum button, AppEnum state, int mods, int x, int y)
{
	Camera3D* cam = mScene.getCamera3D();
	int w = getWidth(), h = getHeight();				// window width & height

	// Track when we are in a mouse drag
	mouse_down = (state == AppEnum::BUTTON_PRESS) ? button : BUTTON_NONE;	

	if ( mouse_down ) {
		Camera3D* cam = mScene.getCamera3D();
		Camera* camobj = mScene.getCameraObj();
		if (mRenderMgr.isAnimating()) camobj->Run ( mScene.getTime() );		
	}	

	if ( state==AppEnum::BUTTON_RELEASE ) {
		#ifdef BUILD_OPTIX
			m_RendOptiX->SetSample (0);
			m_RendOptiX->SetRegion ();
		#endif
		appPostRedisplay();
	}		

}


void Sample::keyboard(int keycode, AppEnum action, int mods, int x, int y)
{
	if (action == AppEnum::BUTTON_RELEASE) return;

	#ifdef USE_WIDGETS
	  if (mInterface.OnKeyboard(keycode)) return;
	#endif

	Camera3D* cam = mScene.getCamera3D();

	float s = (mods==1) ? 100.0 : 10.0;

	switch ( keycode ) {
	case 'e':	m_showeval = !m_showeval; break;

	// sketch modes
	case '`':	mRenderMgr.SetOption(OPT_SKETCH_SHAPE, TOGGLE);		break;		// draw shapes
	case '1':	mRenderMgr.SetOption (OPT_SKETCH_NORM, TOGGLE);		break;		// draw normals
	case '2':	mRenderMgr.SetOption(OPT_SKETCH_OBJ, TOGGLE);		break;			// draw sketches
	case '3':	mRenderMgr.SetOption (OPT_SKETCH_STATE, TOGGLE);	break;		// draw states
	case '4':	mRenderMgr.SetOption (OPT_SKETCH_WIRE, TOGGLE);		break;		// draw wireframe
	case '5':	mRenderMgr.SetOption (OPT_SKETCH_PICK, TOGGLE);		break;		// draw picking buffer

	case 'm': m_stats = !m_stats; break;					// show stats
	case 'i':	m_show_info = !m_show_info; break;	// show info
	
	case ',': case '.':
		// set renderer (,=opengl  .=optix)
		mRenderMgr.TriggerSceneUpdate();
		mRenderMgr.SetRenderer ( (keycode==',') ? 0 : 1 );		// Set Renderer		
		break;
	case '/': {
		mRenderMgr.SetRecording ( 1 );		
		mRenderMgr.SetRendererForRecording ();
		// Set window resolution
		Vec3F res = mRenderMgr.GetFrameRes();
		appResizeWindow ( res.x, res.y );
		} break;	

	case 'r':	ResetScene ( false );		break;		
	case 'g':	ResetScene ( true );		break;
	case 'c':	m_cam_mode = MODE_FLY;		break;
	
	case 'o':	{
		// camera orbit mode
		m_cam_mode = MODE_ORBIT;	
		Vec3F piv = Vec3F(250*0.67, 20, 250.0);		//mInterface.getCenter3D ( 0 );		// center on selected
		cam->SetOrbit ( cam->getAng(), piv, cam->getOrbitDist(), cam->getDolly() );
		} break;
	case 'p':	m_cam_mode = MODE_AUTO_ORBIT;	break;

	case ' ': {
		// play - pause/run animation
		mRenderMgr.SetAnimation ( TOGGLE );			
		Camera3D* cam = mScene.getCamera3D();
		Camera* camobj = mScene.getCameraObj();
		if ( mRenderMgr.isAnimating() ) camobj->Run ( mScene.getTime() );
		} break;

	case 'k': {
		// set camera key
		float t = mScene.getTime();
		Camera* camobj = mScene.getCameraObj();
		camobj->AddKey( t );				// Add camera key
		printf ( "Key added. t=%f\n", t );
	} break;						

	// fly - WASD tracking navigation
	case 'w': case 'W':		MoveCamera('t', Vec3F( 0, 0, -s));  break;		
	case 's': case 'S':		MoveCamera('t', Vec3F( 0, 0, +s));  break;	
	case 'a': case 'A':		MoveCamera('t', Vec3F(-s, 0, 0));	break;	
	case 'd': case 'D':		MoveCamera('t', Vec3F(+s, 0, 0));	break;	
	case 'z': case 'Z':		MoveCamera('t', Vec3F(0, -s, 0));	break;
	case 'q': case 'Q':		MoveCamera('t', Vec3F(0, +s, 0));	break;

	// change selection
	case '[': 		
		if (--m_select < 0 ) m_select = 0; 
		dbgprintf ( "select: %d\n", m_select);
		break;
	case ']': 
		m_select++; 
		dbgprintf ( "select: %d\n", m_select);
		break;

	// save scene = Ctrl+S
	case 19:					
		mScene.SaveScene( m_SceneFile );
		break;
	};

}

void Sample::reshape (int w, int h)
{
	// Resize the opengl screen texture
	glBindFramebuffer ( GL_FRAMEBUFFER, 0 );
	glViewport ( 0, 0, w, h );
	createTexGL ( m_rendergl_tex, w, h );	
	createTexGL ( m_rendergl_pick, w / 2, h / 2, GL_CLAMP_TO_EDGE, GL_RGBA32F, GL_FLOAT, GL_NEAREST);
	if ( m_renderoptix_tex != -1 ) createTexGL ( m_renderoptix_tex, w, h );
	
	// Update all renderers
	mScene.setRes ( w, h );
	mRenderMgr.UpdateRes ( w, h, getMSAA() );		
	
	setview2D ( w, h );

	UpdateCamera ();
	
}


void Sample::startup ()
{
	int w = 1024, h = 1024;

	m_SceneFile = "";

	#ifdef DEBUG_MEM
		_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
	#endif

	appStart ( "Shapes (c) 2010-2025", "Shapes (c) 2010-2025", w, h, 4, 2, 16, DEBUG_GL );

	printf ("SHAPES (c) Quanta Sciences 2010-2025\n");
	printf ("by Rama C. Hoetzlein, ramakarl.com\n");
}


void Sample::shutdown()
{
}

