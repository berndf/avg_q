// Copyright (C) 1996 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#ifndef _Polar_HH
#define _Polar_HH

#include "General.hh"
#include "Point.hh"

class Polar {
public:
 Polar();
 Polar(Point* p);
 virtual ~Polar() {};
 Point to_Point();
 void print();
//private:
 float r;
 float theta;
 float phi;
};
#endif
