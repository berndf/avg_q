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
/*
 * array_linedist calculates the minimum distance between a point x3
 * and a line through x1 and x2.
 * array_parameter_linedist calculates the parameter d with x=x1+(x2-x1)*d
 * giving the point x on the line at which the minimal distance is obtained.
 *						-- Bernd Feige 2.03.1994
 *
 * Note: The next vector in each array is used. The correct placement of
 * each array at the start of the right vector has to be obeyed at time
 * of call.
 */

#include <math.h>
#include <array.h>

/*{{{}}}*/
/*{{{  array_parameter_linedist(array *x1, array *x2, array *x3) {*/
GLOBAL DATATYPE
array_parameter_linedist(array *x1, array *x2, array *x3) {
 DATATYPE d=0, x1p, x2mx1, x2mx1s=0.0, x3mx1;

 do {
  x1p=array_scan(x1);
  x2mx1=array_scan(x2)-x1p;
  x3mx1=array_scan(x3)-x1p;
  d+=x3mx1*x2mx1;
  x2mx1s+=x2mx1*x2mx1;
 } while (x1->message==ARRAY_CONTINUE);
 return (d/x2mx1s);
}
/*}}}  */

/*{{{  array_linedist(array *x1, array *x2, array *x3) {*/
GLOBAL DATATYPE
array_linedist(array *x1, array *x2, array *x3) {
 const DATATYPE d=array_parameter_linedist(x1, x2, x3);
 double sumsq=0.0, hold;

 array_previousvector(x1); array_previousvector(x2); array_previousvector(x3);

 do {
  hold=array_scan(x1)*(1.0-d) + array_scan(x2)*d - array_scan(x3);
  sumsq+=hold*hold;
 } while (x1->message==ARRAY_CONTINUE);

 return sqrt(sumsq);
}
/*}}}  */
