/*
 * Copyright (C) 1993,1994,1997 Bernd Feige
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
/*  Reference: B. Ltkenh”ner. M”glichkeiten und Grenzen der            */
/*     neuromagnetischen Quellenanalyse.  Mnster: LiT, 1992, Appendix  */
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
