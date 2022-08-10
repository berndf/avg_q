// Copyright (C) 1996,2010,2013-2015 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#ifndef _Array_HH
#define _Array_HH

#include "Point.hh"

extern "C" {
#include "array.h"
}

class Array {
public:
 Array(int nr_of_vectors, int nr_of_elements, int element_skip);
 Array(int nr_of_vectors, int nr_of_elements);
 Array(Point* p);
 ~Array();
 void write(DATATYPE f);
 void write(Point* p);
 DATATYPE scan();
 Point scanpoint();
private:
 array thisarray;
};

#endif
