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
#ifndef _Point_HH
#define _Point_HH

#include "General.hh"

class Point {
public:
 Point();
 Point(Point*);
 Point(float xx, float yy, float zz);
 virtual ~Point() {};
 void print();
 float dd(Point* p2);
 float angular_distance(Point* origin, Point* p2);
 float tangential_distance(Point* origin, Point* p2);
 float distance(Point* p1, Point* p2);
 float rdistance(Point* origin, Point* p1, Point* p2);
 float angle(Point* p);
 Point prod(Point* p2);
 Point div(Point* p2);
 void read_from_file(FILE *infile);
 void write_to_file(FILE *outfile);
 float square();
 float abs();
 void normalize();
 Point cross (const Point&);
 Point operator+ (const Point&);
 Point operator- (const Point&);
 Point operator* (const float);
 float operator* (const Point&);
 Point operator/ (const float);
//private:
 float x;
 float y;
 float z;
};
#endif
