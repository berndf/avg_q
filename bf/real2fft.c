/*
 * Copyright (C) 1993,1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#define _XOPEN_SOURCE
#include <math.h>
#include "transform.h"
#include "bf.h"

/*
 * real2fft takes two real arrays of length n and returns a complex array p1 
 * of length n+2 containing the positive-frequency parts of both spectra
 * including the coefficients for frequency 0.
 */

GLOBAL void 
real2fft(DATATYPE *data1, DATATYPE *data2, complex *p1, int n) {
 int nd2=(n>>1)+1, j;  /* nd2 is the number of complex points per spect */
 int nd2m2=nd2-2;
 DATATYPE rep,rem,aip,aim;
 complex *p2=p1+nd2, cmpl; /* p2: Start of the second spectrum */

 for (j=0;j<n;j++) {	/* Build the complex input array for fourier() */
  p1[j].Re=data1[j];
  p1[j].Im=data2[j];
 }
 fourier(p1,n,1);
 /* The resulting spectrum will take n points, but when separating it into
  * two positive half spectra, the 0 and maximum frequencies will be doubled
  * because they appear only once in the fourier array.
  */

 /* Prepare the p2 array: Shift up by 1, duplicating the max freq value, and
  * swap so that lowest freq is first */
 p2[nd2-1]=p1[nd2-1];	/* Duplicate maxfreq */
 p2[nd2m2]=p2[0];	/* Move maxfreq-1 */
 for (j=1;j<n>>2;j++) {
  cmpl=p2[j];
  p2[j]=p2[nd2m2-j];
  p2[nd2m2-j]=cmpl;
 }
 p2[0].Re=p1[0].Im;	/* Prepare coeff for freq 0 */
 p2[0].Im=p1[0].Im=0.0;
 for (j=1;j<nd2;j++) {
  rep=0.5*(p1[j].Re+p2[j].Re);
  rem=0.5*(p1[j].Re-p2[j].Re);
  aip=0.5*(p1[j].Im+p2[j].Im);
  aim=0.5*(p1[j].Im-p2[j].Im);
  p1[j].Re=  rep;
  p1[j].Im=  aim;
  p2[j].Re=  aip;
  p2[j].Im= -rem;
 }
}
