/*
 * Copyright (C) 2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member description' arrays for the NeuroFile format
 *			-- Bernd Feige 26.09.1997 */

#include <stdio.h>
#include <read_struct.h>
#include "nke_format.h"

struct_member_description smd_nke_file_header[]={
 {STRUCT_MEMBER_CHAR, "signature1"},
 {STRUCT_MEMBER_CHAR, "addfilename1"},
 {STRUCT_MEMBER_CHAR, "addfilename2"},
 {STRUCT_MEMBER_CHAR, "datetime"},
 {STRUCT_MEMBER_CHAR, "dummy2"},
 {STRUCT_MEMBER_CHAR, "swversion"},
 {STRUCT_MEMBER_CHAR, "signature2"},
 {STRUCT_MEMBER_UINT, "ctl_block_cnt"},
};
struct_member_description smd_nke_control_block[]={
 {STRUCT_MEMBER_INT, "address"},
 {STRUCT_MEMBER_XINT, "dummy1"},
 {STRUCT_MEMBER_XINT, "dummy2"},
 {STRUCT_MEMBER_XINT, "dummy3"},
 {STRUCT_MEMBER_XINT, "dummy4"},
};
struct_member_description smd_nke_data_block1[]={
 {STRUCT_MEMBER_INT, "dummy1"},
 {STRUCT_MEMBER_CHAR, "name"},
 {STRUCT_MEMBER_INT, "datablock_cnt"},
 {STRUCT_MEMBER_INT, "wfmblock_address"},
};
struct_member_description smd_nke_data_block[]={
 {STRUCT_MEMBER_CHAR, "name"},
 {STRUCT_MEMBER_INT, "wfmblock_address"},
};
struct_member_description smd_nke_wfm_block[]={
 {STRUCT_MEMBER_XINT, "dummy1"},
 {STRUCT_MEMBER_XINT, "dummy2"},
 {STRUCT_MEMBER_XINT, "dummy3"},
 {STRUCT_MEMBER_XINT, "dummy4"},
 {STRUCT_MEMBER_XINT, "dummy5"},
 {STRUCT_MEMBER_XINT, "year"},
 {STRUCT_MEMBER_XINT, "month"},
 {STRUCT_MEMBER_XINT, "day"},
 {STRUCT_MEMBER_XINT, "hour"},
 {STRUCT_MEMBER_XINT, "minute"},
 {STRUCT_MEMBER_XINT, "second"},
 {STRUCT_MEMBER_INT, "sfreq"},
 {STRUCT_MEMBER_INT, "record_duration"},
 {STRUCT_MEMBER_XINT, "dummy6"},
 {STRUCT_MEMBER_XINT, "dummy7"},
 {STRUCT_MEMBER_UINT, "channels"},
};
struct_member_description smd_nke_channel_block[]={
 {STRUCT_MEMBER_INT, "chan"},
 {STRUCT_MEMBER_XINT, "dummy1"},
 {STRUCT_MEMBER_XINT, "dummy2"},
 {STRUCT_MEMBER_XINT, "dummy3"},
};

struct_member_description smd_nke_log_header[]={
 {STRUCT_MEMBER_UINT, "dummy1"},
 {STRUCT_MEMBER_CHAR, "signature"},
 {STRUCT_MEMBER_UINT, "dummy2"},
 {STRUCT_MEMBER_UINT, "log_block_cnt"},
 {STRUCT_MEMBER_UINT, "dummy3"},
};
struct_member_description smd_nke_log_block[]={
 {STRUCT_MEMBER_CHAR, "description"},
 {STRUCT_MEMBER_CHAR, "timestamp"},
};
