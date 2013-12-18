/*
 * Copyright (C) 1993,1996 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
