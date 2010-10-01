
/*
 * fourier.c global function to perform in-place fourier analysis on a 
 * data set; This is from the Numerical Recipes.
 *	-- Bernd Feige 9.12.1992
 */

#include <stdio.h>
#include <math.h>
#include "transform.h"
#include "bf.h"

#define SWAP(a,b) tempr=(a);(a)=(b);(b)=tempr

/*
 * The fast fourier transformation - Complex data is pointed to by *data and is
 * replaced by the complex spectrum in place. nn is the number of complex
 * data points, isign (either +1 or -1) the sign in the exp() argument in
 * the fourier integral (-1: inverse transform spectrum->data; the data has
 * to be devided by nn afterwards)
 */

GLOBAL void 
fourier(complex *indata, int nn, int isign) {
 int n,mmax,m,j,istep,i;
 double wtemp,wr,wpr,wpi,wi,theta;
 DATATYPE tempr,tempi, *data=(DATATYPE *)indata;

 n=nn << 1;
 j=0;
 for (i=0;i<n;i+=2) {
  if (j > i) {
   SWAP(data[j],data[i]);
   SWAP(data[j+1],data[i+1]);
  }
  m=n >> 1;
  while (m >= 2 && j >= m) {
   j -= m;
   m >>= 1;
  }
  j += m;
 }
 mmax=2;
 while (n > mmax) {
  istep=2*mmax;
  theta=6.28318530717959/(isign*mmax);
  wtemp=sin(0.5*theta);
  wpr = -2.0*wtemp*wtemp;
  wpi=sin(theta);
  wr=1.0;
  wi=0.0;
  for (m=1;m<mmax;m+=2) {
   for (i=m-1;i<n;i+=istep) {
    j=i+mmax;
    tempr=wr*data[j]-wi*data[j+1];
    tempi=wr*data[j+1]+wi*data[j];
    data[j]=data[i]-tempr;
    data[j+1]=data[i+1]-tempi;
    data[i] += tempr;
    data[i+1] += tempi;
   }
   wr=(wtemp=wr)*wpr-wi*wpi+wr;
   wi=wi*wpr+wtemp*wpi+wi;
  }
  mmax=istep;
 }
}

#undef SWAP
