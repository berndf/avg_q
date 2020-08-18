// Copyright (C) 1996,1997,2018 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#ifndef _ConvexMesh_HH
#define _ConvexMesh_HH

#include "General.hh"
#include "TPoints.hh"
#include "Triangls.hh"
#include "Boundary.hh"

class ConvexMesh {
public:
 ConvexMesh();
 ConvexMesh(TPoints *ttp);
 virtual ~ConvexMesh();
#ifdef WITH_POSPLOT
 void display();
#endif
 void print();

 TPoints *tp;
 Boundary *boundary;
 Triangles *tr;
private:
 void fit_sphere();
 Triangles* check_triangle(Point* p1, Point* p2, Point* p3);
 Boundary* add_to_boundary(Boundary* b, TPoints* p);
 Triangles* nearest_to_boundary();
 void Revise();

 // These are origin and radius of a fitted sphere
 Point origin;
 float radius;
};
#endif
