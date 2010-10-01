/*
 * Copyright (C) 1993 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
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
