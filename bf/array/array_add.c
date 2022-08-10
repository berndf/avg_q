/*
 * Copyright (C) 1993,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_add.c function to scan the next elements of two arrays and
 * return their sum.
 *
 * The state of the resulting matrix is indicated by array1->message.
 *
 *					-- Bernd Feige 15.09.1993
 */

#include "array.h"

GLOBAL DATATYPE
array_add(array *array1, array *array2) {
 return array_scan(array1)+array_scan(array2);
}

GLOBAL DATATYPE
array_subtract(array *array1, array *array2) {
 return array_scan(array1)-array_scan(array2);
}
