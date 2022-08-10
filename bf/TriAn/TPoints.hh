// Copyright (C) 1996,1997,2010,2013-2015 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#ifndef _TPoints_HH
#define _TPoints_HH

#include "General.hh"
#include "Points.hh"
#include "Triangle.hh"

class TPoints : public Points {
public:
 TPoints();
 TPoints(FILE *infile);
 TPoints(int nx, int ny);
 TPoints(float xx, float yy, float zz);
 TPoints* nearest(Bool inside, float* mindd);
 TPoints* nearest(Bool inside);
 TPoints* nearest(Bool inside, TPoints* NotThis);
 TPoints* nearest();
 TPoints* nearest(Point* p2, Bool inside, float* mindd);
 TPoints* angular_nearest(Point* origin, Point* p2, Bool inside, float* mindd);
 void mark_inside();
 void mark_outside();
 Bool is_inside();
private:
 Bool _is_inside;
};
#endif
