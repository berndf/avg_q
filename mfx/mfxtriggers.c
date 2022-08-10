/*
 * Copyright (C) 1993-1996,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfxtriggers.c - A small mfx application which lists the trigger codes 
 * found in the channel named TRIGGER of the named mfx file.
 *						Bernd Feige 10.02.1993
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mfx_file.h"
/*}}}  */

extern char *optarg;
extern int optind, opterr;

#define DEFAULT_TRIGGERCHANNEL "TRIGGER"

int main(int argc, char **argv) {
 /*{{{  Local vars*/
 MFX_FILE *myfile;
 int i, c, pos, pos2, trig, len, mintrig, maxtrig, pts_in_epoch;
 int verbose=FALSE, errflag=0;
 int *trigcounts=NULL;
 short trigshort;
 char triggerchannel[20];
 double sample_period;
 /*}}}  */

 /*{{{  Process options*/
 strcpy(triggerchannel, DEFAULT_TRIGGERCHANNEL);
 while ((c=getopt(argc, argv, "t:v"))!=EOF) {
  switch (c) {
   case 't':
    strcpy(triggerchannel, optarg);
    printf("%s: Using trigger channel %s\n", argv[0], triggerchannel);
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
  fprintf(stderr, "This is mfxtriggers using the mfx library version\n%s\n\n"
   "Usage: %s [-t trigchannel] [-v] mfxfile\n"
   " -t trigchannel: Specify the name of the trigger channel explicitly\n"
   " -v: Each trigger position is listed verbosely\n"
   , mfx_version, argv[0]);
  return 1;
 }
 /*}}}  */

 /*{{{  Open file and select the trigger channel*/
 if ((myfile=mfx_open(argv[optind], "rb", MFX_SHORTS))==NULL) {
  fprintf(stderr, "mfx_open: %s\n", mfx_errors[mfx_lasterr]);
  return 2;
 }
 if (mfx_cselect(myfile, triggerchannel)!=1) {
  fprintf(stderr, "mfx_cselect: %s\n", mfx_errors[mfx_lasterr]);
  return 3;
 }
 pts_in_epoch=myfile->fileheader.pts_in_epoch;
 sample_period=myfile->fileheader.sample_period;
 /*}}}  */

 /*{{{  if (verbose) {Print header}*/
 if (verbose) {
  printf("Trigger list for file %s\n\n", argv[optind]);
  pos=mfx_tell(myfile);
 }
 /*}}}  */
 i=0;
 while (mfx_lasterr==MFX_NOERR &&
	(trig=mfx_seektrigger(myfile, triggerchannel, 0))!=0) {
  /*{{{  Note next trigger*/
  i++;
  if (verbose) {
   pos2=mfx_tell(myfile); len=0;
   while (mfx_read(&trigshort, 1, myfile)==MFX_NOERR &&
    ((trigshort=DATACONV(myfile->channelheaders[myfile->selected_channels[0]-1], trigshort)+0.1), IS_TRIGGER(trigshort))) {
     if (TRIGGERCODE(trigshort)!=trig) {
      printf("Warning: Trigger number %d in this sequence is %d!=%d\n", len+1, TRIGGERCODE(trigshort), trig);
     }
     len++;
    }
   printf("Trigger at: %6d", pos2);
   if (myfile->mfxlen>pts_in_epoch) {
    printf(" (%11.5fs epoch %d)", (float)(pos2%pts_in_epoch)*sample_period, pos2/pts_in_epoch);
   } else {
    printf(" (%11.5fs)", (float)pos2*sample_period);
   }
   printf(", distance: %6d, code: %2d, length: %3d\n", pos2-pos, trig, len);
  }

  if (trigcounts==NULL) {
   /*{{{  Initialize the trigcounts storage*/
   if ((trigcounts=(int *)calloc(1, sizeof(int)))==NULL) {
    fprintf(stderr, "Error allocating trigcounts buffer\n");
    return 4;
   }
   mintrig=maxtrig=trig;
   /*}}}  */
  } else if (trig<mintrig || trig>maxtrig) {
   /*{{{  Expand the trigcounts storage*/
   int newmintrig=mintrig, newmaxtrig=maxtrig;
   if (trig<mintrig) {
    newmintrig=trig;
   } else {
    newmaxtrig=trig;
   }

   if ((trigcounts=(int *)realloc(trigcounts, (newmaxtrig-newmintrig+1)*sizeof(int)))==NULL) {
    fprintf(stderr, "Error reallocating trigcounts buffer\n");
    return 5;
   }

   if (trig<mintrig) {
    memmove(trigcounts+(mintrig-trig), trigcounts, (maxtrig-mintrig+1)*sizeof(int));
    memset(trigcounts, 0, (mintrig-trig)*sizeof(int));
   } else {
    memset(trigcounts+(maxtrig-mintrig+1), 0, (trig-maxtrig)*sizeof(int));
   }
   mintrig=newmintrig; maxtrig=newmaxtrig;
   /*}}}  */
  }

  trigcounts[trig-mintrig]++;
  if (verbose) pos=pos2;
  /*}}}  */
 }
 /*{{{  Output trigger statistics*/
 if (verbose) printf("\n");
 printf("Trigger statistics for file %s\n\n", argv[optind]);
 if (trigcounts==NULL) {
  printf("No triggers found.\n");
 } else {
  for (trig=mintrig; trig<=maxtrig; trig++) {
   printf("%d times trigger code %d\n", trigcounts[trig-mintrig], trig);
  }
 }
 printf("\nTotal data points: %ld, Total triggers: %d\n", mfx_tell(myfile), i);
 /*}}}  */
 mfx_close(myfile);
 return 0;
}
