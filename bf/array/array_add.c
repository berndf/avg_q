/*
 * Copyright (C) 1993 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
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
