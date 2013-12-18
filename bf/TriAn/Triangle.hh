// Copyright (C) 1996-1998 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#ifndef _Triangle_HH
#define _Triangle_HH

#include "General.hh"
#include "Point.hh"

class Triangle {
public:
 Triangle();
 Triangle(Point *pp1,Point *pp2,Point *pp3);
 virtual ~Triangle();
 Point normal();
 void make_firstpoint(Point *const newp1);
 float max_angle(Bool const reorder);
 int is_above(Point* p, float* orthogonal_coeff);
 int is_above(Point* p);
 Bool is_edge(Point* pa, Point* pb);
 float plane_distance(Point* p, float* dn);
 float plane_distance(Point* p);
 void print();
 void display(int const colour);
 Point* p1;
 Point* p2;
 Point* p3;
private:
};
#endif
