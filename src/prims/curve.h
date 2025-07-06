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

#ifndef DEF_CURVE
	#define	DEF_CURVE

	#include "datax.h"
	#include "object.h"
	#include "camera.h"

	//typedef signed int			xref;
	
	#define CVERTPOS	0
	#define CVERTDIST	1

	#define CKEYPOS		2
	#define CKEYCLR		3
	#define CKEYTANL	4
	#define CKEYTANR	5
	#define CKEYFLAG	6

	class Curve : public DataX, public Object {
	public:	
		Curve ();
		~Curve ();
		virtual objType		getType()		{ return 'Acrv'; }		
		virtual void		Clear();
		virtual void		Sketch (int w, int h, Camera3D* cam);
		
		// Curve editing		
		void				Evaluate ();
		void				RebuildPnts ();
		void				EvaluatePnts ();
		int					AddPoint ( float x, float y, float z );
		void				MovePoint ( int p, float x, float y, float z );
		int					AddKey ( Vec3F ps );
		int					AddKey ( float x, float y, float z, CLRVAL clr );		
		int					AddKey ( Vec3F ps, CLRVAL clr, bool bLine );		// rebuilds the curve
		int					AddKeyFast ( Vec3F ps, bool bLine );				// does not rebuild		
		void				MoveKey ( int n, Vec3F p );
		void				MoveKey ( int n, Vec3F p, CLRVAL clr );
		void				MoveLastKey ( Vec3F p );
		void				RemoveKey ( int n );
		void				DelLastKey ();
		void				CloseCurve ();		
		void				UpdateTangents ();
		bool				IsPointOnCurve ( Vec3F p, Vec3F& q, float eps );		
		float				getCurveLength();
		Vec3F			getPointOnCurve ( float t );
		Vec3F			getNearestPoint ( Vec3F q, Vec3F& result );  // result = <t, dist, dmax>
		Vec3F			GetTangent ( int n );
		Vec3F			GetCurveVec ( int n );

		void				SetSubdivs ( int sd );
		int					getSubdivs ()			{ return m_Subdivs;}
		
		// Curve sides
		void				UpdateSides ();
		
		int					NumKeys ()						{ return GetNumElem( CKEYPOS ); }		
		int					NumPoints ()					{ return GetNumElem( CVERTPOS );  }
		void				SetKeyPos ( int n, Vec3F& p )	{ SetElemVec3 ( CKEYPOS, n, p ); }
		void				SetKeyClr ( int n, CLRVAL c )	{ SetElemClr ( CKEYCLR, n, c ); }
		Vec3F*			GetVertPos ( int n )			{ return (Vec3F*) GetElemVec3 ( CVERTPOS, n ); }
		//CLRVAL*				GetVertClr ( int n )			{ return (CLRVAL*)	  GetElemClr ( CVERTCLR, n ); }
		CLRVAL*				GetKeyClr ( int n )				{ return (CLRVAL*)    GetElemVec3 ( CKEYCLR, n ); }
		Vec3F*			GetKeyPos ( int n )				{ return (Vec3F*) GetElemVec3 ( CKEYPOS, n ); }
		Vec3F*			GetKeyTanL ( int n )			{ return (Vec3F*) GetElemVec3 ( CKEYTANL, n ); }
		Vec3F*			GetKeyTanR ( int n )			{ return (Vec3F*) GetElemVec3 ( CKEYTANR, n ); }				
		void				SetColor ( Vec3F clr )		{ m_Clr = clr; }

	protected:
		Vec3F			m_Clr;
		int					m_Subdivs;	
		bool				m_bClosed;
	};

#endif

