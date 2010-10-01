/*
 * opt_linmin minimizes an n-dimensional function along a straight line
 * of direction xi through a point p. The minimum point is returned in p
 * and the value at this point is the function return value.
 * Adapted from the Numerical Recipes(2.0)
 *					-- Bernd Feige 20.09.1993
 *
 * The procedures are as followes: opt_linmin constructs a 'one-dimensional'
 * optimization struct onedim_ostruct which contains all the information
 * needed by the 'optimized' one-dimensional local function f1dim.
 * The 'N-dimensional' ostructp passed to opt_linmin has, of course, to
 * have properly allocated an OPT_DTYPE array[ostructp->num_opt_args] to
 * the opt_args argument. f1dim sets this N-dimensional argument accordingly
 * and calls the N-dimensional function ostructp->function on it.
 */

#include "optimize.h"

/* Structure definition to tell f1dim what it needs to know about the
 * N-dim function. A pointer to such a struct gets passed as 'fixed_arg' */
struct linmin_struct {
 optimize_struct *ostructp;
 OPT_DTYPE *p;
 OPT_DTYPE *xi;
};

LOCAL OPT_DTYPE
f1dim(optimize_struct *ostructp) {
 struct linmin_struct* lmsp=(struct linmin_struct *)ostructp->fixed_args;
 optimize_struct *ndim_ostructp=lmsp->ostructp;
 int j,n=ndim_ostructp->num_opt_args;
 OPT_DTYPE x=((OPT_DTYPE *)ostructp->opt_args)[0];
 OPT_DTYPE *xt=(OPT_DTYPE *)ndim_ostructp->opt_args,*p=lmsp->p,*xi=lmsp->xi;

 for (j=0;j<n;j++) {
  xt[j]=p[j]+x*xi[j];
 }
 return (*ndim_ostructp->function)(ndim_ostructp);
}

#define TOL 2.0e-4

GLOBAL OPT_DTYPE
opt_linmin(optimize_struct *ostructp, OPT_DTYPE p[], OPT_DTYPE xi[]) {
 int j,n=ostructp->num_opt_args;
 OPT_DTYPE x,xx,xmin,fx,fb,fa,bx,ax,fret;
 struct linmin_struct lms;
 optimize_struct onedim_ostruct;

 lms.ostructp=ostructp; lms.p=p; lms.xi=xi;
 onedim_ostruct.function= &f1dim;
 onedim_ostruct.fixed_args= &lms;
 onedim_ostruct.num_opt_args=1;
 onedim_ostruct.opt_args=(void *)&x;

 ax=0.0;
 xx=1.0;
 opt_mnbrak(&onedim_ostruct,&ax,&xx,&bx,&fa,&fx,&fb);
 fret=opt_brent(&onedim_ostruct,ax,xx,bx,TOL,&xmin);
 for (j=0;j<n;j++) {
  xi[j] *= xmin;
  p[j] += xi[j];
 }

 return fret;
}
#undef TOL
