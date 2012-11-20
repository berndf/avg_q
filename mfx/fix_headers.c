/*
 * Copyright (C) 1995,1997 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
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
