/*
 * Copyright (C) 1993 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*
 * optimize.h defines memory structures that should be able to contain
 * whatever is necessary to optimize (find zeros, min values...) an
 * n-dimensional function without interfering with possible calls to
 * the same optimizer function by the function optimized.
 *					-- Bernd Feige 9.08.1993
 */

#ifndef _OPTIMIZE_H
#define _OPTIMIZE_H
#include <method.h>
#include <array.h>

#define OPT_DTYPE double

typedef struct optimize_struct_struct {
 OPT_DTYPE (*function)(struct optimize_struct_struct *);
 void *fixed_args;	/* These are known to the function optimized only */
 int num_opt_args;
 void *opt_args;	/* These could be optimized by an optimizer method */
 void *opt_storage;	/* For the optimizer method to store temp values */
} optimize_struct;

OPT_DTYPE findroot(optimize_struct *ostructp, OPT_DTYPE von, OPT_DTYPE bis, OPT_DTYPE step, OPT_DTYPE genau);
void opt_mnbrak(optimize_struct *ostructp, OPT_DTYPE *ax, OPT_DTYPE *bx, OPT_DTYPE *cx, OPT_DTYPE *fa, OPT_DTYPE *fb, OPT_DTYPE *fc);
OPT_DTYPE opt_zbrent(optimize_struct *ostructp, OPT_DTYPE x1, OPT_DTYPE x2, OPT_DTYPE tol);
OPT_DTYPE opt_brent(optimize_struct *ostructp, OPT_DTYPE ax, OPT_DTYPE bx, OPT_DTYPE cx, OPT_DTYPE tol, OPT_DTYPE *xmin);
OPT_DTYPE opt_linmin(optimize_struct *ostructp, OPT_DTYPE p[], OPT_DTYPE xi[]);
OPT_DTYPE opt_powell(optimize_struct *ostructp, OPT_DTYPE p[], array *xi, OPT_DTYPE ftol, int *iter);
#endif
