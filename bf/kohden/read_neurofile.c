/*
 * Copyright (C) 1997-1999,2001,2003,2006-2011 Bernd Feige
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
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_neurofile.c module to read data from Nihon Kohden NeuroFile format
 *	-- Bernd Feige 26.09.1997
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "neurofile.h"
#include "transform.h"
#include "bf.h"
/*}}}  */

#ifndef __GNUC__
/* This is because not all compilers accept automatic, variable-size arrays...
 * Have to use constant lengths for those path strings... I hate it... */
#define MAX_PATHLEN 1024
#endif

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

/*{{{  Definition of read_neurofile_storage*/
struct read_neurofile_storage {
 struct sequence seq;
 DATATYPE *last_values;
 DATATYPE factor;	/* Factor to multiply the raw data with to obtain microvolts */
 FILE *infile;
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 int nr_of_channels;
 long stringlength;	/* Bytes to allocate for channel names */
 long points_in_file;
 long current_point;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 float sfreq;
};
/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_neurofile_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_neurofile_storage *local_arg=(struct read_neurofile_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_neurofile_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_neurofile_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_neurofile_storage *local_arg=(struct read_neurofile_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_neurofile_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_neurofile_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  TRACEMS(tinfo->emethods, 0, "read_neurofile_build_trigbuffer: No trigger source known.\n");
 }
}
/*}}}  */
/*{{{  read_neurofile_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_neurofile_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_neurofile_storage *local_arg=(struct read_neurofile_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_neurofile_build_trigbuffer(tinfo);
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

/*{{{  read_neurofile_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_neurofile_init(transform_info_ptr tinfo) {
 struct read_neurofile_storage *local_arg=(struct read_neurofile_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel;
 FILE *dsc;
#ifdef __GNUC__
 char filename[strlen(args[ARGS_IFILE].arg.s)+5];
#else
 char filename[MAX_PATHLEN];
#endif

 growing_buf_init(&local_arg->triggers);

 strcpy(filename, args[ARGS_IFILE].arg.s);
 strcat(filename, ".dsc");
 if((dsc=fopen(filename, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_neurofile_init: Can't open dsc file >%s<\n", MSGPARM(filename));
 }
 if (read_struct((char *)&local_arg->seq, sm_sequence, dsc)==0) {
  ERREXIT1(tinfo->emethods, "read_neurofile_init: Can't read header in file >%s<\n", MSGPARM(filename));
 }
 fclose(dsc);
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->seq, sm_sequence);
#endif
 local_arg->points_in_file = 256L*local_arg->seq.length;
 tinfo->points_in_file=local_arg->points_in_file;
 tinfo->sfreq=local_arg->sfreq=neurofile_map_sfreq(local_arg->seq.fastrate);
 local_arg->nr_of_channels=local_arg->seq.nfast;
 local_arg->factor=local_arg->seq.gain/1000.0;
 local_arg->stringlength=0;
 for (channel=0; channel<local_arg->nr_of_channels; channel++) {
  local_arg->stringlength+=strlen((char *)local_arg->seq.elnam[channel])+1;
 }

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 strcpy(filename+strlen(args[ARGS_IFILE].arg.s), ".eeg");
 if((local_arg->infile=fopen(filename, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_neurofile_init: Can't open file >%s<\n", MSGPARM(filename));
 }
 if ((local_arg->last_values=(DATATYPE *)calloc(local_arg->nr_of_channels, sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "read_neurofile_init: Error allocating last_values memory\n");
 }

 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   growing_buf buf;
   Bool havearg;
   int trigno=0;

   growing_buf_init(&buf);
   growing_buf_takethis(&buf, args[ARGS_TRIGLIST].arg.s);
   buf.delimiters=",";

   havearg=growing_buf_firsttoken(&buf);
   if ((local_arg->trigcodes=(int *)malloc((buf.nr_of_tokens+1)*sizeof(int)))==NULL) {
    ERREXIT(tinfo->emethods, "read_neurofile_init: Error allocating triglist memory\n");
   }
   while (havearg) {
    local_arg->trigcodes[trigno]=atoi(buf.current_token);
    havearg=growing_buf_nexttoken(&buf);
    trigno++;
   }
   local_arg->trigcodes[trigno]=0;   /* End mark */
   growing_buf_free(&buf);
  } else {
   local_arg->trigcodes=NULL;
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   if (local_arg->points_in_file==0) {
    ERREXIT(tinfo->emethods, "read_neurofile: Unable to determine the number of samples in the input file!\n");
   }
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }

 read_neurofile_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;

 tinfo->filetriggersp=&local_arg->triggers;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_neurofile(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_neurofile(transform_info_ptr tinfo) {
 struct read_neurofile_storage *local_arg=(struct read_neurofile_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 FILE *infile=local_arg->infile;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *innamebuf;
 int channel;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->nr_of_channels;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_neurofile: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
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
   tinfo->condition=read_neurofile_read_trigger(tinfo, &trigger_point, &description);
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
  TRACEMS3(tinfo->emethods, 1, "read_neurofile: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_neurofile: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
 }

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_neurofile_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; code=read_neurofile_read_trigger(tinfo, &trigpoint, &thisdescription), 
	 (code!=0 && trigpoint<=file_end_point); ) {
   if (trigpoint>=file_start_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 fseek(infile, file_start_point*tinfo->nr_of_channels, SEEK_SET);

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 tinfo->multiplexed=TRUE;
 myarray.nr_of_elements=tinfo->nr_of_channels;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 if (array_allocate(&myarray)==NULL || 
  (tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
  (innamebuf=(char *)malloc(local_arg->stringlength))==NULL ||
  (tinfo->comment=(char *)malloc(strlen((char *)local_arg->seq.comment)+(1+17+1)))==NULL) {
  ERREXIT(tinfo->emethods, "read_neurofile: Error allocating data\n");
 }
 /*}}}  */

 sprintf(tinfo->comment, "%s %02d/%02d/%02d,%02d:%02d:%02d", local_arg->seq.comment, local_arg->seq.month, local_arg->seq.day, local_arg->seq.year, local_arg->seq.hour, local_arg->seq.minute, local_arg->seq.second);
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  strcpy(innamebuf, (char *)local_arg->seq.elnam[channel]);
  tinfo->channelnames[channel]=innamebuf;
  innamebuf+=strlen(innamebuf)+1;
 }

 do {
  signed char exponent;
  short delta;
  if (fread(&exponent, sizeof(exponent), 1, infile)!=1) {
   ERREXIT(tinfo->emethods, "read_neurofile: Error reading data\n");
  }
  delta=exponent;
  exponent&=0x03;
  if (exponent==0x03) {
   delta>>=2;
  } else {
   delta=(delta&0xfffc)<<(exponent*2);
  }
  local_arg->last_values[myarray.current_element]+=delta;
  array_write(&myarray, local_arg->last_values[myarray.current_element]*local_arg->factor);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 /* There actually are x-y positions in the file, but some of them are zero,
  * so they're not really suited for us */
 tinfo->probepos=NULL;
 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, deepfree_tinfo will free someone else's data ! */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */

 tinfo->z_label=NULL;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_neurofile_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_neurofile_exit(transform_info_ptr tinfo) {
 struct read_neurofile_storage *local_arg=(struct read_neurofile_storage *)tinfo->methods->local_storage;

 fclose(local_arg->infile);
 local_arg->infile=NULL;
 free_pointer((void **)&local_arg->trigcodes);
 growing_buf_free(&local_arg->triggers);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_neurofile(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_neurofile(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_neurofile_init;
 tinfo->methods->transform= &read_neurofile;
 tinfo->methods->transform_exit= &read_neurofile_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_neurofile";
 tinfo->methods->method_description=
  "Get-epoch method to read the NeuroFile II format by Nihon Kohden.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_neurofile_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
