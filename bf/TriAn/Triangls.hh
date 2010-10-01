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
#ifndef _Triangles_HH
#define _Triangles_HH

#include "General.hh"
#include "Triangle.hh"
#include "LinkedObject.hh"

class Triangles : public LinkedObject, public Triangle {
public:
 Triangles();
 Triangles(Point *pp1,Point *pp2,Point *pp3);
 Triangles(Triangle* tr);
 virtual ~Triangles();
 Triangles* find(Point* p);
 Triangles* find(Point* pa, Point* pb);
 Triangles* find_adjacent(Triangle* tr1);
 int is_above(Point* p, Triangles** which);
 int is_above(Point* p, Triangles** which, float orthogonal_distance_limit);
 Point adjacent_normal(Point* pa, Point* pb);
 Point adjacent_normal(Triangle* tr);
 int adjacent_direction(Triangles* tr);
 float adjacent_angle(Triangles* tr);
 void display(int colour);
 void print();
private:
};
#endif
