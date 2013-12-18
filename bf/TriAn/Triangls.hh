// Copyright (C) 1996-1998 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

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
