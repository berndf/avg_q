/*
 * LU decomposition of a square array, adapted from
 * the Numerical Recipes 2.0
 * 				-- Bernd Feige 30.09.1993
 *
 * Arguments are the square input array a, which will be replaced
 * by the LU decomposition on output, and the size N vector index,
 * which will keep track of row permutations and is needed by
 * successive processing steps like solving linear equations or
 * computing the inverse of matrix a.
 * The value returned is either +1 or -1 for even (odd) number of
 * permutations, needed by array_det()
 */

#include <math.h>
#include "array.h"

#include <stdio.h>
#define nrerror(a) fprintf(stderr, "%s\n", a);

#define TINY 1.0e-20

GLOBAL int
array_ludcmp(array *a, array *indx) {
 int i,imax,j,k,d,n=a->nr_of_elements;
 DATATYPE big,dum,sum,temp,hold;
 array vv;

 vv.nr_of_elements=n;
 vv.nr_of_vectors=1;
 vv.element_skip=1;
 if (array_allocate(&vv)==NULL) {
  a->message=ARRAY_ERROR;
  return 0;
 }
 d=1;
 for (i=0; i<n; i++) {
  a->current_element=i;
  big=0.0;
  for (j=0; j<n; j++) {
   a->current_vector=j;
   if ((temp=fabs(READ_ELEMENT(a)))>big) big=temp;
  }
  if (big == 0.0) nrerror("Singular matrix in routine ludcmp");
  array_write(&vv, 1.0/big);
 }
 for (j=0;j<n;j++) {
  for (i=0;i<j;i++) {
   a->current_element=i;
   a->current_vector=j;
   sum=READ_ELEMENT(a);
   for (k=0;k<i;k++) {
    a->current_element=i;
    a->current_vector=k;
    hold=READ_ELEMENT(a);
    a->current_element=k;
    a->current_vector=j;
    sum -= hold*READ_ELEMENT(a);
   }
   a->current_element=i;
   WRITE_ELEMENT(a, sum);
  }
  big=0.0;
  for (i=j;i<n;i++) {
   a->current_element=i;
   a->current_vector=j;
   sum=READ_ELEMENT(a);
   for (k=0;k<j;k++) {
    a->current_vector=k;
    hold=READ_ELEMENT(a);
    a->current_element=k;
    a->current_vector=j;
    sum -= hold*READ_ELEMENT(a);
    a->current_element=i;
   }
   WRITE_ELEMENT(a, sum);
   vv.current_element=i;
   if ( (dum=READ_ELEMENT(&vv)*fabs(sum)) >= big) {
    big=dum;
    imax=i;
   }
  }
  if (j != imax) {
   a->current_element=j;
   for (k=0;k<n;k++) {
    a->current_vector=k;
    hold=READ_ELEMENT(a);
    a->current_element=imax;
    dum=READ_ELEMENT(a);
    WRITE_ELEMENT(a, hold);
    a->current_element=j;
    WRITE_ELEMENT(a, dum);
   }
   d = -d;
   vv.current_element=j;
   hold=READ_ELEMENT(&vv);
   vv.current_element=imax;
   WRITE_ELEMENT(&vv, hold);
  }
  array_write(indx, imax);
  a->current_element=a->current_vector=j;
  if (READ_ELEMENT(a) == 0.0) WRITE_ELEMENT(a, TINY);
  if (j != n-1) {
   dum=1.0/READ_ELEMENT(a);
   a->current_element=j+1;
   array_scale(a, dum);
  }
 }
 array_free(&vv);

 return d;
}
#undef TINY
