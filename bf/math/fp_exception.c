/*
 * Copyright (C) 1994,1997,1999 Bernd Feige
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
#include "bfmath.h"

/* i386 FPU control word: (info from Linux:/usr/include/i386/fpu_control.h)
 *     15-13    12  11-10  9-8     7-6     5    4    3    2    1    0
 * | reserved | IC | RC  | PC | reserved | PM | UM | OM | ZM | DM | IM
 *
 * IM: Invalid operation mask
 * DM: Denormalized operand mask
 * ZM: Zero-divide mask
 * OM: Overflow mask
 * UM: Underflow mask
 * PM: Precision (inexact result) mask
 * 
 * Mask bit is 1 means no interrupt.
 *
 * PC: Precision control
 * 11 - round to extended precision
 * 10 - round to double precision
 * 00 - round to single precision
 *
 * RC: Rounding control
 * 00 - rounding to nearest
 * 01 - rounding down (toward - infinity)
 * 10 - rounding up (toward + infinity)
 * 11 - rounding toward zero
 *
 * IC: Infinity control
 * That is for 8087 and 80287 only.
 */

/* We mask out all floating-point interrupts.
 * This is the default on Linux but not with DJGPP.
 * If we'd like to take better notice on the math errors occurring,
 * we would enable the interrupts but poll the type of error...
 * And then, this handler would have to be set in accordance to how
 * severe an error is under the particular circumstances of the algorithm
 * just executing... Anyway, in avg_q it appears to be pretty 
 * counterproductive to let the program terminate with a signal message or
 * a stack backtrace in case of a math error...
 */
#define OUR_FPU_CONTROL_WORD (0x137f)

GLOBAL void
fp_exception_init() {
#ifdef linux
#include <fpu_control.h>
 __fpu_control=OUR_FPU_CONTROL_WORD;
 _FPU_SETCW(__fpu_control);
#else
#ifdef __DJGPP__
#include <float.h>
 _control87(OUR_FPU_CONTROL_WORD, MCW_EM);
#endif
#endif
}
