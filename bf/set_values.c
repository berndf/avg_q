/*
 * Copyright (C) 1996-2004,2006,2007,2009-2011,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * set method to set any of a number of dataset parameters
 * 						-- Bernd Feige 21.04.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

LOCAL const char *const variables_choice[]={
 "sfreq",
 "sfreq_from_xdata",
 "leaveright",
 "beforetrig",
 "aftertrig",
 "beforetrig_at_xvalue",
 "nrofaverages",
 "accepted_epochs",
 "rejected_epochs",
 "failed_assertions",
 "condition",
 "xchannelname",
 "xdata",
 "xdata_from_channel",
 "posdata",
 "z_label",
 "z_value",
 "FREQ_DATA",
 "basefreq",
 "trigger",
 "triggers_from_trigfile",
 NULL
};
enum variables_choice {
 C_SFREQ,
 C_SFREQ_FROM_XDATA,
 C_LEAVERIGHT,
 C_BEFORETRIG,
 C_AFTERTRIG,
 C_BEFORETRIG_AT_XVALUE,
 C_NROFAVERAGES,
 C_ACCEPTED_EPOCHS,
 C_REJECTED_EPOCHS,
 C_FAILED_ASSERTIONS,
 C_CONDITION,
 C_XCHANNELNAME,
 C_XDATA,
 C_XDATA_FROM_CHANNEL,
 C_POSDATA,
 C_Z_LABEL,
 C_Z_VALUE,
 C_FREQ_DATA,
 C_BASEFREQ,
 C_TRIGGER,
 C_TRIGGERS_FROM_TRIGFILE
};

enum ARGS_ENUM {
 ARGS_VARNAME=0, 
 ARGS_VALUE, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SELECTION, "var_name", "", 0, variables_choice},
 {T_ARGS_TAKES_STRING_WORD, "value", "", ARGDESC_UNUSED, NULL}
};

/*{{{  Definition of set_storage*/
struct set_storage {
 growing_buf triggers;
 long total_points;
};
/*}}}  */

/*{{{  set_init(transform_info_ptr tinfo) {*/
METHODDEF void
set_init(transform_info_ptr tinfo) {
 struct set_storage *local_arg=(struct set_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 growing_buf_init(&local_arg->triggers);
 if ((enum variables_choice)args[ARGS_VARNAME].arg.i==C_TRIGGERS_FROM_TRIGFILE) {
  FILE * const triggerfile=(strcmp(args[ARGS_VALUE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_VALUE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "set_init: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "set_init: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_VALUE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 }
 local_arg->total_points=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  set(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
set(transform_info_ptr tinfo) {
 struct set_storage *local_arg=(struct set_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
  case C_SFREQ:
   tinfo->sfreq=atof(args[ARGS_VALUE].arg.s);
   if (tinfo->sfreq<=0) {
    ERREXIT(tinfo->emethods, "set: sfreq must be positive.\n");
   }
   break;
  case C_SFREQ_FROM_XDATA:
   if (tinfo->xdata==NULL || tinfo->nr_of_points<2) {
    ERREXIT(tinfo->emethods, "set sfreq_from_xdata: No xdata or data too short (need 2 points).\n");
   }
   tinfo->sfreq=1.0/(tinfo->xdata[1]-tinfo->xdata[0]);
   break;
  case C_LEAVERIGHT:
   tinfo->leaveright=atoi(args[ARGS_VALUE].arg.s);
   if (tinfo->leaveright>tinfo->itemsize) {
    ERREXIT2(tinfo->emethods, "set: itemsize=%d<leaveright=%d not allowed.\n", MSGPARM(tinfo->itemsize), MSGPARM(tinfo->leaveright));
   }
   break;
  case C_BEFORETRIG:
   tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_VALUE].arg.s);
   tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
   break;
  case C_AFTERTRIG:
   tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_VALUE].arg.s);
   tinfo->beforetrig=tinfo->nr_of_points-tinfo->aftertrig;
   break;
  case C_BEFORETRIG_AT_XVALUE: {
   DATATYPE value=atof(args[ARGS_VALUE].arg.s);
   if (tinfo->xdata==NULL) {
    ERREXIT(tinfo->emethods, "set beforetrig_at_xvalue: No xdata available!\n");
   }
   tinfo->beforetrig= find_pointnearx(tinfo, value);
   tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
   }
   break;
  case C_NROFAVERAGES:
   tinfo->nrofaverages=atoi(args[ARGS_VALUE].arg.s);
   break;
  case C_ACCEPTED_EPOCHS:
   tinfo->accepted_epochs=atoi(args[ARGS_VALUE].arg.s);
   break;
  case C_REJECTED_EPOCHS:
   tinfo->rejected_epochs=atoi(args[ARGS_VALUE].arg.s);
   break;
  case C_FAILED_ASSERTIONS:
   tinfo->failed_assertions=atoi(args[ARGS_VALUE].arg.s);
   break;
  case C_CONDITION:
   tinfo->condition=atoi(args[ARGS_VALUE].arg.s);
   break;
  case C_XCHANNELNAME:
   tinfo->xchannelname=(char *)malloc(strlen(args[ARGS_VALUE].arg.s)+1);
   if (tinfo->xchannelname==NULL) {
    ERREXIT(tinfo->emethods, "set xchannelname: Error allocating memory.\n");
   }
   strcpy(tinfo->xchannelname, args[ARGS_VALUE].arg.s);
   break;
  case C_XDATA:
   if (atof(args[ARGS_VALUE].arg.s)!=0.0) {
    create_xaxis(tinfo);
   } else {
    tinfo->xchannelname=NULL;
    free_pointer((void **)&tinfo->xdata);
   }
   break;
  case C_XDATA_FROM_CHANNEL: {
   int const channel=find_channel_number(tinfo,args[ARGS_VALUE].arg.s);
   if (channel>=0) {
    int i;
    array indata;
    tinfo_array(tinfo, &indata);
    indata.current_vector=channel;
    if (tinfo->xdata==NULL) create_xaxis(tinfo);
    for (i=0; i<tinfo->nr_of_points; i++) {
     tinfo->xdata[i]=array_scan(&indata);
    }
   } else {
    ERREXIT1(tinfo->emethods, "set xdata_from_channel: Unknown channel name >%s<\n", MSGPARM(args[ARGS_VALUE].arg.s));
   }
   }
   break;
  case C_POSDATA: {
   /* This lets create_channelgrid create new positions, names, or both */
   double value=atof(args[ARGS_VALUE].arg.s);
   if (value==1.0 || value<0.0) {
    /* Create new channel names */
    if (tinfo->channelnames!=NULL) {
     free_pointer((void **)&tinfo->channelnames[0]);
     free_pointer((void **)&tinfo->channelnames);
    }
   }
   if (value!=1.0) {
    /* Create new position values */
    free_pointer((void **)&tinfo->probepos);
   }
   if (value==0.0 || value==1.0) {
    create_channelgrid(tinfo);
   } else {
    create_channelgrid_ncols(tinfo, abs((int)rint(value)));
   }
   }
   break;
  case C_Z_LABEL:
   if (strcmp(args[ARGS_VALUE].arg.s, "NULL")==0) {
    tinfo->z_label=NULL;
   } else {
   tinfo->z_label=(char *)malloc(strlen(args[ARGS_VALUE].arg.s)+1);
   if (tinfo->z_label==NULL) {
    ERREXIT(tinfo->emethods, "set z_label: Error allocating memory.\n");
   }
   strcpy(tinfo->z_label, args[ARGS_VALUE].arg.s);
   }
   break;
  case C_Z_VALUE:
   if (strcmp(args[ARGS_VALUE].arg.s, "nrofaverages")==0) {
    tinfo->z_value=tinfo->nrofaverages;
   } else if (strcmp(args[ARGS_VALUE].arg.s, "accepted_epochs")==0) {
    tinfo->z_value=tinfo->accepted_epochs;
   } else if (strcmp(args[ARGS_VALUE].arg.s, "rejected_epochs")==0) {
    tinfo->z_value=tinfo->rejected_epochs;
   } else if (strcmp(args[ARGS_VALUE].arg.s, "condition")==0) {
    tinfo->z_value=tinfo->condition;
   } else {
    tinfo->z_value=atof(args[ARGS_VALUE].arg.s);
   }
   break;
  case C_FREQ_DATA:
   if (atof(args[ARGS_VALUE].arg.s)!=0.0) {
    if (tinfo->data_type==TIME_DATA) {
     multiplexed(tinfo);
     tinfo->nrofshifts=1;
     tinfo->nroffreq=tinfo->nr_of_points;
     tinfo->data_type=FREQ_DATA;
    }
   } else {
    if (tinfo->data_type==FREQ_DATA) {
     if (tinfo->nrofshifts==1) {
      tinfo->nr_of_points=tinfo->nroffreq;
      tinfo->data_type=TIME_DATA;
     } else {
      ERREXIT(tinfo->emethods, "set FREQ_DATA: Can't convert if nrofshifts>1.\n");
     }
    }
   }
   break;
  case C_BASEFREQ:
   tinfo->basefreq=atof(args[ARGS_VALUE].arg.s);
   break;
  case C_TRIGGER:
   if (strcmp(args[ARGS_VALUE].arg.s, "DELETE")==0) {
    clear_triggers(&tinfo->triggers);
   } else {
   growing_buf buf, tokenbuf;
   long pos;
   int code=1;
   char *description=NULL;
   growing_buf_init(&buf);
   growing_buf_takethis(&buf, args[ARGS_VALUE].arg.s);
   buf.delimiters=":";
   growing_buf_init(&tokenbuf);
   growing_buf_allocate(&tokenbuf,0);
   growing_buf_get_firsttoken(&buf,&tokenbuf);
   if (strncmp(tokenbuf.buffer_start, "x=", 2)==0) {
    if (tinfo->xdata==NULL) create_xaxis(tinfo);
    pos=decode_xpoint(tinfo, tokenbuf.buffer_start+2);
   } else {
    pos=gettimeslice(tinfo, tokenbuf.buffer_start);
   }
   if (growing_buf_get_nexttoken(&buf,&tokenbuf)) {
    code=atoi(tokenbuf.buffer_start);
   }
   if (growing_buf_get_nexttoken(&buf,&tokenbuf)) {
    description=(char *)malloc(strlen(tokenbuf.buffer_start)+1);
    strcpy(description,tokenbuf.buffer_start);
   }
   if (pos<0) pos=0;
   if (pos>=tinfo->nr_of_points) pos=tinfo->nr_of_points-1;

   if (tinfo->triggers.buffer_start==NULL) {
    growing_buf_allocate(&tinfo->triggers, 0);
   }
   if (tinfo->triggers.current_length==0) {
    /* First trigger entry holds file_start_point */
    push_trigger(&tinfo->triggers, 0L, -1, NULL);
   } else {
    /* Remove the old end marker */
    tinfo->triggers.current_length-=sizeof(struct trigger);
   }
   /* Set file start point of current epoch */
   ((struct trigger *)tinfo->triggers.buffer_start)->position=local_arg->total_points;
   push_trigger(&tinfo->triggers, pos, code, description);
   push_trigger(&tinfo->triggers, 0L, 0, NULL); /* End of list */

   growing_buf_free(&tokenbuf);
   growing_buf_free(&buf);
   }
   break;
  case C_TRIGGERS_FROM_TRIGFILE: {
   struct trigger *triggerptr=(struct trigger *)local_arg->triggers.buffer_start;
   if (tinfo->triggers.current_length==0) {
    /* First trigger entry holds file_start_point */
    push_trigger(&tinfo->triggers, 0, -1, NULL);
   } else {
    /* Remove the old end marker */
    tinfo->triggers.current_length-=sizeof(struct trigger);
   }
   /* Set file start point of current epoch */
   ((struct trigger *)tinfo->triggers.buffer_start)->position=local_arg->total_points;
   while ((char *)triggerptr-local_arg->triggers.buffer_start<local_arg->triggers.current_length) {
    if (triggerptr->position>=local_arg->total_points
     && triggerptr->position<local_arg->total_points+tinfo->nr_of_points) {
     push_trigger(&tinfo->triggers, triggerptr->position-local_arg->total_points, triggerptr->code, triggerptr->description);
    }
    triggerptr++;
   }
   push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
   }
   break;
  default:
   break;
 }

 local_arg->total_points+=tinfo->nr_of_points;
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  set_exit(transform_info_ptr tinfo) {*/
METHODDEF void
set_exit(transform_info_ptr tinfo) {
 struct set_storage *local_arg=(struct set_storage *)tinfo->methods->local_storage;

 growing_buf_free(&local_arg->triggers);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_set(transform_info_ptr tinfo) {*/
GLOBAL void
select_set(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &set_init;
 tinfo->methods->transform= &set;
 tinfo->methods->transform_exit= &set_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="set";
 tinfo->methods->method_description=
  "Transform method to set any of a number of data set parameters.\n";
 tinfo->methods->local_storage_size=sizeof(struct set_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
