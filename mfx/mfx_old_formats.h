/*
 * Copyright (C) 1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 *==============================================================================
 *==============================================================================
 * this section is the old channel format     mfx_header_format == 1
 *==============================================================================
 *==============================================================================
*/
struct mfx_channel_data001a
{
	unsigned short data_type;
	char channel_name[16];
	char yaxis_label[16];
	short pad_to_4byte;
	long pad_to_8byte;
	double ymin;
	double ymax;
	double position[3];
	double vector[3];
	double transform[4][4];
};
