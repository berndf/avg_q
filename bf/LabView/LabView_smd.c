/*
 * Copyright (C) 2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member description' arrays for the LabView format
 *			-- Bernd Feige 26.03.2008 */

#include <read_struct.h>
#include "LabView.h"

struct_member_description smd_LabView_TD1[]={
 {STRUCT_MEMBER_UINT, "S"},
 {STRUCT_MEMBER_UINT, "DR"},
 {STRUCT_MEMBER_UINT, "B"},
 {STRUCT_MEMBER_UINT, "P"},
 {STRUCT_MEMBER_UINT, "T"},
 {STRUCT_MEMBER_UINT, "R"},
 {STRUCT_MEMBER_UINT, "Time"},
 {STRUCT_MEMBER_UINT, "TriggerOut"},
 {STRUCT_MEMBER_UINT, "AUX"},
 {STRUCT_MEMBER_UINT, "SecTot"},
 {STRUCT_MEMBER_UINT, "SecDR"}
};

struct_member_description smd_LabView_TD2[]={
 {STRUCT_MEMBER_UINT, "nr_of_points"},
 {STRUCT_MEMBER_UINT, "nr_of_channels"}
};
