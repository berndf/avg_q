/*
 * Copyright (C) 1993,1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#ifndef _BIOMAG_H
#define _BIOMAG_H
#include <transform.h>
#include <array.h>
#include <optimize.h>

#define SENSOR_CURVATURE 12.0	/* Curvature of sensor array in cm; used only as starting value for the sphere fit */
/* Distance between the top and bottom coils in a gradiometer in cm.
 * If not defined, a magnetometer is assumed. If defined, sensor positions
 * indicate the centers of the BOTTOM coils. */
#define GRADIOMETER_LENGTH 5.103
#define COIL_RADIUS 0.988	/* Radius of each coil; not used by now */

int biomag_leadfield_sphere(array *rq, array *center, array *r0, array *u0a, array *f);
void biomag_uxiypsilon(array *r, array *uxip);
int biomag_leadfield2_sphere(array *rq, array *center, array *r0,array *u0a, array *f);

/*{{{}}}*/
/*{{{  Definition of dip_fit's struct do_leastsquares_args*/
/* This is the 'fixedargs' structure passed to do_leastsquares and
 * do_spherefit in dip_fit holding the side information needed for the fits */
struct do_leastsquares_args {
 array a;	/* The leadfield array */
 array b;	/* The original data vector */
 array x;	/* The momentum vector, a*x=b */
 array position;	/* The fitted dipole position */
 array r;	/* The sensor positions array */
 array u;	/* The gradiometer axes unit vector array */
 array center;	/* This contains center and radius of the sensor distribution as a fourth element */
 array x3d;	/* Output of dip_fit, 3-dim momentum vector */
#ifdef GRADIOMETER_LENGTH
 array a2;	/* The leadfield for the upper coils if gradiometer */
 array r2;	/* The sensor positions for the upper coils */
#endif
 OPT_DTYPE residual;	/* The residual variance of b-a*x */
 OPT_DTYPE original_variance;	/* The variance of b */
};
/*}}}  */
#endif
