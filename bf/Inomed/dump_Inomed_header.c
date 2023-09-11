/*
 * Copyright (C) 2008,2012 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "Inomed.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { INOMEDFILEARG=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

int 
main(int argc, char **argv) {
 int filearg;
 int errflag=0, c;
 int channel;

 /*{{{  Process command line*/
 mainargv=argv;
 while ((c=getopt(argc, argv, ""))!=EOF) {
 }

 if (argc-optind<END_OF_ARGS || errflag>0) {
  fprintf(stderr, "Usage: %s [options] Inomed_filename1 Inomed_filename2 ...\n"
   " Options are:\n"
  , argv[0]);
  exit(1);
 }

 for (filearg=INOMEDFILEARG; argc-optind-filearg>=END_OF_ARGS; filearg++) {
  char *filename=MAINARG(filearg);
  FILE *INOMEDFILE=fopen(filename,"rb");
  if(INOMEDFILE==NULL) {
   fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
   continue;
  }

  if (strlen(filename)>=4 && strcmp(filename+strlen(filename)-4, ".dat")==0) {
   PlotWinInfo EEG;
   if (read_struct((char *)&EEG, sm_PlotWinInfo, INOMEDFILE)==0) {
    fprintf(stderr, "%s: Short file %s\n", argv[0], filename);
    continue;
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&EEG, sm_PlotWinInfo);
#endif
   print_structcontents((char *)&EEG, sm_PlotWinInfo, smd_PlotWinInfo, stdout);
  } else {
   MULTI_CHANNEL_CONTINUOUS EEG;
   if (read_struct((char *)&EEG, sm_MULTI_CHANNEL_CONTINUOUS, INOMEDFILE)==0) {
    fprintf(stderr, "%s: Short file %s\n", argv[0], filename);
    continue;
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&EEG, sm_MULTI_CHANNEL_CONTINUOUS);
#endif
   print_structcontents((char *)&EEG, sm_MULTI_CHANNEL_CONTINUOUS, smd_MULTI_CHANNEL_CONTINUOUS, stdout);
   for (channel=0; channel<EEG.sNumberOfChannels; channel++) {
    printf("\nChannel %d\n", channel+1);
    if (read_struct((char *)&EEG.strctChannel[channel], sm_CHANNEL, INOMEDFILE)==0
      ||read_struct((char *)&EEG.strctChannel[channel].strctHighPass, sm_DIG_FILTER, INOMEDFILE)==0
      ||read_struct((char *)&EEG.strctChannel[channel].strctLowPass, sm_DIG_FILTER, INOMEDFILE)==0) {
     fprintf(stderr, "%s: Short file %s\n", argv[0], filename);
     continue;
    }
#ifndef LITTLE_ENDIAN
    change_byteorder((char *)&EEG.strctChannel[channel], sm_CHANNEL);
    change_byteorder((char *)&EEG.strctChannel[channel].strctHighPass, sm_DIG_FILTER);
    change_byteorder((char *)&EEG.strctChannel[channel].strctLowPass, sm_DIG_FILTER);
#endif
    print_structcontents((char *)&EEG.strctChannel[channel], sm_CHANNEL, smd_CHANNEL, stdout);
    printf("HighPass:\n");
    print_structcontents((char *)&EEG.strctChannel[channel].strctHighPass, sm_DIG_FILTER, smd_DIG_FILTER, stdout);
    printf("LowPass:\n");
    print_structcontents((char *)&EEG.strctChannel[channel].strctLowPass, sm_DIG_FILTER, smd_DIG_FILTER, stdout);
   }
  }
  fclose(INOMEDFILE);
 }

 return 0;
}
