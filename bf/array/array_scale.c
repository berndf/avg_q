/*
 * Copyright (C) 1993,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_scale.c function to scale (multiply by a scalar) the next vector
 * in an array in-place.
 *					-- Bernd Feige 21.09.1993
 */

#include "array.h"

GLOBAL void
array_scale(array *thisarray, DATATYPE scalar) {
 do {
  array_write(thisarray, READ_ELEMENT(thisarray)*scalar);
 } while (thisarray->message==ARRAY_CONTINUE);
}
