/*
 * Copyright (C) 1993,1995,1997 Bernd Feige
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
