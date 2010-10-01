/*
 * array_hpsort.c - Heap sort an array.
 * Derived from Numerical Recipes (2.0)
 *    -- Bernd Feige 18.10.1993
 */

#include "array.h"

GLOBAL void
array_hpsort(array *a) {
 unsigned long n=a->nr_of_elements,i,ir,j,l;
 DATATYPE rra, hold, hold2;

 if (n < 2) return;
 l=(n >> 1);
 ir=n-1;
 for (;;) {
  if (l > 0) {
   a->current_element= --l;
   rra=READ_ELEMENT(a);
  } else {
   a->current_element=0;
   hold=READ_ELEMENT(a);
   a->current_element=ir;
   rra=READ_ELEMENT(a);
   WRITE_ELEMENT(a, hold);
   if (--ir == 0) {
    a->current_element=0;
    WRITE_ELEMENT(a, rra);
    break;
   }
  }
  i=l;
  j=l+l+1;
  while (j <= ir) {
   a->current_element=j;
   hold=READ_ELEMENT(a);
   a->current_element=j+1;
   hold2=READ_ELEMENT(a);
   if (j < ir && hold < hold2) {
    j++;
    hold=hold2;
   }
   if (rra < hold) {
    a->current_element=i;
    WRITE_ELEMENT(a, hold);
    i=j;
    j+=j+1;
   } else {
    j=ir+1;
   }
  }
  a->current_element=i;
  WRITE_ELEMENT(a, rra);
 }
}
