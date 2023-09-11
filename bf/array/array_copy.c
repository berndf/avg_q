/*
 * Copyright (C) 1993,1994 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
