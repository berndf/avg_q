/*
 * Copyright (C) 1993,1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_scan is a utility to uniformly scan an array in the order of
 * elements and vectors. (NOTE: use array_transpose to swap the order
 * of these two).
 */

#include <stdio.h>
#include <array.h>

/*{{{}}}*/
/*{{{  array_nextvector(array *thisarray) {*/
GLOBAL void
array_nextvector(array *thisarray) {
 if (++thisarray->current_vector>=thisarray->nr_of_vectors) {
  thisarray->current_vector=0;
  thisarray->message=ARRAY_ENDOFSCAN;
 } else {
  thisarray->message=ARRAY_ENDOFVECTOR;
 }
 thisarray->current_element=0;
}
/*}}}  */

/*{{{  array_previousvector(array *thisarray) {*/
GLOBAL void
array_previousvector(array *thisarray) {
 if (--thisarray->current_vector<0) {
  thisarray->current_vector=thisarray->nr_of_vectors-1;
  thisarray->message=ARRAY_ENDOFSCAN;
 } else {
  thisarray->message=ARRAY_ENDOFVECTOR;
 }
 thisarray->current_element=0;
}
/*}}}  */

/*{{{  array_advance(array *thisarray) {*/
GLOBAL void
array_advance(array *thisarray) {
 if (++thisarray->current_element>=thisarray->nr_of_elements) {
  array_nextvector(thisarray);
 } else {
  thisarray->message=ARRAY_CONTINUE;
 }
}
/*}}}  */

/*{{{  array_stepback(array *thisarray) {*/
GLOBAL void
array_stepback(array *thisarray) {
 if (--thisarray->current_element<0) {
  array_previousvector(thisarray);
  thisarray->current_element=thisarray->nr_of_elements-1;
 } else {
  thisarray->message=ARRAY_CONTINUE;
 }
}
/*}}}  */

/*{{{  array_scan(array *thisarray) {*/
GLOBAL DATATYPE
array_scan(array *thisarray) {
 DATATYPE element;

 element=READ_ELEMENT(thisarray);
 array_advance(thisarray);

 return element;
}
/*}}}  */

/*{{{  array_write(array *thisarray, DATATYPE value) {*/
GLOBAL void
array_write(array *thisarray, DATATYPE value) {
 WRITE_ELEMENT(thisarray, value);
 array_advance(thisarray);
}
/*}}}  */
