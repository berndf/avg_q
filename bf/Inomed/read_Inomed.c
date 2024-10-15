/*
 * Copyright (C) 2008-2011,2013,2014,2024 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_Inomed.c module to read data from Inomed files.
 *	-- Bernd Feige 25.09.2008
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <ctype.h>
#include <setjmp.h>
#include <Intel_compat.h>
#include "Inomed.h"
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

#define MAX_COMMENTLEN 1024

enum ARGS_ENUM {
 ARGS_CONTINUOUS=0, 
 ARGS_TRIGTRANSFER, 
 ARGS_TRIGFILE,
 ARGS_TRIGLIST, 
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
};

/*{{{  Definition of read_Inomed_storage*/
struct read_Inomed_storage {
 MULTI_CHANNEL_CONTINUOUS EEG;
 FILE *infile;
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 long headersize;
 long points_in_file;
 long bytes_per_point;
 long current_point;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;

 jmp_buf error_jmp;
};
/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_Inomed_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_Inomed_storage *local_arg=(struct read_Inomed_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_Inomed_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_Inomed_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_Inomed_storage *local_arg=(struct read_Inomed_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_Inomed_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_Inomed_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
   free_pointer((void **)&description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  TRACEMS(tinfo->emethods, 0, "read_Inomed_build_trigbuffer: No trigger source known.\n");
 }
}
/*}}}  */
/*{{{  read_Inomed_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_Inomed_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_Inomed_storage *local_arg=(struct read_Inomed_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_Inomed_build_trigbuffer(tinfo);
 }
 {
 int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);

 if (local_arg->current_trigger<nevents) {
  struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+local_arg->current_trigger;
  *position=intrig->position;
   code    =intrig->code;
  if (descriptionp!=NULL) *descriptionp=intrig->description;
 }
 }
 local_arg->current_trigger++;
 return code;
}
/*}}}  */

/*{{{  read_Inomed_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_Inomed_init(transform_info_ptr tinfo) {
 struct read_Inomed_storage *local_arg=(struct read_Inomed_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 struct stat statbuff;
 int channel;

 growing_buf_init(&local_arg->triggers);

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_Inomed_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }

 if (read_struct((char *)&local_arg->EEG, sm_MULTI_CHANNEL_CONTINUOUS, local_arg->infile)==0) {
  ERREXIT1(tinfo->emethods, "read_Inomed_init: Short file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->EEG, sm_MULTI_CHANNEL_CONTINUOUS);
#endif
 for (channel=0; channel<local_arg->EEG.sNumberOfChannels; channel++) {
  if (read_struct((char *)&local_arg->EEG.strctChannel[channel], sm_CHANNEL, local_arg->infile)==0
    ||read_struct((char *)&local_arg->EEG.strctChannel[channel].strctHighPass, sm_DIG_FILTER, local_arg->infile)==0
    ||read_struct((char *)&local_arg->EEG.strctChannel[channel].strctLowPass, sm_DIG_FILTER, local_arg->infile)==0) {
   ERREXIT1(tinfo->emethods, "read_Inomed_init: Short file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->EEG.strctChannel[channel], sm_CHANNEL);
  change_byteorder((char *)&local_arg->EEG.strctChannel[channel].strctHighPass, sm_DIG_FILTER);
  change_byteorder((char *)&local_arg->EEG.strctChannel[channel].strctLowPass, sm_DIG_FILTER);
#endif
 }

 tinfo->sfreq=local_arg->EEG.fSamplingFrequency;
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 /* Note that we can't use stat here as args[ARGS_IFILE].arg.s might be "stdin"... */
 fstat(fileno(local_arg->infile),&statbuff);

 local_arg->bytes_per_point=local_arg->EEG.sNumberOfChannels*sizeof(float);
 local_arg->headersize=sm_MULTI_CHANNEL_CONTINUOUS[0].offset+MAX_NUMBER_OF_CHANNELS*(sm_CHANNEL[0].offset+2*sm_DIG_FILTER[0].offset);
 local_arg->points_in_file = (statbuff.st_size-local_arg->headersize)/local_arg->bytes_per_point;
 tinfo->points_in_file=local_arg->points_in_file;

 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_Inomed_init: Error allocating triglist memory\n");
   }
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   if (local_arg->points_in_file==0) {
    ERREXIT(tinfo->emethods, "read_Inomed: Unable to determine the number of samples in the input file!\n");
   }
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }

 read_Inomed_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_value: Central function for reading a number*/
enum ERROR_ENUM {
 ERR_NONE=0,
 ERR_READ,
 ERR_NUMBER
};
LOCAL DATATYPE
read_value(FILE *infile, 
 struct read_Inomed_storage *local_arg, 
 Bool swap_byteorder) {
 DATATYPE dat=0;
 enum ERROR_ENUM err=ERR_NONE;
 float c;
 if (fread(&c, sizeof(float), 1, infile)!=1) err=ERR_READ;
 if (swap_byteorder) Intel_float(&c);
 dat=c*1.0e6; /* Inomed raw data is in Volts; Convert to µV as per avg_q standards */
 if (err!=ERR_NONE) {
  longjmp(local_arg->error_jmp, err);
 }
 return dat;
}
/*}}}  */

/*{{{  read_Inomed(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_Inomed(transform_info_ptr tinfo) {
 struct read_Inomed_storage *local_arg=(struct read_Inomed_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 FILE *infile=local_arg->infile;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->EEG.sNumberOfChannels;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_Inomed: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Find the next window that fits into the actual data*/
 /* This is just for the Continuous option (no trigger file): */
 file_end_point=local_arg->current_point-1;
 do {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Simulate a trigger at current_point+beforetrig */
   file_start_point=file_end_point+1;
   trigger_point=file_start_point+tinfo->beforetrig;
   file_end_point=trigger_point+tinfo->aftertrig-1;
   if (local_arg->points_in_file>0 && file_end_point>=local_arg->points_in_file) return NULL;
   local_arg->current_trigger++;
   local_arg->current_point+=tinfo->nr_of_points;
   tinfo->condition=0;
  } else 
  do {
   tinfo->condition=read_Inomed_read_trigger(tinfo, &trigger_point, &description);
   if (tinfo->condition==0) return NULL;	/* No more triggers in file */
   file_start_point=trigger_point-tinfo->beforetrig+local_arg->offset;
   file_end_point=trigger_point+tinfo->aftertrig-1-local_arg->offset;
   
   if (local_arg->trigcodes==NULL) {
    not_correct_trigger=FALSE;
   } else {
    int trigno=0;
    not_correct_trigger=TRUE;
    while (local_arg->trigcodes[trigno]!=0) {
     if (local_arg->trigcodes[trigno]==tinfo->condition) {
      not_correct_trigger=FALSE;
      break;
     }
     trigno++;
    }
   }
  } while (not_correct_trigger || file_start_point<0 || (local_arg->points_in_file>0 && file_end_point>=local_arg->points_in_file));
 } while (--local_arg->fromepoch>0);
 if (description==NULL) {
  TRACEMS3(tinfo->emethods, 1, "read_Inomed: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_Inomed: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
 }
 /*}}}  */

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_Inomed_reset_triggerbuffer(tinfo);
  for (; (code=read_Inomed_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 fseek(infile, local_arg->headersize+file_start_point*local_arg->bytes_per_point, SEEK_SET);

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 tinfo->multiplexed=TRUE;
 myarray.nr_of_elements=tinfo->nr_of_channels;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 tinfo->xdata=NULL;
 if (array_allocate(&myarray)==NULL
  || (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL) {
  ERREXIT(tinfo->emethods, "read_Inomed: Error allocating data\n");
 }
 /*}}}  */

 if (description==NULL) {
  snprintf(tinfo->comment, MAX_COMMENTLEN, "read_Inomed %s", args[ARGS_IFILE].arg.s);
 } else {
  snprintf(tinfo->comment, MAX_COMMENTLEN, "%s", description);
 }

 switch ((enum ERROR_ENUM)setjmp(local_arg->error_jmp)) {
  case ERR_NONE:
   do {
    do {
     DATATYPE * const item0_addr=ARRAY_ELEMENT(&myarray);
     int itempart;
     /* Multiple items are always stored side by side: */
     for (itempart=0; itempart<tinfo->itemsize; itempart++) {
      item0_addr[itempart]=read_value(infile, local_arg, !LITTLE_ENDIAN);
     }
     array_advance(&myarray);
    } while (myarray.message==ARRAY_CONTINUE);
   } while (myarray.message!=ARRAY_ENDOFSCAN);
   break;
  case ERR_READ:
  case ERR_NUMBER:
   /* Note that a number_err is always a hard error... */
   ERREXIT(tinfo->emethods, "read_Inomed: Error reading data\n");
   break;
 }

 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, deepfree_tinfo will free someone else's data ! */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */

 tinfo->file_start_point=file_start_point;
 tinfo->z_label=NULL;
 tinfo->sfreq=local_arg->EEG.fSamplingFrequency;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_Inomed_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_Inomed_exit(transform_info_ptr tinfo) {
 struct read_Inomed_storage *local_arg=(struct read_Inomed_storage *)tinfo->methods->local_storage;

 if (local_arg->infile!=NULL) fclose(local_arg->infile);
 local_arg->infile=NULL;
 free_pointer((void **)&local_arg->trigcodes);

 if (local_arg->triggers.buffer_start!=NULL) {
  clear_triggers(&local_arg->triggers);
  growing_buf_free(&local_arg->triggers);
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_inomed(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_inomed(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_Inomed_init;
 tinfo->methods->transform= &read_Inomed;
 tinfo->methods->transform_exit= &read_Inomed_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_inomed";
 tinfo->methods->method_description=
  "Get-epoch method reading Inomed raw data files.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_Inomed_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
