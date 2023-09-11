// Copyright (C) 1996,1997,2011,2018 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#include "Boundary.hh"

Boundary::Boundary(Point* p) {
 _point=p;
}
Boundary::Boundary(Triangle* tr) {
 Boundary* inboundary=this;
 _point=tr->p1;
 inboundary=(Boundary*)inboundary->addmember(new Boundary(tr->p2));
 inboundary=(Boundary*)inboundary->addmember(new Boundary(tr->p3));
}
Boundary::~Boundary() {
}
Boundary* Boundary::find(Point* p) {
 Boundary* inboundary=this;
 while (inboundary!=(Boundary*)Empty) {
  if (inboundary->point()==p) break;
  inboundary=(Boundary*)inboundary->next();
 }
 return inboundary;
}
Boundary* Boundary::find(Triangle* t) {
 Boundary* inboundary=this;
 while (inboundary!=(Boundary*)Empty) {
  Point* bp=inboundary->point();
  if (bp==t->p1 || bp==t->p2 || bp==t->p3) {
   bp=((Boundary*)inboundary->ringnext())->point();
   if (bp==t->p1 || bp==t->p2 || bp==t->p3) break;
  }
  inboundary=(Boundary*)inboundary->next();
 }
 return inboundary;
}
Point Boundary::cross2(Boundary* b1, Boundary* b2) {
 return (*b1->point()-*this->point()).cross(*b2->point()-*this->point());
}
Point Boundary::normal_of_1plane() {
 Boundary* next1=(Boundary*)ringnext();
 Boundary* next2=(Boundary*)next1->ringnext();
 Point pn=cross2(next1, next2);
 pn.normalize();
 return pn;
}
Point Boundary::normal_of_allplanes() {
 Point normal=Point(0.0,0.0,0.0);
 Boundary* inboundary=(Boundary*)first();
 while (inboundary!=(Boundary*)Empty) {
  normal=normal+inboundary->normal_of_1plane();
  inboundary=(Boundary*)inboundary->next();
 }
 normal.normalize();
 return normal;
}
int Boundary::curvature_direction(Point& normal) {
 // Takes the previous, this and next points and determines whether the next point is outside (-1),
 // on (0) or inside (1) of the line extrapolated from the previous over this.
 Boundary* prev=(Boundary*)ringprevious();
 Boundary* nxt=(Boundary*)ringnext();
 float pn=prev->cross2(this, nxt)*normal;
 int retval;
 if (pn>0) retval=1;
 else if(pn<0) retval= -1;
 else retval=0;
 return retval;
}
float Boundary::typical_dd() {
 Boundary* inboundary=(Boundary*)first();
 int n=0;
 float mean_dd=0;
 while (inboundary!=(Boundary*)Empty) {
  mean_dd+=inboundary->point()->dd(((Boundary*)inboundary->ringnext())->point());
  n++;
  inboundary=(Boundary*)inboundary->next();
 }
 if (n!=0) mean_dd/=n;
 return mean_dd;
}
int Boundary::curvature_direction() {
 Point normal=normal_of_allplanes();
 return curvature_direction(normal);
}
Boundary* Boundary::non_convex_point(Point& normal) {
 Boundary* inboundary=this;
 while (inboundary!=(Boundary*)Empty) {
  if (inboundary->curvature_direction(normal)!=1) {
   break;
  }
  inboundary=(Boundary*)inboundary->next();
 }
 return inboundary;
}
Boundary* Boundary::non_convex_point() {
 Point normal=normal_of_allplanes();
 return non_convex_point(normal);
}
int Boundary::adjacent_direction (Triangles* tr, Boundary* prev, Boundary* nxt) {
 Point normal=tr->adjacent_normal(prev->point(), point())+tr->adjacent_normal(point(), nxt->point());
 normal.normalize();
 return curvature_direction(normal);
}
// This one takes the normal locally from the adjacent triangles
Boundary* Boundary::non_convex_point(Triangles* tr) {
 Boundary* inboundary=this;
 while (inboundary!=(Boundary*)Empty) {
  Boundary* prev=(Boundary*)inboundary->ringprevious();
  Boundary* nxt=(Boundary*)inboundary->ringnext();
  if (inboundary->adjacent_direction(tr, prev, nxt)== -1) {
   break;
  }
  inboundary=(Boundary*)inboundary->next();
 }
 return inboundary;
}
Bool Boundary::is_convex() {
 return non_convex_point()==(Boundary*)Empty;
}
float Boundary::distance(Boundary* b, Point* p) {
 // This one implements the distance from a line
 return p->distance(this->point(), b->point());
}
float Boundary::distance(Point* p, Boundary** bmin) {
 float mindistance= -1;
 Boundary* inpoint=(Boundary*)first();
 *bmin=(Boundary*)Empty;
 while (inpoint!=(Boundary*)Empty) {
  float thisdistance=inpoint->distance((Boundary*)inpoint->ringnext(), p);
  if (mindistance<0 || thisdistance<mindistance) {
   *bmin=inpoint;
   mindistance=thisdistance;
  }
  inpoint=(Boundary*)inpoint->next();
 }
 return mindistance;
}
float Boundary::distance(Point* p) {
 Boundary* bmin;
 return distance(p, &bmin);
}
float Boundary::distance(Triangles* tr, Point* p, Boundary** bmin) {
 // This finds the smallest distance of p to any triangle in tr adjacent
 // to a section of the boundary
 float mindistance= -1;
 Boundary* inpoint=(Boundary*)first();
 *bmin=(Boundary*)Empty;
 while (inpoint!=(Boundary*)Empty) {
  Point* p1=inpoint->point();
  Point* p2=((Boundary*)inpoint->ringnext())->point();
  Triangles* adjacent_triangle=tr->find(p1, p2);
  if (adjacent_triangle!=(Triangles*)Empty) {
   float thisdistance=adjacent_triangle->plane_distance(p);
   if (mindistance<0 || thisdistance<mindistance) {
    *bmin=inpoint;
    mindistance=thisdistance;
   }
  }
  inpoint=(Boundary*)inpoint->next();
 }
 return mindistance;
}
Boundary* Boundary::nearest(Point* p) {
 Boundary* bmin;
 distance(p, &bmin);
 return bmin;
}
Boundary* Boundary::nearest(Point* p, Boundary* notthis) {
 float mindd= -1;
 Boundary* inpoint=(Boundary*)first();
 Boundary* minpoint=(Boundary*)Empty;
 while (inpoint!=(Boundary*)Empty) {
  float thisdd=inpoint->point()->dd(p);
  if ((mindd<0 || thisdd<mindd) && inpoint!=notthis) {
   minpoint=inpoint;
   mindd=thisdd;
  }
  inpoint=(Boundary*)inpoint->next();
 }
 return minpoint;
}
Point* Boundary::point() {
 return _point;
}
void Boundary::print() {
 _point->print();
}
#ifdef WITH_POSPLOT
void Boundary::display(int colour) {
 color(colour);
 Boundary* inboundary=(Boundary*)first();
 bgnline();
 move(inboundary->point()->x, inboundary->point()->y, inboundary->point()->z);
 inboundary=(Boundary*)inboundary->next();
 while (inboundary!=(Boundary*)Empty) {
  draw(inboundary->point()->x, inboundary->point()->y, inboundary->point()->z);
  inboundary=(Boundary*)inboundary->next();
 }
 inboundary=(Boundary*)first();
 draw(inboundary->point()->x, inboundary->point()->y, inboundary->point()->z);
 endline();
}
#endif
void Boundary::write_to_file(FILE *outfile) {
 Boundary* inpoint=(Boundary*)first();
 fprintf(outfile, "%d\n", nr_of_members());
 while (inpoint!=(Boundary*)Empty) {
  ((Boundary*)inpoint)->point()->write_to_file(outfile);
  inpoint=(Boundary*)inpoint->next();
 }
}
