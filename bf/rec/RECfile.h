/*
 * Copyright (C) 1996,2000,2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
typedef struct {
 char version[8];
 char patient[80];
 char recording[80];
 char startdate[8];
 char starttime[8];
 char bytes_in_header[8];
 char data_format_version[44];
 char nr_of_records[8];
 char duration_s[8];
 char nr_of_channels[4];	/* `signals' in the paper... */
} REC_file_header;

#define CHANNELHEADER_SIZE_PER_CHANNEL 256
typedef struct {
 char (*label)[16];
 char (*transducer)[80];
 char (*dimension)[8];
 char (*physmin)[8];
 char (*physmax)[8];
 char (*digmin)[8];
 char (*digmax)[8];
 char (*prefiltering)[80];
 char (*samples_per_record)[8];
 char (*reserved)[32];
} REC_channel_header;

typedef struct {
 char label[16];
 char transducer[80];
 char dimension[8];
 char physmin[8];
 char physmax[8];
 char digmin[8];
 char digmax[8];
 char prefiltering[80];
 char samples_per_record[8];
 char reserved[32];
} One_REC_channel_header;

extern struct_member sm_REC_file[];
extern struct_member sm_REC_channel_header[];
extern struct_member_description smd_REC_file[];
extern struct_member_description smd_REC_channel_header[];
