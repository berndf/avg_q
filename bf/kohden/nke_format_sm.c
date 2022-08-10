/*
 * Copyright (C) 2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the NeuroFile format
 *			-- Bernd Feige 26.09.1997 */

#include <stdio.h>
#include <read_struct.h>
#include "nke_format.h"

struct_member sm_nke_control_block[]={
 {sizeof(struct nke_control_block), 20, 0, 0},
 {(long)&((struct nke_control_block *)NULL)->address, 0, 4, 1},
 {(long)&((struct nke_control_block *)NULL)->dummy1, 4, 4, 1},
 {(long)&((struct nke_control_block *)NULL)->dummy2, 8, 4, 1},
 {(long)&((struct nke_control_block *)NULL)->dummy3, 12, 4, 1},
 {(long)&((struct nke_control_block *)NULL)->dummy4, 16, 4, 1},
 {0,0,0,0}
};
struct_member sm_nke_data_block[]={
 {sizeof(struct nke_data_block), 22, 0, 0},
 {(long)&((struct nke_data_block *)NULL)->dummy1, 0, 4, 1},
 {(long)&((struct nke_data_block *)NULL)->dummy2, 4, 4, 1},
 {(long)&((struct nke_data_block *)NULL)->dummy3, 8, 4, 1},
 {(long)&((struct nke_data_block *)NULL)->dummy4, 12, 4, 1},
 {(long)&((struct nke_data_block *)NULL)->dummy5, 16, 1, 0},
 {(long)&((struct nke_data_block *)NULL)->datablock_cnt, 17, 1, 0},
 {(long)&((struct nke_data_block *)NULL)->wfmblock_address, 18, 4, 1},
 {0,0,0,0}
};
struct_member sm_nke_wfm_block[]={
 {sizeof(struct nke_wfm_block), 39, 0, 0},
 {(long)&((struct nke_wfm_block *)NULL)->dummy1, 0, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->dummy2, 4, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->dummy3, 8, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->dummy4, 12, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->dummy5, 16, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->year, 20, 1, 0},
 {(long)&((struct nke_wfm_block *)NULL)->month, 21, 1, 0},
 {(long)&((struct nke_wfm_block *)NULL)->day, 22, 1, 0},
 {(long)&((struct nke_wfm_block *)NULL)->hour, 23, 1, 0},
 {(long)&((struct nke_wfm_block *)NULL)->minute, 24, 1, 0},
 {(long)&((struct nke_wfm_block *)NULL)->second, 25, 1, 0},
 {(long)&((struct nke_wfm_block *)NULL)->sfreq, 26, 2, 1},
 {(long)&((struct nke_wfm_block *)NULL)->record_duration, 28, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->dummy6, 32, 4, 1},
 {(long)&((struct nke_wfm_block *)NULL)->dummy7, 36, 2, 1},
 {(long)&((struct nke_wfm_block *)NULL)->channels, 38, 1, 0},
 {0,0,0,0}
};
struct_member sm_nke_channel_block[]={
 {sizeof(struct nke_channel_block), 10, 0, 0},
 {(long)&((struct nke_channel_block *)NULL)->chan, 0, 1, 0},
 {(long)&((struct nke_channel_block *)NULL)->dummy1, 1, 1, 0},
 {(long)&((struct nke_channel_block *)NULL)->dummy2, 2, 4, 1},
 {(long)&((struct nke_channel_block *)NULL)->dummy3, 6, 4, 1},
 {0,0,0,0}
};

struct_member sm_nke_log_block[]={
 {sizeof(struct nke_log_block), NKE_DESCRIPTION_LENGTH+NKE_TIMESTAMP_LENGTH, 0, 0},
 {(long)&((struct nke_log_block *)NULL)->description, 0, NKE_DESCRIPTION_LENGTH, 0},
 {(long)&((struct nke_log_block *)NULL)->timestamp, NKE_DESCRIPTION_LENGTH, NKE_TIMESTAMP_LENGTH, 0},
 {0,0,0,0}
};
