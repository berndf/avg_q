/*
 * array_tred2 - Housholder reduction of a real, symmetric matrix
 * Adapted from the Numerical Recipes (2.0)
 *					-- Bernd Feige 16.09.1993
 *
 * Array a holds the input matrix with nr_of_elements==nr_of_vectors==n;
 * Arrays d and e have to be vectors able to store n elements. None of
 * these requirements is checked.
 * On return, a holds the orthogonal matrix Q effecting the transform,
 * d the diagonal and e the off-diagonal elements of the tridiagonal matrix
 * with e[0]==0.
 */

#include <math.h>
#include "array.h"

GLOBAL void 
array_tred2(array *a, array *d, array *e) {
 int n=a->nr_of_elements,l,k,j,i;
 DATATYPE scale,hh,h,g,f,hold;

 for (i=n-1;i>=1;i--) {
  l=i-1;
  h=scale=0.0;
  if (l > 0) {
   /*{{{}}}*/
   /*{{{  */
   a->current_element=i;
   for (k=0;k<=l;k++) {
    a->current_vector=k;
    scale += fabs(READ_ELEMENT(a));
   }
   if (scale == 0.0) {
    a->current_vector=l;
    e->current_element=i;
    WRITE_ELEMENT(e, READ_ELEMENT(a));
   } else {
    /*{{{  */
    a->current_element=i;
    for (k=0;k<=l;k++) {
     a->current_vector=k;
     WRITE_ELEMENT(a, hold=READ_ELEMENT(a)/scale);
     h += hold*hold;
    }
    a->current_vector=l;
    f=READ_ELEMENT(a);
    g=(f >= 0.0 ? -sqrt(h) : sqrt(h));
    e->current_element=i;
    WRITE_ELEMENT(e, scale*g);
    h -= f*g;
    WRITE_ELEMENT(a, f-g);
    f=0.0;
    for (j=0;j<=l;j++) {
     a->current_element=i;
     a->current_vector=j;
     hold=READ_ELEMENT(a);
     a->current_element=j;
     a->current_vector=i;
     WRITE_ELEMENT(a, hold/h);
     g=0.0;
     for (k=0;k<=j;k++) {
      a->current_element=j;
      a->current_vector=k;
      hold=READ_ELEMENT(a);
      a->current_element=i;
      a->current_vector=k;
      g += hold*READ_ELEMENT(a);
     }
     for (k=j+1;k<=l;k++) {
      a->current_element=k;
      a->current_vector=j;
      hold=READ_ELEMENT(a);
      a->current_element=i;
      a->current_vector=k;
      g += hold*READ_ELEMENT(a);
     }
     e->current_element=j;
     WRITE_ELEMENT(e, hold=g/h);
     a->current_element=i;
     a->current_vector=j;
     f += hold*READ_ELEMENT(a);
    }
    hh=f/(h+h);
    for (j=0;j<=l;j++) {
     a->current_element=i;
     a->current_vector=j;
     f=READ_ELEMENT(a);
     e->current_element=j;
     g=READ_ELEMENT(e)-hh*f;
     WRITE_ELEMENT(e, g);
     for (k=0;k<=j;k++) {
      a->current_element=i;
      a->current_vector=k;
      hold=READ_ELEMENT(a);
      a->current_element=j;
      a->current_vector=k;
      e->current_element=k;
      hold=f*READ_ELEMENT(e)+g*hold;
      WRITE_ELEMENT(a, READ_ELEMENT(a)-hold);
     }
    }
    /*}}}  */
   }
   /*}}}  */
  } else {
   /*{{{  */
   e->current_element=a->current_element=i;
   a->current_vector=l;
   WRITE_ELEMENT(e, READ_ELEMENT(a));
   /*}}}  */
  }
  d->current_element=i;
  WRITE_ELEMENT(d, h);
 }
 d->current_element=0;
 WRITE_ELEMENT(d, 0.0);
 e->current_element=0;
 WRITE_ELEMENT(e, 0.0);

 for (i=0;i<n;i++) {
  l=i-1;
  d->current_element=i;
  if (READ_ELEMENT(d)) {
   for (j=0;j<=l;j++) {
    g=0.0;
    for (k=0;k<=l;k++) {
     a->current_element=i;
     a->current_vector=k;
     hold=READ_ELEMENT(a);
     a->current_element=k;
     a->current_vector=j;
     g += hold*READ_ELEMENT(a);
    }
    for (k=0;k<=l;k++) {
     a->current_element=k;
     a->current_vector=i;
     hold=READ_ELEMENT(a);
     a->current_element=k;
     a->current_vector=j;
     WRITE_ELEMENT(a, READ_ELEMENT(a)-g*hold);
    }
   }
  }
  a->current_element=i;
  a->current_vector=i;
  WRITE_ELEMENT(d, READ_ELEMENT(a));
  WRITE_ELEMENT(a, 1.0);
  for (j=0;j<=l;j++) {
   a->current_element=j;
   a->current_vector=i;
   WRITE_ELEMENT(a, 0.0);
   a->current_element=i;
   a->current_vector=j;
   WRITE_ELEMENT(a, 0.0);
  }
 }
}
