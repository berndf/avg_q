/*
 * biomag_leadfield_sphere.c calculation of the lead field of a dipole
 * derived from B. Luetkenhoener's leadfield_sphere.c
 *					-- Bernd Feige 21.09.1993
 *
 * The dimension of the leadfield output array f determines how many positions
 * in r0 are actually used. This way it is possible to, eg, call this function
 * until r0 reaches ENDOFSCAN with only one channel evaluated at a time.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "biomag.h"

#define CBIOMAG 1000.
#define TINY 1.e-10

/*{{{}}}*/
/*{{{  Description*/
/************************************************************************/
/*                                                                      */
/*  To obtain the magnetic field (in fT), the scalar product of f and   */
/*  the dipole moment (in nAm) has to be calculated.                    */
/*                                                                      */
/*  Parameters                                                          */
/*     rq      - dipole location:  vector of dimension 3                */
/*               (Cartesian coordinates in cm)                          */
/*     center  - center of sphere (Cartesian coordinates in cm)         */
/*               If center==NULL, it is assumed that the center         */
/*               coincides with the origin of the coordinate system.    */
/*     r0      - recording locations: Array 3 elements x N              */
/*               (Cartesian coordinates in cm)                          */
/*     u0a     - unit vectors pointing into the directions of the       */
/*               gradiometer axes: Array 3 elements x N                 */
/*     f       - "lead field vectors" (output): Array 3 elements x N    */
/*                                                                      */
/*  Return Value:  error indicator                                      */
/*     0 = no error                                                     */
/*     1 = recording location too close to center of sphere             */
/*     2 = recording location and dipole location coincide              */
/*                                                                      */
/*  Reference: B. L》kenh馬er. M波lichkeiten und Grenzen der            */
/*     neuromagnetischen Quellenanalyse.  M］ster: LiT, 1992, 106-107   */
/*                                                                      */
/*  Author:                                                             */
/*     B. L》kenh馬er (January, 1993)                                   */
/*                                                                      */
/*  History:                                                            */
/*     FORTRAN function fSphere0                                        */
/*                                                                      */
/************************************************************************/
/*}}}  */

GLOBAL int
biomag_leadfield_sphere(array *rq, array *center, array *r0,
	array *u0a, array *f) {
 int i, i1, i2;
 double d[3], r0r0, r0magn, dd, dmagn, ddd, rqrq, r0rq, r0d, rqd,\
   r0u0, rqu0, du0, r0XrqSquare, alpha, beta, twoalpha, term1, term2,\
   afac, bfac, cfac, rdipol[3], rposition[3], u0[3];

 /*{{{  Transformation of dipole coordinates (origin --> center of sphere)*/
 i=0;
 do {
  if (center!=NULL) {
   rdipol[i] = array_subtract(rq, center);
  } else {
   rdipol[i]=array_scan(rq);
  }
  i++;
 } while (rq->message==ARRAY_CONTINUE);
 /*}}}  */
 rqrq = rdipol[0]*rdipol[0] + rdipol[1]*rdipol[1] + rdipol[2]*rdipol[2];
 
 do {
  /*{{{  For each sensor*/
  /*{{{  Transformation of measurement position (origin --> center of sphere)*/
  i=0;
  do {
   if ( center != NULL ) {
    rposition[i]=array_subtract(r0, center);
   } else {
    rposition[i]=array_scan(r0);
   }
   i++;
  } while (r0->message==ARRAY_CONTINUE);
  /*}}}  */
  /*{{{  Set u0 to the gradiometer direction*/
  i=0;
  do {
   u0[i++]=array_scan(u0a);
  } while (u0a->message==ARRAY_CONTINUE);
  /*}}}  */
  r0r0 = rposition[0]*rposition[0] + rposition[1]*rposition[1] + rposition[2]*rposition[2];
  r0magn = sqrt(r0r0);
  if (r0magn < TINY) return 1;
  r0rq = rposition[0]*rdipol[0] + rposition[1]*rdipol[1] + rposition[2]*rdipol[2];
  for ( i = 0; i < 3; i++ ) d[i] = rposition[i] - rdipol[i];
  dd = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
  dmagn = sqrt(dd);
  if (dmagn < TINY) return 2;
  ddd = dd * dmagn;
  rqd = d[0]*rdipol[0] + d[1]*rdipol[1] + d[2]*rdipol[2];
  r0d = d[0]*rposition[0] + d[1]*rposition[1] + d[2]*rposition[2];
  r0u0 = rposition[0]*u0[0] + rposition[1]*u0[1] + rposition[2]*u0[2];
  rqu0 = rdipol[0]*u0[0] + rdipol[1]*u0[1] + rdipol[2]*u0[2];
  du0  =  d[0]*u0[0] +  d[1]*u0[1] +  d[2]*u0[2];
  alpha = -1.  /  ( dmagn * (dmagn*r0magn + r0d) );
  r0XrqSquare  = r0r0 * rqrq - r0rq*r0rq;              /* |rposition x rdipol|**2 */
  if ( fabs(r0XrqSquare) > TINY) {
   /*{{{  */
   twoalpha = alpha + alpha;
   afac = 1. / dmagn   -  twoalpha * rqrq  -  1. / r0magn;
   bfac = twoalpha * r0rq;
   cfac = - rqd / ddd;
   beta = ( afac*r0u0 + bfac*rqu0 + cfac*du0 ) / r0XrqSquare;
   /*}}}  */
  }
  else beta = 0.;  /* actually, beta may be infinity [because of 1/d]! */
  
  i1 = 1;
  i2 = 2;
  for ( i = 0; i < 3; i++ ) {
   /*{{{  */
   term1  =  u0[i1] * rdipol[i2]  -  u0[i2] * rdipol[i1];
   term2  =  rposition[i1] * rdipol[i2]  -  rposition[i2] * rdipol[i1];
   array_write(f, CBIOMAG  * ( alpha * term1  +  beta * term2 ));
   i1 = i2;
   if (++i2 == 3) i2 = 0;
   /*}}}  */
  }
  /*}}}  */
 } while (f->message!=ARRAY_ENDOFSCAN);
 return 0;
}
