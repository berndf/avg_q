// Copyright (C) 1996,2000 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#ifndef _General_HH
#define _General_HH

extern "C" {
#include <stddef.h>
#include <stdio.h>
#include <math.h>
/* extern int thatmain(void); */

#ifdef WITH_POSPLOT
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
