/*
 * Copyright (C) 1993,1994 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * complex.c routines for handling of complex numbers
 *			-- Bernd Feige 4.05.1993
 */

#include <math.h>
#include "transform.h"
#include "bf.h"

GLOBAL double 
c_phase(complex cmpl) {
 return atan2(cmpl.Im, cmpl.Re);
}

GLOBAL double
c_power(complex cmpl) {
 return cmpl.Re*cmpl.Re+cmpl.Im*cmpl.Im;
}

GLOBAL double 
c_abs(complex cmpl) {
 return sqrt(c_power(cmpl));
}

GLOBAL complex 
c_add(complex cmpl1, complex cmpl2) {
 complex result;
 result.Re=cmpl1.Re+cmpl2.Re;
 result.Im=cmpl1.Im+cmpl2.Im;
 return result;
}

GLOBAL complex 
c_mult(complex cmpl1, complex cmpl2) {
 complex result;
 result.Re=cmpl1.Re*cmpl2.Re-cmpl1.Im*cmpl2.Im;
 result.Im=cmpl1.Re*cmpl2.Im+cmpl1.Im*cmpl2.Re;
 return result;
}

GLOBAL complex
c_smult(complex cmpl, DATATYPE factor) {
 complex result;
 result.Re=cmpl.Re*factor;
 result.Im=cmpl.Im*factor;
 return result;
}

GLOBAL complex
c_konj(complex cmpl) {
 complex result;
 result.Re=  cmpl.Re;
 result.Im= -cmpl.Im;
 return result;
}

GLOBAL complex 
c_inv(complex cmpl) {
 return c_smult(c_konj(cmpl), 1.0/c_power(cmpl));
}
