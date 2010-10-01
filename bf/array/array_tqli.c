/*
 * array_tqli -- QL algorithm with implicit shifts for a real, symmetric,
 * tridiagonal matrix. Original from the numerical recipes 2.0.
 *					-- Bernd Feige 17.09.1993
 *
 * The vectors d and e of some dimension N are the diagonal and off-diagonal
 * elements of a tridiagonal matrix as eg. set by array_tred2.
 * The matrix z of dimension NxN holds the identity matrix or the similarity
 * transform returned by array_tred2. On output, d contains the eigenvalues
 * and the vectors of z hold the corresponding eigenvectors.
 * The contents of e is destroyed on output.
 */

#include <math.h>
#include "bfmath.h"
#include "array.h"

#include <stdio.h>
#define nrerror(a) fprintf(stderr, a);
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

GLOBAL void
array_tqli(array *d, array *e, array *z) {
 int n=d->nr_of_elements,m,l,iter,i,k;
 DATATYPE s,r,p,g,f,dd,c,b,hold;

 for (i=1;i<n;i++) {
  e->current_element=i;
  hold=READ_ELEMENT(e);
  e->current_element=i-1;
  WRITE_ELEMENT(e, hold);
 }
 e->current_element=n-1;
 WRITE_ELEMENT(e, 0.0);
 for (l=0;l<n;l++) {
  iter=0;
  do {
   for (m=l;m<n-1;m++) {
    /*{{{}}}*/
    /*{{{  */
    d->current_element=e->current_element=m;
    hold=fabs(READ_ELEMENT(d));
    d->current_element=m+1;
    dd=hold+fabs(READ_ELEMENT(d));
    if ((DATATYPE)(fabs(READ_ELEMENT(e))+dd) == dd) break;
    /*}}}  */
   }
   if (m != l) {
    /*{{{  */
    if (iter++ == 30) nrerror("Too many iterations in tqli");
    d->current_element=l+1;
    hold=READ_ELEMENT(d);
    d->current_element=e->current_element=l;
    g=(hold-READ_ELEMENT(d))/(2.0*READ_ELEMENT(e));
    r=pythag(g,1.0);
    hold=READ_ELEMENT(d);
    d->current_element=m;
    g=READ_ELEMENT(d)-hold+READ_ELEMENT(e)/(g+SIGN(r,g));
    s=c=1.0;
    p=0.0;
    for (i=m-1;i>=l;i--) {
     e->current_element=i;
     hold=READ_ELEMENT(e);
     f=s*hold;
     b=c*hold;
     e->current_element=d->current_element=i+1;
     WRITE_ELEMENT(e, r=pythag(f,g));
     if (r == 0.0) {
      WRITE_ELEMENT(d, READ_ELEMENT(d)-p);
      e->current_element=m;
      WRITE_ELEMENT(e, 0.0);
      break;
     }
     s=f/r;
     c=g/r;
     g=READ_ELEMENT(d)-p;
     d->current_element=i;
     r=(READ_ELEMENT(d)-g)*s+2.0*c*b;
     d->current_element=i+1;
     WRITE_ELEMENT(d, g+(p=s*r));
     g=c*r-b;
     for (k=0;k<n;k++) {
      z->current_element=k;
      z->current_vector=i+1;
      f=READ_ELEMENT(z);
      z->current_vector=i;
      hold=READ_ELEMENT(z);
      z->current_vector=i+1;
      WRITE_ELEMENT(z, s*hold+c*f);
      z->current_vector=i;
      WRITE_ELEMENT(z, c*hold-s*f);
     }
    }
    if (r == 0.0 && i >= l) continue;
    d->current_element=e->current_element=l;
    WRITE_ELEMENT(d, READ_ELEMENT(d)-p);
    WRITE_ELEMENT(e, g);
    e->current_element=m;
    WRITE_ELEMENT(e, 0.0);
    /*}}}  */
   }
  } while (m != l);
 }
}
