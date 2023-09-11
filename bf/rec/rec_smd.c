/*
 * Copyright (C) 2000,2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
