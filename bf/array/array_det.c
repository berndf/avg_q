/*
 * Calculation of determinant using LU decomposition.
 * Cf. Numerical Recipes (2.0), p. 49
 *				-- Bernd Feige 1.10.1993
 */

#include <stdio.h>
#include "array.h"

GLOBAL double
array_det(array *a) {
 int j,n=a->nr_of_elements;
 double det;
 array y,indx;

 if (a->nr_of_vectors!=n) return 0.0;

 y.nr_of_elements=y.nr_of_vectors=indx.nr_of_elements=n;
 y.element_skip=indx.element_skip=indx.nr_of_vectors=1;
 if (array_allocate(&y)==NULL || array_allocate(&indx)==NULL) {
  a->message=ARRAY_ERROR;
  return 0.0;
 }
 array_copy(a, &y);

 det=array_ludcmp(&y, &indx);
 for (j=0; j<n; j++) {
  y.current_element=y.current_vector=j;
  det*=READ_ELEMENT(&y);
 }
 array_free(&indx); array_free(&y);

 return det;
}
