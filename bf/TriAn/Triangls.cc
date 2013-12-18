// Copyright (C) 1996,1997 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#include "Triangls.hh"

Triangles::Triangles() {
}
Triangles::Triangles(Point *pp1, Point *pp2, Point *pp3) {
 p1=pp1;
 p2=pp2;
 p3=pp3;
}
Triangles::Triangles(Triangle* tr) {
 p1=tr->p1;
 p2=tr->p2;
 p3=tr->p3;
}
Triangles::~Triangles() {
}
// Search proceeds only to following members, thus allowing to continue a
// search for further matches
Triangles* Triangles::find(Point* p) {
 Triangles* intriangle=this;
 while (intriangle!=(Triangles*)Empty) {
  if (intriangle->p1==p || intriangle->p2==p || intriangle->p3==p) {
   break;
  }
  intriangle=(Triangles*)intriangle->next();
 }
 return intriangle;
}
Triangles* Triangles::find(Point* pa, Point* pb) {
 Triangles* intriangle=this;
 while (intriangle!=(Triangles*)Empty) {
  if (intriangle->is_edge(pa, pb)) break;
  intriangle=(Triangles*)intriangle->next();
 }
 return intriangle;
}
Triangles* Triangles::find_adjacent(Triangle* tr1) {
 Triangles* intriangle=this;
 while (intriangle!=(Triangles*)Empty) {
  if (intriangle->is_edge(tr1->p1, tr1->p2)
   || intriangle->is_edge(tr1->p2, tr1->p3)
   || intriangle->is_edge(tr1->p3, tr1->p1)) break;
  intriangle=(Triangles*)intriangle->next();
 }
 return intriangle;
}
int Triangles::is_above(Point* p, Triangles** which) {
 *which=this;
 int retval=0;
 while (*which!=(Triangles*)Empty) {
  if ((retval=((Triangle*)(*which))->is_above(p))!=0) {
   break;
  }
  *which=(Triangles*)(*which)->next();
 }
 return retval;
}
int Triangles::is_above(Point* p, Triangles** which, float orthogonal_distance_limit) {
 float orthogonal_coeff;
 *which=this;
 int retval=0;
 while (*which!=(Triangles*)Empty) {
  if ((retval=((Triangle*)(*which))->is_above(p, &orthogonal_coeff))!=0) {
   if (fabs(orthogonal_coeff)<=orthogonal_distance_limit) {
    break;
   } else {
    retval=0;
   }
  }
  *which=(Triangles*)(*which)->next();
 }
 return retval;
}
Point Triangles::adjacent_normal(Point* pa, Point* pb) {
 Point a_normal;
 int n=0;
 Triangles* intriangle=(Triangles*)first();
 do {
  intriangle=intriangle->find(pa, pb);
  if (intriangle==(Triangles*)Empty) break;
  a_normal=a_normal+intriangle->normal();
  n++;
  intriangle=(Triangles*)intriangle->next();
 } while (intriangle!=(Triangles*)Empty);
 if (n>0) a_normal=a_normal/(float)n;
 return a_normal;
}
Point Triangles::adjacent_normal(Triangle* tr) {
 Point a_normal;
 int n=0;
 Triangles* intriangle=(Triangles*)first();
 do {
  intriangle=intriangle->find_adjacent(tr);
  if (intriangle==(Triangles*)Empty) break;
  a_normal=a_normal+intriangle->normal();
  n++;
  intriangle=(Triangles*)intriangle->next();
 } while (intriangle!=(Triangles*)Empty);
 if (n>0) a_normal=a_normal/(float)n;
 return a_normal;
}
int Triangles::adjacent_direction(Triangles* ntr) {
 // Informs whether the triangle ntr is oriented consistently with its
 // neighbors in the Triangles list this
 Point n=adjacent_normal(ntr);
 float pn=ntr->normal()*n;
 int retval;
 if (pn>0) retval=1;
 else if(pn<0) retval= -1;
 else retval=0;
 return retval;
}
float Triangles::adjacent_angle(Triangles* ntr) {
 // Returns the angle between the normal of ntr and the adjacent triangles
 Point n=adjacent_normal(ntr);
 float pn=ntr->normal()*n;
 return acos(pn);
}
void Triangles::display(int colour) {
 Triangles* intriangle=(Triangles*)first();
 while (intriangle!=(Triangles*)Empty) {
  ((Triangle*)intriangle)->display(colour);
  intriangle=(Triangles*)intriangle->next();
 }
}
void Triangles::print() {
 Triangles* intriangle=(Triangles*)first();
 while (intriangle!=(Triangles*)Empty) {
  ((Triangle*)intriangle)->print();
  printf("Normal is:\n");
  intriangle->normal().print();
  intriangle=(Triangles*)intriangle->next();
 }
}
