/*
 * opt_mnbrak -- Routine for bracketing a minimum
 * Adapted from the Numerical Recipes (2.0)
 *					-- Bernd Feige 20.09.1993
 *
 * *ax, *bx, *cx are distinct initial points modified on output
 * to the points bracketing the minimum of the function
 * (*ostructp->function). *fa, *fb and *fc hold the function
 * values at these points on return.
 */

#include <math.h>
#include "optimize.h"

#define GOLD 1.618034
#define GLIMIT 100.0
#define TINY 1.0e-20
#define SHFT(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);

#define FMAX(a, b) ((a)>(b) ? (a) : (b))
#define SIGN(a, b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

#define EVAL_AT(a) (((OPT_DTYPE *)ostructp->opt_args)[0]=(a), (*ostructp->function)(ostructp))

GLOBAL void
opt_mnbrak(optimize_struct *ostructp, OPT_DTYPE *ax, OPT_DTYPE *bx,
 OPT_DTYPE *cx, OPT_DTYPE *fa, OPT_DTYPE *fb, OPT_DTYPE *fc) {
 OPT_DTYPE ulim,u,r,q,fu,dum;

 *fa=EVAL_AT(*ax);
 *fb=EVAL_AT(*bx);
 if (*fb > *fa) {
  SHFT(dum,*ax,*bx,dum)
  SHFT(dum,*fb,*fa,dum)
 }
 *cx=(*bx)+GOLD*(*bx-*ax);
 *fc=EVAL_AT(*cx);
 while (*fb > *fc) {
  r=(*bx-*ax)*(*fb-*fc);
  q=(*bx-*cx)*(*fb-*fa);
  u=fabs(q-r);
  u=(*bx)-((*bx-*cx)*q-(*bx-*ax)*r)/
   (2.0*SIGN(FMAX(u,TINY),q-r));
  ulim=(*bx)+GLIMIT*(*cx-*bx);
  if ((*bx-u)*(u-*cx) > 0.0) {
   fu=EVAL_AT(u);
   if (fu < *fc) {
    *ax=(*bx);
    *bx=u;
    *fa=(*fb);
    *fb=fu;
    return;
   } else if (fu > *fb) {
    *cx=u;
    *fc=fu;
    return;
   }
   u=(*cx)+GOLD*(*cx-*bx);
   fu=EVAL_AT(u);
  } else if ((*cx-u)*(u-ulim) > 0.0) {
   fu=EVAL_AT(u);
   if (fu < *fc) {
    SHFT(*bx,*cx,u,*cx+GOLD*(*cx-*bx))
    SHFT(*fb,*fc,fu,EVAL_AT(u))
   }
  } else if ((u-ulim)*(ulim-*cx) >= 0.0) {
   u=ulim;
   fu=EVAL_AT(u);
  } else {
   u=(*cx)+GOLD*(*cx-*bx);
   fu=EVAL_AT(u);
  }
  SHFT(*ax,*bx,*cx,u)
  SHFT(*fa,*fb,*fc,fu)
 }
}
#undef GOLD
#undef GLIMIT
#undef TINY
#undef SHFT
