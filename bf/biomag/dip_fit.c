/*
 * Copyright (C) 1996-1999,2003,2010 Bernd Feige
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
/*{{{  Description*/
/*
 * dip_fit.c module to fit dipoles to the input data.
 *  The struct do_leastsquares_args pointed to by methods->local_storage
 *  can be accessed from outside; ->position contains the resulting
 *  dipole position, ->x the dipole moment, residual the residual
 *  variance and original_variance the original variance.
 *	-- Bernd Feige 28.09.1993
 *
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
#include "biomag.h"
#include "array.h"
#include "optimize.h"
/*}}}  */

/*{{{  Defines*/
#define FTOL 1e-4
/*}}}  */

enum ARGS_ENUM {
 ARGS_TIME=0, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "time", "", ARGDESC_UNUSED, (const char *const *)"1s"}
};

struct dip_fit_storage {
 long pointno;
 struct do_leastsquares_args fixedargs;
};

/*{{{  dip_fit_init(transform_info_ptr tinfo) {*/
/*{{{  do_spherefit(optimize_struct *ostructp)*/
LOCAL OPT_DTYPE
do_spherefit(optimize_struct *ostructp) {
 struct do_leastsquares_args *fixedargsp=(struct do_leastsquares_args *)ostructp->fixed_args;
 OPT_DTYPE *opts=(OPT_DTYPE *)ostructp->opt_args, sq=0.0;
 DATATYPE hold, norm;
 int i;

 array_reset(&fixedargsp->r);
 do {
  norm=0.0;
  for (i=0; i<3; i++) {	/* Subtract center */
   hold=array_scan(&fixedargsp->r)-opts[i];
   norm+=hold*hold;
  }
  hold=sqrt(norm)-opts[3];
  sq+=hold*hold;	/* Accumulate the squared deviations from r */
 } while (fixedargsp->r.message!=ARRAY_ENDOFSCAN);

 return sq;
}
/*}}}  */

METHODDEF void
dip_fit_init(transform_info_ptr tinfo) {
 struct dip_fit_storage *local_arg=(struct dip_fit_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 double *posptr=tinfo->probepos;
 OPT_DTYPE sphere_startparameters[]={0.0, 0.0, 0.0, SENSOR_CURVATURE};
 OPT_DTYPE sphere_parameters[4], residual;	/* 3x center, 1x radius */
 DATATYPE norm, hold;
 optimize_struct ostruct;
 array xi, id;
 int i, iter;

 /*{{{  Parse arguments that can be in seconds*/
 local_arg->pointno=gettimeslice(tinfo, args[ARGS_TIME].arg.s);
 /*}}}  */

 local_arg->fixedargs.a.nr_of_vectors= local_arg->fixedargs.b.nr_of_elements=local_arg->fixedargs.r.nr_of_vectors=local_arg->fixedargs.u.nr_of_vectors=tinfo->nr_of_channels;
 local_arg->fixedargs.a.nr_of_elements=local_arg->fixedargs.x.nr_of_elements=2;
 local_arg->fixedargs.x3d.nr_of_elements=local_arg->fixedargs.position.nr_of_elements=local_arg->fixedargs.r.nr_of_elements=local_arg->fixedargs.u.nr_of_elements=3;
 local_arg->fixedargs.center.nr_of_elements=4;
 local_arg->fixedargs.x3d.nr_of_vectors=local_arg->fixedargs.center.nr_of_vectors=local_arg->fixedargs.x.nr_of_vectors=local_arg->fixedargs.position.nr_of_vectors=1;
 local_arg->fixedargs.x3d.element_skip=local_arg->fixedargs.center.element_skip=local_arg->fixedargs.a.element_skip=local_arg->fixedargs.x.element_skip=local_arg->fixedargs.position.element_skip=local_arg->fixedargs.r.element_skip=local_arg->fixedargs.u.element_skip=1;
 /*{{{  Memory allocation*/
 if (array_allocate(&local_arg->fixedargs.position)==NULL || array_allocate(&local_arg->fixedargs.center)==NULL || array_allocate(&local_arg->fixedargs.a)==NULL || array_allocate(&local_arg->fixedargs.x)==NULL || array_allocate(&local_arg->fixedargs.r)==NULL || array_allocate(&local_arg->fixedargs.u)==NULL || array_allocate(&local_arg->fixedargs.x3d)==NULL) {
  ERREXIT(tinfo->emethods, "dip_fit_init: Error allocating memory\n");
 }

# ifdef GRADIOMETER_LENGTH
 local_arg->fixedargs.a2=local_arg->fixedargs.a;
 local_arg->fixedargs.r2=local_arg->fixedargs.r;
 if (array_allocate(&local_arg->fixedargs.a2)==NULL || array_allocate(&local_arg->fixedargs.r2)==NULL) {
  ERREXIT(tinfo->emethods, "dip_fit_init: Error allocating gradiometer memory\n");
 }
# endif
 /*}}}  */
 /*{{{  Initialize r*/
 do {
  array_write(&local_arg->fixedargs.r, (*posptr++)*100); /* Our probe positions are in m */
 } while (local_arg->fixedargs.r.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
 /*{{{  Fit a sphere to the sensor array*/
 ostruct.function= &do_spherefit;
 ostruct.fixed_args= &local_arg->fixedargs;
 ostruct.num_opt_args=4;
 ostruct.opt_args=sphere_parameters;

 /*{{{  Initialize xi*/
 xi.nr_of_elements=xi.nr_of_vectors=4;
 xi.element_skip=1;
 array_reset(&xi);
 id=xi;
 array_setto_identity(&id);
 if (array_allocate(&xi)==NULL) {
  ERREXIT(tinfo->emethods, "dip_fit_init: Error allocating xi memory\n");
 }
 array_copy(&id, &xi);
 /*}}}  */

 residual=opt_powell(&ostruct, sphere_startparameters, &xi, FTOL, &iter);
 array_free(&xi);
 /*}}}  */
 /*{{{  Write the resulting parameters to the center array*/
 array_reset(&local_arg->fixedargs.center);
 printf("Sphere fit to sensor array: ");
 for (i=0; i<4; i++) {
  array_write(&local_arg->fixedargs.center, sphere_parameters[i]);
  printf("%g ", sphere_parameters[i]);
 }
 printf("Residual: %g\n", residual);
 /* This way, center can be passed directly to leadfield_sphere: */
 local_arg->fixedargs.center.nr_of_elements=3;
 /*}}}  */
 /*{{{  Initialize u*/
 i=0;
 array_reset(&local_arg->fixedargs.r);
 array_reset(&local_arg->fixedargs.u);
 array_reset(&local_arg->fixedargs.center); /* This array now has 3 elements only */
 do {
  norm=0.0;
  do {
   hold=array_scan(&local_arg->fixedargs.r)-array_scan(&local_arg->fixedargs.center);
   array_write(&local_arg->fixedargs.u, hold);
   norm+=hold*hold;
  } while (local_arg->fixedargs.r.message==ARRAY_CONTINUE);
  local_arg->fixedargs.u.current_vector=i++;	/* The vector just written */
  array_scale(&local_arg->fixedargs.u, 1.0/sqrt(norm));
 } while (local_arg->fixedargs.r.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
#ifdef GRADIOMETER_LENGTH
 /*{{{  Initialize r2: Positions of the upper coils*/
 array_reset(&local_arg->fixedargs.r2);
 do {
  array_write(&local_arg->fixedargs.r2, array_scan(&local_arg->fixedargs.r)+GRADIOMETER_LENGTH*array_scan(&local_arg->fixedargs.u));
 } while (local_arg->fixedargs.r2.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
#endif

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  dip_fit(transform_info_ptr tinfo) {*/
/*{{{  do_leastsquares(optimize_struct *ostructp)*/
LOCAL OPT_DTYPE
do_leastsquares(optimize_struct *ostructp) {
 struct do_leastsquares_args *fixedargsp=(struct do_leastsquares_args *)ostructp->fixed_args;
 DATATYPE hold;
 OPT_DTYPE *opts=(OPT_DTYPE *)ostructp->opt_args;
 double sq;
 int i;

 /*{{{  Punish points lying outside the sphere*/
 sq=0;
 for (i=0; i<3; i++) {
  sq+=opts[i]*opts[i];
 }
 hold=fixedargsp->center.start[3];	/* Radius */
 hold*=hold;
 if (sq>hold) {
  return 1e9*(sq-hold);
 }
 /*}}}  */

 array_reset(&fixedargsp->position);
 array_reset(&fixedargsp->r);
 array_reset(&fixedargsp->u);
 array_reset(&fixedargsp->a);
 array_reset(&fixedargsp->b);
 array_reset(&fixedargsp->x);
 for (i=0; i<3; i++) {
  array_write(&fixedargsp->position, opts[i]);
 }

 biomag_leadfield2_sphere(&fixedargsp->position, &fixedargsp->center, &fixedargsp->r, &fixedargsp->u, &fixedargsp->a);
#ifdef GRADIOMETER_LENGTH
 array_reset(&fixedargsp->a2);
 array_reset(&fixedargsp->r2);
 biomag_leadfield2_sphere(&fixedargsp->position, &fixedargsp->center, &fixedargsp->r2, &fixedargsp->u, &fixedargsp->a2);
 do {
  array_write(&fixedargsp->a, READ_ELEMENT(&fixedargsp->a)-array_scan(&fixedargsp->a2));
 } while (fixedargsp->a.message!=ARRAY_ENDOFSCAN);
#endif
 array_transpose(&fixedargsp->a);
 array_svd_solution(&fixedargsp->a, &fixedargsp->b, &fixedargsp->x);
 array_transpose(&fixedargsp->a);
 array_reset(&fixedargsp->b);
 sq=0.0;
 do {
  hold=array_multiply(&fixedargsp->a, &fixedargsp->x, MULT_VECTOR);
  hold-=array_scan(&fixedargsp->b);
  sq+=hold*hold;
 } while (fixedargsp->a.message!=ARRAY_ENDOFSCAN);

 return sq;
}
/*}}}  */

METHODDEF DATATYPE *
dip_fit(transform_info_ptr tinfo) {
 struct dip_fit_storage *local_arg=(struct dip_fit_storage *)tinfo->methods->local_storage;
 DATATYPE hold;
 double variance;
 optimize_struct ostruct;
 array xi, id;
 OPT_DTYPE startpos[]={0.0, 0.0, 9.0}, optpos[3], residual;
 int iter, i;

 nonmultiplexed(tinfo);
 /*{{{  Initialize vector b*/
 local_arg->fixedargs.b.nr_of_vectors=tinfo->nr_of_points;
 local_arg->fixedargs.b.vector_skip=tinfo->itemsize;
 local_arg->fixedargs.b.element_skip=local_arg->fixedargs.b.nr_of_vectors*local_arg->fixedargs.b.vector_skip;
 local_arg->fixedargs.b.start=tinfo->tsdata;
 array_setreadwrite(&local_arg->fixedargs.b);
 local_arg->fixedargs.b.current_vector=local_arg->pointno;
 array_setto_vector(&local_arg->fixedargs.b);
 /*}}}  */

 /*{{{  Initialize xi*/
 xi.nr_of_elements=xi.nr_of_vectors=3;
 xi.element_skip=1;
 array_reset(&xi);
 id=xi;
 array_setto_identity(&id);
 if (array_allocate(&xi)==NULL) {
  ERREXIT(tinfo->emethods, "dip_fit: Error allocating xi memory\n");
 }
 array_copy(&id, &xi);
 /*}}}  */
 /*{{{  Initialize ostruct*/
 ostruct.function= &do_leastsquares;
 ostruct.fixed_args= &local_arg->fixedargs;
 ostruct.num_opt_args=3;
 ostruct.opt_args=(void *)optpos;
 /*}}}  */
 residual=opt_powell(&ostruct, startpos, &xi, FTOL, &iter);
 /*{{{  Calculate original variance*/
 variance=0.0;
 do {
  hold=array_scan(&local_arg->fixedargs.b);
  variance+=hold*hold;
 } while (local_arg->fixedargs.b.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
 /*{{{  Print results and store them in fixedargs*/
 array_reset(&local_arg->fixedargs.position);
 printf("Iter=%d, Residual: %g, %%Variance: %g\nPosition=", iter, residual, (variance-residual)/variance*100);
 for (i=0; i<3; i++) {
  array_write(&local_arg->fixedargs.position, startpos[i]);
  printf("%g ", startpos[i]);
 }
 printf("\nDipole momentum (local 2-dim)=");
 do {
  printf("%g ", array_scan(&local_arg->fixedargs.x));
 } while (local_arg->fixedargs.x.message!=ARRAY_ENDOFSCAN);
 printf("\n");

 /*{{{  Calculate 3-d momentum*/
 {
 array uxiuyp, rc;
 DATATYPE uxiuypvalues[6], rcvalues[3];

 rc.nr_of_vectors=1;
 uxiuyp.nr_of_vectors=2;
 rc.nr_of_elements=uxiuyp.nr_of_elements=3;
 rc.element_skip=  uxiuyp.element_skip  =1;
 rc.vector_skip=rc.nr_of_elements;
 uxiuyp.vector_skip=uxiuyp.nr_of_elements;
 rc.start=rcvalues; uxiuyp.start=uxiuypvalues;
 array_setreadwrite(&rc); array_setreadwrite(&uxiuyp);
 array_reset(&rc); array_reset(&uxiuyp);

 array_reset(&local_arg->fixedargs.center);
 do {
  array_write(&rc, array_subtract(&local_arg->fixedargs.position, &local_arg->fixedargs.center));
 } while (rc.message!=ARRAY_ENDOFSCAN);

 /* Construct local vector base */
 biomag_uxiypsilon(&rc, &uxiuyp);

 array_reset(&local_arg->fixedargs.x);
 array_reset(&local_arg->fixedargs.x3d);
 array_transpose(&uxiuyp);
 printf("Dipole momentum in world coords=");
 for (i=0; i<3; i++) {
  hold=array_multiply(&uxiuyp, &local_arg->fixedargs.x, MULT_VECTOR);
  array_write(&local_arg->fixedargs.x3d, hold);
  printf("%g ", hold);
 }
 printf("\n");
 }
 /*}}}  */

 local_arg->fixedargs.residual=residual;
 local_arg->fixedargs.original_variance=variance;
 /*}}}  */
 /*{{{  Replace b with this dipole field*/
 array_reset(&local_arg->fixedargs.b);
 do {
  array_write(&local_arg->fixedargs.b, array_multiply(&local_arg->fixedargs.a, &local_arg->fixedargs.x, MULT_VECTOR));
 } while (local_arg->fixedargs.a.message!=ARRAY_ENDOFSCAN);
 /*}}}  */

 array_free(&xi);
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  dip_fit_exit(transform_info_ptr tinfo) {*/
METHODDEF void
dip_fit_exit(transform_info_ptr tinfo) {
 struct dip_fit_storage *local_arg=(struct dip_fit_storage *)tinfo->methods->local_storage;

 array_free(&local_arg->fixedargs.u);
 array_free(&local_arg->fixedargs.r);
 array_free(&local_arg->fixedargs.x);
 array_free(&local_arg->fixedargs.a);
 array_free(&local_arg->fixedargs.center);
 array_free(&local_arg->fixedargs.position);
#ifdef GRADIOMETER_LENGTH
 array_free(&local_arg->fixedargs.a2);
 array_free(&local_arg->fixedargs.r2);
#endif

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_dip_fit(transform_info_ptr tinfo) {*/
GLOBAL void
select_dip_fit(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &dip_fit_init;
 tinfo->methods->transform= &dip_fit;
 tinfo->methods->transform_exit= &dip_fit_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="dip_fit";
 tinfo->methods->method_description=
  "Method to calculate the best fitting dipole\n";
 tinfo->methods->local_storage_size=sizeof(struct dip_fit_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
