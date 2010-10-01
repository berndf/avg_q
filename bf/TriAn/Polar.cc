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
