/*
 * Copyright (C) 1992-1994,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfx_infoout dumps the contents of the file and channel headers
 * to the stream outfile (in ASCII). This is modeled after corresponding
 * program parts by Scott Hampson.
 */

#include <stdio.h>
#include "mfx_file.h"

static char *datatype_names[DT_LASTENTRY]={
 "NONE", "MEG", "EEG", "REFERENCE", "EXTERNAL",
 "TRIGGER", "UTILITY", "DERIVED", "SHORTENED"
};

mfx_errtype
mfx_infoout(MFX_FILE* mfxfile, FILE* outfile) {
 int i, j, k;

 fprintf(outfile, "mfx_file_name : %s\n", mfxfile->filename);
 switch (mfxfile->subversion) {
  case SUBVERSION_TILL1192:
   fprintf(outfile, " This is an old-style file (created before 12/92)\n");
   break;
  case SUBVERSION_PAST1192:
   fprintf(outfile, " This is a new-style file (created past 11/92)\n");
   break;
  default:
   fprintf(outfile, " UNKNOWN FILE SUB-VERSION\n");
   break;
 }
 fprintf(outfile, "\n====== mfx_header_format_header ======\n");
 fprintf(outfile, "mfx_header_format	= %d\n", mfxfile->typeheader.mfx_header_format);
 fprintf(outfile, "mfx_data_format	= %d\n\n", mfxfile->typeheader.mfx_data_format);

 fprintf(outfile, "file_descriptor	= %s\n", mfxfile->fileheader.file_descriptor);
 fprintf(outfile, "time_stamp		= %s\n", mfxfile->fileheader.time_stamp);
 fprintf(outfile, "epoch_begin_latency	= %g\n", mfxfile->fileheader.epoch_begin_latency);
 fprintf(outfile, "sample_period		= %g (=>Sfreq = %g Hz)\n", mfxfile->fileheader.sample_period, 1.0/mfxfile->fileheader.sample_period);
 fprintf(outfile, "total_epochs		= %d\n", mfxfile->fileheader.total_epochs);
 fprintf(outfile, "input_epochs		= %d\n", mfxfile->fileheader.input_epochs);
 fprintf(outfile, "pts_in_epoch		= %d (= %g s)\n", mfxfile->fileheader.pts_in_epoch, mfxfile->fileheader.pts_in_epoch*mfxfile->fileheader.sample_period);
 fprintf(outfile, "total_chans		= %d\n", mfxfile->fileheader.total_chans);
 fprintf(outfile, "reserved		= %s\n", mfxfile->fileheader.reserved);

 for (i = 0; i < mfxfile->fileheader.total_chans; i++) {
  fprintf(outfile, "\nchannel_number		= %d\n", i+1);
  fprintf(outfile, "data_type		= %d (%s)\n", mfxfile->channelheaders[i].data_type, (mfxfile->channelheaders[i].data_type>=DT_LASTENTRY ? "INVALID !" : datatype_names[mfxfile->channelheaders[i].data_type]));
  fprintf(outfile, "channel_name		= %s\n", mfxfile->channelheaders[i].channel_name);
  fprintf(outfile, "yaxis_label		= %s\n", mfxfile->channelheaders[i].yaxis_label);
  fprintf(outfile, "ymin			= %g\n", mfxfile->channelheaders[i].ymin);
  fprintf(outfile, "ymax			= %g\n", mfxfile->channelheaders[i].ymax);
  fprintf(outfile, "conv_factor		= %g\n", mfxfile->channelheaders[i].conv_factor);
  fprintf(outfile, "shift_factor		= %g\n", mfxfile->channelheaders[i].shift_factor);

  fprintf(outfile, "position		= ");
  for (j = 0; j < 3; j++) fprintf(outfile, "% 7.5f\t", mfxfile->channelheaders[i].position[j]);
  fprintf(outfile, "\n");
  fprintf(outfile, "vector			= ");
  for (j = 0; j < 3; j++) fprintf(outfile, "% 7.5f\t", mfxfile->channelheaders[i].vector[j]);
  fprintf(outfile, "\n");
  fprintf(outfile, "transform		= ");
  for (j = 0; j < 4; j++) {
   for (k = 0; k < 4; k++) fprintf(outfile, "% 7.5f\t", mfxfile->channelheaders[i].transform[j][k]);
   fprintf(outfile, "\n			  ");
  }
 }
 return mfx_lasterr=MFX_NOERR;
}
