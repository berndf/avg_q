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
 void display();
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
