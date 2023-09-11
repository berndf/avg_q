/*
 * Copyright (C) 2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member description' arrays for the Sigma PLpro format
 *			-- Bernd Feige 14.08.2020 */

#include <stdio.h>
#include <read_struct.h>
#include "sigma_header.h"

struct_member_description smd_sigma_file[]={
 {STRUCT_MEMBER_CHAR, "version"},
 {STRUCT_MEMBER_INT, "headlen"},
 {STRUCT_MEMBER_INT, "pos_channelheader"},
 {STRUCT_MEMBER_INT, "pos_patinfo"},
};
struct_member_description smd_sigma_file1[]={
 {STRUCT_MEMBER_INT, "n_channels"},
};
struct_member_description smd_sigma_channel[]={
 {STRUCT_MEMBER_INT, "channelno"},
 {STRUCT_MEMBER_INT, "notch"},
 {STRUCT_MEMBER_INT, "sfreq_len"},
 {STRUCT_MEMBER_CHAR, "sfreq"},
 {STRUCT_MEMBER_INT, "LowPass_len"},
 {STRUCT_MEMBER_CHAR, "LowPass"},
 {STRUCT_MEMBER_INT, "HighPass_len"},
 {STRUCT_MEMBER_CHAR, "HighPass"},
 {STRUCT_MEMBER_INT, "gerfac_len"},
 {STRUCT_MEMBER_CHAR, "gerfac"},
 {STRUCT_MEMBER_INT, "Offset"},
 {STRUCT_MEMBER_INT, "calibration_len"},
 {STRUCT_MEMBER_CHAR, "calibration"},
 {STRUCT_MEMBER_INT, "dimension_len"},
 {STRUCT_MEMBER_CHAR, "dimension"},
 {STRUCT_MEMBER_INT, "Impedance_len"},
 {STRUCT_MEMBER_CHAR, "Impedance"},
 {STRUCT_MEMBER_INT, "Sensitivity_len"},
 {STRUCT_MEMBER_CHAR, "Sensitivity"},
 {STRUCT_MEMBER_INT, "XPos"},
 {STRUCT_MEMBER_INT, "YPos"},
 {STRUCT_MEMBER_INT, "Label_len"},
 {STRUCT_MEMBER_CHAR, "Label"},
 {STRUCT_MEMBER_INT, "Reference_len"},
 {STRUCT_MEMBER_CHAR, "Reference"},
 {STRUCT_MEMBER_INT, "Unit_len"},
 {STRUCT_MEMBER_CHAR, "Unit"},
 {STRUCT_MEMBER_INT, "Transducer_len"},
 {STRUCT_MEMBER_CHAR, "Transducer"},
};
