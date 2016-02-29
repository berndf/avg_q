/*
 * Copyright (C) 1998-2000,2003,2006,2007,2010,2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * assert: reject_method to test values of various system variables
 * 						-- Bernd Feige 11.12.1998
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const variables_choice[]={
 "sfreq",
 "nr_of_points",
 "nr_of_channels",
 "itemsize",
 "leaveright",
 "length_of_output_region",
 "beforetrig",
 "aftertrig",
 "nroffreq",
 "nrofaverages",
 "accepted_epochs",
 "rejected_epochs",
 "failed_assertions",
 "condition",
 "firstvalue",
 "z_label",
 "z_value",
 "comment",
 "xchannelname",
 "channelname",
 "nr_of_triggers",
 NULL
};
enum variables_choice {
 C_SFREQ,
 C_NR_OF_POINTS,
 C_NR_OF_CHANNELS,
 C_ITEMSIZE,
 C_LEAVERIGHT,
 C_LENGTH_OF_OUTPUT_REGION,
 C_BEFORETRIG,
 C_AFTERTRIG,
 C_NROFFREQ,
 C_NROFAVERAGES,
 C_ACCEPTED_EPOCHS,
 C_REJECTED_EPOCHS,
 C_FAILED_ASSERTIONS,
 C_CONDITION,
 C_FIRSTVALUE,
 C_Z_LABEL,
 C_Z_VALUE,
 C_COMMENT,
 C_XCHANNELNAME,
 C_CHANNELNAME,
 C_NR_OF_TRIGGERS
};

LOCAL const char *const comparisons_choice[]={
 "==",
 "!=",
 "<",
 "<=",
 ">",
 ">=",
 "=~",
 "!~",
 NULL
};
enum COMPARISON_ENUM {
 EQUAL,
 UNEQUAL,
 LESSTHAN,
 LESSEQUAL,
 GREATERTHAN,
 GREATEREQUAL,
 MATCH,
 NOMATCH,
};

enum ARGS_ENUM {
 ARGS_ERROR=0, 
 ARGS_STOP, 
 ARGS_VARNAME, 
 ARGS_WHICHCOMP, 
 ARGS_VALUE, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Generate an error if the condition is not met", "E", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Stop the iterated queue if the condition is not met", "S", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "var_name", "", 0, variables_choice},
 {T_ARGS_TAKES_SELECTION, "comparison", "", 0, comparisons_choice},
 {T_ARGS_TAKES_STRING_WORD, "value", "", ARGDESC_UNUSED, NULL}
};

LOCAL Bool
compare_strings(char const *const s1, char const *const s2, enum COMPARISON_ENUM whichcomp) {
 Bool retval=FALSE;
 if (s1!=NULL && s2!=NULL) {
  if (whichcomp==MATCH || whichcomp==NOMATCH) {
   retval=(strstr(s1, s2)!=NULL);
   if (whichcomp==NOMATCH) retval= !retval;
  } else {
  int const comp=strcmp(s1, s2);
  switch (whichcomp) {
   case MATCH:
   case EQUAL:
    retval=(comp==0);
    break;
   case NOMATCH:
   case UNEQUAL:
    retval=(comp!=0);
    break;
   case LESSTHAN:
    retval=(comp<0);
    break;
   case LESSEQUAL:
    retval=(comp<=0);
    break;
   case GREATERTHAN:
    retval=(comp>0);
    break;
   case GREATEREQUAL:
    retval=(comp>=0);
    break;
  }
  }
 }
 return retval;
}
LOCAL Bool
compare_values(DATATYPE const v1, DATATYPE const v2, enum COMPARISON_ENUM const whichcomp) {
 Bool retval=FALSE;
 switch (whichcomp) {
  case MATCH:
  case EQUAL:
   retval=(v1==v2);
   break;
  case NOMATCH:
  case UNEQUAL:
   retval=(v1!=v2);
   break;
  case LESSTHAN:
   retval=(v1<v2);
   break;
  case LESSEQUAL:
   retval=(v1<=v2);
   break;
  case GREATERTHAN:
   retval=(v1>v2);
   break;
  case GREATEREQUAL:
   retval=(v1>=v2);
   break;
 }
 return retval;
}

/*{{{  assert_init(transform_info_ptr tinfo) {*/
METHODDEF void
assert_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  assert(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
assert(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 enum COMPARISON_ENUM const whichcomp=(enum COMPARISON_ENUM)args[ARGS_WHICHCOMP].arg.i;
 Bool accept=FALSE;

 switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
  case C_SFREQ:
   accept=compare_values(tinfo->sfreq, atof(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_NR_OF_POINTS:
   accept=compare_values(tinfo->nr_of_points, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_NR_OF_CHANNELS:
   accept=compare_values(tinfo->nr_of_channels, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_ITEMSIZE:
   accept=compare_values(tinfo->itemsize, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_LEAVERIGHT:
   accept=compare_values(tinfo->leaveright, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_LENGTH_OF_OUTPUT_REGION:
   accept=compare_values(tinfo->length_of_output_region, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_BEFORETRIG:
   accept=compare_values(tinfo->beforetrig, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_AFTERTRIG:
   accept=compare_values(tinfo->aftertrig, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_NROFFREQ:
   accept=compare_values(tinfo->nroffreq, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_NROFAVERAGES:
   accept=compare_values(tinfo->nrofaverages, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_ACCEPTED_EPOCHS:
   accept=compare_values(tinfo->accepted_epochs, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_REJECTED_EPOCHS:
   accept=compare_values(tinfo->rejected_epochs, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_FAILED_ASSERTIONS:
   accept=compare_values(tinfo->failed_assertions, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_CONDITION:
   accept=compare_values(tinfo->condition, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_FIRSTVALUE:
   accept=compare_values(tinfo->tsdata[0], get_value(args[ARGS_VALUE].arg.s,NULL), whichcomp);
   break;
  case C_Z_LABEL:
   accept=compare_strings(tinfo->z_label, args[ARGS_VALUE].arg.s, whichcomp);
   break;
  case C_Z_VALUE:
   accept=compare_values(tinfo->z_value, atof(args[ARGS_VALUE].arg.s), whichcomp);
   break;
  case C_COMMENT:
   accept=compare_strings(tinfo->comment, args[ARGS_VALUE].arg.s, whichcomp);
   break;
  case C_XCHANNELNAME:
   accept=compare_strings(tinfo->xchannelname, args[ARGS_VALUE].arg.s, whichcomp);
   break;
  case C_CHANNELNAME: {
   int channel;
   for (channel=0; channel<tinfo->nr_of_channels; channel++) {
    accept=compare_strings(tinfo->channelnames[channel], args[ARGS_VALUE].arg.s, whichcomp);
    /* Only in the `equal' comparison, we want to return TRUE if any channel
     * fulfills the condition; with all other comparisons, all channels have to. */
    if ((whichcomp==EQUAL && accept==TRUE) || (whichcomp!=EQUAL && accept==FALSE)) break;
   }
   }
   break;
  case C_NR_OF_TRIGGERS: {
   int nr_of_triggers=0;
   if (tinfo->triggers.buffer_start!=NULL) {
    struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
    while (intrig->code!=0) {
     intrig++;
     nr_of_triggers++;
    }
   }
   accept=compare_values(nr_of_triggers, atoi(args[ARGS_VALUE].arg.s), whichcomp);
   }
   break;
  default:
   break;
 }

 if (!accept) {
  tinfo->failed_assertions++;
  if (args[ARGS_ERROR].is_set) {
   ERREXIT(tinfo->emethods, "assert: Assertion failed.\n");
  }
  if (args[ARGS_STOP].is_set) {
   tinfo->stopsignal=TRUE;
  }
 }

 return (accept ? tinfo->tsdata : NULL);
}
/*}}}  */

/*{{{  assert_exit(transform_info_ptr tinfo) {*/
METHODDEF void
assert_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_assert(transform_info_ptr tinfo) {*/
GLOBAL void
select_assert(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &assert_init;
 tinfo->methods->transform= &assert;
 tinfo->methods->transform_exit= &assert_exit;
 tinfo->methods->method_type=REJECT_METHOD;
 tinfo->methods->method_name="assert";
 tinfo->methods->method_description=
  "Rejection method to assert conditions for values of various system variables.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
