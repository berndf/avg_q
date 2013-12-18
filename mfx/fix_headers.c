/*
 * Copyright (C) 1995,1997,2012 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*
 * fix_headers encapsulates the process of changing the various MFX headers
 * between little- and big-endian number representation. Calling any of these 
 * twice on the same data should result in the original header.
 *						Bernd Feige 22.05.1995
 */

#include "mfx_file.h"
#include "mfx_old_formats.h"
#include "Intel_compat.h"
#include "fix_headers.h"

void
fix_typeheader(struct mfx_header_format_data *typeheader_p) {
 Intel_int16((uint16_t *)&typeheader_p->mfx_header_format);
 Intel_int16((uint16_t *)&typeheader_p->mfx_data_format);
}

void
fix_fileheader(struct mfx_header_data001 *fileheader_p) {
 Intel_float(&fileheader_p->epoch_begin_latency);
 Intel_float(&fileheader_p->sample_period);
 Intel_int32((uint32_t *)&fileheader_p->total_epochs);
 Intel_int32((uint32_t *)&fileheader_p->input_epochs);
 Intel_int32((uint32_t *)&fileheader_p->pts_in_epoch);
 Intel_int32((uint32_t *)&fileheader_p->total_chans);
}

void
fix_channelheader(struct mfx_channel_data001 *channelheader_p) {
 int i;
 Intel_int16(&channelheader_p->data_type);
 Intel_double(&channelheader_p->ymin);
 Intel_double(&channelheader_p->ymax);
 Intel_double(&channelheader_p->conv_factor);
 Intel_double(&channelheader_p->shift_factor);
 for (i=0; i<22; i++) {
  Intel_double(channelheader_p->position+i);
 }
}
