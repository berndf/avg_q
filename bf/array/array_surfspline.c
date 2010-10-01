/*
 * Copyright (C) 1994 Bernd Feige
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
/*{{{}}}*/
/*{{{  #includes*/
#include <stdlib.h>
#include <math.h>
#include <method.h>

#include "array.h"
/*}}}  */

/*{{{  #defines*/
#define EPS (1.e-14)
#define TINY ((DATATYPE)1.e-20L)
/*}}}  */

/*{{{  Description*/
/*
 *  m-th degree surface spline interpolation:  Evaluation of parameters
 *
 *  Parameter
 *     inpoints input array of dimension n x 3
 *     degree - input parameter: degree of surface spline
 *     par    - output vector: parameters of surface spline,
 *              dimension n+m*(m+1)/2; the array is initialized (and memory
 *		allocated) by the program
 *
 *  Reference: Perrin et al. (1987): Mapping of scalp potentials by
 *             surface spline interpolation.
 *             Electroenceph. Clin. Neurophysiol 66, 75ff
 */
/*}}}  */

/*{{{  array_surfspline()*/
GLOBAL int
array_surfspline(surfspline_desc *sspline) {
 const int n=sspline->inpoints.nr_of_vectors;
 const int m=sspline->degree;
 const int ndim=n+m*(m+1)/2;
 int i, j, irow, icol, err=0;
 array aux;
 DATATYPE xtemp, ytemp, s, t, f;
 double arg;

 aux.nr_of_elements=sspline->par.nr_of_elements=aux.nr_of_vectors=ndim;
 sspline->par.nr_of_vectors=1;
 aux.element_skip=sspline->par.element_skip=1;

 if (array_allocate(&sspline->par)==NULL || array_allocate(&aux)==NULL) {
  sspline->par.message=ARRAY_ERROR;
  return -2;
 }
 array_reset(&aux); array_reset(&sspline->par);

 /*
  * Evaluation of Coefficients of the system of linear equations to be solved
  */
 /*{{{  the first n equations:  K*P + E*Q = Z (cf. Perrin et al., 1987)*/
 for (irow=0; irow<n; irow++) {
  sspline->inpoints.current_vector=irow;
  sspline->inpoints.current_element=0;
  xtemp=array_scan(&sspline->inpoints);
  ytemp=array_scan(&sspline->inpoints);
  array_write(&sspline->par, array_scan(&sspline->inpoints));
  for (icol=0; icol<n; icol++) {
   if (irow != icol) {
    sspline->inpoints.current_vector=icol;
    sspline->inpoints.current_element=0;
    s=xtemp-array_scan(&sspline->inpoints);
    t=ytemp-array_scan(&sspline->inpoints);
    arg=s*s+t*t;
    f=log(arg) * pow(arg, m-1);
   } else {
    f=0.0;
   }
   array_write(&aux, f); /* matrix element K[irow,icol] */
  }
  for (i=0; i<m; i++) {
   for (j=0; j<=i; j++) {
    array_write(&aux, pow(xtemp, i-j) * pow(ytemp, j)); /* row irow of matrix E */
   }
  }
 }
 /*}}}  */
 /*{{{  The last m(m+1)/2 equations: E'*P=0   (cf. Perrin et al., 1987)*/
 for (; irow<ndim; irow++) {
  for (icol=0; icol<n; icol++) {
   /* transpose matrix E */
   aux.current_vector=icol;
   aux.current_element=irow;
   f=READ_ELEMENT(&aux);
   aux.current_vector=irow;
   aux.current_element=icol;
   WRITE_ELEMENT(&aux, f);
  }
 }
 /*}}}  */
 
 /*{{{  Solve system of linear equations*/
 {
 array indx;

 indx.nr_of_elements=ndim;
 indx.nr_of_vectors=indx.element_skip=1;
 if (array_allocate(&indx)==NULL) {
  err= -3;
 } else {
  array_ludcmp(&aux, &indx);
  if (aux.message==ARRAY_ERROR) {
   err= -4;
  } else {
   array_lubksb(&aux, &indx, &sspline->par);
  }
  array_free(&indx);
 }
 if (err!=0) {
  array_free(&sspline->par);
  sspline->par.message=ARRAY_ERROR;
 }
 }
 /*}}}  */
 array_free(&aux);
 return err;
}
/*}}}  */

/*{{{  array_fsurfspline()*/
/*
 *  Evaluation of z[x,y], where z[x,y] is an m-th degree surface spline
 */
GLOBAL DATATYPE
array_fsurfspline(surfspline_desc *sspline, DATATYPE x, DATATYPE y) {
 const int n=sspline->inpoints.nr_of_vectors;
 const int m=sspline->degree;
 int i, j;
 DATATYPE zz, s, t, f, hold;
 double arg;

 zz=0.0;
 array_reset(&sspline->par);
 for (i=0; i<n; i++) {
  sspline->inpoints.current_vector=i;
  sspline->inpoints.current_element=0;
  s=x-array_scan(&sspline->inpoints);
  t=y-array_scan(&sspline->inpoints);
  hold=array_scan(&sspline->par);
  arg=s*s+t*t;
  if (arg > TINY) {
   f=log(arg) * pow(arg, m-1);
   zz+=hold*f;
  }
 }
 for (i=0; i<m; i++) {
  for (j=0; j<=i; j++) {
   zz+=array_scan(&sspline->par) * pow(x, i-j) * pow(y, j);
  }
 }
 return zz;
}
/*}}}  */

/*{{{  array_dsurfspline()*/
/*
 *  Evaluation of dz[x,y]/dx and dz[x,y]/dy, where z[x,y] is an m-th
 *  degree surface spline
 */
GLOBAL void
array_dsurfspline(surfspline_desc *sspline, 
  DATATYPE x, DATATYPE y, DATATYPE *dzdx, DATATYPE *dzdy) {
 const int n=sspline->inpoints.nr_of_vectors;
 const int m=sspline->degree;
 int i, j;
 DATATYPE fx, fy, s, t, temp, hold;
 double arg;

 fx=0.0;
 fy=0.0;
 for (i=0; i<n; i++) {
  sspline->inpoints.current_vector=i;
  sspline->inpoints.current_element=0;
  s=x-array_scan(&sspline->inpoints);
  t=y-array_scan(&sspline->inpoints);
  hold=array_scan(&sspline->par);
  arg=s*s+t*t;
  if (arg > TINY) {
   temp=2*hold * ((m-1)*log(arg)+1) * pow(arg, m-2);
   fx=fx+s*temp;
   fy=fy+t*temp;
  }
 }

 array_advance(&sspline->par);
 for (i=1; i<m; i++) {
  hold=array_scan(&sspline->par);
  for (j=0; j<i; j++) {
   temp=pow(x, i-j-1) * pow(y, j);
   fx=fx + (i-j)*hold*temp;
   hold=array_scan(&sspline->par);
   fy=fy + (j+1)*hold*temp;
  }
 }
 *dzdx=fx;
 *dzdy=fy;
}
/*}}}  */
