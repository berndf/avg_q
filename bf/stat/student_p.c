/*
 * student_p.c implements Student's t-test probability using functions from
 * the numerical recipes. Take care: This is the TWO-SIDED probability.
 */

#include "stat.h"

float student_p(int df, float t) {
 return betai(0.5*df, 0.5, df/(df+t*t));
}
