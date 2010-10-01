// Copyright (C) 1996-1998 Bernd Feige
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
