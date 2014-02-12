/*
 * Copyright (C) 1993,2003,2004 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * btimontage.c modified version of Bernd Luetkenhoener's BTi37montage.c
 * 	Bernd Feige 14.07.1993
 */

/*{{{}}}*/
/*{{{  Description*/
/************************************************************************/
/*                                                                      */
/*  Parameters                                                          */
/*     nring     - number of rings of coils surrounding the center coil */
/*                 (3 in the case of a 37 channel system)               */
/*     span      - angle (degrees) between two opposite outer coils     */
/*                 (63 degrees for original 37 channel system)          */
/*     curvature - radius of curvature (cm)                             */
/*                 (12 cm for original 37 channel system)               */
/*     center    - center of the measurement surface (Cartesian         */
/*                 coordinates in cm).  If center==NULL, it is assumed  */
/*                 that the center coincides with the origin of the     */
/*                 coordinate system.                                   */
/*     axis0     - the axis of the center coil is assumed to point      */
/*                 to a position with the coordinates:                  */
/*                   (axis0[0], axis0[1], axis0[2])                     */
/*                 If axis0==NULL, it is assumed that the axis of the   */
/*                 center coil corresponds to the z axis.               */
/*                                                                      */
/*  Example:                                                            */
/*     #define NRING 3                                                  */
/*     #define SPAN 63.                                                 */
/*     #define CURVATURE 12.                                            */
/*     double *r;                                                       */
/*     int number_of_coils;                                             */
/*     BTi37montage_init( NRING, SPAN, CURVATURE, NULL, NULL );         */
/*     number_of_coils = BTi37montage_numberofcoils();                  */
/*        ... e.g. memory allocation for coil positions                 */
/*     while ((r=BTi37montage())!=NULL) {                               */
/*        ...other statements...  (e.g. storage of positions)           */
/*     }                                                                */
/*                                                                      */
/*  Author:                                                             */
/*     B. Lütkenhöner (January, 1993)                                   */
/*                                                                      */
/*  History:                                                            */
/*     FORTRAN function BTi37coilconfiguration                          */
/*                                                                      */
/************************************************************************/
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  Defines*/
/* Defines for the construction of channel names */
#define CHNAMELEN 5
#define CHNAMECONST "A%d"
/*}}}  */

void btimontage(transform_info_ptr tinfo, int nRing, double Span,
	double Curvature, double *Center, double *Axis0) {
 /*{{{  Local vars*/
 double xxyy, r0magn, sqrtxxyy;
 double a11, a12, a13, a21, a22, a31, a32, a33;  /*rotation matrix*/
 const double TWOPI = 6.283185307179586476925287;
 const double CONST = TWOPI/360.;
 int ring=0, coil=0, n=0, channel, i;
 double *pos, pos1[3], radius_sintheta, radius_costheta;
 double phi, theta;
 char *channelnames;
 /*}}}  */

 Curvature/=100;	/* Output unit is m, not cm */
 /*{{{  Allocate memory*/
 tinfo->nr_of_channels=1 + 3*nRing*(nRing+1);
 if ((tinfo->probepos=(double *)malloc(tinfo->nr_of_channels*3*sizeof(double)))==NULL ||
     (channelnames=(char *)malloc(tinfo->nr_of_channels*CHNAMELEN))==NULL ||
     (tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL) {
  ERREXIT(tinfo->emethods, "btimontage: Error allocating memory\n");
 }
 /*}}}  */

 if (Axis0 != NULL) {
  /*{{{  Calculate rotation matrix*/
  /* rotation matrix:  axis0 ---> z axis  (a23 is always zero) */
  /* (see Lütkenhöner, 1992 (Mnster: Lit))        */
  xxyy = Axis0[0]*Axis0[0] + Axis0[1]*Axis0[1];
  sqrtxxyy = sqrt(xxyy);
  r0magn = sqrt( xxyy + Axis0[2]*Axis0[2] );
  a31 =  Axis0[0] / r0magn;
  a32 =  Axis0[1] / r0magn;
  a33 =  Axis0[2] / r0magn;
  a22 =  Axis0[0] / sqrtxxyy;
  a11 =  a22 * a33;
  a21 = -Axis0[1] / sqrtxxyy;
  a12 = -a21 * a33;
  a13 = -sqrtxxyy / r0magn;
  /*}}}  */
 }

 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  /*{{{  Assign probe position*/
  pos=tinfo->probepos+3*channel;

  if (channel==0) {
   pos[0] = pos[1] = 0.;
   pos[2] = Curvature;
  } else {
   if (++coil >= n) {
    coil = 0; ring++;
    n = 6*ring;
    theta = ring * CONST * Span / (2*nRing);
    radius_sintheta = Curvature * sin(theta);
    radius_costheta = Curvature * cos(theta);
   }
   phi = coil * (TWOPI/n);
   pos[0] = radius_sintheta * cos(phi);
   pos[1] = radius_sintheta * sin(phi);
   pos[2] = radius_costheta;
  }

  if (Axis0 != NULL) { /* rotation */
   pos1[0] = a11*pos[0] + a21*pos[1] + a31*pos[2]; /*        */
   pos1[1] = a12*pos[0] + a22*pos[1] + a32*pos[2]; /*inverse = transpose */
   pos1[2] = a13*pos[0]      + a33*pos[2]; /*        */
   pos[0]=pos1[0];
   pos[1]=pos1[1];
   pos[2]=pos1[2];
  }

  if (Center != NULL) { /* shift of center */
   for (i=0; i<3; i++)
    pos[i] = pos[i] + Center[i]/100;
  }
  /*}}}  */
  /*{{{  Assign channel name*/
  tinfo->channelnames[channel]=channelnames+channel*CHNAMELEN;
  snprintf(tinfo->channelnames[channel], CHNAMELEN, CHNAMECONST, channel+1);
  /*}}}  */
 }
}
