/*
 * Copyright (C) 1992-1994 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfx_header_format.h
 * 
 * 
 * This header file is for header information dealing withe the Munster File eXchange format (MFX)
 *
 *  NOTE :
 *
 *  This header file MUST be c-compatible for porting to the IBM workstations
 */

/*
 *========================================================================================
 *========================================================================================
 * this section includes header files important for supporting the MFX file format
 *========================================================================================
 *========================================================================================
 */
#include "mfx.h"

/*
 *==============================================================================
 *==============================================================================
 * this section is the header data format
 *==============================================================================
 *==============================================================================
 */
struct mfx_header_format_data
{
	short mfx_header_format;
	short mfx_data_format;
};

/*
 *==============================================================================
 *==============================================================================
 * this section is the header format     mfx_header_format == 1
 *==============================================================================
 *==============================================================================
 */

#define MFX_FILEDESCRIPTORLENGTH 512
#define MFX_TIMESTAMPLENGTH 26

struct mfx_header_data001
{
	char file_descriptor[MFX_FILEDESCRIPTORLENGTH];
	char time_stamp[MFX_TIMESTAMPLENGTH];
	float epoch_begin_latency;
	float sample_period;
	int total_epochs;
	int input_epochs;
	int pts_in_epoch;
	int total_chans;
	char reserved[32];
};

/*
 *==============================================================================
 *==============================================================================
 * this section is the channel header format for mfx_header_format == 1
 *==============================================================================
 *==============================================================================
 */
enum datatypes001 {
 DT_NONE=0, DT_MEGTYPE=1, DT_EEGTYPE, DT_REFERENCE, DT_EXTERNAL, 
 DT_TRIGGER, DT_UTILITY, DT_DERIVED, DT_SHORTENED, DT_LASTENTRY
};

#define MFX_CHANNELNAMELENGTH	16
#define MFX_YAXISLABELLENGTH	16

struct mfx_channel_data001
{
	unsigned short data_type;
	char channel_name[MFX_CHANNELNAMELENGTH];
	char yaxis_label[MFX_YAXISLABELLENGTH];
	short pad_to_4byte;
	long pad_to_8byte;
	double ymin;
	double ymax;
	double conv_factor;
	double shift_factor;
	double position[3];
	double vector[3];
	double transform[4][4];
};
