/*
 * Copyright (C) 1993,1994,1999,2010,2013-2015,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfxwritetrigs mfxfile triggerfile
 * is a small mfx application that writes trigger codes to the
 * TRIGGER channel of the specified mfxfile; The trigger positions
 * in the triggerfile are given in points [-s: in seconds], the format is:
 * [Heading without any numbers]
 * pos11 pos12 ... pos1n
 * pos21 pos22 ... pos2n
 * ...
 * Where the respective triggercode is set to the number of the
 * column in which the position appears (1..n).
 * Negative positions are ignored, so that is a way to leave out
 * specific trigger codes.
 * Alternatively, mfxwritetrigs can read the output of mfxtriggers -v
 * as well (-t option)
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "mfx_file.h"

extern char *optarg;
extern int optind, opterr;
/*}}}  */

/*{{{  #defines*/
#define LENOFBUFFER 160
#define POS_START "at:"
#define CODE_START "code:"
/*}}}  */

/*{{{  Global vars*/
char buffer[LENOFBUFFER];
short trig;
char *inbuf, *endnum;
MFX_FILE *myfile;
FILE *trigfile;
/*}}}  */

/*{{{  void write1trig(double position, int code, int seconds) {*/
static void 
write1trig(double position, int code, int seconds) {
 printf("Writing code %d at %f%c\n", code, position, seconds ? 's' : 'p');
 if (seconds) position/=myfile->fileheader.sample_period;
 if (mfx_seek(myfile, (long)position, 0)!=MFX_NOERR) {
  fprintf(stderr, "mfx_seek: %s\n", mfx_errors[mfx_lasterr]);
  exit(4);
 }
 trig=MAKE_TRIGGER(code);
 trig=CONVDATA(myfile->channelheaders[myfile->selected_channels[0]-1], trig);
 if (mfx_write(&trig, 1, myfile)!=MFX_NOERR) {
  fprintf(stderr, "mfx_write: %s\n", mfx_errors[mfx_lasterr]);
  exit(5);
 }
}
/*}}}  */

/* This was taken from bf/trafo_std.c: */
/*{{{  read_trigger_from_trigfile(FILE *triggerfile, DATATYPE sfreq, long *trigpoint)*/
#define TRIGGER_LINEBUFFERLENGTH 80

static int
read_trigger_from_trigfile(FILE *triggerfile, float sfreq, long *trigpoint) {
 char buffer[TRIGGER_LINEBUFFERLENGTH];
 int code=0;

 /* The do while (1) is here to be able to skip comment lines */
 do {
  /* The trigger file format is  point code  on a line by itself. Lines starting
   * with '#' are ignored. The `point' string may be a floating point number
   * and may have 's' or 'ms' appended to denote seconds or milliseconds. */
  if (fgets(buffer, TRIGGER_LINEBUFFERLENGTH, triggerfile)!=NULL) {
   char *inbuffer=buffer;
   double trigpos;

   if (*buffer=='#' || *buffer=='\n') continue;
   trigpos=strtod(inbuffer, &inbuffer);
   switch (*inbuffer) {
    case 'm':
     trigpos/=1000.0;
     inbuffer++;
    case 's':
     trigpos*=sfreq;
     inbuffer++;
    default:
     *trigpoint=(long)trigpos;
     break;
   }
   code=strtol(inbuffer, &inbuffer, 10);
  }
  break;
 } while (1);

 return code;
}
/*}}}  */

int main(int argc, char **argv) {
 int i, c, errflag=0, seconds=FALSE;
 enum {
  JUSTPOINTS,
  MFXTRIGFORMAT,
  AVG_Q_FORMAT
 } trigfileformat=JUSTPOINTS;
 double position;

 /*{{{  Process commandline options*/
 while ((c=getopt(argc, argv, "stE"))!=EOF) {
  switch (c) {
   case 's':
    seconds=TRUE;
    break;
   case 't':
    trigfileformat=MFXTRIGFORMAT;
    break;
   case 'E':
    trigfileformat=AVG_Q_FORMAT;
    break;
   case '?':
   default:
    errflag++;
    continue;
  }
 }
 if (argc-optind!=2 || errflag>0) {
  fprintf(stderr, "This is mfxwritetrigs using the mfx library version\n%s\n\n"
        	  "Usage: %s [-s | -t] mfxfile triggerfile\n"
                  " triggerfile format:\n"
                  "  position_trig1 position_trig2 ...\n"
                  "  ... (negative values are blank placeholders)\n\n"
        	  " -s: Trigger positions specified in seconds, not points\n"
                  " -t: Reads mfxtriggers -v output\n"
                  " -E: Reads avg_q's trigger file format\n"
        	  , mfx_version, argv[0]);
  return 1;
 }
 /*}}}  */

 /*{{{  Prepare i/o files*/
 if ((myfile=mfx_open(argv[optind++], "r+b", MFX_SHORTS))==NULL) {
  fprintf(stderr, "mfx_open: %s\n", mfx_errors[mfx_lasterr]);
  return 2;
 }
 if ((trigfile=fopen(argv[optind], "r"))==NULL) {
  fprintf(stderr, "Can't open %s\n", argv[optind]);
  return 2;
 }
 if (mfx_cselect(myfile, "TRIGGER")==0) {
  fprintf(stderr, "mfx_cselect: %s\n", mfx_errors[mfx_lasterr]);
  return 3;
 }
 /*}}}  */

 switch(trigfileformat) {
  case JUSTPOINTS:
   while ((fgets(buffer, LENOFBUFFER, trigfile))!=NULL) {
    /*{{{  Process triggerfile line*/
    /* endnum is set to inbuf if strtod can't form any number from the string */
    for (i=1, inbuf=buffer; position=strtod(inbuf, &endnum), inbuf!=endnum; inbuf=endnum, i++) {
     if (position<0) continue;
     write1trig(position, i, seconds);
    }
    /*}}}  */
   }
   break;
  case MFXTRIGFORMAT:
   while ((fgets(buffer, LENOFBUFFER, trigfile))!=NULL) {
    /*{{{  Process triggerfile line*/
    if (*buffer=='#') continue;	/* Line commented out */
    /*{{{  Read position value*/
    if ((inbuf=strstr(buffer, POS_START))==NULL) continue;
    position=strtod(inbuf+strlen(POS_START), &endnum);
    if (inbuf==endnum) {
     fprintf(stderr, "Can't read position on line:\n%s", buffer);
     continue;
    }
    /*}}}  */
    /*{{{  Read trigcode value*/
    if ((inbuf=strstr(buffer, CODE_START))==NULL) continue;
    i=strtod(inbuf+strlen(CODE_START), &endnum);
    if (inbuf==endnum) {
     fprintf(stderr, "Can't read trigger code on line:\n%s", buffer);
     continue;
    }
    /*}}}  */
    write1trig(position, i, FALSE);
    /*}}}  */
   }
   break;
  case AVG_Q_FORMAT: {
   float const sfreq= 1.0/myfile->fileheader.sample_period;
   long trigpoint;
   while((i=read_trigger_from_trigfile(trigfile, sfreq, &trigpoint))!=0) {
    write1trig((double)trigpoint, i, FALSE);
   }
   }
   break;
 }

 fclose(trigfile);
 mfx_close(myfile);
 return 0;
}
