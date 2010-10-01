/*
 * pythag function modified from the Numerical Recipes 2.0
 *					-- Bernd Feige 17.09.1993
 */

#include <math.h>
#include "bfmath.h"

GLOBAL DATATYPE 
pythag(DATATYPE a, DATATYPE b) {
 DATATYPE absa,absb, tmp;
 absa=fabs(a);
 absb=fabs(b);
 tmp=absb/absa;
 tmp*=tmp;
 if (absa > absb) return absa*sqrt(1.0+tmp);
 else return (absb == 0.0 ? 0.0 : absb*sqrt(1.0+1.0/tmp));
}
