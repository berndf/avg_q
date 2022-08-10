/*
 * Copyright (C) 1993,1994,1997,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * biomag_uxiypsilon.c evaluation of planar vectors uxi and uyp from
 * a vector r, adapted from B. Luetkenhoener's uxiypsilon.c
 * r has to be a 3-dimensional vector and uxiuyp an array of two
 * 3-dim vectors.
 * The unit vectors uxi and uyp returned are perpendicular to each
 * other and to the input vector r.
 *					-- Bernd Feige 21.09.1993
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "biomag.h"

#define TINY 1.e-10

/*{{{}}}*/
/*{{{  Description*/
/************************************************************************/
/*                                                                      */
/*  Evaluation the of unit vectors u_xi and u_ypsilon associated with   */
/*  the vector r.                                                       */
/*                                                                      */
/*  Reference: B. Lütkenhöner. Möglichkeiten und Grenzen der            */
/*     neuromagnetischen Quellenanalyse.  Münster: LiT, 1992, Appendix  */
/*                                                                      */
/*  History:                                                            */
/*     FORTRAN function uxiyp                                           */
/*                                                                      */
/************************************************************************/
/*}}}  */

GLOBAL void
biomag_uxiypsilon(array *r, array *uxiuyp) {
 int northern;
 double x, y, z, xx, yy, xy, xz, yz, rr, rmagn, xxyy;

 r->current_element=2;
 northern=(READ_ELEMENT(r) >= 0.);	/* "northern/southern hemisphere" */

 array_reset(r);
 rmagn=(northern ? array_abs(r) : -array_abs(r));;

 array_reset(r);
 x = array_scan(r)/rmagn;
 y = array_scan(r)/rmagn;
 z = array_scan(r)/rmagn;
 xx = x * x;
 yy = y * y;
 xxyy = xx + yy;

 if ( xxyy <= TINY) {
  /* To avoid numerical error, set uxi and uyp to 100 and 010 */
  array r2;
  r2= *uxiuyp;
  array_setto_identity(&r2);
  array_copy(&r2, uxiuyp);
 } else {
  xy = x * y;
  xz = x * z;
  yz = y * z;
  array_write(uxiuyp, (x*xz+yy)/xxyy);
  array_write(uxiuyp, rr= -(1.-z)*xy/xxyy);
  array_write(uxiuyp, -x);
  array_write(uxiuyp, rr);
  array_write(uxiuyp, (y*yz+xx)/xxyy);
  array_write(uxiuyp, -y);
 }
}
