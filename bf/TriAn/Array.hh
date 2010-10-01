// Copyright (C) 1996 Bernd Feige
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
