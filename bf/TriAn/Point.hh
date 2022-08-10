// Copyright (C) 1996,1997,2010,2013-2015 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

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
