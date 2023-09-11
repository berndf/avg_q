/*
 * Copyright (C) 2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the sigma PLpro format
 *			-- Bernd Feige 14.08.2020 */

#include <stdio.h>
#include <read_struct.h>
#include "sigma_header.h"

struct_member sm_sigma_file[]={
 {sizeof(sigma_file_header), 148, 0, 0},
 {(long)&((sigma_file_header *)NULL)->version, 0, 16, 0},
 {(long)&((sigma_file_header *)NULL)->headlen, 16, 4, 1},
 {(long)&((sigma_file_header *)NULL)->pos_channelheader, 20, 4, 1},
 {(long)&((sigma_file_header *)NULL)->pos_patinfo, 28, 4, 1},
 {0,0,0,0}
};
struct_member sm_sigma_file1[]={
 {sizeof(int16_t), 2, 0, 0},
 {0, 0, 2, 1},
 {0,0,0,0}
};
struct_member sm_sigma_channel[]={
 {sizeof(sigma_channel_header), 203, 0, 0},
 {(long)&((sigma_channel_header *)NULL)->channelno, 0, 2, 0},
 {(long)&((sigma_channel_header *)NULL)->notch, 2, 2, 0},
 {(long)&((sigma_channel_header *)NULL)->sfreq_len, 4, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->sfreq, 5, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->LowPass_len, 23, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->LowPass, 24, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->HighPass_len, 42, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->HighPass, 43, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->gerfac_len, 61, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->gerfac, 62, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->Offset, 80, 2, 1},
 {(long)&((sigma_channel_header *)NULL)->calibration_len, 82, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->calibration, 83, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->dimension_len, 101, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->dimension, 102, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->Impedance_len, 120, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->Impedance, 121, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->Sensitivity_len, 139, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->Sensitivity, 140, 18, 0},
 {(long)&((sigma_channel_header *)NULL)->XPos, 158, 4, 0},
 {(long)&((sigma_channel_header *)NULL)->YPos, 162, 4, 0},
 {(long)&((sigma_channel_header *)NULL)->Label_len, 166, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->Label, 167, 8, 0},
 {(long)&((sigma_channel_header *)NULL)->Reference_len, 175, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->Reference, 176, 10, 0},
 {(long)&((sigma_channel_header *)NULL)->Unit_len, 186, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->Unit, 187, 7, 0},
 {(long)&((sigma_channel_header *)NULL)->Transducer_len, 194, 1, 0},
 {(long)&((sigma_channel_header *)NULL)->Transducer, 195, 8, 0},
 {0,0,0,0}
};
