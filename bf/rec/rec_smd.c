/*
 * Copyright (C) 2000,2008 Bernd Feige
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
/* Definition of `structure member description' arrays for the EDF format
 *			-- Bernd Feige 3.3.2000 */

#include <stdio.h>
#include <read_struct.h>
#include "RECfile.h"

struct_member_description smd_REC_file[]={
 {STRUCT_MEMBER_CHAR, "version"},
 {STRUCT_MEMBER_CHAR, "patient"},
 {STRUCT_MEMBER_CHAR, "recording"},
 {STRUCT_MEMBER_CHAR, "startdate"},
 {STRUCT_MEMBER_CHAR, "starttime"},
 {STRUCT_MEMBER_CHAR, "bytes_in_header"},
 {STRUCT_MEMBER_CHAR, "data_format_version"},
 {STRUCT_MEMBER_CHAR, "nr_of_records"},
 {STRUCT_MEMBER_CHAR, "duration_s"},
 {STRUCT_MEMBER_CHAR, "nr_of_channels"},
};

struct_member_description smd_REC_channel_header[]={
 {STRUCT_MEMBER_CHAR, "label"},
 {STRUCT_MEMBER_CHAR, "transducer"},
 {STRUCT_MEMBER_CHAR, "dimension"},
 {STRUCT_MEMBER_CHAR, "physmin"},
 {STRUCT_MEMBER_CHAR, "physmax"},
 {STRUCT_MEMBER_CHAR, "digmin"},
 {STRUCT_MEMBER_CHAR, "digmax"},
 {STRUCT_MEMBER_CHAR, "prefiltering"},
 {STRUCT_MEMBER_CHAR, "samples_per_record"},
 {STRUCT_MEMBER_CHAR, "reserved"},
};
