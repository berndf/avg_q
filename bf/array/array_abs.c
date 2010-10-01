/*
 * Copyright (C) 1994 Bernd Feige
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
 * array_abs functions to return the square and the absolute value of the
 *  next vector in the array
 *					-- Bernd Feige 24.01.1994
 */

#include <math.h>
#include "array.h"

GLOBAL DATATYPE
array_square(array *thisarray) {
 DATATYPE sumsq=0.0, hold;

 do {
  hold=array_scan(thisarray);
  sumsq+=hold*hold;
 } while (thisarray->message==ARRAY_CONTINUE);

 return (sumsq);
}

GLOBAL DATATYPE
array_abs(array *thisarray) {
 return sqrt(array_square(thisarray));
}
