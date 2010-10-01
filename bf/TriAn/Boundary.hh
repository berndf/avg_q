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
#ifndef _Boundary_HH
#define _Boundary_HH

#include "General.hh"
#include "Point.hh"
#include "Points.hh"
#include "Triangle.hh"
#include "Triangls.hh"

class Boundary : public LinkedObject {
public:
 Boundary() {};
 Boundary(Point* p);
 Boundary(Triangle* p);
 virtual ~Boundary();
 Boundary* find(Point* p);
 Boundary* find(Triangle* t);
 Point cross2(Boundary* b1, Boundary* b2);
 Point normal_of_1plane();
 Point normal_of_allplanes();
 float typical_dd();
 int curvature_direction(Point& normal);
 int curvature_direction();
 Boundary* non_convex_point(Point& normal);
 Boundary* non_convex_point();
 int adjacent_direction (Triangles* tr, Boundary* b2, Boundary* b3);
 Boundary* non_convex_point(Triangles* tr);
 Bool is_convex();
 float distance(Boundary* b, Point* p);
 float distance(Point* p, Boundary** bmin);
 float distance(Point* p);
 float distance(Triangles* tr, Point* p);
 float distance(Triangles* tr, Point* p, Boundary** bmin);
 Boundary* nearest(Point* p);
 Boundary* nearest(Point* p, Boundary* notthis);
 void write_to_file(FILE* outfile);
 void print();
 void display(int colour);
 Point* point();
private:
 Point* _point;
};
#endif
