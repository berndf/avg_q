/*
 * biomag_leadfield2_sphere.c calculation of the lead field of a dipole
 * derived from B. Luetkenhoener's leadfield2_sphere.c
 *					-- Bernd Feige 23.09.1993
 *
 * As in biomag_leadfield_sphere, the number of lead field vectors evaluated
 * is given by the number of vectors in f, not by the number of sensor
 * positions r0. The current vector in the latter array is merely incremented
 * for each sensor position output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <array.h>

#include "biomag.h"

/*{{{}}}*/
/*{{{  Description*/
/************************************************************************/
/*                                                                      */
/*  Like leadfield_sphere, however the magnetic field (in fT) is        */
/*  obtained by evaluating a two-dimensional scalar product (a rotating */
/*  coordinate system spanned by the unit vectors uxi und uypsilon is   */
/*  used, i.e. the components of the dipole moment correspond to the    */
/*  projections onto uxi and uypsilon.)                                 */
/*                                                                      */
/*  Parameters                                                          */
/*     rq      - dipole location:  vector of dimension 3                */
/*               (Cartesian coordinates in cm)                          */
/*     center  - center of sphere (Cartesian coordinates in cm)         */
/*               If center==NULL, it is assumed that the center         */
/*               coincides with the origin of the coordinate system.    */
/*     r0      - recording locations: Array 3 elements * N              */
/*               (Cartesian coordinates in cm)                          */
/*     u0a     - unit vectors pointing into the directions of the       */
/*               gradiometer axes: Array 3 elements * N                 */
/*     f       - "lead field vectors" (output): Array 2 elements * N    */
/*                                                                      */
/*  Return Value:  error indicator                                      */
/*     0 = no error                                                     */
/*     1 = recording location too close to center of sphere             */
/*     2 = recording location and dipole location coincide              */
/*     3 = malloc error							*/
/*                                                                      */
/*  Reference: B. Ltkenh”ner. M”glichkeiten und Grenzen der            */
/*     neuromagnetischen Quellenanalyse.  Mnster: LiT, 1992, 106-107   */
/*                                                                      */
/*  History:                                                            */
/*     FORTRAN function f2Sphere0                                       */
/*                                                                      */
/************************************************************************/
/*}}}  */

GLOBAL int
biomag_leadfield2_sphere(array *rq, array *center, array *r0,
	array *u0a, array *f) {
 int ier;
 DATATYPE gvalues[3], uxiuypvalues[6], rcvalues[3];
 array g, uxiuyp, rc;

 rc.nr_of_vectors=g.nr_of_vectors=1;
 uxiuyp.nr_of_vectors=2;
 rc.nr_of_elements=uxiuyp.nr_of_elements=g.nr_of_elements=3;
 rc.element_skip  =uxiuyp.element_skip  =g.element_skip=1;
 rc.vector_skip   =uxiuyp.vector_skip   =g.vector_skip=3;
 rc.start=rcvalues; uxiuyp.start=uxiuypvalues; g.start=gvalues;
 array_setreadwrite(&rc); array_setreadwrite(&uxiuyp); array_setreadwrite(&g);
 array_reset(&rc); array_reset(&uxiuyp); array_reset(&g);

 /*{{{  Transformation of dipole coordinates for rc (origin --> center of sphere)*/
 /* rc=rq-center: The rotating coordinate system is relative to rc */
 if (center!=NULL) {
  do {
   array_write(&rc, array_subtract(rq, center));
  } while (rq->message==ARRAY_CONTINUE);
 } else {
  array_copy(rq, &rc);
 }
 /*}}}  */

 biomag_uxiypsilon(&rc, &uxiuyp);

 /* For each sensor: */
 do {
  if ((ier=biomag_leadfield_sphere(rq, center, r0, u0a, &g))!=0) return ier;

  /* Calculate f_1, f_2 as the projections of g onto uxi and uyp */
  do {
   array_write(f, array_multiply(&g, &uxiuyp, MULT_VECTOR));
  } while (f->message==ARRAY_CONTINUE);
 } while (f->message!=ARRAY_ENDOFSCAN);

 return 0;
}
