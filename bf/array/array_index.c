/*
 * array_index.c - Return an index array which reflects the ordering
 *  by size of the elements in the input array. The latter is only
 *  read by the program and not modified in any way.
 * Based on array_hpsort.c derived from the Numerical Recipes (2.0).
 *					-- Bernd Feige 18.10.1993
 */

#include <stdlib.h>
#include "array.h"

GLOBAL void
array_index(array *a, unsigned long *indx) {
 unsigned long n=a->nr_of_elements,i,ir,j,l,irra;
 DATATYPE rra, hold, hold2;

 for (i=0; i<n; i++) indx[i]=i;
 if (n < 2) return;
 l=(n >> 1);
 ir=n-1;
 for (;;) {
  if (l > 0) {
   a->current_element=irra=indx[--l];
   rra=READ_ELEMENT(a);
  } else {
   a->current_element=irra=indx[ir];
   rra=READ_ELEMENT(a);
   indx[ir]=indx[0];
   if (--ir == 0) {
    indx[0]=irra;
    break;
   }
  }
  i=l;
  j=l+l+1;
  while (j <= ir) {
   a->current_element=indx[j];
   hold=READ_ELEMENT(a);
   if (j < ir) {
    a->current_element=indx[j+1];
    hold2=READ_ELEMENT(a);
    if (hold < hold2) {
     j++;
     hold=hold2;
    }
   }
   if (rra < hold) {
    indx[i]=indx[j];
    i=j;
    j+=j+1;
   } else {
    j=ir+1;
   }
  }
  indx[i]=irra;
 }
}

GLOBAL DATATYPE
array_quantile(array *a, DATATYPE q) {
 int const n=a->nr_of_elements;
 unsigned long i1=(n-1)*q, i2=n*q;
 unsigned long *indx;
 DATATYPE d;

 if (i1<0 || i2>=n) {
  if (i1<0 && i2>=0) i1=i2;
  if (i2>=n && i1<n) i2=i1;
  if (i1<0 || i2>=n) return 0.0;
 }
 if ((indx=(unsigned long *)malloc(n*sizeof(unsigned long)))==NULL) return 0.0;
 array_index(a, indx);
 a->current_element=indx[i1];
 d=READ_ELEMENT(a);
 if (i2!=i1) {
  a->current_element=indx[i2];
  d=(d+READ_ELEMENT(a))/2;
 }
 free(indx);
 /* This is cleaner for programs working through data vector for vector: */
 a->current_element=0;
 return d;
}

GLOBAL DATATYPE
array_median(array *a) {
 return array_quantile(a,0.5);
}
