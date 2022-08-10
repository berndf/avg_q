/*
 * Copyright (C) 1994-1996,1999,2004,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#ifndef ARRAY_H
#define ARRAY_H
#include <stddef.h>	/* size_t etc */
#include <stdio.h>	/* FILE * */
#include <method.h>

#ifdef SPLINE_GRIDDER
# ifdef DATATYPE
#  undef DATATYPE
# endif
# define DATATYPE double
#endif

#ifdef NO_MEMMOVE
#ifndef memmove
#define memmove(to,from,N) bcopy(from,to,N)
#endif
#endif

#ifndef EPSILON
/* Used to test against 'zero' in numerical applications. */
#define EPSILON 1e-20
#endif

enum array_message {ARRAY_CONTINUE, ARRAY_ENDOFVECTOR, ARRAY_ENDOFSCAN,
	ARRAY_ERROR};

typedef struct array_struct {
 DATATYPE *start;
 DATATYPE *(*element_address)(struct array_struct *);
 DATATYPE (*read_element)(struct array_struct *);
 void (*write_element)(struct array_struct *, DATATYPE value);
 int element_skip;
 int vector_skip;
 int nr_of_elements;
 int nr_of_vectors;
 int current_item;
 int current_element;
 int current_vector;
 enum array_message message;

 DATATYPE *ringstart;	/* Used for ring buffer */
 DATATYPE *end;		/* Points just behind the array end in memory */

 /* The following is auxiliary storage to keep the original functions
  * if modifying functions are inserted */
 DATATYPE (*org_read_element)(struct array_struct *);
} array;

#define ARRAY_ELEMENT(arr) ((*(arr)->element_address)(arr))
#define READ_ELEMENT(arr)  ((*(arr)->read_element)(arr))
#define WRITE_ELEMENT(arr, value) ((*(arr)->write_element)(arr, value))

enum mult_levels {MULT_CONTRACT, MULT_SAMESIZE, MULT_VECTOR, MULT_BLOWUP};

/*{{{}}}*/
/*{{{  Functions to access the stored data*/
void array_setreadwrite(array *thisarray);
void array_setreadwrite_double(array *thisarray);
/*}}}  */
/*{{{  General functions to manipulate arrays*/
void array_reset(array *thisarray);
void array_transpose(array *thisarray);

void array_nextvector(array *thisarray);
void array_previousvector(array *thisarray);
void array_advance(array *thisarray);
void array_stepback(array *thisarray);
DATATYPE array_scan(array *thisarray);
void array_write(array *thisarray, DATATYPE value);

void array_use_item(array *thisarray, int item);
void array_ringadvance(array *thisarray);

DATATYPE *array_allocate(array *thisarray);
void array_free(array *thisarray);

void array_copy(array *array1, array *array2);

void array_remove_vectorrange(array *thisarray, long vector_from, long vector_to);
/*}}}  */
/*{{{  Special value settings*/
void array_setto_identity(array *thisarray);
void array_setto_null(array *thisarray);
void array_setto_diagonal(array *thisarray);
void array_setto_vector(array *thisarray);
/*}}}  */
/*{{{  Array arithmetic*/
DATATYPE array_add(array *array1, array *array2);
DATATYPE array_subtract(array *array1, array *array2);
DATATYPE array_multiply(array *array1, array *array2, enum mult_levels mult_level);
DATATYPE array_sum(array *thisarray);
DATATYPE array_mean(array *thisarray);
void array_scale(array *thisarray, DATATYPE scalar);
DATATYPE array_max(array *thisarray);
DATATYPE array_min(array *thisarray);
DATATYPE array_square(array *thisarray);
DATATYPE array_abs(array *thisarray);
DATATYPE array_parameter_linedist(array *x1, array *x2, array *x3);
DATATYPE array_linedist(array *x1, array *x2, array *x3);
void array_make_orthogonal(array *array1);

void array_tred2(array *a, array *d, array *e);
void array_tqli(array *d, array *e, array *z);
void array_eigsrt(array *d, array *v);
void array_svdcmp(array *a, array *w, array *v);
void array_svd_solution(array *a, array *b, array *x);
void array_pseudoinverse(array *a);
int array_ludcmp(array *a, array *indx);
void array_lubksb(array *a, array *indx, array *b);
void array_inverse(array *a);
double array_det(array *a);

void array_hpsort(array *a);
void array_index(array *a, unsigned long *indx);
DATATYPE array_quantile(array *a, DATATYPE q);
DATATYPE array_median(array *a);
void array_ranks(array *a, array *ranks);
/*}}}  */
/*{{{  Array dump package*/
typedef enum {ARRAY_ASCII=0, ARRAY_MATLAB, ARRAY_DUMPFORMATS} array_dump_format;
void array_dump(FILE *fp, array *thisarray, array_dump_format format);
GLOBAL void array_undump(FILE *fp, array *thisarray);
/*}}}  */

/*{{{  surfspline package*/
typedef struct surfspline_struct {
 array inpoints;	/* Input points, dimension n x 3 */
 array par;		/* Spline descriptor vector, dim n+m*(m+1)/2 */
 int degree;		/* Spline degree */
} surfspline_desc;

/* Note: Allocates par, returns negative error code on failure */
int array_surfspline(surfspline_desc *sspline);
DATATYPE array_fsurfspline(surfspline_desc *sspline, DATATYPE x, DATATYPE y);
void array_dsurfspline(surfspline_desc *sspline, DATATYPE x, DATATYPE y, DATATYPE *dzdx, DATATYPE *dzdy);
/*}}}  */
/*{{{  MALLOC_TRACE package*/
#ifdef MALLOC_TRACE
#define malloc(x) traced_malloc(x)
#define calloc(x,s) traced_calloc(x,s)
#define realloc(x,s) traced_realloc(x, s)
#define free(x) traced_free(x)
void *traced_malloc(size_t size);
void *traced_calloc(size_t nelem, size_t elsize);
void *traced_realloc(void *ptr, size_t size);
void traced_free(void *ptr);
#endif
/*}}}  */
#endif	/* ARRAY_H */
