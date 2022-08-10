/*
 * Copyright (C) 1996-2004,2007,2010,2013-2016 Bernd Feige
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
#include "neurohdr.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { SYNAMPSFILE=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

int 
main(int argc, char **argv) {
 SETUP EEG;
 ELECTLOC *Channels=(ELECTLOC *)NULL;
 TEEG TagType;
 int ntags;
 long SizeofHeader;          /* no. of bytes in header of source file */
 enum NEUROSCAN_SUBTYPES SubType; /* This tells, once and for all, the file type */
 int errflag=0, c;
 enum {EVENTLIST_NONE, EVENTLIST_SYNAMPS, EVENTLIST_AVG_Q} event_list=EVENTLIST_NONE;
 enum {EVENTPOS_POINTS, EVENTPOS_MSEC, EVENTPOS_SEC} eventpos_type=EVENTPOS_POINTS;

 char *filename;
 FILE *SCAN;
 int NoOfChannels, channel;

 int filearg;

 /*{{{  Process command line*/
 mainargv=argv;
 while ((c=getopt(argc, argv, "eEms"))!=EOF) {
  switch (c) {
   case 'e':
    event_list=EVENTLIST_SYNAMPS;
    break;
   case 'E':
    event_list=EVENTLIST_AVG_Q;
    break;
   case 'm':
    eventpos_type=EVENTPOS_MSEC;
    break;
   case 's':
    eventpos_type=EVENTPOS_SEC;
    break;
   case '?':
   default:
    errflag++;
    continue;
  }
 }

 if (argc-optind<END_OF_ARGS || errflag>0) {
  fprintf(stderr, "Usage: %s [options] synamps_filename1 synamps_filename2 ...\n"
   " Options are:\n"
   "\t-e: Output the event table in NeuroScan format\n"
   "\t-E: Output the event table in avg_q format\n"
   "\t-m: avg_q Marker positions should be output in milliseconds\n"
   "\t-s: avg_q Marker positions should be output in seconds\n"
  , argv[0]);
  exit(1);
 }

 for (filearg=SYNAMPSFILE; argc-optind-filearg>=END_OF_ARGS; filearg++) {

 filename=MAINARG(filearg);
 SCAN=fopen(filename,"rb");
 if(SCAN==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  continue;
 }

 if (read_struct((char *)&EEG, sm_SETUP, SCAN)==0) {
  fprintf(stderr, "%s: Short file %s\n", argv[0], filename);
  continue;
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&EEG, sm_SETUP);
#endif
 if (strncmp(&EEG.rev[0],"Version",7)!=0) {
  fprintf(stderr, "%s: %s is not a NeuroScan file\n", argv[0], filename);
  continue;
 }
 /* The actual criteria to decide upon the file type (average, continuous etc)
  * by the header have been moved to neurohdr.h because it's black magic... */
 SubType=NEUROSCAN_SUBTYPE(&EEG);
 if (event_list==EVENTLIST_NONE) {
 printf("NeuroScan File %s: File type is `%s'\n\n", filename, neuroscan_subtype_names[SubType]);
 print_structcontents((char *)&EEG, sm_SETUP, smd_SETUP, stdout);
 }

 if (EEG.ChannelOffset<=1) EEG.ChannelOffset=sizeof(short);
 /*{{{  Allocate channel header*/
 if ((Channels=(ELECTLOC *)malloc(EEG.nchannels*sizeof(ELECTLOC)))==NULL) {
  fprintf(stderr, "%s: Error allocating Channels list\n", argv[0]);
  exit(1);
 }
 /*}}}  */
 if (event_list==EVENTLIST_NONE) {
 printf("\nCHANNEL HEADERS"
        "\n---------------\n");
 }
 /* For minor_rev<4, 32 electrode structures were hardcoded - but the data is
  * there for all the channels... */
 NoOfChannels = (EEG.minor_rev<4 ? 32 : EEG.nchannels);
 for (channel=0; channel<NoOfChannels; channel++) {
  read_struct((char *)&Channels[channel], sm_ELECTLOC, SCAN);
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&Channels[channel], sm_ELECTLOC);
#endif
  if (event_list==EVENTLIST_NONE) {
  printf("\nChannel number %d:\n", channel+1);
  print_structcontents((char *)&Channels[channel], sm_ELECTLOC, smd_ELECTLOC, stdout);
  }
 }
 if (EEG.minor_rev<4) {
  NoOfChannels = EEG.nchannels;
  /* Copy the channel descriptors over from the first 32: */
  for (; channel<NoOfChannels; channel++) {
   memcpy(&Channels[channel], &Channels[channel%32], sizeof(ELECTLOC));
  }
 }
 SizeofHeader = ftell(SCAN);

 if (event_list==EVENTLIST_AVG_Q) {
  long int const NumSamples = (EEG.EventTablePos - SizeofHeader)/NoOfChannels/sizeof(short);
  printf("# NeuroScan File %s: File type is `%s'\n# Sfreq=%d\n# NumSamples=%ld\n", filename, neuroscan_subtype_names[SubType], EEG.rate, NumSamples);
 }

 switch(SubType) {
  case NST_CONTINUOUS:
  case NST_SYNAMPS:
   /*{{{  Read the events header and array*/
   if (event_list==EVENTLIST_NONE) {
   printf("\nEVENT TABLE HEADER"
	  "\n------------------\n");
   }
   fseek(SCAN,EEG.EventTablePos,SEEK_SET);
   /* Here we face two evils in one header: Coding enums as chars and
    * allowing longs at odd addresses. Well... */
   if (1!=read_struct((char *)&TagType, sm_TEEG, SCAN)) {
    fprintf(stderr, "%s: Error reading event table header\n", argv[0]);
    exit(1);
   }
#   ifndef LITTLE_ENDIAN
   change_byteorder((char *)&TagType, sm_TEEG);
#   endif
   if (event_list==EVENTLIST_NONE) {
   print_structcontents((char *)&TagType, sm_TEEG, smd_TEEG, stdout);
   }
   if (TagType.Teeg==TEEG_EVENT_TAB1 || TagType.Teeg==TEEG_EVENT_TAB2) {
    struct_member * const sm_EVENT=(TagType.Teeg==TEEG_EVENT_TAB1 ? sm_EVENT1 : sm_EVENT2);
    struct_member_description * const smd_EVENT=(TagType.Teeg==TEEG_EVENT_TAB1 ? smd_EVENT1 : smd_EVENT2);
    EVENT2 event;
    int tag;
    int TrigVal;
    int KeyBoard;
    int KeyPad;
    int code;
    enum NEUROSCAN_ACCEPTVALUES Accept;

    ntags=TagType.Size/sm_EVENT[0].offset;	/* sm_EVENT[0].offset is the size of the structure in file. */
    if (event_list==EVENTLIST_NONE) {
    printf("\nEVENT TABLE (Type %d)"
	   "\n--------------------\n", TagType.Teeg);
    }
    for (tag=0; tag<ntags; tag++) {
     if (1!=read_struct((char *)&event, sm_EVENT, SCAN)) {
      fprintf(stderr, "%s: Can't access event table header, CNT file is probably corrupted.\n", argv[0]);
      exit(1);
     }
#   ifndef LITTLE_ENDIAN
     change_byteorder((char *)&event, sm_EVENT);
#   endif
     TrigVal=event.StimType &0xff;
     KeyBoard=event.KeyBoard&0xf;
     KeyPad=Event_KeyPad_value(event);
     Accept=Event_Accept_value(event);
     /* The `accept' values NAV_DCRESET and NAV_STARTSTOP always occur alone,
      * while the NAV_REJECT and NAV_ACCEPT enclose rejected CNT blocks and
      * might occur within a marker as well (at least NAV_REJECT). */
     code=TrigVal-KeyPad+neuroscan_accept_translation[Accept];
     if (code==0) {
      code= -((KeyBoard+1)<<4);
     }
     switch(event_list) {
      case EVENTLIST_NONE:
       print_structcontents((char *)&event, sm_EVENT, smd_EVENT, stdout);
       printf("\n");
       break;
      case EVENTLIST_SYNAMPS:
       if (code!=0) printf("%4d    %4d %4d  0     0.0000 %ld\n", tag+1, TrigVal, KeyBoard+KeyPad, event.Offset);
       break;
      case EVENTLIST_AVG_Q:
       if (code!=0) {
	long const pos=(event.Offset-SizeofHeader)/sizeof(short)/EEG.nchannels;
	if (Accept==NAV_REJECT && code!=neuroscan_accept_translation[NAV_REJECT]) {
	 printf("#");
	}
	switch (eventpos_type) {
	 case EVENTPOS_POINTS:
	  printf("%ld %d\n", pos, code);
	  break;
	 case EVENTPOS_MSEC:
	  printf("%.8gms %d\n", ((float)pos)/EEG.rate*1000, code);
	  break;
	 case EVENTPOS_SEC:
	  printf("%.8gs %d\n", ((float)pos)/EEG.rate, code);
	  break;
	}
       }
       break;
     }
    }
   } else {
    fprintf(stderr, "%s: Unknown tag type %d\n", argv[0], TagType.Teeg);
    exit(1);
   }
   /*}}}  */
   break;
  case NST_EPOCHS: {
   NEUROSCAN_EPOCHED_SWEEP_HEAD sweephead;
   int epoch, code;
   if (event_list==EVENTLIST_NONE) {
    printf("\nEPOCHED SWEEP HEADERS"
	   "\n---------------------\n");
   }
   for (epoch=0; epoch<EEG.compsweeps; epoch++) {
    long const filepos=epoch*(sm_NEUROSCAN_EPOCHED_SWEEP_HEAD[0].offset+EEG.pnts*EEG.nchannels*sizeof(short))+SizeofHeader;
    /* sm_NEUROSCAN_EPOCHED_SWEEP_HEAD[0].offset is sizeof(NEUROSCAN_EPOCHED_SWEEP_HEAD) on those other systems... */
    fseek(SCAN,filepos,SEEK_SET);
    read_struct((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD, SCAN);
#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD);
#   endif
    code=(sweephead.type!=0 ? sweephead.type : -sweephead.response);
    switch(event_list) {
     case EVENTLIST_NONE:
      printf("\nEpoch number %d:\n", epoch+1);
      print_structcontents((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD, smd_NEUROSCAN_EPOCHED_SWEEP_HEAD, stdout);
      break;
     case EVENTLIST_SYNAMPS:
      /* The fourth item is actually `acc'(uracy) (of response, -1=no resp), but let's use it for the accept value now */
      printf("%4d    %4d %4d  %d     0.0000 %ld\n", epoch+1, sweephead.type, sweephead.response, sweephead.accept, filepos);
      break;
     case EVENTLIST_AVG_Q: {
      long const pos=epoch*EEG.pnts;
      if (sweephead.accept==0) {
       printf("#");
      }
      switch (eventpos_type) {
       case EVENTPOS_POINTS:
	printf("%ld %d\n", pos, code);
	break;
       case EVENTPOS_MSEC:
	printf("%gms %d\n", ((float)pos)/EEG.rate*1000, code);
	break;
       case EVENTPOS_SEC:
	printf("%gs %d\n", ((float)pos)/EEG.rate, code);
	break;
      }
      }
      break;
    }
   }
  }
   break;
  default:
   break;
 }

 free(Channels);
 fclose(SCAN);

 }

 return 0;
}
