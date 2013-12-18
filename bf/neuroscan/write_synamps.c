/*
 * Copyright (C) 1996-1999,2001,2003,2004,2006-2010,2012 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_synamps.c module to write data to a NeuroScan epoched (.EEG) file
 *	-- Bernd Feige 10.10.1995
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <math.h>
#include <float.h>
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "neurohdr.h"
/*}}}  */

LOCAL char *write_synamps_version="$Header: /home/charly/.cvsroot/avg_q/bf/neuroscan/write_synamps.c,v 2.40 2007/05/21 12:48:05 charly Exp $";

LOCAL char const *const output_format_choice[]={
 "-E", "-c", "-A", NULL
};
enum output_formats {
 FORMAT_EEGFILE=0, FORMAT_CNTFILE, FORMAT_AVGFILE
};

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_LINKED, 
 ARGS_OUTPUTFORMAT,
 ARGS_OFILE,
 ARGS_CONVFACTOR,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Write all linked datasets", "L", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "Select the output format: EEG, CNT or AVG (default: EEG)", " ", 0, output_format_choice},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_DOUBLE, "conv_factor", "", 1, NULL}
};

/*{{{  struct write_synamps_storage {*/
struct write_synamps_storage {
 FILE *SCAN;	/* Input file */
 SETUP EEG;	/* header info. */
 ELECTLOC *Channels;
 long SizeofHeader;          /* no. of bytes in header of source file */
 enum output_formats output_format;
 growing_buf triggers;
};
/*}}}  */

/*{{{  Local functions to transform file offsets <-> point numbers*/
LOCAL long
offset2point(transform_info_ptr tinfo, long offset) {
 struct write_synamps_storage *local_arg=(struct write_synamps_storage *)tinfo->methods->local_storage;
 return (offset-local_arg->SizeofHeader)/sizeof(int16_t)/local_arg->EEG.nchannels;
}
LOCAL long
point2offset(transform_info_ptr tinfo, long point) {
 struct write_synamps_storage *local_arg=(struct write_synamps_storage *)tinfo->methods->local_storage;
 return point*local_arg->EEG.nchannels*sizeof(int16_t)+local_arg->SizeofHeader;
}
/*}}}  */

/*{{{  Defines for the display arrangement*/
#define RANGE_TO_COVER 500
#define XSCALEVALUE 1600
#define XSCALEINTERVAL 1280
#define XOFFSET 50
#define YSCALEVALUE 93.7637863
#define YSCALEINTERVAL 50
#define YOFFSET 40
/*}}}  */

/*{{{  write_synamps_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_synamps_init(transform_info_ptr tinfo) {
 struct write_synamps_storage *local_arg=(struct write_synamps_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 double xmin, xmax, ymin, ymax;
 int NoOfChannels=tinfo->nr_of_channels;
 int channel;

 growing_buf_init(&local_arg->triggers);
 local_arg->output_format=(args[ARGS_OUTPUTFORMAT].is_set ? (enum output_formats)args[ARGS_OUTPUTFORMAT].arg.i : FORMAT_EEGFILE);
 local_arg->SCAN=fopen(args[ARGS_OFILE].arg.s, "r+b");
 if (!args[ARGS_APPEND].is_set || local_arg->SCAN==NULL) {   /* target does not exist*/
 /*{{{  Create file*/
 if (local_arg->SCAN!=NULL) fclose(local_arg->SCAN);
 /*{{{  Calculate the span in x-y channel positions*/
 xmin=ymin=  FLT_MAX; 
 xmax=ymax= -FLT_MAX; 
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  const double this_x=tinfo->probepos[3*channel], this_y=tinfo->probepos[3*channel+1];
  if (this_x>xmax) xmax=this_x;
  if (this_x<xmin) xmin=this_x;
  if (this_y>ymax) ymax=this_y;
  if (this_y<ymin) ymin=this_y;
 }
 if (xmax==xmin) {
  xmax+=0.5;
  xmin-=0.5;
 }
 if (ymax==ymin) {
  ymax+=0.5;
  ymin-=0.5;
 }
 /*}}}  */

 if ((local_arg->SCAN=fopen(args[ARGS_OFILE].arg.s, "wb"))==NULL) {
  ERREXIT1(tinfo->emethods, "write_synamps_init: Can't open %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 /*{{{  Many settings, taken from example files...*/
 strcpy(local_arg->EEG.rev, "Version 3.0");
 local_arg->EEG.AdditionalFiles=local_arg->EEG.NextFile=local_arg->EEG.PrevFile=0;
 switch (local_arg->output_format) {
  case FORMAT_EEGFILE:
   local_arg->EEG.review = 1;
   local_arg->EEG.savemode=NSM_EEGF;
   local_arg->EEG.type = NTY_EPOCHED;
   local_arg->EEG.ContinousType=0;
   local_arg->EEG.nsweeps=local_arg->EEG.compsweeps=local_arg->EEG.acceptcnt=0;
   break;
  case FORMAT_CNTFILE:
   local_arg->EEG.review = 1;
   local_arg->EEG.savemode=NSM_CONT;
   local_arg->EEG.type = 0;
   local_arg->EEG.ContinousType=NST_SYNAMPS-NST_CONT0;
   local_arg->EEG.nsweeps=1;
   local_arg->EEG.compsweeps=local_arg->EEG.acceptcnt=0;
   break;
  case FORMAT_AVGFILE:
   local_arg->EEG.review = 0;
   local_arg->EEG.savemode=NSM_AVGD;
   local_arg->EEG.type = NTY_AVERAGED;
   local_arg->EEG.ContinousType=0;
   local_arg->EEG.nsweeps=local_arg->EEG.compsweeps=local_arg->EEG.acceptcnt=(tinfo->nrofaverages>0 ? tinfo->nrofaverages : 1);
   break;
 }
 /* Since the comment can be of arbitrary length and thus also larger than the
  * rest of the header, we should limit ourselves to the size of the id field */
 strncpy(local_arg->EEG.id, tinfo->comment, sizeof(local_arg->EEG.id));
 /* The date is coded in the comment, and read_synamps appends the date and time
  * from the corresponding fields and the contents of the id field, so that
  * the comment2time reader could be confused by the two date/time fields if we
  * would not somehow invalidate remnants of the date field here... */
 {char *slash;
  while ((slash=strchr(local_arg->EEG.id, '/'))!=NULL) {
   *slash='|';
  }
 }
 strcpy(local_arg->EEG.oper, "Unspecified");
 strcpy(local_arg->EEG.doctor, "Unspecified");
 strcpy(local_arg->EEG.referral, "Unspecified");
 strcpy(local_arg->EEG.hospital, "Unspecified");
 strcpy(local_arg->EEG.patient, "Unspecified");
 local_arg->EEG.age = 0;
 local_arg->EEG.sex = 'U';
 local_arg->EEG.hand = 'U';
 strcpy(local_arg->EEG.med, "Unspecified");
 strcpy(local_arg->EEG.category, "Unspecified");
 strcpy(local_arg->EEG.state, "Unspecified");
 strcpy(local_arg->EEG.label, "Unspecified");
 {short dd, mm, yy, yyyy, hh, mi, ss;
 if (comment2time(tinfo->comment, &dd, &mm, &yy, &yyyy, &hh, &mi, &ss)) {
  char buffer[16]; /* Must be long enough to fit date (10) and time (12) +1 */
  /* This is necessary because the date field was devised for 2-digit years.
   * With 4-digit years it uses all 10 bytes and the trailing zero
   * does not fit in any more. */
  snprintf(buffer, 16, "%02d/%02d/%04d", mm, dd, yyyy);
  strncpy(local_arg->EEG.date, buffer, sizeof(local_arg->EEG.date));
  snprintf(buffer, 16, "%02d:%02d:%02d", hh, mi, ss);
  strncpy(local_arg->EEG.time, buffer, sizeof(local_arg->EEG.time));
 }
 }
 local_arg->EEG.rejectcnt = 0;
 local_arg->EEG.pnts=(tinfo->data_type==FREQ_DATA ? tinfo->nroffreq : tinfo->nr_of_points);
 local_arg->EEG.nchannels=NoOfChannels;
 local_arg->EEG.avgupdate=1;
 local_arg->EEG.domain=(tinfo->data_type==FREQ_DATA ? 1 : 0);
 local_arg->EEG.variance=0;
 local_arg->EEG.rate=(uint16_t)rint(tinfo->sfreq);
 if (tinfo->data_type==FREQ_DATA) {
  /* I know that this field is supposed to contain the taper window size in %,
     but we need some way to store basefreq and 'rate' is only integer... */
  local_arg->EEG.SpectWinLength=tinfo->basefreq;
 }
 if (local_arg->EEG.rate==0) {
  local_arg->EEG.rate=1;
  TRACEMS(tinfo->emethods, 0, "write_synamps_init: Rate was zero, corrected to 1!\n");
 }
 local_arg->EEG.scale=3.4375;	/* This is a common sensitivity factor, used if sensitivities for channels are zero */
 local_arg->EEG.veogcorrect=0;
 local_arg->EEG.heogcorrect=0;
 local_arg->EEG.aux1correct=0;
 local_arg->EEG.aux2correct=0;
 local_arg->EEG.veogtrig=15;	/* Trigger threshold in percent of the maximum */
 local_arg->EEG.heogtrig=10;
 local_arg->EEG.aux1trig=10;
 local_arg->EEG.aux2trig=10;
 local_arg->EEG.veogchnl=find_channel_number(tinfo, "VEOG");
 local_arg->EEG.heogchnl=find_channel_number(tinfo, "HEOG");
 local_arg->EEG.veogdir=0;	/* 0=positive, 1=negative */
 local_arg->EEG.veog_n=10;	/* "Number of points per waveform", really: minimum acceptable # averages */
 local_arg->EEG.heog_n=10;
 local_arg->EEG.veogmaxcnt=(int16_t)rint(0.3*tinfo->sfreq);	/* "Number of observations per point", really: event window size in points */
 local_arg->EEG.heogmaxcnt=(int16_t)rint(0.5*tinfo->sfreq);
 local_arg->EEG.AmpSensitivity=10;	/* External Amplifier gain */
 local_arg->EEG.baseline=0;
 local_arg->EEG.reject=0;
 local_arg->EEG.trigtype=2;	/* 2=Port */
 local_arg->EEG.trigval=255;	/* Hold */
 local_arg->EEG.dir=0;	/* Invert (negative up)=0 */
 local_arg->EEG.dispmin= -1;	/* displayed y range */
 local_arg->EEG.dispmax= +1;
 local_arg->EEG.DisplayXmin=local_arg->EEG.AutoMin=local_arg->EEG.rejstart=local_arg->EEG.offstart=local_arg->EEG.xmin= -tinfo->beforetrig/tinfo->sfreq;
 local_arg->EEG.DisplayXmax=local_arg->EEG.AutoMax=local_arg->EEG.rejstop=local_arg->EEG.offstop=local_arg->EEG.xmax= tinfo->aftertrig/tinfo->sfreq;
 local_arg->EEG.zmin=0.0;
 local_arg->EEG.zmax=0.1;
 strcpy(local_arg->EEG.ref, "A1-A2");
 strcpy(local_arg->EEG.screen,   "--------");
 local_arg->EEG.CalMode=2;
 local_arg->EEG.CalMethod=0;
 local_arg->EEG.CalUpdate=1;
 local_arg->EEG.CalBaseline=0;
 local_arg->EEG.CalSweeps=5;
 local_arg->EEG.CalAttenuator=1;
 local_arg->EEG.CalPulseVolt=1;
 local_arg->EEG.CalPulseStart=0;
 local_arg->EEG.CalPulseStop=0;
 local_arg->EEG.CalFreq=10;
 strcpy(local_arg->EEG.taskfile, "--------");
 strcpy(local_arg->EEG.seqfile,  "--------");	/* Otherwise tries to read a seqfile */
 local_arg->EEG.HeadGain=150;
 local_arg->EEG.FspFValue=2.5;
 local_arg->EEG.FspBlockSize=200;
 local_arg->EEG.fratio=1.0;
 local_arg->EEG.minor_rev=12;	/* Necessary ! Otherwise a different file structure is assumed... */
 local_arg->EEG.eegupdate=1;

 local_arg->EEG.xscale=local_arg->EEG.yscale=0;
 local_arg->EEG.xsize=40;
 local_arg->EEG.ysize=20;
 local_arg->EEG.ACmode=0;

 local_arg->EEG.XScaleValue=XSCALEVALUE;
 local_arg->EEG.XScaleInterval=XSCALEINTERVAL;
 local_arg->EEG.YScaleValue=YSCALEVALUE;
 local_arg->EEG.YScaleInterval=YSCALEINTERVAL;

 local_arg->EEG.ScaleToolX1=20;
 local_arg->EEG.ScaleToolY1=170;
 local_arg->EEG.ScaleToolX2=23.1535;
 local_arg->EEG.ScaleToolY2=153.87;

 local_arg->EEG.port=715;
 local_arg->EEG.NumSamples=0;
 local_arg->EEG.FilterFlag=0;
 local_arg->EEG.LowCutoff=4;
 local_arg->EEG.LowPoles=2;
 local_arg->EEG.HighCutoff=50;
 local_arg->EEG.HighPoles=2;
 local_arg->EEG.FilterType=3;
 local_arg->EEG.FilterDomain=1;
 local_arg->EEG.SnrFlag=0;
 local_arg->EEG.CoherenceFlag=0;
 local_arg->EEG.ContinousSeconds=4;
 local_arg->EEG.ChannelOffset=sizeof(int16_t);
 local_arg->EEG.AutoCorrectFlag=0;
 local_arg->EEG.DCThreshold='F';
 /*}}}  */
 /*{{{  Write SETUP structure*/
# ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->EEG, sm_SETUP);
# endif
 write_struct((char *)&local_arg->EEG, sm_SETUP, local_arg->SCAN);
# ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->EEG, sm_SETUP);
# endif
 /*}}}  */

 if ((local_arg->Channels=(ELECTLOC *)calloc(NoOfChannels,sizeof(ELECTLOC)))==NULL) {
  ERREXIT(tinfo->emethods, "write_synamps_init: Error allocating Channels list\n");
 }
 for (channel=0; channel<NoOfChannels; channel++) {
  /*{{{  Settings in the channel structure*/
  strncpy(local_arg->Channels[channel].lab, tinfo->channelnames[channel],sizeof(local_arg->Channels[channel].lab));
  local_arg->Channels[channel].reference=0;
  local_arg->Channels[channel].skip=0;
  local_arg->Channels[channel].reject=0;
  local_arg->Channels[channel].display=1;
  local_arg->Channels[channel].bad=0;
  local_arg->Channels[channel].n=(local_arg->output_format==FORMAT_AVGFILE ? local_arg->EEG.acceptcnt : (local_arg->output_format==FORMAT_CNTFILE ? 0 : 1));
  local_arg->Channels[channel].avg_reference=0;
  local_arg->Channels[channel].ClipAdd=0;
  local_arg->Channels[channel].x_coord= (tinfo->probepos[3*channel  ]-xmin)/(xmax-xmin)*RANGE_TO_COVER+XOFFSET;
  local_arg->Channels[channel].y_coord=((ymax-tinfo->probepos[3*channel+1])/(ymax-ymin)*RANGE_TO_COVER+YOFFSET)/3;
  local_arg->Channels[channel].veog_wt=0;
  local_arg->Channels[channel].veog_std=0;
  local_arg->Channels[channel].heog_wt=0;
  local_arg->Channels[channel].heog_std=0;
  local_arg->Channels[channel].baseline=0;
  local_arg->Channels[channel].Filtered=0;
  local_arg->Channels[channel].sensitivity=204.8/args[ARGS_CONVFACTOR].arg.d; /* This arranges for conv_factor to act as the expected product before integer truncation */
  local_arg->Channels[channel].Gain=5;
  local_arg->Channels[channel].HiPass=0;	/* 0=DC */
  local_arg->Channels[channel].LoPass=4;
  local_arg->Channels[channel].Page=0;
  local_arg->Channels[channel].Size=0;
  local_arg->Channels[channel].Impedance=0;
  local_arg->Channels[channel].PhysicalChnl=channel;	/* Channel mapping */
  local_arg->Channels[channel].Rectify=0;
  local_arg->Channels[channel].calib=1.0;
  /*}}}  */
  /*{{{  Write ELECTLOC struct*/
#  ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->Channels[channel], sm_ELECTLOC);
#  endif
  write_struct((char *)&local_arg->Channels[channel], sm_ELECTLOC, local_arg->SCAN);
#  ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->Channels[channel], sm_ELECTLOC);
#  endif
  /*}}}  */
 }
 local_arg->SizeofHeader = ftell(local_arg->SCAN);
 TRACEMS2(tinfo->emethods, 1, "write_synamps_init: Creating file %s, format `%s'\n", MSGPARM(args[ARGS_OFILE].arg.s), MSGPARM(neuroscan_subtype_names[NEUROSCAN_SUBTYPE(&local_arg->EEG)]));
 /*}}}  */
 } else {
  /*{{{  Append to file*/
  enum NEUROSCAN_SUBTYPES SubType;
  if (read_struct((char *)&local_arg->EEG, sm_SETUP, local_arg->SCAN)==0) {
   ERREXIT(tinfo->emethods, "write_synamps_init: Can't read file header.\n");
  }
#  ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->EEG, sm_SETUP);
#  endif
  NoOfChannels = local_arg->EEG.nchannels;
  /*{{{  Allocate channel header*/
  if ((local_arg->Channels=(ELECTLOC *)malloc(NoOfChannels*sizeof(ELECTLOC)))==NULL) {
   ERREXIT(tinfo->emethods, "write_synamps_init: Error allocating Channels list\n");
  }
  /*}}}  */
  for (channel=0; channel<NoOfChannels; channel++) {
   if (read_struct((char *)&local_arg->Channels[channel], sm_ELECTLOC, local_arg->SCAN)==0) {
    ERREXIT(tinfo->emethods, "write_synamps_init: Can't read channel headers.\n");
   }
#  ifndef LITTLE_ENDIAN
   change_byteorder((char *)&local_arg->Channels[channel], sm_ELECTLOC);
#  endif
  }
  local_arg->SizeofHeader = ftell(local_arg->SCAN);
  SubType=NEUROSCAN_SUBTYPE(&local_arg->EEG);
  switch (SubType) {
   case NST_EPOCHS:
    if (local_arg->output_format!=FORMAT_EEGFILE) {
     TRACEMS(tinfo->emethods, 0, "write_synamps_init: Appending to epoch file, discarding option -c\n");
     local_arg->output_format=FORMAT_EEGFILE;
    }
    fseek(local_arg->SCAN, 0, SEEK_END);
    break;
   case NST_CONTINUOUS:
   case NST_SYNAMPS: {
    TEEG TagType;
    EVENT1 event;
    if (local_arg->output_format!=FORMAT_CNTFILE) {
     TRACEMS(tinfo->emethods, 0, "write_synamps_init: Appending to continuous file, assuming option -c\n");
     local_arg->output_format=FORMAT_CNTFILE;
    }
    fseek(local_arg->SCAN, local_arg->EEG.EventTablePos, SEEK_SET);
    /* Here we face two evils in one header: Coding enums as chars and
     * allowing longs at odd addresses. Well... */
    if (1==read_struct((char *)&TagType, sm_TEEG, local_arg->SCAN)) {
#    ifndef LITTLE_ENDIAN
     change_byteorder((char *)&TagType, sm_TEEG);
#    endif
     if (TagType.Teeg==TEEG_EVENT_TAB1) {
      /*{{{  Read the event table*/
      int tag;
      int const ntags=TagType.Size/sm_EVENT1[0].offset;	/* sm_EVENT1[0].offset is sizeof(EVENT1) in the file. */
      for (tag=0; tag<ntags; tag++) {
       if (1!=read_struct((char *)&event, sm_EVENT1, local_arg->SCAN)) {
	ERREXIT(tinfo->emethods, "write_synamps_init: Can't read an event table entry.\n");
	break;
       }
#      ifndef LITTLE_ENDIAN
       change_byteorder((char *)&event, sm_EVENT1);
#      endif
       {
       int const TrigVal=event.StimType &0xff;
       int const KeyBoard=event.KeyBoard&0xf;
       int const KeyPad=Event_KeyPad_value(event);
       int const Accept=Event_Accept_value(event);
       int code=TrigVal-KeyPad+neuroscan_accept_translation[Accept];
       if (code==0) {
        code= -((KeyBoard+1)<<4);
       }
       push_trigger(&local_arg->triggers,offset2point(tinfo, event.Offset),code,NULL);
       }
      }
      /*}}}  */
     } else {
      ERREXIT(tinfo->emethods, "write_synamps_init: Type 2 events are not yet supported.\n");
     }
    } else {
     ERREXIT(tinfo->emethods, "write_synamps_init: Can't read the event table header.\n");
    }
    fseek(local_arg->SCAN, local_arg->EEG.EventTablePos, SEEK_SET);
    }
    break;
   default:
    ERREXIT1(tinfo->emethods, "write_synamps: Cannot append to file type `%s'\n", MSGPARM(neuroscan_subtype_names[SubType]));
    break;
  }
  /* Conformance to the incoming epochs is checked in write_synamps */
  TRACEMS1(tinfo->emethods, 1, "write_synamps_init: Appending to file %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
  /*}}}  */
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_synamps(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_synamps(transform_info_ptr calltinfo) {
 struct write_synamps_storage *local_arg=(struct write_synamps_storage *)calltinfo->methods->local_storage;
 transform_argument *args=calltinfo->methods->arguments;
 NEUROSCAN_EPOCHED_SWEEP_HEAD sweephead;
 long tsdata_step, tsdata_steps, tsdata_stepwidth;
 array myarray;
 /* Note that 'tinfo' instead of 'tinfoptr' is used here so that we don't have to modify 
  * all of the older code which stored only one epoch */
 transform_info_ptr tinfo=calltinfo;

 if (args[ARGS_LINKED].is_set) {
  /* Go to the start: */
  for (; tinfo->previous!=NULL; tinfo=tinfo->previous);
 }
 for (; tinfo!=NULL; tinfo=tinfo->next) {
  DATATYPE * const orig_tsdata=tinfo->tsdata;
 /*{{{  Assert that epoch size didn't change & itemsize==1*/
 if (tinfo->itemsize!=1) {
  ERREXIT(tinfo->emethods, "write_synamps: Only itemsize=1 is supported.\n");
 }
 if (local_arg->EEG.nchannels!=tinfo->nr_of_channels) {
  ERREXIT2(tinfo->emethods, "write_synamps: nr_of_channels was %d, now %d\n", MSGPARM(local_arg->EEG.nchannels), MSGPARM(tinfo->nr_of_channels));
 }
 /*}}}  */

 if (tinfo->data_type==FREQ_DATA) {
  tinfo->nr_of_points=tinfo->nroffreq;
  tsdata_steps=tinfo->nrofshifts;
  tsdata_stepwidth=tinfo->nr_of_channels*tinfo->nroffreq*tinfo->itemsize;
 } else {
  tsdata_steps=1;
  tsdata_stepwidth=0;
 }
 for (tsdata_step=0; tsdata_step<tsdata_steps; tinfo->tsdata+=tsdata_stepwidth, tsdata_step++) {
 switch (local_arg->output_format) {
  case FORMAT_EEGFILE:
 if (local_arg->EEG.pnts!=tinfo->nr_of_points) {
  ERREXIT2(tinfo->emethods, "write_synamps: nr_of_points was %d, now %d\n", MSGPARM(local_arg->EEG.pnts), MSGPARM(tinfo->nr_of_points));
 }
 /*{{{  Set sweephead values*/
 sweephead.accept=1;
 sweephead.type=254;
 sweephead.correct=0;
 sweephead.rt=0;
 sweephead.response=0;
 if (tinfo->condition>0) {
  sweephead.type=tinfo->condition&0xff;
 } else if (tinfo->condition<0 && (-tinfo->condition)<=0xf) {
  sweephead.type=0;
  sweephead.response= -tinfo->condition;
 } else {
  /* KeyBoard event: Do it the same way 'Edit' does when epoching,
   * mapping F1 to type=1 and so on (overlap with stim codes, but they
   * should know how they want it...) */
  sweephead.type= (-tinfo->condition)>>4;
 }
 /*}}}  */
 /*{{{  Write sweephead struct*/
# ifndef LITTLE_ENDIAN
 change_byteorder((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD);
# endif
 write_struct((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD, local_arg->SCAN);
# ifndef LITTLE_ENDIAN
 change_byteorder((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD);
# endif
 /*}}}  */
 local_arg->EEG.nsweeps++;	/* This gets patched into the header by write_synamps_exit */
 local_arg->EEG.compsweeps++;
 local_arg->EEG.acceptcnt++;
   break;
  case FORMAT_AVGFILE:
   if (local_arg->EEG.NumSamples!=0) {
    ERREXIT(tinfo->emethods, "write_synamps: Only a single epoch may be written to an .AVG file!\n");
   }
  case FORMAT_CNTFILE:
   if (tinfo->triggers.buffer_start!=NULL) {
    struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
    while (intrig->code!=0) {
     push_trigger(&local_arg->triggers,local_arg->EEG.NumSamples+intrig->position,intrig->code,intrig->description);
     intrig++;
    }
   }
   local_arg->EEG.NumSamples+=tinfo->nr_of_points;
   break;
 }

 tinfo_array(tinfo, &myarray);
 if (local_arg->output_format==FORMAT_AVGFILE) {
  /* points are the elements */
  float *pdata, * const buffer=(float *)malloc(myarray.nr_of_elements*sizeof(float));
  const char *fill="\0\0\0\0\0";
  if (buffer==NULL) {
   ERREXIT(tinfo->emethods, "write_synamps: Error allocating AVGFILE buffer\n");
  }
  do {
   int const channel=myarray.current_vector;
   /* Write the `5-byte channel header that is no longer used' */
   fwrite(fill, 1, 5, local_arg->SCAN);
   pdata=buffer;
   do {
    *pdata=NEUROSCAN_FLOATCONV(&local_arg->Channels[channel], array_scan(&myarray));
# ifndef LITTLE_ENDIAN
    Intel_float(pdata);
# endif
    pdata++;
   } while (myarray.message==ARRAY_CONTINUE);
   if ((int)fwrite(buffer,sizeof(float),myarray.nr_of_elements,local_arg->SCAN)!=myarray.nr_of_elements) {
    ERREXIT(tinfo->emethods, "write_synamps: Error writing data point\n");
   }
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  free(buffer);
 } else {
 int16_t * const buffer=(int16_t *)malloc(myarray.nr_of_vectors*sizeof(int16_t));
 if (buffer==NULL) {
  ERREXIT(tinfo->emethods, "write_synamps: Error allocating buffer\n");
 }
 array_transpose(&myarray);	/* channels are the elements */
 do {
  int channel=0;
  do {
   DATATYPE hold=array_scan(&myarray);
   buffer[channel]=NEUROSCAN_SHORTCONV(&local_arg->Channels[channel], hold);
# ifndef LITTLE_ENDIAN
   Intel_int16(&buffer[channel]);
# endif
   channel++;
  } while (myarray.message==ARRAY_CONTINUE);
  if ((int)fwrite(buffer,sizeof(int16_t),myarray.nr_of_elements,local_arg->SCAN)!=myarray.nr_of_elements) {
   ERREXIT(tinfo->emethods, "write_synamps: Error writing data point\n");
  }
 } while (myarray.message!=ARRAY_ENDOFSCAN);
 free(buffer);
 }
 } /* FREQ_DATA shifts loop */
 tinfo->tsdata=orig_tsdata;
  if (!args[ARGS_LINKED].is_set) {
   break;
  }
 } /* Linked epochs loop */

 return calltinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_synamps_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_synamps_exit(transform_info_ptr tinfo) {
 struct write_synamps_storage *local_arg=(struct write_synamps_storage *)tinfo->methods->local_storage;
#define SETUP_OFFSET_NSWEEPS 360
#define SETUP_OFFSET_NUMSAMPLES 864
#define SETUP_OFFSET_EVENTTABLEPOS 886

 /* Apparently NeuroScan write this even for epoched files although there's no event table there... 
  * It is important there to compute bytes_per_sample */
 local_arg->EEG.EventTablePos=ftell(local_arg->SCAN);
 if (local_arg->output_format==FORMAT_CNTFILE) {
  TEEG TagType;
  EVENT1 event;
  int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);
  int n;

  TagType.Teeg=TEEG_EVENT_TAB1;
  TagType.Size=(nevents+1)*sm_EVENT1[0].offset;	/* sm_EVENT1[0].offset is sizeof(EVENT1) in the file. */
  TagType.p_o.Offset=0L;
# ifndef LITTLE_ENDIAN
  change_byteorder((char *)&TagType, sm_TEEG);
# endif
  write_struct((char *)&TagType, sm_TEEG, local_arg->SCAN);

  for (n=0; n<nevents; n++) {
   struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+n;
   int acc=NAV_STARTSTOP, type=0, resp=0, keyboard=0;
   while (acc>=0 && intrig->code!=neuroscan_accept_translation[acc]) acc--;
   if (acc<0) {
    acc=0;
    if (intrig->code>0) {
     type=intrig->code;
    } else {
     resp=(-intrig->code)&0xf;
     if (resp==0) {
      keyboard=(-intrig->code)>>4;
      if (keyboard==0) {
       TRACEMS1(tinfo->emethods, 1, "write_synamps_exit: Can't decipher event code %d\n", MSGPARM(intrig->code));
      } else {
       keyboard--;
      }
     }
    }
   }
   event.StimType=type; event.KeyBoard=keyboard;
   Event_KeyPad_set(event, resp);
   Event_Accept_set(event, acc);
   event.Offset=point2offset(tinfo, intrig->position);
#  ifndef LITTLE_ENDIAN
   change_byteorder((char *)&event, sm_EVENT1);
#  endif
   write_struct((char *)&event, sm_EVENT1, local_arg->SCAN);
  }
  growing_buf_free(&local_arg->triggers);

  /* Write the end event */
  event.StimType=0;	event.KeyBoard=0;
  Event_KeyPad_set(event, 0);
  Event_Accept_set(event, NAV_STARTSTOP);
  event.Offset=local_arg->EEG.EventTablePos;
# ifndef LITTLE_ENDIAN
  change_byteorder((char *)&event, sm_EVENT1);
#endif
  write_struct((char *)&event, sm_EVENT1, local_arg->SCAN);

# ifndef LITTLE_ENDIAN
 Intel_int32(&local_arg->EEG.NumSamples);
 Intel_int32(&local_arg->EEG.EventTablePos);
#endif
  fseek(local_arg->SCAN, SETUP_OFFSET_NUMSAMPLES, SEEK_SET);
  fwrite(&local_arg->EEG.NumSamples,sizeof(int32_t),1,local_arg->SCAN);
  fseek(local_arg->SCAN, SETUP_OFFSET_EVENTTABLEPOS, SEEK_SET);
  fwrite(&local_arg->EEG.EventTablePos,sizeof(int32_t),1,local_arg->SCAN);
 } else {
 /*{{{  Patch the number of written epochs into the header*/
 fseek(local_arg->SCAN, SETUP_OFFSET_NSWEEPS, SEEK_SET);
# ifndef LITTLE_ENDIAN
 Intel_int16(&local_arg->EEG.nsweeps);
 Intel_int16(&local_arg->EEG.compsweeps);
 Intel_int16(&local_arg->EEG.acceptcnt);
 Intel_int32(&local_arg->EEG.EventTablePos);
# endif
 fwrite(&local_arg->EEG.nsweeps,sizeof(int16_t),1,local_arg->SCAN);
 fwrite(&local_arg->EEG.compsweeps,sizeof(int16_t),1,local_arg->SCAN);
 fwrite(&local_arg->EEG.acceptcnt,sizeof(int16_t),1,local_arg->SCAN);
 fseek(local_arg->SCAN, SETUP_OFFSET_EVENTTABLEPOS, SEEK_SET);
 fwrite(&local_arg->EEG.EventTablePos,sizeof(int32_t),1,local_arg->SCAN);
 /*}}}  */
 }

 fclose(local_arg->SCAN);
 local_arg->SCAN=NULL;
 free_pointer((void **)&local_arg->Channels);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_synamps(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_synamps(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_synamps_init;
 tinfo->methods->transform= &write_synamps;
 tinfo->methods->transform_exit= &write_synamps_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_synamps";
 tinfo->methods->method_description=
  "Put-epoch method to write data to a NeuroScan file\n";
 tinfo->methods->local_storage_size=sizeof(struct write_synamps_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
