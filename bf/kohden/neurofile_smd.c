/*
 * Copyright (C) 1997,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
