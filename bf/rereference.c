/*
 * Copyright (C) 1996-1999,2001 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * rereference.c method to subtract the mean across a number of reference
 *  channels from all channels
 *	-- Bernd Feige 29.12.1995
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_EXCLUDEOTHER=0, 
 ARGS_EXCLUDECHANS, 
 ARGS_REFCHANS, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Exclude all non-reference channels", "E", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "exclude_channelnames", "e", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "ref_channelnames", "", ARGDESC_UNUSED, NULL},
};

/*{{{  Local definitions*/
struct rereference_args_struct {
 int *refchannel_numbers;
 int *excludechannel_numbers;
};
/*}}}  */

/*{{{  rereference_init(transform_info_ptr tinfo)*/
METHODDEF void
rereference_init(transform_info_ptr tinfo) {
 struct rereference_args_struct *rereference_args=(struct rereference_args_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 rereference_args->refchannel_numbers=expand_channel_list(tinfo, args[ARGS_REFCHANS].arg.s);
 if (rereference_args->refchannel_numbers==NULL) {
  ERREXIT(tinfo->emethods, "rereference_init: No reference channel was selected.\n");
 }
 if (args[ARGS_EXCLUDECHANS].is_set) {
  rereference_args->excludechannel_numbers=expand_channel_list(tinfo, args[ARGS_EXCLUDECHANS].arg.s);
  if (rereference_args->excludechannel_numbers==NULL) {
   TRACEMS(tinfo->emethods, 0, "rereference_init: No exclude channels were actually selected!\n");
  }
 } else {
  rereference_args->excludechannel_numbers=NULL;
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  rereference(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
rereference(transform_info_ptr tinfo) {
 struct rereference_args_struct *rereference_args=(struct rereference_args_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int itempart;
 transform_info_ptr const tinfoptr=tinfo;
 array indata;

  tinfo_array(tinfoptr, &indata);
  array_transpose(&indata);	/* Vector=map */
  for (itempart=0; itempart<tinfoptr->itemsize-tinfoptr->leaveright; itempart++) {
   array_use_item(&indata, itempart);
   do {
    DATATYPE mean=0;
    int n=0;

    do {
     if (is_in_channellist(indata.current_element+1, rereference_args->refchannel_numbers)) {
      mean+=array_scan(&indata);
      n++;
     } else {
      array_advance(&indata);
     }
    } while (indata.message==ARRAY_CONTINUE);
    mean/=n;
    array_previousvector(&indata);
    do {
     if ((args[ARGS_EXCLUDEOTHER].is_set && !is_in_channellist(indata.current_element+1, rereference_args->refchannel_numbers)) 
      || is_in_channellist(indata.current_element+1, rereference_args->excludechannel_numbers)) {
      array_advance(&indata);
     } else {
      array_write(&indata, READ_ELEMENT(&indata)-mean);
     }
    } while (indata.message==ARRAY_CONTINUE);
   } while (indata.message!=ARRAY_ENDOFSCAN);
  }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  rereference_exit(transform_info_ptr tinfo)*/
METHODDEF void
rereference_exit(transform_info_ptr tinfo) {
 struct rereference_args_struct *rereference_args=(struct rereference_args_struct *)tinfo->methods->local_storage;

 free_pointer((void **)&rereference_args->refchannel_numbers);
 free_pointer((void **)&rereference_args->excludechannel_numbers);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_rereference(transform_info_ptr tinfo)*/
GLOBAL void
select_rereference(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &rereference_init;
 tinfo->methods->transform= &rereference;
 tinfo->methods->transform_exit= &rereference_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="rereference";
 tinfo->methods->method_description=
  "Method to subtract the mean of a number of reference channels from\n"
  " each incoming map\n";
 tinfo->methods->local_storage_size=sizeof(struct rereference_args_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
