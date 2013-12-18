/*
 * Copyright (C) 1993 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_transpose transposes an array in memory, merely by swapping
 * the definitions for vector and element skips.
 * 					-- Bernd Feige 13.09.1993
 */

#include "array.h"

#define SWAP(a, b) temp=a; a=b; b=temp

GLOBAL void
array_transpose(array *thisarray) {
 int temp;

 SWAP(thisarray->element_skip, thisarray->vector_skip);
 SWAP(thisarray->nr_of_elements, thisarray->nr_of_vectors);
 SWAP(thisarray->current_element, thisarray->current_vector);
}
