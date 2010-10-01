/*
 * Routine to sort the vectors of array v in descending order of the
 * elements of vector d, (not only) for eigenvalues/vectors
 * Adapted from the Numerical Recipes (2.0)
 *					-- Bernd Feige 17.09.1993
 */

#include "array.h"

GLOBAL void
array_eigsrt(array *d, array *v) {
 int n=d->nr_of_elements,k,j,i;
 DATATYPE p,hold;

 for (i=0;i<n-1;i++) {
  k=i;
  d->current_element=i;
  p=READ_ELEMENT(d);
  for (j=i+1;j<n;j++) {
   d->current_element=j;
   hold=READ_ELEMENT(d);
   if (hold >= p) {
    k=j;
    p=hold;
   }
  }
  if (k != i) {
   d->current_element=i;
   hold=READ_ELEMENT(d);
   d->current_element=k;
   WRITE_ELEMENT(d, hold);
   d->current_element=i;
   WRITE_ELEMENT(d, p);
   for (j=0;j<n;j++) {
    v->current_element=j;
    v->current_vector=i;
    p=READ_ELEMENT(v);
    v->current_vector=k;
    hold=READ_ELEMENT(v);
    v->current_vector=i;
    WRITE_ELEMENT(v, hold);
    v->current_vector=k;
    WRITE_ELEMENT(v, p);
   }
  }
 }
}
