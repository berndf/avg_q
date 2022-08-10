/*
 * Copyright (C) 1993,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * findroot.c - simple root finding
 */

#include "optimize.h"

#define BED(a) (((OPT_DTYPE *)ostructp->opt_args)[0]=(a), (*ostructp->function)(ostructp)>0)

GLOBAL OPT_DTYPE
findroot(optimize_struct *ostructp, OPT_DTYPE von, OPT_DTYPE bis, OPT_DTYPE step, OPT_DTYPE genau) {
 register OPT_DTYPE x;
 OPT_DTYPE min, max, mid;
 int vor, nun;

 vor=BED(von);
 for (x=von; x<=bis; x+=step) {	/* Root bracketing by scanning */
  if ((nun=BED(x))!=vor) {
   min=x-step; max=x;
   for (;;) {
    mid=(min+max)/2;
    if (mid-min<genau) break;
    if (BED(mid)==nun) max=mid;
    else	       min=mid;
   }
   return(mid);
  }
  vor=nun;
 }
 return (x);
}

#undef BED
