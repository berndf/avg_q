/*
 * array_ranks.c - output the ranks of the numbers within an array.
 *					-- Bernd Feige 19.10.1993
 *
 * Ranks start with 1. If there are n equal numbers, they are all assigned
 * the rank number that is the average of the rank range they occupy.
 * eg [3 2 2] will have the ranks [3 1.5 1.5].
 */

#include <stdio.h>
#include <stdlib.h>
#include "array.h"

GLOBAL void
array_ranks(array *a, array *ranks) {
 DATATYPE val, lastval, valsum=0.0;
 unsigned long *indx;
 int i,j,nval=0,n=a->nr_of_elements;

 if ((indx=(unsigned long *)malloc(n*sizeof(unsigned long)))==NULL) return;
 array_index(a, indx);

 for (i=0; i<n; i++) {
  a->current_element=indx[i];
  val=READ_ELEMENT(a);
  if (nval==0 || (nval>0 && val==lastval)) {
   valsum+=i+1;
   nval++;
  } else {
   /*{{{}}}*/
   /*{{{  Output the rank numbers for this set of equal numbers*/
   valsum/=nval;
   for (j=i-nval; j<i; j++) {
    ranks->current_element=indx[j];
    WRITE_ELEMENT(ranks, valsum);
   }
   /*}}}  */
   valsum=i+1;
   nval=1;
  }
  lastval=val;
 }
 if (nval>0) {
  /*{{{  Output the rank numbers for the remaining set of equal numbers*/
  valsum/=nval;
  for (j=i-nval; j<i; j++) {
   ranks->current_element=indx[j];
   WRITE_ELEMENT(ranks, valsum);
  }
  /*}}}  */
 }
 free(indx);
}
