/*
 * Copyright (C) 1993,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_reset.c tiny function to reset the array position counters
 * to the start of the array		-- Bernd Feige 15.09.1993
 *
 */

#include "array.h"

GLOBAL void
array_reset(array *thisarray) {
 thisarray->current_element=thisarray->current_vector=0;
 thisarray->message=ARRAY_CONTINUE;
}
