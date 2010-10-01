/*
 * Array inversion using LU decomposition.
 * Cf. Numerical Recipes (2.0), p. 48
 *				-- Bernd Feige 1.10.1993
 */

#include <stdio.h>
#include "array.h"

GLOBAL void
array_inverse(array *a) {
 int i,j,n=a->nr_of_elements;
 array y, indx, col;

 if (n!=a->nr_of_vectors) {
  a->message=ARRAY_ERROR;
  return;
 }
 y.nr_of_elements=y.nr_of_vectors=indx.nr_of_elements=n;
 indx.nr_of_vectors=1;
 y.element_skip=indx.element_skip=1;
 if (array_allocate(&y)==NULL || array_allocate(&indx)==NULL) {
  a->message=ARRAY_ERROR;
  return;
 }

 array_ludcmp(a, &indx);
 for (j=0; j<n; j++) {
  y.current_vector=j;
  col=y;
  array_setto_vector(&col);
  for (i=0; i<n; i++) array_write(&col, (i==j ? 1 : 0));
  array_lubksb(a, &indx, &col);
 }
 array_reset(&y); array_reset(a);
 array_copy(&y, a);
 array_free(&indx); array_free(&y);
}
