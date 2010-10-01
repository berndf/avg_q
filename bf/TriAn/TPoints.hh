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
