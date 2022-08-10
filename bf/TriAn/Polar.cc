// Copyright (C) 1996,2010,2013-2015 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#include "Polar.hh"

Polar::Polar() {
 r=0;
}
Polar::Polar(Point* p) {
 r=p->abs();
 theta=acos(p->z/r);
 phi=atan2(p->x, p->y);
}
Point Polar::to_Point() {
 Point p;
 double cos_theta=cos(theta);
 double sin_theta=sin(theta);
 double cos_phi=cos(phi);
 double sin_phi=sin(phi);
 p.x=r*cos_phi*sin_theta;
 p.y=r*sin_phi*sin_theta;
 p.z=r*        cos_theta;
 return p;
}
void Polar::print() {
 printf("%g %g %g\n", r, theta, phi);
}
