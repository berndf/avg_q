/*
 * Solve a linear set of equations using LU decomposition.
 * Adapted from the Numerical Recipes (2.0).
 *				-- Bernd Feige 30.09.1993
 *
 * Input is the output of array_ludcmp (decomposed array a,
 * row permutation array indx) and the inhomogenity vector
 * b. On output, b is replaced by the solution vector x.
 */

#include "array.h"

GLOBAL void
array_lubksb(array *a, array *indx, array *b) {
 int i,ii= -1,j,n=a->nr_of_elements;
 DATATYPE sum, hold;

 for (i=0;i<n;i++) {
  b->current_element=i;
  hold=READ_ELEMENT(b);
  b->current_element=(int)array_scan(indx);
  sum=READ_ELEMENT(b);
  WRITE_ELEMENT(b, hold);
  if (ii>=0) {
   b->current_element=ii;
   a->current_element=i;
   for (j=ii;j<i;j++) {
    a->current_vector=j;
    sum -= READ_ELEMENT(a)*array_scan(b);
   }
  } else if (sum) ii=i;
  b->current_element=i;
  WRITE_ELEMENT(b, sum);
 }
 for (i=n-1;i>=0;i--) {
  a->current_element=b->current_element=i;
  sum=array_scan(b);
  for (j=i+1;j<n;j++) {
   a->current_vector=j;
   sum -= READ_ELEMENT(a)*array_scan(b);
  }
  a->current_vector=b->current_element=i;
  WRITE_ELEMENT(b, sum/READ_ELEMENT(a));
 }
}
