/*
 * array_pseudoinverse.c is actually a slightly modified version of 
 * array_svd_solution.c.
 * It uses singular value decomposition (SVD) to
 * obtain the least squares solution of the inhomogeneous linear set
 * of equations Ax=1(n,m) with A(n,m) given. The resulting x'(m,n) is 
 * stored back to A.
 * Least squares actually means two things:
 * 1) If the problem is underdetermined, ie there are less independent
 *    equations than variables, the one solution with smallest |x| is
 *    obtained.
 * 2) If the problem is overdetermined, ie there are more independent
 *    equations than variables, a 'best approximation' is obtained,
 *    ie the x which minimizes |Ax-b|.
 * Cf. Numerical Recipes 2.0, p. 62
 *					-- Bernd Feige 5.07.1999
 *
 * array_pseudoinverse overwrites the contents of a.
 */

#include <stdio.h>
#include "array.h"

#define TOL 1e-5

GLOBAL void
array_pseudoinverse(array *a) {
 DATATYPE hold, threshold;
 array u, v, w, prod;
 u.nr_of_elements=prod.nr_of_vectors=a->nr_of_elements;
 u.nr_of_vectors=v.nr_of_vectors=v.nr_of_elements=w.nr_of_elements=prod.nr_of_elements=a->nr_of_vectors;
 w.nr_of_vectors=1;
 u.element_skip=v.element_skip=w.element_skip=prod.element_skip=1;

 if (array_allocate(&u) == NULL || array_allocate(&v) == NULL || array_allocate(&w) == NULL || array_allocate(&prod) == NULL) {
  fprintf(stderr, "array_pseudoinverse: error allocating memory\n");
  return;
 }
 array_copy(a, &u);
 array_svdcmp(&u, &w, &v);
 array_reset(&u); array_reset(&w); array_reset(&v);
 threshold=TOL*array_max(&w);
 do {
  hold=READ_ELEMENT(&w);
  if (hold<threshold) {
   hold=0.0;
  } else {
   hold=1.0/hold;
  }
  array_write(&w, hold);
 } while (w.message!=ARRAY_ENDOFSCAN);
 array_setto_diagonal(&w);
 /* Calculate W*U' */
 array_transpose(&u);
 do {
  array_write(&prod, array_multiply(&w, &u, MULT_SAMESIZE));
 } while (prod.message!=ARRAY_ENDOFSCAN);
 /* Calculate a=V*(W*U') */
 array_transpose(&v);
 array_transpose(a);
 do {
  array_write(a, array_multiply(&v, &prod, MULT_SAMESIZE));
 } while (a->message!=ARRAY_ENDOFSCAN);
 array_transpose(a);
 array_free(&prod); array_free(&w); array_free(&v); array_free(&u);
}
