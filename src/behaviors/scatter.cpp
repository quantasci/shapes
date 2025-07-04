

#include "scatter.h"
#include "object_list.h"
#include "scene.h"
#include "shapes.h"
#include "mesh.h"

#include "gxlib.h"
using namespace glib;

#define S_COUNT		0
#define S_DENSITY	1
#define S_DIST		2
#define S_ZOOM		3
#define S_SIZE		4

Scatter::Scatter() : Object()
{
	m_numlod = 0;
	m_numvari = 0;
}

void Scatter::Define(int x, int y)
{
	AddInput("time",		'Atim');		// scatter updated every frame
	AddInput("material", 'Amtl');		// material	

	AddInput("density", 'Aimg');		// density field [optional]
	AddInput("height",  'Aimg');		// height field [optional]

	AddInput("source", 'Amsh');			// mesh to scatter 
	AddInput("target", '****');			// object to scatter over
	
	
	m_wt.LoadTileSet();

	AddParam(S_COUNT,  "count", "i");	SetParamI (S_COUNT, 0, 1000);
	AddParam(S_DENSITY, "density",	"f");	SetParamF(S_DENSITY, 0, 700.0 );			
	AddParam(S_DIST,	"distance", "3");	SetParamV3(S_DIST,	0, Vec3F(0, 300, 400));
	AddParam(S_ZOOM,	"zoom",		"f");	SetParamF(S_ZOOM, 0, 1.0);
	AddParam(S_SIZE,	"size",		"3");	SetParamV3(S_SIZE, 0, Vec3F(0.1,0.1,0.1) );

	mTimeRange.Set(0, 10000, 0 );

}



void Scatter::Generate(int x, int y)
{
	m_rand.seed( 1623 );

	// Mesh to scatter over
	

	// Density function
	Image* img = (Image*)getInput("density");
	if (img != 0x0) {
		Vec3F sz = getParamV3(S_SIZE);
		m_wt.SetDensityImg(img, sz);
	}
	
	// Output shapes
	CreateOutput('Ashp');	

	ImageX* hgt = (ImageX*) getInput("height");
	if (hgt != 0x0) {
	
		// Scatter over height field
		///
		Object* obj = getInput("object");
		if (obj != 0x0) {
			// module res
			Vec3I res = obj->getParamI3(obj->getParamByName("res"));
			m_numlod = res.x;
			m_numvari = res.y;

			Vec3F dst = getParamV3(S_DIST);
			float d;
			for (int n = 0; n < 1024; n++) {
				d = sqrt(float(n) / 1024.f) * dst.z;
				m_distlod[n] = (m_numlod - 1) * std::min(d, dst.y) / dst.y;
			}

			Shapes* src = getInputShapes("object");
			m_numsrc = imin(src->getNumShapes(), 100);
			for (int j = 0; j < m_numsrc; j++) {
				m_srcgrp[j] = (Shapes*)gAssets.getObj(src->getShape(j)->meshids.x);
			}
		}

	} else {

		// Scatter over another object -- NO height field
		//
		Vec8S mat_id = getInputMat("material");		
		Vec3F sz = getParamV3(S_SIZE);
		int cnt = getParamI(S_COUNT);

		// source mesh - object to scatter
		int src_mesh = getInputID("source", 'Amsh');

		// target behavior - object(s) to scatter over
		Shapes* target = getInputShapes("target");
		Shape* tshp = target->getShape(0);					// single shape
		Mesh* target_mesh = dynamic_cast<Mesh*> ( gAssets.getObj( tshp->meshids.x ) );		// mesh on that shape
		Matrix4F xform = getInput("target")->getXform();					// transform of behavior (applies to all shapes)
		
		int numf = target_mesh->GetNumFace3();
		int f;
		float u,v;
		Vec3F v1, v2, v3, n1, n2, n3;

		for (int n=0; n < cnt; n++) {
			Shape *s = AddShape();
			s->type = S_MESH;			
			s->matids = mat_id;					
			// set source mesh 
			s->meshids.Set ( src_mesh, 0, 0, 0);		

			// choose random point inside a random face on target
			f = m_rand.randI(0, numf);
      AttrV3 fv = *target_mesh->GetFace3(f);
			v1 = *target_mesh->GetVertPos(fv.v1); v1 *= xform; n1 = *target_mesh->GetVertNorm(fv.v1);
			v2 = *target_mesh->GetVertPos(fv.v2); v2 *= xform; n2 = *target_mesh->GetVertNorm(fv.v2);
			v3 = *target_mesh->GetVertPos(fv.v3); v3 *= xform; n3 = *target_mesh->GetVertNorm(fv.v3);
		
			u = m_rand.randF(0,1);
			v = m_rand.randF(0,1);
			if (u+v > 1.0f) { u = 1.0f-u; v = 1.0f-v; }
			s->pos = v1 + (v2-v1)*u + (v3-v1)*v;
			s->rot.fromDirectionAndUp ( (n1+n2+n3)/3.0f, Vec3F(0,1,0) );
			s->rot.normalize();
			s->scale = sz;
			//s->clr = VECCLR(pal[m_distlod[int(pt.z * 1024 / (dst.z * dst.z))]]);
		} 
	}
	
	m_rand.seed ( 6124 );

	MarkDirty();
}

// Scatter Objects over Heightfield 
// using wang tiles, given 3D camera, zoom factor, and tone scale
//
void Scatter::ScatterOverHeightfield()
{
	Vec3F pt, a, b, c;
	c.Set(1, 40, 1);
	Shape* s, * q;
	Quaternion r;
	float sz;
	int lod, vari, id;

	Vec4F pal[5];
	pal[0].Set(1, 0, 0, 1);
	pal[1].Set(1, 1, 0, 1);
	pal[2].Set(0, 1, 0, 1);
	pal[3].Set(0, 0, 1, 1);
	pal[4].Set(0, 1, 1, 1);

	ImageX* hgt = (ImageX*)getInput("height");
	int src_mesh = getInputID("source", 'Amsh');

	Vec3F size = getParamV3(S_SIZE);

	float dens = getParamF(S_DENSITY);
	float zm = getParamF(S_ZOOM);
	Vec3F dst = getParamV3(S_DIST);

	Camera3D* cam = gScene->getCamera3D();
	int pnts = m_wt.Recurse3D(cam, zm, dens, dst.z);

	ClearShapes();

	Shapes* src = getInputShapes("target");

	if (src == 0x0) {
		// No shapes. Just use boxes.
		for (int n = 0; n < pnts; n++) {
			pt = m_wt.getPnt(n);
			a.Set(pt.x, 0, pt.y);
			b = a + c;
			if (cam->boxInFrustum(a, b)) {
				s = AddShape();
				s->type = S_MESH;
				s->pos = a;
				s->scale = c;
				s->clr = VECCLR(pal[m_distlod[int(pt.z * 1024 / (dst.z * dst.z))]]);
				s->meshids.Set( src_mesh, 0, 0, 0);
			}
		}
	}
	else {
		// Setup source LODS on Modules
		float h = 0;
		int ndx;

		// Scatter shapes
		for (int n = 0; n < pnts; n++) {

			pt = m_wt.getPnt(n);
			if (hgt != 0x0) {
				h = size.y * hgt->GetPixelUV(pt.x / size.x, pt.y / size.z).x;			// get height from terrain image
			}
			a.Set(pt.x, h, pt.y);
			b = a + c;					// c should be bounding box of src

			if (cam->boxInFrustum(a, b)) {

				ndx = int(pt.x * pt.y);

				// select lod
				lod = m_distlod[int(pt.z * 1024 / (dst.z * dst.z))];	// lookup table
				vari = ndx % m_numvari;
				id = vari * m_numlod + lod;

				//ang = int(pt.x * pt.y * 39761) % 360;
				//r.fromAngleAxis ( ang*DEGtoRAD, Vec3F(0,1,0) );/				
				sz = 1.0 + (ndx % 71) / 71.0f;
				ndx = (ndx % 100);

				for (int j = 0; j < m_srcgrp[id]->getNumShapes(); j++) {
					q = m_srcgrp[id]->getShape(j);
					s = AddShapeByCopy(q);
					s->pos = q->pos * sz + a;
					s->scale *= sz;
					s->clr = VECCLR(Vec4F(1 - float(ndx) * 0.3 / 100.0, 1, 1 - float(ndx) * 0.3 / 100.0, 1));
					//s->rot *= r;

				}
			}
		}
	}

	/*for (int n = 0; n < pnts; n++) {

		pt = wt.getPnt(n);											// get generated point

		a = Vec3F(pt.x, 0, pt.y);								// get 3D position
		o = int(pt.y * imgres.x) + int(pt.x);						// offset into density func
		b = a + Vec3F(.1, m_density[o] * 5.0, .1);				// get 3D box dimensions. height = density

		if (m_cam->boxInFrustum(a, b)) {							// cull box if outside camera frustum
			d = 100.0 / (m_cam->getPos() - a).LengthFast();				// use distance as alpha falloff
			pix = img.GetPixelUV(pt.x / imgres.x, pt.y / imgres.y);		// get point color (sample original image)
			drawCube3D(a, b, pix.x, pix.y, pix.z, d);				// draw box
		}
	}
	*/
}


void Scatter::Run(float time)
{
	if (!m_wt.isReady() ) return;

	ImageX* hgt = (ImageX*) getInput ("height");	
	if (hgt != 0) {
		// Scatter objects over height field - ** dynamic update **
		ScatterOverHeightfield ();
	}	

	MarkClean();

}

void Scatter::Render()
{
}


void Scatter::Sketch(int w, int h, Camera3D* cam)
{	
	float sc = 1.0;
	Vec4F pal[5];
	pal[0].Set(1, 0, 0, 1);
	pal[1].Set(1, 1, 0, 1);
	pal[2].Set(0, 1, 0, 1);
	pal[3].Set(1, 0, 1, 1);
	pal[4].Set(0, 0, 0, 1);

	end3D();

	start2D( w, h );

	// Draw camera frustum in 2D view (yellow)	
	Vec3F a,b,c,pt;
	a = cam->getPos();
	b = a + cam->inverseRay(0, 0, w, h) * 50.0f;
	c = a + cam->inverseRay(w, h, w, h) * 50.0f;
	drawLine( Vec2F(a.x/sc, a.z/sc), Vec2F(b.x/sc, b.z/sc), Vec4F(1, 1, 0, 1) );
	drawLine( Vec2F(a.x/sc, a.z/sc), Vec2F(c.x/sc, c.z/sc), Vec4F(1, 1, 0, 1) );	

	// Draw points
	Vec3F dst = getParamV3(S_DIST);
	Vec4F pix ( 1,1,1,1 );
	
	c.Set(1, 40, 1);
	for (int n = 0; n < m_wt.numPnts(); n ++) {			// skip every other pnt (faster)
		pt = m_wt.getPnt(n);
		a.Set(pt.x, 0, pt.y);
		b = a + c;					// c should be bounding box of src
		if (cam->boxInFrustum(a, b)) {
			pix = pal [ m_distlod[ int(pt.z * 1024/(dst.z*dst.z)) ] ] ;
			drawCircle ( Vec2F(a.x/sc, a.z/sc), 3.0, pix );
			//drawPnt(a.x / sc, a.y / sc, pix);
		}
	}
	end2D();

	start3D(cam);
}






