/*
 * Copyright (C) 2008 Bernd Feige
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
