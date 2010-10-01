/*
 * Copyright (C) 1992,1993,1998,2000,2003 Bernd Feige
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

#include <math.h> /* For rint() */

#ifdef __MINGW32__
#if __GNUC__ < 3
#define rint(f) ((int)((f)+0.5))
#endif
#endif

/* The DATACONV macro does the transformation of short data into a floating point number */
#define DATACONV(colstruct, datashort) (colstruct.shift_factor+(datashort)*colstruct.conv_factor)
/* The CONVDATA macro does the reverse transformation */
#define CONVDATA(colstruct, datafloat) ((short)rint(((datafloat)-colstruct.shift_factor)/colstruct.conv_factor))
/* The WRITECONVSTRUCT macro writes the necessary translation codes into the colstruct;
 * The last term for colstruct.shift_factor is due to the asymmetry of the short data range */
#define WRITECONVSTRUCT(colstruct, newymin, newymax) colstruct.ymin=newymin; colstruct.ymax=newymax; colstruct.conv_factor=(newymax-newymin)/65535; colstruct.shift_factor=(newymax+newymin)/2+0.5*colstruct.conv_factor

#define IS_TRIGGER(a) (((a)&0x7f00)!=0)
#define TRIGGERCODE(a) ((a)&0xff)
#define MAKE_TRIGGER(a) ((a)|0x100)
