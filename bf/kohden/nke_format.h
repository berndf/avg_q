/*
 * Header for Nihon Kohden 2110/1100 formats
 */
#ifndef _NKFORMAT_H_INCLUDED
struct nke_control_block {
 unsigned long	address;
 unsigned long	dummy1;
 unsigned long	dummy2;
 unsigned long	dummy3;
 unsigned long	dummy4;
};
struct nke_data_block {
 unsigned long	dummy1;
 unsigned long	dummy2;
 unsigned long	dummy3;
 unsigned long	dummy4;
 unsigned char	dummy5;
 unsigned char	datablock_cnt;
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

extern struct_member sm_nke_control_block[];
extern struct_member_description smd_nke_control_block[];
extern struct_member sm_nke_data_block[];
extern struct_member_description smd_nke_data_block[];
extern struct_member sm_nke_wfm_block[];
extern struct_member_description smd_nke_wfm_block[];
extern struct_member sm_nke_channel_block[];
extern struct_member_description smd_nke_channel_block[];
#endif