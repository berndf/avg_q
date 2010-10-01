/*
 * linreg.c module to perform line fits on data; cf. Numerical Recipes p.524
 * 						-- Bernd Feige 23.04.1993
 */

#include "bf.h"


/*
 * the linreg function fits a line to the data pointed to by data and
 * returns the coefficients in the places pointed to by linreg_constp
 * and linreg_factp. The 'x axis' used is the point number starting with 0.
 * This assumption of equally spaced points and equal weight of each point
 * is used to optimize the process.
 * The data points are skip*sizeof(DATATYPE) bytes apart. This allows to
 * fit lines to vector items.
 */

GLOBAL void
linreg(DATATYPE *data, int points, int skip, DATATYPE *linreg_constp, DATATYPE *linreg_factp) {
 DATATYPE Sy=0, Sxy=0, Sx, Sxx, delta, x;
 int n=points;

 while (n--) {
  x=data[n*skip];
  Sy+=x;
  Sxy+=n*x;
 }
 /* This is the sum of the n: */
 Sx=points*(points-1)/2;
 /* This is the sum of the n^2: */
 Sxx=((2*points*points-3*points+1.0)*points)/6;
 delta=points*Sxx-Sx*Sx;
 *linreg_constp=(Sxx*Sy-Sx*Sxy)/delta;
 *linreg_factp=(points*Sxy-Sx*Sy)/delta;
}
