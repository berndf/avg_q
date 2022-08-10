// Copyright (C) 1996,1997,2010,2013-2015 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

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
