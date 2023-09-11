/*
 * Copyright (C) 1993,1994 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfxcleartrigs mfxfile
 * is a small mfx application that clears all trigger codes in the
 * TRIGGER channel of the specified mfxfile.
 */

#include <stdio.h>
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include "mfx_file.h"

extern char *optarg;
extern int optind, opterr;

short trig;
float seconds;
char *inbuf, *endnum;
MFX_FILE *myfile;

int main(int argc, char **argv) {
 struct mfx_channel_data001 *trchannelp;
 int pos, len, leave_first=FALSE, verbose=FALSE, c, errflag=0, trigs=0;
 int valid_offset=0;
 short trigshort, nullshort;

 while ((c=getopt(argc, argv, "F:fv"))!=EOF) {
  switch (c) {
   case 'F':
    valid_offset=atoi(optarg);
   case 'f':
    leave_first=TRUE;
    break;
   case 'v':
    verbose=TRUE;
    break;
   case '?':
   default:
    errflag++;
    continue;
  }
 }
 if (argc-optind!=1 || errflag>0) {
  fprintf(stderr, "This is mfxcleartrigs using the mfx library version\n%s\n\n"
		  "Usage: %s [-f | -F offset] [-v] mfxfile\n"
		  " Clears [-f: reduces to the first] the triggers in the mfxfile; -v: verbose mode\n"
                  " -F offset: takes the valid trigger code to write from triglocation+offset-1\n"
		  , mfx_version, argv[0]);
  return 1;
 }
 if ((myfile=mfx_open(argv[optind], "r+b", MFX_SHORTS))==NULL) {
  fprintf(stderr, "mfx_open: %s\n", mfx_errors[mfx_lasterr]);
  return 2;
 }
 if (mfx_cselect(myfile, "TRIGGER")==0) {
  fprintf(stderr, "mfx_cselect: %s\n", mfx_errors[mfx_lasterr]);
  return 3;
 }
 trchannelp=myfile->channelheaders+myfile->selected_channels[0]-1;
 /* This is the ultimate choice for a NULL trigger value */
 nullshort=CONVDATA((*trchannelp), trchannelp->ymin>0 ? trchannelp->ymin : 0.0);
 while (mfx_lasterr==MFX_NOERR && 
	(trig=mfx_seektrigger(myfile, "TRIGGER", 0))!=0) {
  pos=mfx_tell(myfile);
  if (leave_first) {
   mfx_seek(myfile, 1, 1);
   len=0;
  } else {
   mfx_write(&nullshort, 1, myfile);
   len=1;
  }
  while (mfx_read(&trigshort, 1, myfile)==MFX_NOERR && 
	(trigshort=DATACONV((*trchannelp), trigshort)+0.1), 
	IS_TRIGGER(trigshort)) {
   mfx_seek(myfile, -1, 1);
   /* If not leave_first then don't even warn about changing trigcodes... */
   if (leave_first && TRIGGERCODE(trigshort)!=trig) {
    if (valid_offset!=0) {
     if (len==valid_offset) {
      printf("The valid trigger code number %d is %d, not %d -- writing...\n", valid_offset, TRIGGERCODE(trigshort), trig);
      trig=TRIGGERCODE(trigshort);
      mfx_seek(myfile, -len-1, 1);
      if (mfx_write(&trigshort, 1, myfile)!=MFX_NOERR) break;
      mfx_seek(myfile, len, 1);
     }
    } else {
     printf("Warning: Trigger number %d in this sequence is %d!=%d\n", len+2, TRIGGERCODE(trigshort), trig);
    }
   }
   if (mfx_write(&nullshort, 1, myfile)!=MFX_NOERR) break;
   len++;
  }
  if (len>0) trigs++;
  if (verbose) printf("%s trigger code %2d at %7d (%gs) - length %d\n", leave_first ? "Fixing" : "Clearing", trig, pos, (float)pos*myfile->fileheader.sample_period, len);
 }
 printf("%d triggers %s.\n", trigs, leave_first ? "fixed" : "cleared");
 fprintf(stderr, "Last error: %s\n", mfx_errors[mfx_lasterr]);
 mfx_close(myfile);
 return 0;
}
