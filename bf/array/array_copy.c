/*
 * Copyright (C) 1993,1994 Bernd Feige
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
 * array_copy.c function to copy the contents of the first array into
 * the second.
 *					-- Bernd Feige 20.09.1993
 */

#include "array.h"

GLOBAL void
array_copy(array *array1, array *array2) {
 do {
  array_write(array2, array_scan(array1));
 } while (array1->message!=ARRAY_ENDOFSCAN && array2->message!=ARRAY_ENDOFSCAN);
}
