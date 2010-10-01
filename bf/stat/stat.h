/*
 * Copyright (C) 1994,2009 Bernd Feige
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
