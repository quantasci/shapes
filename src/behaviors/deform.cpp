
#include "deform.h"
#include "shapes.h"

#define D_BEND_CTR		0
#define D_BEND_SIZE		1
#define D_BEND_AMT		2

#define D_TWIST_CTR		3
#define D_TWIST_SIZE	4
#define D_TWIST_AMT0	5
#define D_TWIST_AMT1	6

#define D_FOLD_CTR		7
#define D_FOLD_SIZE		8
#define D_FOLD_AMT0		9
#define D_FOLD_AMT1		10

void Deform::Define (int x, int y)
{
	AddInput ( "shapes",	'Ashp' );
	AddInput ( "shader",	'Ashd' );
	AddInput ( "tex",		'Aimg' );
	AddInput ( "mesh",		'Amsh' );

	SetInput ("shader",		"shade_mesh");	// default shader for this object
	SetInput ("mesh",		"model_cube");

	AddParam (D_BEND_CTR,	"bend_ctr",		"3");	SetParamV3 (D_BEND_CTR, 0, Vec3F(0, 0, 0) );
	AddParam (D_BEND_SIZE,	"bend_size",	"3");	SetParamV3(D_BEND_SIZE, 0, Vec3F(1, 1, 1));
	AddParam (D_BEND_AMT,	"bend_amt",		"3");	SetParamV3(D_BEND_AMT, 0, Vec3F(0, 0, 0));
	
	AddParam (D_TWIST_CTR,	"twist_ctr",	"3");	SetParamV3 (D_TWIST_CTR, 0, Vec3F(0, 0, 0));
	AddParam (D_TWIST_SIZE, "twist_size",	"3");	SetParamV3 (D_TWIST_SIZE, 0, Vec3F(1, 1, 1));
	AddParam (D_TWIST_AMT0,	"twist_min",	"3");	SetParamV3 (D_TWIST_AMT0, 0, Vec3F(0, 0, 0));
	AddParam (D_TWIST_AMT1, "twist_max",	"3");	SetParamV3(D_TWIST_AMT1, 0, Vec3F(0, 0, 0));
	
	AddParam (D_FOLD_CTR,	"fold_ctr",		"3");	SetParamV3(D_FOLD_CTR, 0, Vec3F(0, 0, 0));
	AddParam (D_FOLD_SIZE,	"fold_size",	 "3");	SetParamV3(D_FOLD_SIZE, 0, Vec3F(1, 1, 1));
	AddParam (D_FOLD_AMT0,	"fold_min",		"3");	SetParamV3(D_FOLD_AMT0, 0, Vec3F(0, 0, 0));
	AddParam (D_FOLD_AMT1,  "fold_max",		"3");	SetParamV3(D_FOLD_AMT1, 0, Vec3F(0, 0, 0));

	mTimeRange = Vec3F(0, 10000, 0);
}

void Deform::DeformBend()
{
	Vec3F p, k, c;
	Vec3F dp;
	Quaternion q, r1, r2;
	Vec3F d;
	float v, a0, a1, r, bsgn;

	// Bending moves the original geometry along a displaced circle:
	//
	// For bending along X-axis with Y-up:
	//    bend_ctr.x = given axial coordinate for bending
	//    bend_amt.x = given total amount of bending (degrees)
	//   bend_size.x = given range over which to apply bend
	// 
	//             C                  C = bend center
	//            / |--              a0 = base angle of bend = bend_amt * x/s
	//           /a0|a1 --           a1 = final angle of bend = bend_amt * x/s   
	//          /   |      --
	//       r /    |Cy       --
	//        /     |           --
	//       /      |              --      x = bend axial ctr 
	// (0,0) ---x---|----------------- s   s = bend size
	//       .      |            .....          
	//         ..   v    ........          . = circle centered at C
	//            .......    
	// Solution: construct a right triangle with bend center at: <x, x/tan(a0), 0>,   tan(a0) = x / Cy
	//
	Vec3F bend_ctr = getParamV3(D_BEND_CTR);
	Vec3F bend_size = getParamV3(D_BEND_SIZE);
	Vec3F bend_amt = getParamV3(D_BEND_AMT);
	float e, ang, sz, ctr;
	
	//----------- Bend along X-axis, toward Y-axis, rotation in Z-axis
	ang = bend_amt.x;
	if ( ang != 0) {
		sz = bend_size.x;
		ctr = bend_ctr.x;
		bsgn = (ang > 0) ? 1 : -1;
		r = (360.0 / ang) * sz / (2*3.141592);		// C = 2*PI*r = 360/ang * L    -> when ang=360, length wraps to circumference
		a0 = -fabs(ang) * ctr/sz;
		a1 = ang + a0;
		c = Vec3F( ctr, r, 0 );
		k = Vec3F(0, -r * bsgn, 0); r1.fromAngleAxis(std::min(a0, a1) * bsgn * DEGtoRAD, Vec3F(0, 0, 1)); p = c + (k * r1); e = p.y;
		Shape* s;
		for (int n = 0; n < getNumShapes(); n++) {
			s = getShape(n);
			p = s->pos;
			v = (ctr/sz) + (p.x-ctr) / sz;				// bend fraction (0 < v < 1)
			k = Vec3F(0, -r * bsgn, 0);
			p.x = 0;									// move cross-section to 0
			r1.fromAngleAxis((a0 + v * (a1 - a0)) * bsgn * DEGtoRAD, Vec3F(0, 0, 1));		// Z-axis
			p *= r1;									// rotate cross-section		
			p += c + (k * r1) - Vec3F(0,e,0);		// bend - shift section onto the circle
			s->pos = p;
			s->rot *= r1;
		}
	}

	//----------- Bend along Y-axis, toward X-axis, rotation in Z-axis
	ang = bend_amt.y;
	if (ang != 0) {
		sz = bend_size.y;
		ctr = bend_ctr.y;
		bsgn = (ang > 0) ? 1 : -1;
		r = (360.0 / ang) * sz / (2 * 3.141592);		// C = 2*PI*r = 360/ang * L    -> when ang=360, length wraps to circumference
		a0 = -fabs(ang) * ctr / sz;
		a1 = ang + a0;
		c = Vec3F(r, ctr, 0);
		k = Vec3F(-r*bsgn,0,0); r1.fromAngleAxis( std::min(a0,a1) * bsgn * DEGtoRAD, Vec3F(0, 0, -1)); p = c + (k*r1); e = p.x;
		Shape* s;
		for (int n = 0; n < getNumShapes(); n++) {
			s = getShape(n);
			p = s->pos;
			v = (ctr / sz) + (p.y - ctr) / sz;			// bend fraction (0 < v < 1)
			k = Vec3F(-r * bsgn, 0, 0);			// toward X-axis
			p.y = 0;									// move cross-section to 0
			r1.fromAngleAxis((a0 + v * (a1 - a0)) * bsgn * DEGtoRAD, Vec3F(0, 0, -1));		// Z-axis
			p *= r1;									// rotate cross-section		
			p += c + (k * r1) - Vec3F(e,0,0);		// bend - shift section onto the circle
			s->pos = p;
			s->rot *= r1;
		}
	}

	//----------- Bend along Z-axis, toward X-axis, rotation in Y
	ang = bend_amt.z;
	if (ang != 0) {
		sz = bend_size.z;
		ctr = bend_ctr.z;
		bsgn = (ang > 0) ? 1 : -1;
		r = (360.0 / ang) * sz / (2 * 3.141592);		// C = 2*PI*r = 360/ang * L    -> when ang=360, length wraps to circumference
		a0 = -fabs(ang) * ctr / sz;
		a1 = ang + a0;
		c = Vec3F(r, 0, ctr);
		k = Vec3F(-r * bsgn, 0, 0); r1.fromAngleAxis(std::min(a0, a1) * bsgn * DEGtoRAD, Vec3F(0, 1, 0)); p = c + (k * r1); e = p.x;
		Shape* s;
		for (int n = 0; n < getNumShapes(); n++) {
			s = getShape(n);
			p = s->pos;
			v = (ctr / sz) + (p.z - ctr) / sz;		// bend fraction (0 < v < 1)
			k = Vec3F(-r * bsgn, 0, 0);				// toward X-axis
			p.z = 0;									// move cross-section to 0
			r1.fromAngleAxis((a0 + v * (a1 - a0)) * bsgn * DEGtoRAD, Vec3F(0, 1, 0));		// Y-axis rotation 
			p *= r1;									// rotate cross-section		
			p += c + (k * r1) - Vec3F(e, 0, 0);		// bend - shift section onto the circle
			s->pos = p;
			s->rot *= r1;
		}
	}
}

void Deform::DeformTwist()
{
	Vec3F p;
	Vec3F dp;
	Vec3F u;
	Quaternion q, q1, q2, q3;

	// Twist is simply a rotation along the twist axis
	//      v             v = fraction of rotation = x / s
	//      |
	// --x--|----------s  --> twist axis, where s = twist_size.x 
	//      |

	Vec3F twist_ctr = getParamV3(D_TWIST_CTR);
	Vec3F twist_size = getParamV3(D_TWIST_SIZE);
	Vec3F twist_amt0 = getParamV3(D_TWIST_AMT0);
	Vec3F twist_amt1 = getParamV3(D_TWIST_AMT1);
	
	Shape* s;
	Vec3F t;
	Vec3F amt;
	for (int n = 0; n < getNumShapes(); n++) {
		s = getShape(n);
		p = s->pos - twist_ctr;
		t = p / twist_size;
		amt = twist_amt0 + (twist_amt1-twist_amt0)*t;

		q1.fromAngleAxis(t.x * amt.x * DEGtoRAD, Vec3F(1, 0, 0));
		q2.fromAngleAxis(t.y * amt.y * DEGtoRAD, Vec3F(0, 1, 0));
		q3.fromAngleAxis(t.z * amt.z * DEGtoRAD, Vec3F(0, 0, 1));
		q1 = q3 * q2 * q1;

		p *= q1;
		q = s->rot * q1;

		s->pos = p + twist_ctr;
		s->rot = q;
	}
}

void Deform::DeformFold()
{
	Vec3F p;
	Vec3F dp;
	Vec3F u;
	Quaternion q, q1, q2, q3, qx;

	// Folding is a constant rotation that is neg/pos on either side of the axis,
	// where the angle is same on both sides
	//    \    /
	//     \  /       looking straight down x-axis
	//     a\/a
	//  <---0----> z-axis     0 = origin
	//
	Vec3F fold_ctr = getParamV3(D_FOLD_CTR);
	Vec3F fold_size = getParamV3(D_FOLD_SIZE);
	Vec3F fold_amt0 = getParamV3(D_FOLD_AMT0);
	Vec3F fold_amt1 = getParamV3(D_FOLD_AMT1);

	Shape* s;
	Vec3F t, amt;
	for (int n = 0; n < getNumShapes(); n++) {
		s = getShape(n);
		p = s->pos - fold_ctr;
		t = p / fold_size;
		amt = fold_amt0 + (fold_amt1 - fold_amt0) * t;

		q1.fromAngleAxis( amt.x * DEGtoRAD, Vec3F(1, 0, 0));
		q2.fromAngleAxis( amt.y * DEGtoRAD, Vec3F(0, 1, 0));
		q3.fromAngleAxis( amt.z * DEGtoRAD, Vec3F(0, 0, 1));		

		qx = (p.z == 0) ? q1.identity() : (p.z < 0) ? q1 : q1.conjugate();		// quaternion conjugate = negative rotation
		qx *= (p.x == 0) ? q2.identity() : (p.x > 0) ? q2 : q2.conjugate();
		qx *= (p.x == 0) ? q3.identity() : (p.x > 0) ? q3 : q3.conjugate();
		p *= qx;
		q = s->rot * qx;

		s->pos = p + fold_ctr;
		s->rot = q;
	}
}


void Deform::Generate (int x, int y)
{
	CreateOutput ( 'Ashp' );

	// Copy input shapes
	if (!CopyShapesFromInput ( "shapes" )) return;

	// Deform
	DeformFold();
	DeformTwist();
	DeformBend();

	// Place end-to-end
	PlaceShapesEndToEnd();
	
	MarkClean();
}

