// Copyright (C) 1996,1997 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#include "Points.hh"

Points::Points(Point* p) {
 this->x=p->x;
 this->y=p->y;
 this->z=p->z;
}
Points::Points(FILE *infile) {
 read_from_file(infile);
}
Points::~Points() {
// printf("Points Destructor called:\n");
// print();
}
int Points::read_from_file(FILE *infile) {
 int nr_of_points;
 Points* inpoints=this;
 fscanf(infile, "%d\n", &nr_of_points);
 ((Point*)inpoints)->read_from_file(infile);
 int i;
 for (i=1; i<nr_of_points; i++) {
  inpoints=(Points*)inpoints->addmember(new Points());
  ((Point*)inpoints)->read_from_file(infile);
 }
 return nr_of_points;
}
void Points::write_to_file(FILE *outfile) {
 Points* inpoint=(Points*)first();
 fprintf(outfile, "%d\n", nr_of_members());
 while (inpoint!=(Points*)Empty) {
  ((Point*)inpoint)->write_to_file(outfile);
  inpoint=(Points*)inpoint->next();
 }
}
Point Points::CenterOfGravity(void) {
 Points* inpoint=(Points*)first();
 int nr_of_points=0;
 Point p;
 while (inpoint!=(Points*)Empty) {
  p=p+*inpoint;
  inpoint=(Points*)inpoint->next();
  nr_of_points++;
 }
 if (nr_of_points>1) p=p/nr_of_points;
 return p;
}
Points* Points::nearest(Point* p, float* mindd) {
 *mindd= -1;
 Points* inpoint=(Points*)first();
 Points* minpoint=(Points*)Empty;
 while (inpoint!=(Points*)Empty) {
  if (inpoint!=p) {
  float const thisdd=inpoint->dd(p);
  if (*mindd<0 || thisdd<*mindd) {
   minpoint=inpoint;
   *mindd=thisdd;
  }
  }
  inpoint=(Points*)inpoint->next();
 }
 return minpoint;
}
Points* Points::nearest(Point* p) {
 float mindd;
 return nearest(p, &mindd);
}
float Points::typical_dd() {
 Points* inpoint=(Points*)first();
 int n=0;
 float mean_dd=0;

 while (inpoint!=(Points*)Empty) {
  float thisdd;
  nearest(inpoint, &thisdd);
  mean_dd+=thisdd;
  n++;
  inpoint=(Points*)inpoint->next();
 }
 if (n!=0) mean_dd/=n;
 return mean_dd;
}
Points* Points::copy() {
 Points* inpoint=(Points*)first();
 Points* newpoints;
 if (inpoint==Empty) {
  newpoints=(Points*)Empty;
 } else {
  newpoints=new Points(inpoint);
  inpoint=(Points*)inpoint->next();
  while (inpoint!=(Points*)Empty) {
   newpoints=(Points*)newpoints->addmember(new Points(inpoint));
   inpoint=(Points*)inpoint->next();
  }
 }
 return newpoints;
}
