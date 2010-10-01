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
 * These functions do the obvious --
 * they return (vector-wise) the max or min values of the array vectors.
 *				-- Bernd Feige 30.09.1993
 */

#include "array.h"

GLOBAL DATATYPE
array_max(array *thisarray) {
 DATATYPE amax=array_scan(thisarray), hold;

 while (thisarray->message==ARRAY_CONTINUE) {
  hold=array_scan(thisarray);
  if (hold>amax) amax=hold;
 }

 return amax;
}

GLOBAL DATATYPE
array_min(array *thisarray) {
 DATATYPE amin=array_scan(thisarray), hold;

 while (thisarray->message==ARRAY_CONTINUE) {
  hold=array_scan(thisarray);
  if (hold<amin) amin=hold;
 }

 return amin;
}
