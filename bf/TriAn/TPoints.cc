// Copyright (C) 1996-1998 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#include "TPoints.hh"

TPoints::TPoints() : Points::Points() {
 mark_outside();
}
TPoints::TPoints(float xx, float yy, float zz) {
 x=xx; y=yy; z=zz;
 mark_outside();
}
TPoints::TPoints(FILE *infile) : Points::Points(infile) {
}
TPoints::TPoints(int nx, int ny) {
 int i, j;
 TPoints* inp=this;
 for (i=0; i<nx; i++) {
  for (j=0; j<ny; j++) {
   if (i!=0 || j!=0) {
    inp=(TPoints*)inp->addmember(new TPoints());
   }
   inp->x=i; inp->y=j; inp->z=0;
  }
 }
}
TPoints* TPoints::nearest(Bool inside, float* mindd) {
 *mindd= -1;
 TPoints* inpoint=(TPoints*)first();
 TPoints* minpoint=(TPoints*)Empty;
 while (inpoint!=(TPoints*)Empty) {
  if (inpoint->_is_inside==inside) {
   float thisdd=dd(inpoint);
   if (*mindd<0 || thisdd<*mindd) {
    minpoint=inpoint;
    *mindd=thisdd;
   }
  }
  inpoint=(TPoints*)inpoint->next();
 }
 return minpoint;
}
TPoints* TPoints::nearest(Bool inside) {
 float mindd;
 return nearest(inside, &mindd);
}
TPoints* TPoints::nearest(Bool inside, TPoints* NotThis) {
 float mindd= -1;
 TPoints* inpoint=(TPoints*)first();
 TPoints* minpoint=(TPoints*)Empty;
 while (inpoint!=(TPoints*)Empty) {
  if (inpoint->_is_inside==inside && inpoint!=NotThis) {
   float thisdd=dd(inpoint);
   if (mindd<0 || thisdd<mindd) {
    minpoint=inpoint;
    mindd=thisdd;
   }
  }
  inpoint=(TPoints*)inpoint->next();
 }
 return minpoint;
}
TPoints* TPoints::nearest() {
 TPoints* minpoint=nearest(FALSE);
 if (minpoint!=(TPoints*)Empty) minpoint->mark_inside();
 return minpoint;
}
TPoints* TPoints::nearest(Point* p2, Bool inside, float* mindd) {
 // This returns the nearest point to the line between this and p2
 *mindd= -1;
 TPoints* inpoint=(TPoints*)first();
 TPoints* minpoint=(TPoints*)Empty;
 while (inpoint!=(TPoints*)Empty) {
  if (inpoint->_is_inside==inside) {
   float thisdd=inpoint->distance(this, p2);
   if (*mindd<0 || thisdd<*mindd) {
    minpoint=inpoint;
    *mindd=thisdd;
   }
  }
  inpoint=(TPoints*)inpoint->next();
 }
 return minpoint;
}
TPoints* TPoints::angular_nearest(Point* origin, Point* p2, Bool inside, float* mindd) {
 // This returns the point closest to p2. Note that `this' here is only used
 // for the collection of points and not as a point itself!
 *mindd= -1;
 TPoints* inpoint=(TPoints*)first();
 TPoints* minpoint=(TPoints*)Empty;
 while (inpoint!=(TPoints*)Empty) {
  if (inpoint->_is_inside==inside) {
//   float thisdd=inpoint->rdistance(origin, this, p2);
   float thisdd=inpoint->angular_distance(origin, p2);
   if (*mindd<0 || thisdd<*mindd) {
    minpoint=inpoint;
    *mindd=thisdd;
   }
  }
  inpoint=(TPoints*)inpoint->next();
 }
 return minpoint;
}
void TPoints::mark_inside() {
 _is_inside=TRUE;
}
void TPoints::mark_outside() {
 _is_inside=FALSE;
}
Bool TPoints::is_inside() {
 return _is_inside;
}
