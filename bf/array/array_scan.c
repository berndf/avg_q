/*
 * Copyright (C) 1993,1994 Bernd Feige
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
