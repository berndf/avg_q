/*
 * Copyright (C) 1993,1995,1997,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_setto.c collection of fixed array definitions (function defined
 * arrays, not memory-contained)	-- Bernd Feige 15.09.1993
 */

#include <stdio.h>
#include "array.h"

/*{{{}}}*/
/*{{{  array_setto_identity*/
LOCAL DATATYPE
array_identity(array *thisarray) {
 return (thisarray->current_element==thisarray->current_vector ? 1.0 : 0.0);
}

GLOBAL void
array_setto_identity(array *thisarray) {
 thisarray->element_address=NULL;
 thisarray->read_element= &array_identity;
 thisarray->write_element=NULL;
 thisarray->start=NULL;
 thisarray->message=ARRAY_CONTINUE;
}
/*}}}  */

/*{{{  array_setto_null*/
LOCAL DATATYPE
array_null(array *thisarray) {
 return 0.0;
}

GLOBAL void
array_setto_null(array *thisarray) {
 thisarray->element_address=NULL;
 thisarray->read_element= &array_null;
 thisarray->write_element=NULL;
 thisarray->message=ARRAY_CONTINUE;
}
/*}}}  */

/*{{{  array_setto_diagonal*/
/* The setto_diagonal package effectively transforms a vector into
 * a diagonal array. element_skip has to be set as well as start;
 * nr_of_vectors is set to nr_of_elements.
 * This diagonal array can also be written to. Writes to non-diagonal elements
 * are ignored.
 */
LOCAL DATATYPE *
diagonal_element_address(array *thisarray) {
 return thisarray->start+thisarray->current_element*thisarray->element_skip;
}
LOCAL DATATYPE
array_read_diagonal(array *thisarray) {
 if (thisarray->current_element==thisarray->current_vector) {
  return *(*thisarray->element_address)(thisarray);
 } else {
  return 0.0;
 }
}
LOCAL void
array_write_diagonal(array *thisarray, DATATYPE value) {
 if (thisarray->current_element==thisarray->current_vector) {
  *(*thisarray->element_address)(thisarray)=value;
 }
}

GLOBAL void
array_setto_diagonal(array *thisarray) {
 thisarray->element_address= &diagonal_element_address;
 thisarray->read_element= &array_read_diagonal;
 thisarray->write_element= &array_write_diagonal;
 thisarray->nr_of_vectors=thisarray->nr_of_elements;
}
/*}}}  */

/*{{{  array_setto_vector*/
/* array_setto_vector works on memory arrays; it sets the array start
 * and vector count so that the current vector at the time of call
 * is the only vector in the array on return.
 */
GLOBAL void
array_setto_vector(array *thisarray) {
 if (thisarray->start==NULL) {
  thisarray->message=ARRAY_ERROR;
  return;
 }
 thisarray->current_element=0;
 thisarray->ringstart=ARRAY_ELEMENT(thisarray);
 thisarray->current_vector=0;
 thisarray->nr_of_vectors=1;
 thisarray->message=ARRAY_CONTINUE;
}
/*}}}  */
