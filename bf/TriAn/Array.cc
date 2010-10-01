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
#include "Array.hh"

Array::Array(int nr_of_vectors, int nr_of_elements, int element_skip) {
 thisarray.nr_of_vectors=nr_of_vectors;
 thisarray.nr_of_elements=nr_of_elements;
 thisarray.element_skip=element_skip;
 array_allocate(&thisarray);
}
Array::Array(int nr_of_vectors, int nr_of_elements) {
 Array(nr_of_vectors, nr_of_elements, 1);
}
Array::Array(Point* p) {
 Array(1, 3, 1);
 write(p->x);
 write(p->y);
 write(p->z);
}
Array::~Array() {
 array_free(&thisarray);
}
void Array::write(DATATYPE f) {
 array_write(&thisarray, f);
}
void Array::write(Point *p) {
 array_write(&thisarray, p->x);
 array_write(&thisarray, p->y);
 array_write(&thisarray, p->z);
}
DATATYPE Array::scan() {
 return array_scan(&thisarray);
}
Point Array::scanpoint() {
 Point p;
 p.x=array_scan(&thisarray);
 p.y=array_scan(&thisarray);
 p.z=array_scan(&thisarray);
 return p;
}
