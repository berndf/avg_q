/*
 * opt_powell N-dimensional optimization using the direction-set
 * powell method. Adapted from the Numerical Recipes (2.0).
 *					-- Bernd Feige 20.09.1993
 *
 * The arguments needed are the N-dim starting point p and the NxN-dim
 * array xi holding the N direction vectors (usually the N unit vectors).
 * ftol specifies the fractional tolerance in the function value; failure
 * to decrease it by more than this amount indicates doneness.
 * On return, p holds the minimal point found, xi the then-current
 * direction set and the function value at p is returned.
 * iter holds the number of iterations taken.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <array.h>
#include "optimize.h"

#define ITMAX 200
#define nrerror(a) fprintf(stderr, "%s\n", a)

LOCAL OPT_DTYPE
SQR(OPT_DTYPE x) {
 return x*x;
}

#define EVAL_AT(p) evalit(ostructp, p)

/* The local function evalit evaluates the function to be optimized
 * at the N-dimensional point p */
LOCAL OPT_DTYPE
evalit(optimize_struct *ostructp, OPT_DTYPE p[]) {
 int i, n=ostructp->num_opt_args;
 OPT_DTYPE *opts=(OPT_DTYPE *)ostructp->opt_args;

 for (i=0; i<n; i++) opts[i]=p[i];
 return (*ostructp->function)(ostructp);
}

GLOBAL OPT_DTYPE
opt_powell(optimize_struct *ostructp, OPT_DTYPE p[], array *xi,
 OPT_DTYPE ftol, int *iter) {
 int ibig,j,n=ostructp->num_opt_args;
 OPT_DTYPE del,fp,fptt,t,*pt,*ptt,*xit,fret;

 if ((pt=(OPT_DTYPE *)malloc(n*sizeof(OPT_DTYPE)))==NULL || 
     (ptt=(OPT_DTYPE *)malloc(n*sizeof(OPT_DTYPE)))==NULL || 
     (xit=(OPT_DTYPE *)malloc(n*sizeof(OPT_DTYPE)))==NULL) {
  fprintf(stderr, "opt_powell: Error allocating memory\n");
  exit(-1);
 }

 fret=EVAL_AT(p);
 for (j=0;j<n;j++) pt[j]=p[j];
 for (*iter=1;;++(*iter)) {
  fp=fret;
  ibig=0;
  del=0.0;
  array_reset(xi);
  do {
   j=0;
   do {
    xit[j++]=array_scan(xi);
   } while (xi->message==ARRAY_CONTINUE);
   fptt=fret;
   fret=opt_linmin(ostructp,p,xit);
   if (fabs(fptt-fret) > del) {
    del=fabs(fptt-fret);
    /* current_vector was already incremented */
    ibig=(xi->current_vector>0 ? xi->current_vector : n)-1;
   }
  } while (xi->message!=ARRAY_ENDOFSCAN);
  if (2.0*fabs(fp-fret) <= ftol*(fabs(fp)+fabs(fret))) {
   free(xit); free(ptt); free(pt);
   return fret;
  }
  if (*iter == ITMAX) nrerror("powell exceeding maximum iterations.");
  for (j=0;j<n;j++) {
   ptt[j]=2.0*p[j]-pt[j];
   xit[j]=p[j]-pt[j];
   pt[j]=p[j];
  }
  fptt=EVAL_AT(ptt);
  if (fptt < fp) {
   t=2.0*(fp-2.0*fret+fptt)*SQR(fp-fret-del)-del*SQR(fp-fptt);
   if (t < 0.0) {
    DATATYPE hold;

    fret=opt_linmin(ostructp,p,xit);
    for (j=0;j<n;j++) {
     xi->current_element=j;
     xi->current_vector=n-1;
     hold=READ_ELEMENT(xi);
     WRITE_ELEMENT(xi, xit[j]);
     xi->current_vector=ibig;
     WRITE_ELEMENT(xi, hold);
    }
   }
  }
 }
}
#undef ITMAX
