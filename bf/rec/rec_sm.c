/*
 * Copyright (C) 1996,2000,2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the synamps format
 *			-- Bernd Feige 6.08.1995 */

#include <stdio.h>
#include <read_struct.h>
#include "RECfile.h"

struct_member sm_REC_file[]={
 {sizeof(REC_file_header), 256, 0, 0},
 {(long)&((REC_file_header *)NULL)->version, 0, 8, 0},
 {(long)&((REC_file_header *)NULL)->patient, 8, 80, 0},
 {(long)&((REC_file_header *)NULL)->recording, 88, 80, 0},
 {(long)&((REC_file_header *)NULL)->startdate, 168, 8, 0},
 {(long)&((REC_file_header *)NULL)->starttime, 176, 8, 0},
 {(long)&((REC_file_header *)NULL)->bytes_in_header, 184, 8, 0},
 {(long)&((REC_file_header *)NULL)->data_format_version, 192, 44, 0},
 {(long)&((REC_file_header *)NULL)->nr_of_records, 236, 8, 0},
 {(long)&((REC_file_header *)NULL)->duration_s, 244, 8, 0},
 {(long)&((REC_file_header *)NULL)->nr_of_channels, 252, 4, 0},
 {0,0,0,0}
};

/* This is not actually used for reading because in the file, info of one sort
 * for all channels appears in one sequence, then the next info... */
struct_member sm_REC_channel_header[]={
 {sizeof(One_REC_channel_header), CHANNELHEADER_SIZE_PER_CHANNEL, 0, 0},
 {(long)&((One_REC_channel_header *)NULL)->label, 0, 16, 0},
 {(long)&((One_REC_channel_header *)NULL)->transducer, 16, 80, 0},
 {(long)&((One_REC_channel_header *)NULL)->dimension, 96, 8, 0},
 {(long)&((One_REC_channel_header *)NULL)->physmin, 104, 8, 0},
 {(long)&((One_REC_channel_header *)NULL)->physmax, 112, 8, 0},
 {(long)&((One_REC_channel_header *)NULL)->digmin, 120, 8, 0},
 {(long)&((One_REC_channel_header *)NULL)->digmax, 128, 8, 0},
 {(long)&((One_REC_channel_header *)NULL)->prefiltering, 136, 80, 0},
 {(long)&((One_REC_channel_header *)NULL)->samples_per_record, 216, 8, 0},
 {(long)&((One_REC_channel_header *)NULL)->reserved, 224, 32, 0},
 {0,0,0,0}
};
