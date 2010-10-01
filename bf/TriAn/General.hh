// Copyright (C) 1996,2000 Bernd Feige
// 
// This file is part of avg_q.
// 
// avg_q is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// avg_q is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
#ifndef _General_HH
#define _General_HH

extern "C" {
#include <stddef.h>
#include <stdio.h>
#include <math.h>
/* extern int thatmain(void); */

#ifdef SGI
#include "gl.h"
#include "device.h"
#else
#define originx 0
#define originy 0
typedef short Int16;
typedef float Float32;
#include "vogl.h"
#include "vodevice.h"
#endif            
}

#ifndef Bool
#define Bool int
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
