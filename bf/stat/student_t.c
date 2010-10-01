/*
 * student_t.c implements Student's t-test  maximum t as function of
 * probability p using functions from the numerical recipes.
 */

#include <optimize.h>
#include "stat.h"

static OPT_DTYPE onetrial(optimize_struct *ostruct) {
 float df=((float *)ostruct->fixed_args)[0], p=((float *)ostruct->fixed_args)[1];
 float t=((OPT_DTYPE *)ostruct->opt_args)[0];

 return student_p((int)df, t)-p;
}

float student_t(int df, float p) {
 float fixedargs[2];
 OPT_DTYPE t;
 optimize_struct ostruct;

 ostruct.function= &onetrial;
 fixedargs[0]=df; fixedargs[1]=p;
 ostruct.fixed_args=(void *)fixedargs;
 ostruct.opt_args=(void *)&t;

 opt_zbrent(&ostruct, 0.0, 50, 1e-6);

 return t;
}
