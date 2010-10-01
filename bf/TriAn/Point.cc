// Copyright (C) 1996,1997 Bernd Feige
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
#include "Point.hh"

Point::Point() {
 x=y=z=0;
}
Point::Point(Point* p) {
 *this=*p;
}
Point::Point(float xx, float yy, float zz) {
 x=xx; y=yy; z=zz;
}
void Point::print() {
 printf("%g %g %g\n", x, y, z);
}
float Point::square() {
 return (x*x+y*y+z*z);
}
float Point::abs() {
 return sqrt(square());
}
void Point::normalize() {
 float length=abs();
 if (length!=0.0) {
  x/=length; y/=length; z/=length;
 }
}
float Point::dd(Point* p2) {
 float dx=p2->x-x;
 float dy=p2->y-y;
 float dz=p2->z-z;
 return dx*dx+dy*dy+dz*dz;
}
float Point::angular_distance(Point* origin, Point* p2) {
 Point tn= *this - *origin;
 Point pn= *p2 - *origin;
 float ta=tn.abs();
 float pa=pn.abs();
 return acos((tn*pn)/(ta*pa));
}
float Point::tangential_distance(Point* origin, Point* p2) {
 Point tn= *this - *origin;
 Point pn= *p2 - *origin;
 float ta=tn.abs();
 float pa=pn.abs();
 return (acos((tn*pn)/(ta*pa))*(ta+pa)/2.0);
}
float Point::distance(Point* p1, Point* p2) {
 // This one implements the distance of *this from a line between p1 and p2
 Point d=*p2-*p1;
 float dl=d.abs();
 d.normalize();
 Point dp=*this-*p1;
 float projection=dp*d;
 float dist;
 if (projection<0) {
  dist=dp.abs();
 } else if (projection>dl) {
  dist=(*this-*p2).abs();
 } else {
  dist=(dp-d*projection).abs();
 }
 return dist;
}
float Point::rdistance(Point* origin, Point* p1, Point* p2) {
 // This one implements the distance of *this from a line between p1 and p2
 Point tn= *this - *origin;
 Point pn1= *p1 - *origin;
 Point pn2= *p2 - *origin;
 tn.normalize();
 pn1.normalize();
 pn2.normalize();

 Point normal=pn1.cross(pn2); // Normal of the plane defined by p1, p2 and the origin
 Point p1normal=normal.cross(pn1);
 // The third normal is pn1 itself

 float cosphi_normal=(tn*normal);
 Point tnplane=tn-tn*cosphi_normal; // tn projected into the plane
 tnplane.normalize();

 float tn_phi=atan2(tnplane*p1normal, tnplane*pn1);

 float dist;
 if (tn_phi>0.0 && tn_phi<atan2(pn2*p1normal, pn2*pn1)) {
  // If the projection to the plane falls between the two points, return
  // the orthogonal distance to the plane
  dist=fabs(asin(cosphi_normal));
 } else {
  float cosphi_p1=tn*pn1;
  float cosphi_p2=tn*pn2;
  // Return the minimum of the two distances to p1 and p2
  dist=acos(cosphi_p1>cosphi_p2 ? cosphi_p1 : cosphi_p2);
 }

 return dist;
}
float Point::angle(Point* p) {
 return acos(((*this)*(*p))/abs()/p->abs());
}
Point Point::prod(Point* p2) {
 return Point(x*p2->x, y*p2->y, z*p2->z);
}
Point Point::div(Point* p2) {
 return Point(x/p2->x, y/p2->y, z/p2->z);
}
void Point::write_to_file(FILE *outfile) {
 fprintf(outfile, "%g %g %g\n", x, y, z);
}
void Point::read_from_file(FILE *infile) {
 fscanf(infile, "%f %f %f\n", &x, &y, &z);
}
Point Point::operator- (const Point& p2) {
 return Point(x-p2.x, y-p2.y, z-p2.z);
}
Point Point::operator+ (const Point& p2) {
 return Point(x+p2.x, y+p2.y, z+p2.z);
}
Point Point::operator* (const float f) {
 return Point(x*f, y*f, z*f);
}
float Point::operator* (const Point& p2) {
 return (x*p2.x+y*p2.y+z*p2.z);
}
Point Point::operator/ (const float f) {
 return Point(x/f, y/f, z/f);
}
Point Point::cross (const Point& p2) {
 return Point(y*p2.z-z*p2.y,z*p2.x-x*p2.z,x*p2.y-y*p2.x);
}
