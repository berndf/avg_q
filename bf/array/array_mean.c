/*
 * Copyright (C) 1993,1996 Bernd Feige
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
 * array_mean function to return the mean of the next vector in the array
 *					-- Bernd Feige 16.09.1993
 */

#include "array.h"

GLOBAL DATATYPE
array_sum(array *thisarray) {
 DATATYPE sum=0.0;

 do {
  sum+=array_scan(thisarray);
 } while (thisarray->message==ARRAY_CONTINUE);

 return sum;
}

GLOBAL DATATYPE
array_mean(array *thisarray) {
 return (array_sum(thisarray)/thisarray->nr_of_elements);
}
