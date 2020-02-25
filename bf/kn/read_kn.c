/*
 * Copyright (C) 1996-2001,2003,2010,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_kn.c module to read data from Konstanz (Patrick Berg) file.
 * part of this code was taken from Patrick's `3ascexp.c'.
 *	-- Bernd Feige 4.06.1995
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "kn.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_TRIGLIST=0, 
 ARGS_TRIGFORM, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "Trigger list: Restrict epochs to these `conditions'. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "string: Specify how trigger codes are formed from the 3 condition and 5 marker values, eg: m5c2. Default: c1", "T", ARGDESC_UNUSED, (const char *const *)"c1"},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.raw"}
};

/*{{{  Definition of read_kn_storage*/
struct read_kn_storage {
 struct Rtrial trialb;   /* trial base */
 short *trialarray;        /* trial array */
 short *trialdata;         /* trial data */
 struct Rheader headb;   /* header base */
 char headstring[MAXHDSTRING];/* header string */
 short *headarray;         /* header array */
 int *trigcodes;
 long offset;
 long fromepoch;
 long epochs;
 int current_epoch;
 short *trigger_form[sizeof(int)];
};
/*}}}  */

/*{{{  GetScale(struct read_kn_storage *local_arg) {*/
LOCAL short *
GetScale(struct read_kn_storage *local_arg) {
 if(local_arg->headb.subject == -1)
  return(local_arg->trialb.parray + local_arg->trialb.nkan);
 else
  return(local_arg->headb.parray + local_arg->headb.maxchannels);
}
/*}}}  */

/*{{{  parse_trigform(transform_info_ptr tinfo, struct Rtrial *trialb, short **trigger_form, char const *trigform) {*/
GLOBAL void
parse_trigform(transform_info_ptr tinfo, struct Rtrial *trialb, short **trigger_form, char const *trigform) {
 int i;
 char const *inform=trigform+strlen(trigform);
 for (i=sizeof(int)-1; i>=0; i--) {
  if (inform==trigform) {
   trigger_form[i]=NULL;
  } else {
   inform--;
   while (inform>=trigform && isdigit(*inform)) inform--;
   if (inform>=trigform) {
    int const number=atoi(inform+1);
    if (number<=0) {
     ERREXIT(tinfo->emethods, "parse_trigform (kn): Index must be >=1\n");
    }
    switch(*inform) {
     case 'c':
      if (number>T_CONDITION_SIZE) {
       ERREXIT1(tinfo->emethods, "parse_trigform (kn): condition index too large (>%d)\n", MSGPARM(T_CONDITION_SIZE));
      }
      trigger_form[i]= &trialb->condition[number-1];
      break;
     case 'm':
      if (number>T_MARKER_SIZE) {
       ERREXIT1(tinfo->emethods, "parse_trigform (kn): marker index too large (>%d)\n", MSGPARM(T_MARKER_SIZE));
      }
      trigger_form[i]= &trialb->marker[number-1];
      break;
     default:
      ERREXIT(tinfo->emethods, "parse_trigform (kn): Unknown identifier char\n");
    }
   } else {
    ERREXIT(tinfo->emethods, "parse_trigform (kn): Identifier char missing\n");
   }
  }
 }
}

LOCAL int
construct_trigger(short **trigger_form) {
 int trigger=0;
 unsigned int i;
 for (i=0; i<sizeof(int); i++) {
  if (trigger_form[i]!=NULL) {
   trigger=(trigger<<8)+ *trigger_form[i];
  }
 }
 return trigger;
}
/*}}}  */

extern struct external_methods_struct *kn_emethods;	/* Used by rawdatan.c */

/*{{{  read_kn_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_kn_init(transform_info_ptr tinfo) {
 struct read_kn_storage *local_arg=(struct read_kn_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /*{{{  Process options*/
 if (args[ARGS_TRIGLIST].is_set) {
  local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
  if (local_arg->trigcodes==NULL) {
   ERREXIT(tinfo->emethods, "read_kn_init: Error allocating triglist memory\n");
  }
 } else {
  local_arg->trigcodes=NULL;
 }
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 parse_trigform(tinfo, &local_arg->trialb, local_arg->trigger_form, (args[ARGS_TRIGFORM].is_set ? args[ARGS_TRIGFORM].arg.s : "c1"));

 kn_emethods=tinfo->emethods;
 ROpen(args[ARGS_IFILE].arg.s,&local_arg->headb,&local_arg->trialb,"rb");
 local_arg->current_epoch=1;	/* Patrick's trial numbers start with 1 */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_kn(transform_info_ptr tinfo) {*/
/*
 * The method read_kn() returns a pointer to the allocated tsdata memory
 * location or NULL if End Of File encountered. EOF within the data set
 * results in an ERREXIT; NULL is returned ONLY if the first read resulted
 * in an EOF condition. This allows programs to look for multiple data sets
 * in a single file.
 */

METHODDEF DATATYPE *
read_kn(transform_info_ptr tinfo) {
 struct read_kn_storage *local_arg=(struct read_kn_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char *innamebuf;
 short *puV;	/* Scale for import */
 short *pdata;
 array myarray;
 int not_correct_trigger;

 if (local_arg->epochs--==0) return NULL;

 kn_emethods=tinfo->emethods;
 do {
  do {
   if (local_arg->current_epoch>local_arg->headb.trials) return NULL;
   /*{{{  Read the next trial header and see if it's one of the triggered*/
   RGetTrialBase(local_arg->current_epoch++,&local_arg->trialb);
   if (local_arg->trigcodes==NULL) {
    not_correct_trigger=FALSE;
   } else {
    int trigno=0;
    not_correct_trigger=TRUE;
    while (local_arg->trigcodes[trigno]!=0) {
     int const marker=construct_trigger(local_arg->trigger_form);
     if (local_arg->trigcodes[trigno]==marker) not_correct_trigger=FALSE;
     trigno++;
    }
   }
   /*}}}  */
  } while (not_correct_trigger);
 } while (--local_arg->fromepoch>0);
 tinfo->condition=construct_trigger(local_arg->trigger_form);
 TRACEMS2(tinfo->emethods, 1, "read_kn: Reading trial %d, condition=%d\n", MSGPARM(local_arg->current_epoch-1), MSGPARM(tinfo->condition));
 RGetTrial(local_arg->current_epoch-1,&local_arg->trialb);
 tinfo->sfreq=1e6/local_arg->trialb.nabt;
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_elements=tinfo->nr_of_points=local_arg->trialb.npoints;
 myarray.nr_of_vectors=tinfo->nr_of_channels=local_arg->trialb.nkan;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_kn: Error allocating data\n");
 }
 array_transpose(&myarray);	/* Kn byte order is multiplexed, ie channels fastest */
 tinfo->multiplexed=FALSE;
 pdata=local_arg->trialb.pdata;
 puV = GetScale(local_arg);
 do {
  /* puV is indexed by the channel number */
  array_write(&myarray, ((DATATYPE)*pdata++)/puV[myarray.current_element]);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 /* Force create_channelgrid to allocate both channelnames and probepos: */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 create_channelgrid(tinfo);

 tinfo->xdata=NULL;
 tinfo->xchannelname=tinfo->z_label=NULL;
 if ((innamebuf=(char *)malloc(strlen(local_arg->headb.pstring)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_kn: Error allocating headb.pstring\n");
 }
 strcpy(innamebuf, local_arg->headb.pstring);	/* Comment */
 tinfo->comment=innamebuf;

 tinfo->tsdata=myarray.start;
 tinfo->beforetrig= -local_arg->offset;
 tinfo->aftertrig=tinfo->nr_of_points+local_arg->offset;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;
 tinfo->nrofaverages=1;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_kn_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_kn_exit(transform_info_ptr tinfo) {
 struct read_kn_storage *local_arg=(struct read_kn_storage *)tinfo->methods->local_storage;

 kn_emethods=tinfo->emethods;
 RClose(&local_arg->headb,&local_arg->trialb);
 free_pointer((void **)&local_arg->trigcodes);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_kn(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_kn(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_kn_init;
 tinfo->methods->transform= &read_kn;
 tinfo->methods->transform_exit= &read_kn_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_kn";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from a Konstanz (Patrick Berg) file.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_kn_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
