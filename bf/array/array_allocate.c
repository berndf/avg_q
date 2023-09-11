/*
 * Copyright (C) 1993,1994,2001 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_allocate.c function to allocate the memory storage sufficient to
 * hold the number of memory elements defined in the array header
 * NOTE -- element_skip has to be defined and gives the size of one
 * element in DATATYPE units		-- Bernd Feige 14.09.1993
 */

#include <stdlib.h>
#include "array.h"

GLOBAL DATATYPE *
array_allocate(array *thisarray) {
 DATATYPE *newstart;

 if ((newstart=(DATATYPE *)calloc(thisarray->nr_of_vectors*thisarray->nr_of_elements, thisarray->element_skip*sizeof(DATATYPE)))!=NULL) {
  thisarray->start=newstart;
  thisarray->vector_skip=thisarray->nr_of_elements*thisarray->element_skip;
  array_setreadwrite(thisarray);
  array_reset(thisarray);
 } else {
  thisarray->message=ARRAY_ERROR;
 }

 return newstart;
}

GLOBAL void
array_free(array *thisarray) {
 array_reset(thisarray);
 if (thisarray->start!=NULL) {
  free(thisarray->start);
  thisarray->start=NULL;
 }
 thisarray->read_element=NULL;
 thisarray->write_element=NULL;
}
