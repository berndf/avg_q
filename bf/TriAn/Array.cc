// Copyright (C) 1996 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

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
