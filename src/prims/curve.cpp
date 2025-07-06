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

#include "curve.h"
#include "gxlib.h"
using namespace glib;


Curve::Curve () : DataX()
{	
	m_Subdivs = 16;
	m_bClosed = false;

	AddBuffer ( CVERTPOS, "pos", sizeof(Vec3F), 0 );	
	AddBuffer ( CVERTDIST, "dist", sizeof(float), 0 );	

	AddBuffer ( CKEYPOS, "kpos", sizeof(Vec3F), 0 );	
	AddBuffer ( CKEYCLR, "kclr", sizeof(CLRVAL), 0 );
	AddBuffer ( CKEYTANL, "ktanl", sizeof(Vec3F), 0 );
	AddBuffer ( CKEYTANR, "ktanr", sizeof(Vec3F), 0 );
	AddBuffer ( CKEYFLAG, "kflag", sizeof(char), 0 );
}

Curve::~Curve ()
{
}


void Curve::Clear ()
{
	EmptyBuffer ( CVERTPOS );
	EmptyBuffer ( CVERTDIST );
	
	EmptyBuffer ( CKEYPOS );	
	EmptyBuffer ( CKEYCLR );	
	EmptyBuffer ( CKEYTANL );	
	EmptyBuffer ( CKEYTANR );	
	EmptyBuffer ( CKEYFLAG );	
}

void Curve::SetSubdivs ( int sd )
{ 
	m_Subdivs = sd;
}

void Curve::CloseCurve ()
{	
	Vec3F v = * GetElemVec3 ( CKEYPOS, 0 );
	Vec3F vl = * GetElemVec3 ( CKEYPOS, NumKeys()-1 );
	vl -= v;

	m_bClosed = true;
	
	if ( vl.Length() > 0.001) 
		AddKey ( v );
}

void Curve::Sketch ( int w, int h, Camera3D* cam )
{
	Vec3F p0, p1;
	int pnts = GetNumElem(CVERTPOS);
	int keys = GetNumElem(CKEYPOS);
	if ( pnts==0) return;

	p0 = *GetVertPos ( 0 );
	for (int n=1; n < pnts; n++ ) {
		p1 = *GetVertPos ( n );
		drawLine3D ( p0, p1, Vec4F(m_Clr, 1) );
		p0 = p1;
	}	
	for (int k=0; k < keys; k++ ) {		
		p0 = *GetKeyPos( k );
		drawBox3D ( p0-Vec3F(0.1f,0.1f,0.1f), p0+Vec3F(0.1f,0.1f,0.1f), Vec4F(1,1,0,1) );
	}	
}


// Rebuild all points -- Only when number of keys changes
void Curve::RebuildPnts ()
{
	double t;					// Blending coefficients (u0 = 1)	
	double qx, qy, qz, qx1, qy1, qz1, qx2, qy2, qz2, qx3, qy3, qz3;
	double dt2, dt3, a, b;	
	uchar *flagl;
	float dt = 1.0f / m_Subdivs;
	
	CLRVAL *clrl, *clrr;
	Vec3F *key, *keyn, *tl, *tln, *tr, *trn;
	dt2 = dt*dt; dt3 = dt2*dt;
	
	Vec4F clr; 

	EmptyBuffer ( CVERTPOS );	
	EmptyBuffer ( CVERTDIST );		

	if ( NumKeys() < 2 ) return;

	UpdateTangents ();
	
	key		= (Vec3F*)	GetStart ( CKEYPOS );
	keyn	= (Vec3F*)	key + 1;
	clrl	= (CLRVAL*)		GetStart ( CKEYCLR );
	clrr	= (CLRVAL*)		clrl + 1;	
	tl		= (Vec3F*)	GetStart ( CKEYTANL );
	tln		= (Vec3F*)	tl + 1;
	tr		= (Vec3F*)	GetStart ( CKEYTANR );
	trn		= (Vec3F*)	tr + 1;
	flagl	= (uchar*)		GetStart ( CKEYFLAG );

	int p = 0, ndx;
	float dist = 0;

	for (int n=0; n < NumKeys()-1; n++) {
		if ( tr->x == 0.0 && tr->y == 0.0 && tr->z == 0.0 && tln->x == 0.0 && tln->y == 0.0 && tln->z == 0.0 ) {
			ndx = AddPoint ( key->x, key->y, key->z );
			SetElemFloat ( CVERTDIST, ndx, dist );
			p++;
		} else {					
			// Compute forward difference initial conditions (Foley van Dam, p. 512)		
			qx = key->x; qy = key->y; qz = key->z;
			a =  2*key->x +  tr->x +tln->x -2*keyn->x;
			b = -3*key->x -2*tr->x -tln->x +3*keyn->x;
			qx1 =   dt3*a +   dt2*b + dt* tr->x; qx2 = 6*dt3*a + 2*dt2*b; qx3 = 6*dt3*a;
			a =  2*key->y +  tr->y +tln->y -2*keyn->y;
			b = -3*key->y -2*tr->y -tln->y +3*keyn->y;
			qy1 =   dt3*a +   dt2*b + dt* tr->y; qy2 = 6*dt3*a + 2*dt2*b; qy3 = 6*dt3*a;
			a =  2*key->z +  tr->z +tln->z -2*keyn->z;
			b = -3*key->z -2*tr->z -tln->z +3*keyn->z;
			qz1 =   dt3*a +   dt2*b + dt* tr->z; qz2 = 6*dt3*a + 2*dt2*b; qz3 = 6*dt3*a;

			t = 0.0;
			for (int i=0; i < m_Subdivs; i++) {				
				t += (1.0f / m_Subdivs );
				ndx = AddPoint ( qx, qy, qz );
				SetElemFloat ( CVERTDIST, ndx, dist );
				p++;
				qx += qx1; qx1 += qx2; qx2 += qx3;
				qy += qy1; qy1 += qy2; qy2 += qy3;
				qz += qz1; qz1 += qz2; qz2 += qz3;				
				dist += sqrt(qx1*qx1+qy1*qy1+qz1*qz1);
			}
		}
		*flagl = 0;
		flagl++;
		key++; keyn++;
		tl++; tln++;
		tr++; trn++;
		clrl++; clrr++;
	}
	ndx = AddPoint ( key->x, key->y, key->z );
	SetElemFloat ( CVERTDIST, ndx, dist );	

	*flagl = 0;		// clear all update flags
}

int Curve::AddKey ( Vec3F ps )
{
	return AddKey ( ps, (CLRVAL) COLORA(1,1,1,1), false );
}
int Curve::AddKey ( float x, float y, float z, CLRVAL clr )
{
	return AddKey ( Vec3F(x,y,z), clr, false );
}

int Curve::AddKey ( Vec3F ps, CLRVAL clr, bool bLine )
{
	Vec3F ptan;
	uxlong ndx;

	ndx = AddElem ( CKEYPOS );
	AddElem ( CKEYCLR );
	AddElem ( CKEYTANL );
	AddElem ( CKEYTANR );
	AddElem ( CKEYFLAG );

	SetElemVec3 ( CKEYPOS, ndx, ps );
	SetElemClr ( CKEYCLR, ndx, clr );

	if ( bLine )	ptan.Set ( 0.0, 0.0, 0.0 );		// 0 length tanget = line segment
	else			ptan.Set ( 0.1, 0.1, 0.1 );		
	
	SetElemVec3 ( CKEYTANL, ndx, ptan );	
	SetElemVec3 ( CKEYTANR, ndx, ptan );	
	SetElemChar ( CKEYFLAG, ndx, 1 );							// mark this and previous key as dirty		

	RebuildPnts ();

	return ndx;
}

int Curve::AddKeyFast ( Vec3F ps, bool bLine )
{
	Vec3F ptan;
	uxlong ndx;	
	ndx = AddElem ( CKEYPOS );
	AddElem ( CKEYCLR );
	AddElem ( CKEYTANL );
	AddElem ( CKEYTANR );
	AddElem ( CKEYFLAG );

	SetElemVec3 ( CKEYPOS, ndx, ps );
	if ( bLine )	ptan.Set ( 0.0, 0.0, 0.0 );		// 0 length tanget = line segment
	else			ptan.Set ( 0.1, 0.1, 0.1 );
	SetElemClr ( CKEYCLR, ndx, (CLRVAL) COLORA(1,1,1,1) );
	SetElemVec3 ( CKEYTANL, ndx, ptan );	
	SetElemVec3 ( CKEYTANR, ndx, ptan );
	SetElemChar ( CKEYFLAG, ndx, 1 );							// mark this and previous key as dirty	
	return ndx;
}


void Curve::RemoveKey ( int n )
{	
	DelElem ( CKEYPOS, n );
	DelElem ( CKEYCLR, n );
	DelElem ( CKEYTANL, n );
	DelElem ( CKEYTANR, n );
	DelElem ( CKEYFLAG, n );

	RebuildPnts ();	
}
bool Curve::IsPointOnCurve ( Vec3F p, Vec3F& q, float eps )
{
	Vec3F* v1; 
	Vec3F v, w;
	float dist;

	if ( NumKeys() == 0 ) return false;	

	for (int n=0; n < NumPoints()-2; n++ ) {
		v1 = GetElemVec3 ( CVERTPOS, n );		
		dist = p.Dist ( *v1 );
		if ( dist < eps ) {				
			return true;
		}		
	}
	return false;
}
/*
	int l = 0;
	int r = NumKeys()-1;
	int m;

	while ( l <= r )
		m = (l+r) >> 1;
		if ( )


	// binary search implementation (not that much faster) (precondition: array_size > 0)
  size_t left  = 1;
  size_t right = array_size - 1;
  while(left <= right)
  {
    size_t mid = (left + right) / 2;
    if(array[mid] <= value) left = mid + 1;			 // the value to find is more to the right
    else if(array[mid - 1] > value) right = mid - 1; // the value to find is more to the left
    else return mid - 1;
  }
  return array_size - 1;

	float dl = GetElemFloat ( CVERTDIST, 0 );
	float d;

	// binary search
	for (int n=1; n < NumPoints(); n++ ) {
		d = GetElemFloat ( CVERTDIST, n );
		if ( )
	}
	*/

float Curve::getCurveLength()
{
	if ( NumPoints()==0 ) return 0;
	return GetElemFloat( CVERTDIST, NumPoints()-1 );
}




// GetNearestPoint
// return = nearest point on curve
// result = <t, dist, dmax>
Vec3F Curve::getNearestPoint ( Vec3F testpnt, Vec3F& result )
{
	Vec3F r, r2, bp;
	float bdist, d, bu = 0;
	bdist = 1e10;

	int subdiv = 64;		// accuracy

	Vec3F q;
	double qx1, qy1, qz1, qx2, qy2, qz2, qx3, qy3, qz3;
	double dt2, dt3, a, b;		
	
	Vec3F *key, *keyn, *tl, *tln, *tr, *trn;	
	key		= (Vec3F*)	GetStart ( CKEYPOS );		keyn = (Vec3F*)	key + 1;	
	tl		= (Vec3F*)	GetStart ( CKEYTANL );		tln	= (Vec3F*)	tl + 1;
	tr		= (Vec3F*)	GetStart ( CKEYTANR );		trn	= (Vec3F*)	tr + 1;	
	
	float dt = 1.0f / subdiv;
	dt2 = dt*dt; dt3 = dt2*dt;
	float dist = 0;

	for (int n=0; n < NumKeys()-1; n++) {
		// Compute forward difference initial conditions (Foley van Dam, p. 512)		
		q.x = key->x; q.y = key->y; q.z = key->z;
		a =  2*key->x +  tr->x +tln->x -2*keyn->x;
		b = -3*key->x -2*tr->x -tln->x +3*keyn->x;
		qx1 =   dt3*a +   dt2*b + dt* tr->x; qx2 = 6*dt3*a + 2*dt2*b; qx3 = 6*dt3*a;
		a =  2*key->y +  tr->y +tln->y -2*keyn->y;
		b = -3*key->y -2*tr->y -tln->y +3*keyn->y;
		qy1 =   dt3*a +   dt2*b + dt* tr->y; qy2 = 6*dt3*a + 2*dt2*b; qy3 = 6*dt3*a;
		a =  2*key->z +  tr->z +tln->z -2*keyn->z;
		b = -3*key->z -2*tr->z -tln->z +3*keyn->z;
		qz1 =   dt3*a +   dt2*b + dt* tr->z; qz2 = 6*dt3*a + 2*dt2*b; qz3 = 6*dt3*a;

		for (int i=0; i < subdiv; i++ ) {
			r = testpnt - q;
			d = r.Length();
			if ( d < bdist ) {
				bp = q;				
				bdist = d;
				result.x = (n + float(i)/float(subdiv)) / (NumKeys()-1);		// determine t value
				result.y = dist;
				result.z = GetElemFloat( CVERTDIST, NumPoints()-1 );
			}
			q.x += qx1; qx1 += qx2; qx2 += qx3;
			q.y += qy1; qy1 += qy2; qy2 += qy3;
			q.z += qz1; qz1 += qz2; qz2 += qz3;
			r2.Set(qx1, qy1, qz1);
			dist += r2.Length();
		}		
		key++; keyn++;
		tl++; tln++;
		tr++; trn++;
	}
	
	return bp;
}

Vec3F Curve::getPointOnCurve ( float t )
{
	if ( NumKeys()==0 ) return Vec3F(0,0,0);
	if ( t <= 0.0 ) return *GetKeyPos(0);
	if ( t >= 1.0 ) return *GetKeyPos( NumKeys()-1 );

	int l = t*(NumKeys()-1);
	int r = l + 1;

	// Determine local parametric time		
	float u = t*float(NumKeys()-1) - float(l);

	// Compute blending coefficients
	float u2, u3, q0, q1, q2, q3;
	u2 = u*u; u3 = u2*u;
	q3 =  2*u3 - 3*u2 + 1;
	q2 =    u3 - 2*u2 + u;
	q1 =    u3 - 1*u2 + 0;
	q0 = -2*u3 + 3*u2 + 0;

	// Compute curve position
	Vec3F pl =		*GetKeyPos (l);
	Vec3F plout =	*GetKeyTanR (l);
	Vec3F pr =		*GetKeyPos (r);
	Vec3F prin =	*GetKeyTanL (r);
	Vec3F p;
	p.x = pl.x*q3 + plout.x*q2 + prin.x*q1 + pr.x*q0;
	p.y = pl.y*q3 + plout.y*q2 + prin.y*q1 + pr.y*q0;
	p.z = pl.z*q3 + plout.z*q2 + prin.z*q1 + pr.z*q0;
	return p;
}


void Curve::MoveLastKey ( Vec3F p )
{
	MoveKey ( NumKeys()-1, p );
}

void Curve::MoveKey ( int n, Vec3F p )
{
	if ( n >= 0 && n <= NumKeys()-1 ) {			
		SetElemVec3 ( CKEYPOS, n, p );	
		SetElemChar ( CKEYFLAG, n, 1 );		// mark this key as dirty		
	}
}

void Curve::MoveKey ( int n, Vec3F p, CLRVAL clr )
{
	if ( n >= 0 && n <= NumKeys()-1 ) {			
		SetElemVec3 ( CKEYPOS, n, p );	
		SetElemClr ( CKEYCLR, n, clr );
		SetElemChar ( CKEYFLAG, n, 1 );		// mark this key as dirty		
	}
}

void Curve::DelLastKey ()
{
	if ( NumKeys() > 0 ) {
		// DelElem ( .. )
		RebuildPnts ();
	}
}

void Curve::UpdateTangents ()
{
	float tension = 0.8;

	int n;
	Vec3F v;
	Vec3F *key, *keyn, *tl, *tr;

	if ( NumKeys() <= 2 ) return;

	tr = GetElemVec3 ( CKEYTANR, 0 );
	if ( tr->x != 0.0f || tr->y != 0.0f || tr->z != 0.0f ) {				
		v = * GetElemVec3 ( CKEYPOS, 1 ); v -= * GetElemVec3 ( CKEYPOS, 0 ); v *= tension;		
		*tr = v;
	}
	key = GetElemVec3 ( CKEYPOS, 0 );
	keyn = GetElemVec3 ( CKEYPOS, 2 );
	tl = GetElemVec3 ( CKEYTANL, 1 );
	tr = GetElemVec3 ( CKEYTANR, 1 );
	for ( n=1; n <= (int) NumKeys()-2; n++) {		
		v = *keyn; v -= *key; v *= tension;
		if ( v.x==0 && v.y==0 && v.z==0 ) v.Set ( 0.1, 0.1, 0.1 );
		if ( tl->x != 0.0f || tl->y != 0.0f || tl->z != 0.0f ) 	*tl = v;
		if ( tr->x != 0.0f || tr->y != 0.0f || tr->z != 0.0f )	*tr = v;
		tl++; tr++;
		key++; keyn++;
	}
	
	if ( m_bClosed && n > 0 ) {
		tl = GetElemVec3 ( CKEYTANL, n );
		tr = GetElemVec3 ( CKEYTANR, 0 );

		key = GetElemVec3 ( CKEYPOS, n-1 );
		keyn = GetElemVec3 ( CKEYPOS, 1 );
		if ( (key->x != 0.0 || key->y != 0.0 || key->z != 0.0 ) && ( keyn->x!= 0.0 || keyn->y != 0.0 || keyn->z!= 0.0)) {
			v = * GetElemVec3 ( CKEYPOS, 1 ); v -= * GetElemVec3 ( CKEYPOS, n-1 ); v *= tension;			
			if ( v.x==0.0f && v.y==0.0f && v.z==0.0f ) v.Set ( 0.1, 0.1, 0.1 );
			*tl = v;
			*tr = v;
		} else {
			tl->Set ( 0, 0, 0 );
			tr->Set ( 0, 0, 0 );
		}
	} else {
		tl = GetElemVec3 ( CKEYTANL, n );
		tr = GetElemVec3 ( CKEYTANR, n );
		if ( n >= 1 && (tl->x != 0.0f || tl->y != 0.0f || tl->z != 0.0f ) ) {
			v = * GetElemVec3 ( CKEYPOS, n ); v -= * GetElemVec3 ( CKEYPOS, n-1 ); v *= tension;			
			if ( v.x==0.0f && v.y==0.0f && v.z==0.0f ) v.Set ( 0.1, 0.1, 0.1 );			
			*tl = v;			
		}
	}
}


int Curve::AddPoint ( float x, float y, float z )
{
	uxlong ndx = AddElem ( CVERTPOS );
	AddElem ( CVERTDIST );	

	Vec3F v(x,y,z);
	SetElemVec3 ( CVERTPOS, ndx, v );	
	return ndx;
}
void Curve::MovePoint ( int p, float x, float y, float z )
{
	Vec3F v(x,y,z);
	SetElemVec3 ( CVERTPOS, p, v );
}

void Curve::Evaluate ()
{
	UpdateTangents ();
	EvaluatePnts ();	
}

void Curve::EvaluatePnts ()
{	
	double qx, qy, qz, qxl, qyl, qzl, qx1, qy1, qz1, qx2, qy2, qz2, qx3, qy3, qz3;
	double dt2, dt3, a, b;	
	uchar *flagl, *flagr;
	float dt = 1.0f / m_Subdivs;

	Vec3F *key, *keyn, *tl, *tln, *tr, *trn;
	Vec4F clr;
	dt2 = dt*dt; dt3 = dt2*dt;

	if ( NumKeys() < 2 ) return;

	key		= (Vec3F*)	GetStart ( CKEYPOS );
	keyn	= (Vec3F*)	key + 1;	
	tl		= (Vec3F*)	GetStart ( CKEYTANL );
	tln		= (Vec3F*)	tl + 1;
	tr		= (Vec3F*)	GetStart ( CKEYTANR );
	trn		= (Vec3F*)	tr + 1;
	flagl	= (uchar*)		GetStart ( CKEYFLAG );
	flagr	= (uchar*)		flagl + 1;
	
	qxl = key->x;
	qyl = key->y;
	qzl = key->z;

	int p = 0;

	for (int n=0; n < NumKeys()-1; n++) {
		if ( tr->x == 0.0 && tr->y == 0.0 && tr->z == 0.0 && tln->x == 0.0 && tln->y == 0.0 && tln->z == 0.0 ) {
			MovePoint ( p++, key->x, key->y, key->z );						
		} else {
			// Check if this section of the curve is dirty
			if ( *flagl == 1 || *flagr == 1 ) {
				// Compute forward difference initial conditions (Foley van Dam, p. 512)		
				qx = key->x; qy = key->y; qz = key->z;
				a =  2*key->x +  tr->x +tln->x -2*keyn->x;
				b = -3*key->x -2*tr->x -tln->x +3*keyn->x;
				qx1 =   dt3*a +   dt2*b + dt* tr->x; qx2 = 6*dt3*a + 2*dt2*b; qx3 = 6*dt3*a;
				a =  2*key->y +  tr->y +tln->y -2*keyn->y;
				b = -3*key->y -2*tr->y -tln->y +3*keyn->y;
				qy1 =   dt3*a +   dt2*b + dt* tr->y; qy2 = 6*dt3*a + 2*dt2*b; qy3 = 6*dt3*a;
				a =  2*key->z +  tr->z +tln->z -2*keyn->z;
				b = -3*key->z -2*tr->z -tln->z +3*keyn->z;
				qz1 =   dt3*a +   dt2*b + dt* tr->z; qz2 = 6*dt3*a + 2*dt2*b; qz3 = 6*dt3*a;

				for (int i=0; i < m_Subdivs; i++ ) {
					MovePoint ( p++, qx, qy, qz );
					qx += qx1; qx1 += qx2; qx2 += qx3;
					qy += qy1; qy1 += qy2; qy2 += qy3;
					qz += qz1; qz1 += qz2; qz2 += qz3;
				}
			} else {
				p += m_Subdivs;	// skip unchanged points 
			}
		}
		*flagl = 0;			// clear update flags
		flagl++; flagr++;
		key++; keyn++;
		tl++; tln++;
		tr++; trn++;
	}
	p = NumPoints()-1;
	key =   (Vec3F*) GetStart ( CKEYPOS ) + NumKeys()-1;
	MovePoint ( p, key->x, key->y, key->z );
	
	assert ( p == NumPoints()-1 );
	assert ( p == GetNumElem ( CVERTPOS )-1 );

	*flagl = 0;	
}


Vec3F Curve::GetTangent ( int n )
{
	if ( n < 0 ) n = 0;
	if ( n >= NumPoints()-2 ) n = NumPoints()-2;
	Vec3F vec = * GetElemVec3 ( CKEYPOS, n+1 );
	vec -= * GetElemVec3 ( CKEYPOS, n );
	vec.Normalize ();
	return vec;
}

Vec3F Curve::GetCurveVec ( int n )
{
	/*if ( n <= 0 ) n = 1;
	if ( n >= NumPoints()-2 ) n = NumPoints()-2;

	Vec3F v1, v2, vc;
	v2 = *GetElemVec ( CVPOS, n+1 );
	vc = *GetElemVec ( CVPOS, n );
	v1 = *GetElemVec ( CVPOS, n-1 );
	
	v2 -= vc;
	vc -= v1;
	v2.Normalize ();
	vc.Normalize ();
	v2.Cross ( vc );
	v2.Normalize ();*/
	return Vec3F(1,0,0);
}


void Curve::UpdateSides ()
{
	/*m_Sides.clear ();
	m_SidePnt.clear ();
	m_SidePnt.push_back ( 0 );
	
	int c = -1;
	int cnt = 0;
	for (int n=0; n < m_NumPnts-1; n++) {
		if ( m_Curve(CTANR_X,n) == 0.0 && m_Curve(CTANR_Y,n) == 0.0 && m_Curve(CTANR_Z,n) == 0.0 && m_Curve(CTANL_X,n+1) == 0.0 && m_Curve(CTANL_Y,n+1) == 0.0 && m_Curve(CTANL_Z,n+1) == 0.0 ) {
			if ( c != -1 ) { 
				m_Sides.push_back ( c ); 
				m_SidePnt.push_back ( cnt );
				c = -1; 
			}
			cnt += 1;
			m_Sides.push_back ( n );
			m_SidePnt.push_back ( cnt );
		} else {
			if ( c == -1 ) 	c = n; 
			cnt += (1.0 / m_dt);
		}
	}	
	if ( c != -1 ) { m_Sides.push_back ( c ); m_SidePnt.push_back ( cnt ); c = -1; }*/
}

/*int Curve::GetSideStart ( int side )
{
	return m_SidePnt[side];
}

int Curve::GetSideEnd ( int side )
{	
	return m_SidePnt[side+1];
}*/


/* ---  CURVE DERIVATIONS & EQUATIONS
				
		// Time vector = T[]
		t2 = t*t; t3 = t2*t;

		// Hermite basis matrix (matrix Mh, Foley p. 484, eqn 11.19)
		mh0 =  2*t3 + -3*t2 + 1;				
		mh1 =    t3 + -2*t2 + t;
		mh2 =    t3 + -1*t2;
		mh3 = -2*t3 +  3*t2 ;			

		// Functional form, Fm
		// (Note: P0, P1, P2, P3 = m_Curve = geometry vector )
		p = (P0*2+P1+P2-2*P3)*t3 + (-3*P0-2*P1-P2+3*P3)*t2 + P1*t + P0;
		a = 2p0+p1+p2-2p3, b = -3p0-2p1-p2+3p3, c = p1, d = p0 

		// Solution 1: Q(t) = T Mh Gh (very slow)
		// (Note: T Mh = Bh = Hermite blending functions, Foley p. 485, eqn 11.21)
		// - Ideal for single point evaluation (GetPoint), 20 mult, 15 adds
		px = m_Curve(CPOS_X,n)*mh0 + m_Curve(CTANR_X,n)*mh1 + m_Curve(CTANL_X,n+1)*mh2 + m_Curve(CPOS_X,n+1)*mh3;	
		py = m_Curve(CPOS_Y,n)*mh0 + m_Curve(CTANR_Y,n)*mh1 + m_Curve(CTANL_Y,n+1)*mh2 + m_Curve(CPOS_Y,n+1)*mh3;
		pz = m_Curve(CPOS_Z,n)*mh0 + m_Curve(CTANR_Z,n)*mh1 + m_Curve(CTANL_Z,n+1)*mh2 + m_Curve(CPOS_Z,n+1)*mh3;

		// Solution 2: Q(t+dt) = Q(t) + D*C, (forward differencing, Foley p. 512, eqn. 11.72)
		// - Fast, ideal for drawing curve
		// qx1 =   dt3*(2p0+p1+p2-2p3) +   dt2*(-3p0-2p1-p2+3p3) + dt*p1;
		// qx2 = 6*dt3*(2p0+p1+p2-2p3) + 2*dt2*(-3p0-2p1-p2+3p3) + dt*p1;
		// qx3 = 6*dt3*(2p0+p1+p2-2p3);
		a =  2*m_Curve(CPOS_X,n)+  m_Curve(CTANR_X,n)+m_Curve(CTANL_X,n+1)-2*m_Curve(CPOS_X,n+1);
		b = -3*m_Curve(CPOS_X,n)-2*m_Curve(CTANR_X,n)-m_Curve(CTANL_X,n+1)+3*m_Curve(CPOS_X,n+1);
		qx1 =   dt3*a + dt2*b + dt*m_Curve(CTANR_X,n);
		qx2 = 6*dt3*a + dt2*b;
		qx3 = 6*dt3*a;

		
		// Set dt, Evaluate once...
		qx1 =   dt3*m_Curve(CPOS_X,n) +   dt2*m_Curve(CTANR_X,n) + dt*);
		qx2 = 6*dt3*m_Curve(CPOS_X,n) + 2*dt2*m_Curve(CTANR_X,n);
		qx3 = 6*dt3*m_Curve(CPOS_X,n);
		qy1 =   dt3*m_Curve(CPOS_Y,n) +   dt2*m_Curve(CTANR_Y,n) + dt*(-3*m_Curve(CPOS_Y,n)-2*m_Curve(CTANR_Y,n)-m_Curve(CTANL_Y,n+1)+3*m_Curve(CPOS_Y,n+1));
		qy2 = 6*dt3*m_Curve(CPOS_Y,n) + 2*dt2*m_Curve(CTANR_Y,n);
		qy3 = 6*dt3*m_Curve(CPOS_Y,n);
		qz1 =   dt3*m_Curve(CPOS_Z,n) +   dt2*m_Curve(CTANR_Z,n) + dt*(-3*m_Curve(CPOS_Z,n)-2*m_Curve(CTANR_Z,n)-m_Curve(CTANL_Z,n+1)+3*m_Curve(CPOS_Z,n+1));
		qz2 = 6*dt3*m_Curve(CPOS_Z,n) + 2*dt2*m_Curve(CTANR_Z,n);
		qz3 = 6*dt3*m_Curve(CPOS_Z,n);
		// Evaluate in loop...
		qx += px1; qx1 += qx2; qx2 += qx3;
		qy += py1; qy1 += qy2; qy2 += qy3;
		qz += pz1; qz1 += qz2; qz2 += qz3;

*/

// NOTE: CURRENTLY POINTS MUST BE SPECIFIED IN TIME-INCREASING ORDER
// (There is a better solution that inserts points in their 
//  proper time via binary search)
/*void Curve2D::SetPoint ( double t, Vec2F p )
{
	if ( t < m_tMin) m_tMin = t;
	if ( t > m_tMax) m_tMax = t;
	m_Curve.push_back ( CurvePnt2D ( t, p ) );
	UpdateTangents ();
}


Vec2F Curve2D::GetPoint ( double t )
{
	int l, r;							// Left/right points
	double u;							// Local parametic time
	double u1, u2, u3;					// Blending coefficients (u0 = 1)
	double q0, q1, q2, q3;
	double px, py;						// Current curve position	

	if ( m_Curve.size() < 2) return Vec2F(0, 0);
	if ( t < m_Curve[0].t || t > m_Curve[m_Curve.size()-1].t ) return Vec2F (0, 0);

	if ( t >= m_Curve[m_iLastPnt].t && t < m_Curve[m_iLastPnt+1].t ) {
		l = m_iLastPnt;
		r = m_iLastPnt+1;		
	} else {
		// This should be a binary search
		l = 0;
		r = l+1;
		for (int n=0; n < (int) m_Curve.size()-1; n++) {
			if ( t < m_Curve[n+1].t) {
				l = n; 
				r = n+1;
				m_iLastPnt = l;
				break;
			}
		}
	}
	// Determine local parametric time		
	u = (t - m_Curve[l].t) / (m_Curve[r].t - m_Curve[l].t);
	// Compute blending coefficients
	u1 = u; u2 = u*u; u3 = u2*u;
	q3 =  2*u3 + -3*u2 + 0   + 1;
	q2 =    u3 + -2*u2 + u1  + 0;
	q1 =    u3 + -1*u2 + 0   + 0;
	q0 = -2*u3 +  3*u2 + 0   + 0;
	// Compute curve position
	px = m_Curve[l].pos.x*q3 + m_Curve[l].dout.x*q2 + m_Curve[r].din.x*q1 + m_Curve[r].pos.x*q0;
	py = m_Curve[l].pos.y*q3 + m_Curve[l].dout.y*q2 + m_Curve[r].din.y*q1 + m_Curve[r].pos.y*q0;
	return Vec2F ( px, py );	
}

*/




