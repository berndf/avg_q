/*
 * Copyright (C) 1993 Bernd Feige
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
