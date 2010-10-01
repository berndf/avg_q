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
#include "Triangle.hh"

Triangle::Triangle() {
 p1=p2=p3=(Point*)0;
}
Triangle::Triangle(Point *pp1, Point *pp2, Point *pp3) {
 p1=pp1;
 p2=pp2;
 p3=pp3;
}
Triangle::~Triangle() {
 p1=p2=p3=(Point*)0;
}
Point Triangle::normal() {
 Point n=(*p2-*p1).cross(*p3-*p1);
 n.normalize();
 return n;
}
void Triangle::make_firstpoint(Point *const newp1) {
 /* Reorder the points in tr so that newp1 is the first */
 /* It is assumed that one of the points in the triangle actually is newp1! */
 Point* pt;
 if (p2==newp1) {
  pt=p1; p1=p2; p2=p3; p3=pt;
 } else if (p3==newp1) {
  pt=p2; p2=p1; p1=p3; p3=pt;
 }
}
float Triangle::max_angle(Bool const reorder) {
 // This method reorders the points in the triangle so that p1-p2 is the
 // line opposing the maximum angle and returns this maximum angle
 float a1=(*p1 - *p3).angle(&(*p2 - *p3));
 float a2=(*p2 - *p1).angle(&(*p3 - *p1));
 float a3=(*p3 - *p2).angle(&(*p1 - *p2));
 float maxangle=a1;
 Point* pt;

 if (a2>a1 && a2>a3) {
  if (reorder) {
   pt=p1; p1=p2; p2=p3; p3=pt;
  }
  maxangle=a2;
 } else if (a3>a1 && a3>a2) {
  if (reorder) {
   pt=p2; p2=p1; p1=p3; p3=pt;
  }
  maxangle=a3;
 }
 return maxangle;
}
int Triangle::is_above(Point* p, float* orthogonal_coeff) {
 // This method tells whether p's projection on this falls within the area
 // of this triangle; If it does, it tells if it is above (+1) or below (-1)
 // The idea is that we have three lines. For each line, any of the other two
 // defines the direction in which the triangle area lies. If we project the
 // point p into the plane of the triangle by subtracting the normal component,
 // we can test whether this projection falls into the triangle area by ensuring
 // that the projection is in the correct direction of all three lines.
 Point p2n=*p2-*p1;
 p2n.normalize();
 Point p3n=*p3-*p1;
 p3n.normalize();
 Point p32n=*p3-*p2;
 p32n.normalize();

 // p2o is the normal of (p3-p1) on (p2-p1)
 Point p2o=*p3-*p1;
 p2o=p2o-p2n*(p2o*p2n);
 //p2o.normalize(); // These needn't be normalized because only the direction is important

 // p3o is the normal of (p2-p1) on (p3-p1)
 Point p3o=*p2-*p1;
 p3o=p3o-p3n*(p3o*p3n);
 //p3o.normalize();

 // p32o is the normal of (p1-p2) on (p3-p2)
 Point p32o=*p1-*p2;
 p32o=p32o-p32n*(p32o*p32n);
 //p32o.normalize();

 Point n=normal();
 // pr is the vector to p within the plane of this triangle
 // pr1 with respect to p1, pr2 with respect to p2
 Point pr1=*p-*p1;
 float prn=pr1*n;
 pr1=pr1-n*prn;
 Point pr2=*p-*p2;
 pr2=pr2-n*(pr2*n);

 float pr_p2o=pr1*p2o;
 float pr_p3o=pr1*p3o;
 float pr_p32o=pr2*p32o;
 int retval=0;
 if (pr_p2o>0 && pr_p3o>0 && pr_p32o>0) {
  *orthogonal_coeff=prn;
  retval=(prn>=0.0 ? 1 : -1);
 }
 return retval;
}
int Triangle::is_above(Point* p) {
 float dummy;
 return is_above(p, &dummy);
}
Bool Triangle::is_edge(Point* pa, Point* pb) {
 return ((p1==pa || p2==pa || p3==pa) && (p1==pb || p2==pb || p3==pb));
}
float Triangle::plane_distance(Point* p, float* dn) {
 // This one gives the distance to the border of a triangle, measured on it's plane
 Point tr_normal=normal();
 Point pp=*p-*p1;
 *dn=pp*tr_normal;
 pp=pp-tr_normal*(*dn);
 Point pp1;
 Point pp2=*p2-*p1;
 Point pp3=*p3-*p1;
 float mindist=pp.distance(&pp1, &pp2);
 float nextdist=pp.distance(&pp2, &pp3);
 if (nextdist<mindist) mindist=nextdist;
 nextdist=pp.distance(&pp3, &pp1);
 if (nextdist<mindist) mindist=nextdist;
 return mindist;
}
float Triangle::plane_distance(Point* p) {
 float f;
 return plane_distance(p, &f);
}
void Triangle::print() {
 p1->print();
 p2->print();
 p3->print();
 p1->print();
 printf("\n");
}
void Triangle::display(int const colour) {
 color(colour);
 move(p1->x, p1->y, p1->z);
 draw(p2->x, p2->y, p2->z);
 draw(p3->x, p3->y, p3->z);
 draw(p1->x, p1->y, p1->z);
}
