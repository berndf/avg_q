// Copyright (C) 1996 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

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
#ifdef WITH_POSPLOT
 cm->display();
#endif
}
