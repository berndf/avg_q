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
#ifndef _Points_HH
#define _Points_HH

#include "General.hh"
#include "Point.hh"
#include "LinkedObject.hh"

class Points : public LinkedObject, public Point {
public:
 Points() {};
 Points(Point* p);
 Points(FILE *infile);
 virtual ~Points();
 int read_from_file(FILE *infile);
 void write_to_file(FILE *outfile);
 Point CenterOfGravity(void);
 Points* nearest(Point* p, float* mindd);
 Points* nearest(Point* p);
 float typical_dd();
 Points* copy();
};
#endif
