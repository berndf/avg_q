/*
 * Copyright (C) 1993,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * array_multiply.c function to scan the next vectors of two arrays and
 * return their scalar product; if the two array pointers are equal,
 * an error results because independent position counters are necessary
 * to track the progress of the operation. If the square of a certain data
 * set has to be calculated, simply copy the array struct pointing to it,
 * transpose the first, and call array_multiply with the two arrays.
 *
 *	M=nr_of_elements, N=nr_of_vectors
 * The three ways of multiplying two matrices are indicated by the variable
 * mult_level. If this is MULT_BLOWUP, the result of each single multiplication
 * is returned, resulting in M1*N1*M2*N2 single elements. If it is
 * MULT_SAMESIZE, then the results of whole vector products is returned,
 * resulting in N1*N2 elements. If it is MULT_CONTRACT, then only a single
 * value is returned, namely the sum of all vector products.
 *
 * A special contraction is MULT_VECTOR, which allows to treat the two incoming
 * arrays as two simple sequences of vectors. The resulting matrix will have
 * size 1xN2.
 *
 * The state of the resulting matrix is indicated by array2->message.
 *
 * NOTE: Since array_multiply always multiplies the input matrices' vectors
 * and matrix products AB with a[vector][element] are defined by
 * (ab)[i][j]=a[k][j]*b[i][k]=a'[j][k]*b[i][k] where A' is the transposed
 * matrix A, we see that A has to be transposed to get a normal matrix product.
 * Put the other way round: array_multiply computes A'B.
 *
 *					-- Bernd Feige 14.09.1993
 */

#include "array.h"
#ifdef ARRAY_BOUNDARIES
#include <stdio.h>
#endif

GLOBAL DATATYPE
array_multiply(array *array1, array *array2, enum mult_levels mult_level) {
 DATATYPE multiple=0.0;

 if (array1==array2) {
  array1->message=ARRAY_ERROR;
  return 0.0;
 }

#ifdef ARRAY_BOUNDARIES
 if (array1->nr_of_elements!=array2->nr_of_elements) {
  fprintf(stderr, "array_multiply: Element counts different, %d!=%d\n", array1->nr_of_elements, array2->nr_of_elements);
 }
#endif

 do {
  do {
   multiple+=array_scan(array1)*array_scan(array2);
  } while ((mult_level==MULT_CONTRACT || mult_level==MULT_SAMESIZE || mult_level==MULT_VECTOR) && array1->message==ARRAY_CONTINUE && array2->message==ARRAY_CONTINUE);
  if (mult_level==MULT_VECTOR) break;

  /*{{{}}}*/
  /*{{{  Redo for each vector in array1 for this vector in array2*/
  if (array1->message==ARRAY_ENDOFVECTOR) {
   if (array2->message==ARRAY_ENDOFVECTOR) {
    array2->current_vector--;
   } else if (array2->message==ARRAY_ENDOFSCAN) {
    /* Have to place it on the last vector again */
    array2->current_vector=array2->nr_of_vectors-1;
   }
   array2->message=ARRAY_CONTINUE;
  }
  /* else implicit: array1->message==ARRAY_ENDOFSCAN and let
   * array2->current_vector drop through */
  /*}}}  */

 } while (mult_level==MULT_CONTRACT && array2->message!=ARRAY_ENDOFSCAN);

 return multiple;
}
