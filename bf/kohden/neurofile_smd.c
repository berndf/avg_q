/*
 * Copyright (C) 1997 Bernd Feige
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
/* Definition of `structure member description' arrays for the NeuroFile format
 *			-- Bernd Feige 26.09.1997 */

#include <stdio.h>
#include <read_struct.h>
#include "neurofile.h"

struct_member_description smd_sequence[]={
 {STRUCT_MEMBER_CHAR, "comment"},
 {STRUCT_MEMBER_INT, "second"},
 {STRUCT_MEMBER_INT, "minute"},
 {STRUCT_MEMBER_INT, "hour"},
 {STRUCT_MEMBER_INT, "day"},
 {STRUCT_MEMBER_INT, "month"},
 {STRUCT_MEMBER_INT, "year"},
 {STRUCT_MEMBER_INT, "length"},
 {STRUCT_MEMBER_CHAR, "els"},
 {STRUCT_MEMBER_INT, "ref"},
 {STRUCT_MEMBER_INT, "nchan"},
 {STRUCT_MEMBER_INT, "nfast"},
 {STRUCT_MEMBER_INT, "nwil"},
 {STRUCT_MEMBER_INT, "ncoh"},
 {STRUCT_MEMBER_CHAR, "elnam"},
 {STRUCT_MEMBER_INT, "coord"},
 {STRUCT_MEMBER_INT, "ewil"},
 {STRUCT_MEMBER_INT, "ecoh"},
 {STRUCT_MEMBER_FLOAT, "gain"},
 {STRUCT_MEMBER_INT, "fastrate"},
 {STRUCT_MEMBER_INT, "slowrate"},
 {STRUCT_MEMBER_INT, "syncode"},
 {STRUCT_MEMBER_INT, "sync0"}
};
