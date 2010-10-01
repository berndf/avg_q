/*
 * Copyright (C) 1993,1994,2001 Bernd Feige
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
