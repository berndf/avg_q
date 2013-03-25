/*
 * Copyright (C) 1995,2008 Bernd Feige
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
/* This module provides routines to rearrange the order of various
 * binary values within memory between little- and big-endian.
 * Two calls to one function will result in the original ordering.
 *					Bernd Feige 22.05.1995
 */

#include "Intel_compat.h"

void Intel_int16(uint16_t *x) {
 *x = ((*x&0x00ffU)<<8) | ((*x&0xff00U)>>8);
}

/* int size is usually 2 bytes on 32 bit and 4 on 64 bit systems,
 * but may also be 8 bytes */
void Intel_int(unsigned int *x) {
 if (sizeof(int)==2) {
  Intel_int16((uint16_t *)x);
 } else if (sizeof(int)==4) {
  Intel_int32((uint32_t *)x);
 } else {
  Intel_int64((uint64_t *)x);
 }
}

void Intel_int32(uint32_t *x) {
 *x = ( (((*x&0xff000000UL)>>24) | ((*x&0x00ff0000UL)>>8))\
      | (((*x&0x0000ff00UL)<<8)  | ((*x&0x000000ffUL)<<24)) );
}

void Intel_float(float *f) {
 Intel_int32((uint32_t *)f);
}

void Intel_int64(uint64_t *d) {
 uint32_t *x=(uint32_t *)d, save;

 save = *x;
 *x = *(x+1);
 *(x+1) = save;
 Intel_int32(x);
 Intel_int32(x+1);
}

void Intel_double(double *d) {
 Intel_int64((uint64_t *)d);
}
