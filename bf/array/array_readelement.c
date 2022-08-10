/*
 * Copyright (C) 1993-1995,1997,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_readelement.c function to read one element from a memory-contained
 * array				-- Bernd Feige 14.09.1993
 */

#include <string.h>
#include "array.h"

#ifdef ARRAY_BOUNDARIES
#include <stdio.h>
/*{{{}}}*/
/*{{{  array_check_boundaries(array *thisarray) {*/
LOCAL void
array_check_boundaries(array *thisarray) {
 if (thisarray->current_element>=thisarray->nr_of_elements) {
  fprintf(stderr, "Element out of range !\n");
 }
 if (thisarray->current_vector>=thisarray->nr_of_vectors) {
  fprintf(stderr, "Vector out of range !\n");
 }
}
/*}}}  */
#endif

/*{{{  element_address(array *thisarray) {*/
LOCAL DATATYPE *
element_address(array *thisarray) {
 DATATYPE *address=thisarray->ringstart+thisarray->current_vector*thisarray->vector_skip+thisarray->current_element*thisarray->element_skip;
 if (address>=thisarray->end) {
  address-=thisarray->end-thisarray->start;
 }
 return address;
}
/*}}}  */

/*{{{  array_readelement(array *thisarray) {*/
LOCAL DATATYPE
array_readelement(array *thisarray) {
#ifdef ARRAY_BOUNDARIES
 array_check_boundaries(thisarray);
#endif
 return *ARRAY_ELEMENT(thisarray);
}
/*}}}  */

/*{{{  array_writeelement(array *thisarray, DATATYPE value) {*/
LOCAL void
array_writeelement(array *thisarray, DATATYPE value) {
#ifdef ARRAY_BOUNDARIES
 array_check_boundaries(thisarray);
#endif
 *ARRAY_ELEMENT(thisarray)=value;
}
/*}}}  */

/*{{{  array_use_item(array *thisarray, int item) {*/
GLOBAL void 
array_use_item(array *thisarray, int item) {
 thisarray->ringstart+=(item-thisarray->current_item);
 thisarray->current_item=item;
}
/*}}}  */

/*{{{  array_ringadvance(array *thisarray) {*/
GLOBAL void 
array_ringadvance(array *thisarray) {
 /* This function shifts the ring buffer position forward by 1 along the 'vector' axis,
  * ie it appears as if all vectors were moved down by one that the former 0 vector
  * appears at the highest index. */
 thisarray->current_element=0; thisarray->current_vector=1;
 thisarray->ringstart=ARRAY_ELEMENT(thisarray);

 /* The writing position is set to the 'new', recycled element, because most
  * probably the next action will be a write to this position */
 thisarray->current_vector=thisarray->nr_of_vectors-1;
 thisarray->message=ARRAY_CONTINUE;
}
/*}}}  */

/*{{{  array_setreadwrite(array *thisarray) {*/
GLOBAL void
array_setreadwrite(array *thisarray) {
 thisarray->current_item=0;
 thisarray->ringstart=thisarray->start;
 if (thisarray->vector_skip==thisarray->nr_of_elements*thisarray->element_skip) {
  thisarray->end=thisarray->start+thisarray->nr_of_vectors*thisarray->vector_skip;
 } else {
  thisarray->end=thisarray->start+thisarray->nr_of_elements*thisarray->element_skip;
#ifdef ARRAY_BOUNDARIES
  if (thisarray->element_skip!=thisarray->nr_of_vectors*thisarray->vector_skip) {
   fprintf(stderr, "array_setreadwrite: Inconsistent array detected !\n");
  }
#endif
 }
 thisarray->element_address= &element_address;
 thisarray->read_element= &array_readelement;
 thisarray->write_element= &array_writeelement;
}
/*}}}  */

/*{{{  array_remove_vectorrange(array *thisarray, long vector_from, long vector_to) {*/
/* This function is defined in this place because it depends on the actual
 * representation of the array in memory. The intention is to remove a channel
 * so that the memory size of the array as a whole diminishes correspondingly. */
GLOBAL void
array_remove_vectorrange(array *thisarray, long vector_from, long vector_to) {
 DATATYPE *start=thisarray->start;
 long itemsize=0, element_skip=thisarray->element_skip, vector_skip=thisarray->vector_skip;
 long nr_of_elements=thisarray->nr_of_elements, nr_of_vectors=thisarray->nr_of_vectors;
 long vector_after=vector_to+1, nr_of_removed=vector_after-vector_from;
 int vector_is_row;	/* This means, a vector is stored continuously */

 /*{{{  Find the state of the array (vector_is_row ? errors ?)*/
 thisarray->message=ARRAY_ERROR;
 if (vector_from<0 || vector_to<0 || vector_from>vector_to || vector_to>=nr_of_vectors) return;
 if (vector_skip%nr_of_elements==0) {
  itemsize=vector_skip/nr_of_elements;
  if (element_skip!=itemsize) return;
  vector_is_row=TRUE;
 } else if (element_skip%nr_of_vectors==0) {
  itemsize=element_skip/nr_of_vectors;
  if (vector_skip!=itemsize) return;
  vector_is_row=FALSE;
 } else return;
 /*}}}  */

 if (vector_is_row) {
  /*{{{  This can be done my a single memmove instruction*/
  if (vector_after<nr_of_vectors) {
   memmove((void *)(start+vector_from*vector_skip), (void *)(start+vector_after*vector_skip), (nr_of_vectors-vector_after)*vector_skip*sizeof(DATATYPE));
  }
  nr_of_vectors-=nr_of_removed;
  thisarray->nr_of_vectors=nr_of_vectors;
  /*}}}  */
 } else {
  /*{{{  In each single row, elements belonging to the deleted vectors must be deleted*/
  long row, new_element_skip=element_skip-nr_of_removed*itemsize;
  const long firstmove_size=vector_from*itemsize*sizeof(DATATYPE);
  const long secondmove_size=(nr_of_vectors-vector_after)*itemsize*sizeof(DATATYPE);
  DATATYPE *current_to_position1=start, *current_from_position1=start;
  DATATYPE *current_to_position2=start+vector_from*itemsize, *current_from_position2=start+vector_after*itemsize;

  for (row=0; row<nr_of_elements; row++) {
   /* First move the elements of the vectors lower than vector_from */
   memmove((void *)current_to_position1, (void *)current_from_position1, firstmove_size);
   /* Then the elements of the vectors higher than vector_to */
   memmove((void *)current_to_position2, (void *)current_from_position2, secondmove_size);
   current_from_position1+=element_skip;
   current_from_position2+=element_skip;
   current_to_position1+=new_element_skip;
   current_to_position2+=new_element_skip;
  }
  nr_of_vectors-=nr_of_removed;
  thisarray->nr_of_vectors=nr_of_vectors;
  thisarray->element_skip=new_element_skip;
  /*}}}  */
 }

 thisarray->message=ARRAY_CONTINUE;
}
/*}}}  */
