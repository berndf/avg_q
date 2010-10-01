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
