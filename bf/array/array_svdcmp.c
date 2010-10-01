/*
 * Singular Value Decomposition of an arbitrary matrix of M elements x N
 * vectors to an orthonormal column matrix U of the same size, a diagonal
 * matrix w (here: The vector of its diagonal elements, the singular values)
 * and a square orthogonal matrix v of dimension NxN such that
 * A=U W V', where ' means transposition.
 * Adapted from the Numerical Recipes 2.0
 *					-- Bernd Feige 24.09.1993
 */

#include <math.h>
#include <stdio.h>
#include <bfmath.h>
#include "array.h"

#define nrerror(a) fprintf(stderr, "%s\n", a)
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))
#define IMIN(a,b) ((a)<(b) ? (a) : (b))

GLOBAL void
array_svdcmp(array *a, array *w, array *v) {
 int flag,i,its,j,jj,k,l,nm;
 int m=a->nr_of_elements, n=a->nr_of_vectors;
 DATATYPE anorm,c,f,g,h,s,scale,x,y,z,hold;
 array rv1;

 rv1.nr_of_vectors=rv1.element_skip=1;
 rv1.nr_of_elements=n;
 if (array_allocate(&rv1)==NULL) nrerror("Error allocating memory in array_svdcmp");

 g=scale=anorm=0.0;
 for (i=0;i<n;i++) {
  /*{{{}}}*/
  /*{{{  */
  l=i+1;
  rv1.current_element=i;
  WRITE_ELEMENT(&rv1, scale*g);

  g=s=scale=0.0;
  if (i < m) {
   a->current_element=a->current_vector=i;
   do {
    scale += fabs(array_scan(a));
   } while (a->message==ARRAY_CONTINUE);
   if (scale) {
    a->current_element=a->current_vector=i;
    do {
     hold=READ_ELEMENT(a)/scale;
     array_write(a, hold);
     s+=hold*hold;
    } while (a->message==ARRAY_CONTINUE);
    a->current_element=a->current_vector=i;
    f=READ_ELEMENT(a);
    g = -SIGN(sqrt(s),f);
    h=f*g-s;
    WRITE_ELEMENT(a, f-g);
    for (j=l;j<n;j++) {
     for (s=0.0,k=i;k<m;k++) {
      a->current_element=k;
      a->current_vector=i;
      hold=READ_ELEMENT(a);
      a->current_vector=j;
      s += hold*READ_ELEMENT(a);
     }
     f=s/h;
     for (k=i;k<m;k++) {
      a->current_element=k;
      a->current_vector=i;
      hold=READ_ELEMENT(a);
      a->current_vector=j;
      WRITE_ELEMENT(a, READ_ELEMENT(a)+f*hold);
     }
    }
    a->current_element=a->current_vector=i;
    array_scale(a, scale);
   }
  }
  w->current_element=i;
  WRITE_ELEMENT(w, scale*g);
  array_transpose(a);
  g=s=scale=0.0;
  if (i < m && i != n-1) {
   a->current_element=l;
   a->current_vector=i;
   do {
    scale += fabs(array_scan(a));
   } while (a->message==ARRAY_CONTINUE);
   if (scale) {
    a->current_element=l;
    a->current_vector=i;
    do {
     hold=READ_ELEMENT(a)/scale;
     array_write(a, hold);
     s+=hold*hold;
    } while (a->message==ARRAY_CONTINUE);
    a->current_element=l;
    a->current_vector=i;
    f=READ_ELEMENT(a);
    g = -SIGN(sqrt(s),f);
    h=f*g-s;
    rv1.current_element=a->current_element=l;
    a->current_vector=i;
    WRITE_ELEMENT(a, f-g);
    do {
     array_write(&rv1, array_scan(a)/h);
    } while (a->message==ARRAY_CONTINUE);
    for (j=l;j<m;j++) {
     for (s=0.0,k=l;k<n;k++) {
      a->current_element=k;
      a->current_vector=i;
      hold=READ_ELEMENT(a);
      a->current_vector=j;
      s += hold*READ_ELEMENT(a);
     }
     rv1.current_element=a->current_element=l;
     a->current_vector=j;
     do {
      array_write(a, READ_ELEMENT(a)+array_scan(&rv1)*s);
     } while (a->message==ARRAY_CONTINUE);
    }
    a->current_element=l;
    a->current_vector=i;
    array_scale(a, scale);
   }
  }
  w->current_element=rv1.current_element=i;
  hold=fabs(READ_ELEMENT(w))+fabs(READ_ELEMENT(&rv1));
  if (hold>anorm) anorm=hold;
  array_transpose(a);
  /*}}}  */
 }
 array_transpose(a);
 for (i=n-1;i>=0;i--) {
  /*{{{  Accumulation of right-hand transformations*/
  if (i < n-1) {
   if (g) {
    v->current_element=a->current_element=l;
    v->current_vector=a->current_vector=i;
    hold=READ_ELEMENT(a);
    do {
     array_write(v, (array_scan(a)/hold)/g);
    } while (a->message==ARRAY_CONTINUE);
    for (j=l;j<n;j++) {
     v->current_element=a->current_element=l;
     v->current_vector=j;
     a->current_vector=i;
     s=0.0;
     do {
      s += array_scan(a)*array_scan(v);
     } while (a->message==ARRAY_CONTINUE);
     for (k=l;k<n;k++) {
      v->current_element=k;
      v->current_vector=i;
      hold=READ_ELEMENT(v);
      v->current_vector=j;
      WRITE_ELEMENT(v, READ_ELEMENT(v)+s*hold);
     }
    }
   }
   for (j=l;j<n;j++) {
    v->current_element=i;
    v->current_vector=j;
    WRITE_ELEMENT(v, 0.0);
    v->current_element=j;
    v->current_vector=i;
    WRITE_ELEMENT(v, 0.0);
   }
  }
  v->current_element=i;
  v->current_vector=i;
  WRITE_ELEMENT(v, 1.0);
  rv1.current_element=i;
  g=READ_ELEMENT(&rv1);
  l=i;
  /*}}}  */
 }
 array_transpose(a);
 for (i=IMIN(m,n)-1;i>=0;i--) {
  /*{{{  Accumulation of left-hand transformations*/
  l=i+1;
  w->current_element=i;
  g=READ_ELEMENT(w);
  a->current_element=i;
  for (j=l; j<n; j++) {
   a->current_vector=j;
   WRITE_ELEMENT(a, 0.0);
  }
  if (g) {
   g=1.0/g;
   for (j=l;j<n;j++) {
    for (s=0.0,k=l;k<m;k++) {
     a->current_element=k;
     a->current_vector=i;
     hold=READ_ELEMENT(a);
     a->current_vector=j;
     s += hold*READ_ELEMENT(a);
    }
    a->current_element=a->current_vector=i;
    f=(s/READ_ELEMENT(a))*g;
    for (k=i;k<m;k++) {
     a->current_element=k;
     a->current_vector=i;
     hold=READ_ELEMENT(a);
     a->current_vector=j;
     WRITE_ELEMENT(a, READ_ELEMENT(a)+f*hold);
    }
   }
   a->current_element=a->current_vector=i;
   array_scale(a, g);
  } else {
   a->current_element=a->current_vector=i;
   do {
    WRITE_ELEMENT(a, 0.0);
   } while(a->message==ARRAY_CONTINUE);
  }
  a->current_element=a->current_vector=i;
  WRITE_ELEMENT(a, READ_ELEMENT(a)+1.0);
  /*}}}  */
 }
 for (k=n-1;k>=0;k--) {
  /*{{{  */
  for (its=1;its<=30;its++) {
   flag=1;
   for (l=k;l>=0;l--) {
    nm=l-1;
    rv1.current_element=l;
    if ((DATATYPE)(fabs(READ_ELEMENT(&rv1))+anorm) == anorm) {
     flag=0;
     break;
    }
    w->current_element=nm;
    if ((DATATYPE)(fabs(READ_ELEMENT(w))+anorm) == anorm) break;
   }
   if (flag) {
    c=0.0;
    s=1.0;
    for (i=l;i<=k;i++) {
     w->current_element=rv1.current_element=i;
     hold=READ_ELEMENT(&rv1);
     f=s*hold;
     WRITE_ELEMENT(&rv1, c*hold);
     if ((DATATYPE)(fabs(f)+anorm) == anorm) break;
     g=READ_ELEMENT(w);
     h=pythag(f,g);
     WRITE_ELEMENT(w, h);
     h=1.0/h;
     c=g*h;
     s = -f*h;
     for (j=0;j<m;j++) {
      a->current_element=j;
      a->current_vector=nm;
      y=READ_ELEMENT(a);
      a->current_vector=i;
      z=READ_ELEMENT(a);
      WRITE_ELEMENT(a, z*c-y*s);
      a->current_vector=nm;
      WRITE_ELEMENT(a, y*c+z*s);
     }
    }
   }
   w->current_element=k;
   z=READ_ELEMENT(w);
   if (l == k) {
    if (z < 0.0) {
     WRITE_ELEMENT(w, -z);
     v->current_element=0;
     v->current_vector=k;
     do {
      array_write(v, -READ_ELEMENT(v));
     } while (v->message==ARRAY_CONTINUE);
    }
    break;
   }
   if (its == 30) nrerror("no convergence in 30 svdcmp iterations");
   w->current_element=l;
   x=READ_ELEMENT(w);
   nm=k-1;
   w->current_element=rv1.current_element=nm;
   y=READ_ELEMENT(w);
   g=READ_ELEMENT(&rv1);
   rv1.current_element=k;
   h=READ_ELEMENT(&rv1);
   f=((y-z)*(y+z)+(g-h)*(g+h))/(2.0*h*y);
   g=pythag(f,1.0);
   f=((x-z)*(x+z)+h*((y/(f+SIGN(g,f)))-h))/x;
   c=s=1.0;
   for (j=l;j<=nm;j++) {
    i=j+1;
    w->current_element=rv1.current_element=i;
    y=READ_ELEMENT(w);
    g=READ_ELEMENT(&rv1);
    h=s*g;
    g=c*g;
    z=pythag(f,h);
    rv1.current_element=j;
    WRITE_ELEMENT(&rv1, z);
    c=f/z;
    s=h/z;
    f=x*c+g*s;
    g = g*c-x*s;
    h=y*s;
    y *= c;
    for (jj=0;jj<n;jj++) {
     v->current_element=jj;
     v->current_vector=j;
     x=READ_ELEMENT(v);
     v->current_vector=i;
     z=READ_ELEMENT(v);
     WRITE_ELEMENT(v, z*c-x*s);
     v->current_vector=j;
     WRITE_ELEMENT(v, x*c+z*s);
    }
    z=pythag(f,h);
    w->current_element=j;
    WRITE_ELEMENT(w, z);
    if (z) {
     z=1.0/z;
     c=f*z;
     s=h*z;
    }
    f=c*g+s*y;
    x=c*y-s*g;
    for (jj=0;jj<m;jj++) {
     a->current_element=jj;
     a->current_vector=j;
     y=READ_ELEMENT(a);
     a->current_vector=i;
     z=READ_ELEMENT(a);
     WRITE_ELEMENT(a, z*c-y*s);
     a->current_vector=j;
     WRITE_ELEMENT(a, y*c+z*s);
    }
   }
   rv1.current_element=l;
   WRITE_ELEMENT(&rv1, 0.0);
   w->current_element=rv1.current_element=k;
   WRITE_ELEMENT(&rv1, f);
   WRITE_ELEMENT(w, x);
  }
  /*}}}  */
 }
 array_free(&rv1);
}
