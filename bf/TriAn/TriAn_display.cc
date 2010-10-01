// Copyright (C) 1996 Bernd Feige
// 
// This file is part of avg_q.
// 
// avg_q is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// avg_q is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
#include "LinkedObject.hh"
#include "Point.hh"
#include "Points.hh"
#include "TPoints.hh"
#include "Triangle.hh"
#include "Triangls.hh"
#include "ConvexMesh.hh"

extern "C" {
#include "bf.h"
#include "transform.h"

extern void TriAn_init(transform_info_ptr tinfo);
extern void TriAn_display(transform_info_ptr tinfo);
}

static ConvexMesh* cm;

void 
TriAn_init(transform_info_ptr tinfo) {
 int channel=0;
 TPoints* tp=(TPoints*)Empty;
 do {
  tp=(TPoints*)tp->addmember(new TPoints(tinfo->probepos[3*channel], tinfo->probepos[3*channel+1], tinfo->probepos[3*channel+2]));
  channel++;
 } while (channel<tinfo->nr_of_channels);
 cm=new ConvexMesh(tp);
 //cm->print();
}

void 
TriAn_display(transform_info_ptr tinfo) {
 cm->display();
}
