/*
 * Copyright (C) 1994,2009 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
#ifndef STAT_H
#define STAT_H

#include <stdio.h>

#define nrerror(err) fprintf(stderr, "%s\n", err)
float betacf(float a, float b, float x);
float betai(float a, float b, float x);
float gammln(float xx);
float factln(int n);
float bico(int n, int k);

float student_p(int df, float t);
float student_t(int df, float p);

#endif	/* ifndef STAT_H */
