/*
 * Copyright (C) 1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_make_orthogonal.c function to orthogonalize the vectors in
 * the array. The first vector is left unmodified. If there are
 * linearly dependent vectors, the function will stop and set array1->message
 * to ARRAY_ERROR.
 *
 *					-- Bernd Feige 27.09.1994
 */

#include "array.h"

GLOBAL void
array_make_orthogonal(array *array1) {
 array array2, absvalues;

 array_reset(array1);
 /*{{{}}}*/
 /*{{{  Allocate and set up absvalues array to buffer the vector lengths*/
 absvalues.nr_of_elements=array1->nr_of_vectors;
 absvalues.nr_of_vectors=absvalues.element_skip=1;
 if (array_allocate(&absvalues)==NULL) {
  array1->message=ARRAY_ERROR; return;
 }

 {
 /* Only the power of the first vector is calculated here, the others will
  * still be modified */
 DATATYPE hold=array_square(array1);
 if (hold==0.0) {
  array_free(&absvalues); array1->message=ARRAY_ERROR; return;
 }
 array_write(&absvalues, hold);
 }
 /*}}}  */

 array2= *array1;	/* A second array on the same data */
 /* Note: array2.current_vector==1 ! */
 while (array2.message!=ARRAY_ENDOFSCAN) {
  DATATYPE abs_result;

  array_reset(array1); array_reset(&absvalues);
  do {
   /*{{{  Orthogonalize array2 to one array1 vector*/
   /* aprod is the projection of the current array2 vector onto the array1
    * direction divided by aabs ready to be multiplied with array1 again */
   DATATYPE aprod=array_multiply(array1, &array2, MULT_VECTOR)/array_scan(&absvalues);
   abs_result=0.0;

   array_previousvector(array1); array_previousvector(&array2);
   do {
    DATATYPE hold=READ_ELEMENT(&array2)-aprod*array_scan(array1);
    abs_result+=hold*hold;
    array_write(&array2, hold);
   } while (array1->message==ARRAY_CONTINUE);
   /*{{{  Is the resulting vector zero ? -> Error*/
   if (abs_result==0.0) {
    array_free(&absvalues); array1->message=ARRAY_ERROR; return;
   }
   /*}}}  */
   array_previousvector(&array2);
   /*}}}  */
  } while (array1->current_vector<array2.current_vector);
  array_write(&absvalues, abs_result);
  array_nextvector(&array2);
 }

 array_free(&absvalues);
 array_reset(array1);
}
