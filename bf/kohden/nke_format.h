/*
 * Copyright (C) 2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * Header for Nihon Kohden 2110/1100 formats
 */
#ifndef _NKFORMAT_H_INCLUDED
#define NKE_DESCRIPTION_LENGTH 20
#define NKE_TIMESTAMP_LENGTH 25
struct nke_file_header {
 char	signature1[16];
 char	addfilename1[16];
 char	addfilename2[32];
 char	datetime[16];
 char	dummy2[31];
 char	swversion[18];
 char	signature2[16];
 unsigned char	ctl_block_cnt;
};
struct nke_control_block {
 unsigned long	address;
 unsigned long	dummy1;
 unsigned long	dummy2;
 unsigned long	dummy3;
 unsigned long	dummy4;
};
struct nke_data_block1 {
 unsigned char	dummy1;
 unsigned char	name[16];
 unsigned char	datablock_cnt;
 unsigned long	wfmblock_address;
};
struct nke_data_block {
 unsigned char	name[16];
 unsigned long	wfmblock_address;
};
struct nke_wfm_block {
 unsigned long	dummy1;
 unsigned long	dummy2;
 unsigned long	dummy3;
 unsigned long	dummy4;
 unsigned long	dummy5;
 unsigned char	year; /* Next 6 values in BCD */
 unsigned char	month;
 unsigned char	day;
 unsigned char	hour;
 unsigned char	minute;
 unsigned char	second;
 unsigned short	sfreq; /* Must be bitwise 'and'ed: & 0x3fff */
 unsigned long	record_duration; /* In 0.1s */
 unsigned short	dummy6;
 unsigned short	dummy7;
 unsigned char	channels;
};
struct nke_channel_block {
 unsigned char	chan;
 unsigned char	dummy1;
 unsigned long	dummy2;
 unsigned long	dummy3;
};

struct nke_log_header {
 unsigned char	dummy1;
 char	signature[16];
 unsigned char	dummy2;
 unsigned char	log_block_cnt;
 unsigned char	dummy3;
};
struct nke_log_block {
 unsigned char	description[NKE_DESCRIPTION_LENGTH];
 unsigned char	timestamp[NKE_TIMESTAMP_LENGTH];
};

#include <read_struct.h>
extern struct_member sm_nke_file_header[];
extern struct_member_description smd_nke_file_header[];
extern struct_member sm_nke_control_block[];
extern struct_member_description smd_nke_control_block[];
extern struct_member sm_nke_data_block1[];
extern struct_member_description smd_nke_data_block1[];
extern struct_member sm_nke_data_block[];
extern struct_member_description smd_nke_data_block[];
extern struct_member sm_nke_wfm_block[];
extern struct_member_description smd_nke_wfm_block[];
extern struct_member sm_nke_channel_block[];
extern struct_member_description smd_nke_channel_block[];
extern struct_member sm_nke_log_header[];
extern struct_member_description smd_nke_log_header[];
extern struct_member sm_nke_log_block[];
extern struct_member_description smd_nke_log_block[];
#endif
