/*
 * Copyright (C) 2012 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
