/*
 * Copyright (C) 1993 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
