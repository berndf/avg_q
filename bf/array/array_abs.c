/*
 * Copyright (C) 1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
