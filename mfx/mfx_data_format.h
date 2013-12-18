/*
 * Copyright (C) 1992,1993,1998,2000,2003,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*
 * mfx_data_format.h
 * 
 *
 * This header file is for data information dealing with the Munster File eXchange format (MFX)
 * 
 *  NOTE :    
 * 
 *  This header file MUST be c-compatible for porting to the IBM workstations 
 *
*/

#include <stdint.h>
#include <math.h> /* For rint() */

#ifdef __MINGW32__
#if __GNUC__ < 3
#define rint(f) ((int)((f)+0.5))
#endif
#endif

/* The DATACONV macro does the transformation of short data into a floating point number */
#define DATACONV(colstruct, datashort) (colstruct.shift_factor+(datashort)*colstruct.conv_factor)
/* The CONVDATA macro does the reverse transformation */
#define CONVDATA(colstruct, datafloat) ((int16_t)rint(((datafloat)-colstruct.shift_factor)/colstruct.conv_factor))
/* The WRITECONVSTRUCT macro writes the necessary translation codes into the colstruct;
 * The last term for colstruct.shift_factor is due to the asymmetry of the short data range */
#define WRITECONVSTRUCT(colstruct, newymin, newymax) colstruct.ymin=newymin; colstruct.ymax=newymax; colstruct.conv_factor=(newymax-newymin)/65535; colstruct.shift_factor=(newymax+newymin)/2+0.5*colstruct.conv_factor

#define IS_TRIGGER(a) (((a)&0x7f00)!=0)
#define TRIGGERCODE(a) ((a)&0xff)
#define MAKE_TRIGGER(a) ((a)|0x100)
