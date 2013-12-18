/*
 * Copyright (C) 1993,1994 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
#define _XOPEN_SOURCE
#include <math.h>
#include "transform.h"
#include "bf.h"

/*
 * data is a pointer to the data (not to data-1 as in numrec)
 * nn is the number of real points (not half the number as in numrec)
 * isign is 1 for forward fft, -1 for reverse
 * data has to be two points larger than nn to hold the additional 
 * fourier coefficient
 */

GLOBAL void 
realfft(DATATYPE *data, int nn, int isign) {
 int i,i1,i2,i3,i4;
 DATATYPE c1=0.5,c2,h1r,h1i,h2r,h2i;
 double wr,wi,wpr,wpi,wtemp,theta;

 theta=2.0*M_PI/(double) nn;
 if (isign == 1) {
  c2 = -0.5;
  fourier((complex *)data,nn>>1,1);
 } else {
  c2=0.5;
  theta = -theta;
 }
 wtemp=sin(0.5*theta);
 wpr = -2.0*wtemp*wtemp;
 wpi=sin(theta);
 wr=1.0+wpr;
 wi=wpi;
 for (i=1;i<(nn>>2);i++) {
  i1=2*i;   i2=i1+1;
  i3=nn-i1; i4=i3+1;
  h1r=  c1*(data[i1]+data[i3]);
  h1i=  c1*(data[i2]-data[i4]);
  h2r= -c2*(data[i2]+data[i4]);
  h2i=  c2*(data[i1]-data[i3]);
  data[i1]=  h1r+wr*h2r-wi*h2i;
  data[i2]=  h1i+wr*h2i+wi*h2r;
  data[i3]=  h1r-wr*h2r+wi*h2i;
  data[i4]= -h1i+wr*h2i+wi*h2r;
  wr=(wtemp=wr)*wpr-wi*wpi+wr;
  wi=wi*wpr+wtemp*wpi+wi;
 }
 if (isign == 1) {
  data[0] = (h1r=data[0])+data[1];
  data[nn]=h1r-data[1];
  data[1]=data[nn+1]=0.0;
 } else {
  data[0]=c1*((h1r=data[0])+data[nn]);
  data[1]=c1*(h1r-data[nn]);
  fourier((complex *)data,nn>>1,-1);
 }
}
