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

#include "point_base.h"
#include "points.h"

#include "gxlib.h"
using namespace glib;

void PointBase::Sketch  ( int w, int h, Camera3D* cam )
{
	Points* pnts = getOutputPoints();
	if (pnts==0x0) return;

	Vec3F bmin = pnts->getBMin();
	Vec3F bmax = pnts->getBMax();
	drawBox3D ( bmin, bmax, Vec4F(1,1,1,1) );
}





