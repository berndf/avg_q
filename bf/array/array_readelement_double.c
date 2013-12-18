/*
 * Copyright (C) 1993,1995,1997 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_readelement_double.c functions to read one element from a memory-contained
 * array of double values. This is a bit faky to be able to access double existing
 * double (long float) arrays using the array mechanism.
 * 						-- Bernd Feige 14.09.1993
 */

#include "array.h"

#define ARRAY_DELEMENT(arr) (double *)((*(arr)->element_address)(arr))
/*{{{}}}*/
/*{{{  element_address(array *thisarray) {*/
LOCAL DATATYPE *
element_address(array *thisarray) {
 return (DATATYPE *)(((double *)thisarray->start)+thisarray->current_vector*thisarray->vector_skip+thisarray->current_element*thisarray->element_skip);
}
/*}}}  */

/*{{{  array_readelement_double(array *thisarray) {*/
LOCAL DATATYPE
array_readelement_double(array *thisarray) {
 return *ARRAY_DELEMENT(thisarray);
}
/*}}}  */

/*{{{  array_writeelement_double(array *thisarray) {*/
LOCAL void
array_writeelement_double(array *thisarray, DATATYPE value) {
 *ARRAY_DELEMENT(thisarray)=value;
}
/*}}}  */
#undef ARRAY_DELEMENT

/*{{{  array_setreadwrite_double(array *thisarray) {*/
GLOBAL void
array_setreadwrite_double(array *thisarray) {
 thisarray->element_address= &element_address;
 thisarray->read_element= &array_readelement_double;
 thisarray->write_element= &array_writeelement_double;
}
/*}}}  */
