/*
 * Copyright (C) 2012 Bernd Feige
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
/*{{{}}}*/
/* Definition of `structure member' arrays for the 'tucker' (EGI 'simple
 * binary') format			-- Bernd Feige 18.12.2012 */

#include <stdio.h>
#include <read_struct.h>
#include "tucker.h"

struct_member_description smd_tucker[]={
 {STRUCT_MEMBER_INT, "Version"},
 {STRUCT_MEMBER_INT, "Year"},
 {STRUCT_MEMBER_INT, "Month"},
 {STRUCT_MEMBER_INT, "Day"},
 {STRUCT_MEMBER_INT, "Hour"},
 {STRUCT_MEMBER_INT, "Minute"},
 {STRUCT_MEMBER_INT, "Sec"},
 {STRUCT_MEMBER_INT, "MSec"},
 {STRUCT_MEMBER_INT, "SampRate"},
 {STRUCT_MEMBER_INT, "NChan"},
 {STRUCT_MEMBER_INT, "Gain"},
 {STRUCT_MEMBER_INT, "Bits"},
 {STRUCT_MEMBER_INT, "Range"},
 {STRUCT_MEMBER_INT, "NPoints"},
 {STRUCT_MEMBER_INT, "NEvents"},
};
