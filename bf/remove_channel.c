/*
 * Copyright (C) 1996-2001,2003,2004,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * remove_channel method to remove one channel from the data.
 * 						-- Bernd Feige 25.01.1994
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

enum ARGS_ENUM {
 ARGS_BYNAME=0, 
 ARGS_KEEPCHANNELS, 
 ARGS_REMOVERANGES, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Remove channels by name", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Keep these channels, ie remove all but the named channels", "k", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_SENTENCE, "channel_number_start1 [channel_number_end1  channel_number_start2 ...]", " ", ARGDESC_UNUSED, NULL}
};

/*{{{  Define the local storage struct*/
struct remove_channel_storage {
 int *keep_list;
 int *remove_ranges;
};
/*}}}  */

/*{{{  remove_channel_init(transform_info_ptr tinfo) {*/
LOCAL Bool
is_in_one_of_the_ranges(int val, int *ranges) {
 while (*ranges!=0) {
  if (val>=ranges[0] && val<=ranges[1]) return TRUE;
  ranges+=2;
 }
 return FALSE;
}

LOCAL int
read_int(transform_info_ptr tinfo, char *token) {
 char *endptr;
 int const value=strtol(token, &endptr, 10);
 if (*endptr!='\0' || value<1) {
  ERREXIT1(tinfo->emethods, "remove_channel_init: Invalid or malformed channel number: %s\n", MSGPARM(token));
 }
 return value;
}

METHODDEF void
remove_channel_init(transform_info_ptr tinfo) {
 struct remove_channel_storage *local_arg=(struct remove_channel_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 local_arg->keep_list=NULL;
 local_arg->remove_ranges=NULL;

 if (args[ARGS_KEEPCHANNELS].is_set || args[ARGS_BYNAME].is_set) {
  /* These are handled in remove_channel */
 } else if (args[ARGS_REMOVERANGES].is_set) {
  /*{{{  Build remove_ranges*/
  growing_buf buf, tokenbuf;
  int nr_of_ranges_to_remove=0, last_channel;
  int *in_remove_ranges;

  growing_buf_init(&buf);
  growing_buf_takethis(&buf, args[ARGS_REMOVERANGES].arg.s);
  growing_buf_init(&tokenbuf);
  growing_buf_allocate(&tokenbuf, 0);

  if (!growing_buf_get_firsttoken(&buf, &tokenbuf)) {
   ERREXIT(tinfo->emethods, "remove_channel_init: Not enough arguments\n");
  }

  /*{{{  Count ranges and check consistency*/
  last_channel=0;
  while (tokenbuf.current_length>0) {
   if (last_channel==0) {
    last_channel=read_int(tinfo, tokenbuf.buffer_start);
   } else {
    int const diff=read_int(tinfo, tokenbuf.buffer_start)-last_channel;
    if (diff<0 || last_channel<=0) {
     ERREXIT(tinfo->emethods, "remove_channel_init: Channel start/end pairs must be >0 and ascending\n");
    }
    last_channel=0;
    nr_of_ranges_to_remove++;
   }
   growing_buf_get_nexttoken(&buf, &tokenbuf);
  }
  if (last_channel!=0) nr_of_ranges_to_remove++;
  /*}}}  */

  if ((local_arg->remove_ranges=(int *)malloc((2*nr_of_ranges_to_remove+1)*sizeof(int)))==NULL) {
   ERREXIT(tinfo->emethods, "remove_channel_init: Error allocating remove_ranges memory\n");
  }

  /*{{{  Transfer ranges to remove_ranges list*/
  growing_buf_get_firsttoken(&buf, &tokenbuf);
  last_channel=0;
  in_remove_ranges=local_arg->remove_ranges;
  while (tokenbuf.current_length>0) {
   if (last_channel==0) {
    last_channel=read_int(tinfo, tokenbuf.buffer_start);
   } else {
    *in_remove_ranges++ =last_channel;
    *in_remove_ranges++ =read_int(tinfo, tokenbuf.buffer_start);
    last_channel=0;
   }
   growing_buf_get_nexttoken(&buf, &tokenbuf);
  }
  if (last_channel!=0) {
   /* No range end given: Range starts and ends with last_channel */
   *in_remove_ranges++ = last_channel;
   *in_remove_ranges++ = last_channel;
  }
  *in_remove_ranges=0;	/* End of list */
  /*}}}  */
  growing_buf_free(&tokenbuf);
  growing_buf_free(&buf);
  /*}}}  */
 } else {
  ERREXIT(tinfo->emethods, "remove_channel_init: Not enough arguments\n");
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  remove_channel(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
remove_channel(transform_info_ptr tinfo) {
 struct remove_channel_storage *local_arg=(struct remove_channel_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel, item, stringlen, *in_keep_list;
 char **new_channelnames, *in_channelnames;
 double *new_probepos;
 array myarray, newarray;
 long tsdata_step, tsdata_steps, tsdata_stepwidth;
 DATATYPE * const orig_tsdata=tinfo->tsdata;
 int new_nr_of_channels;

 /*{{{  If channels are selected by names, build the keep list here */
 if (args[ARGS_KEEPCHANNELS].is_set) {
  /* Note that this will be NULL if no channel was selected... */
  local_arg->keep_list=expand_channel_list(tinfo, args[ARGS_KEEPCHANNELS].arg.s);
 } else if (args[ARGS_BYNAME].is_set) {
  int *remove_list=expand_channel_list(tinfo, args[ARGS_BYNAME].arg.s);

  if (remove_list==NULL) {
   TRACEMS(tinfo->emethods, 1, "remove_channel: Not removing any channel: Bypassing...\n");
   return tinfo->tsdata;
  }
  new_nr_of_channels=0;
  for (channel=1; channel<=tinfo->nr_of_channels; channel++) {
   if (!is_in_channellist(channel, remove_list)) new_nr_of_channels++;
  }
  if ((local_arg->keep_list=(int *)malloc((new_nr_of_channels+1)*sizeof(int)))==NULL) {
   ERREXIT(tinfo->emethods, "remove_channel: Error allocating keep_list memory\n");
  }

  in_keep_list=local_arg->keep_list;
  for (channel=1; channel<=tinfo->nr_of_channels; channel++) {
   if (!is_in_channellist(channel, remove_list)) {
    *in_keep_list++ = channel;
   }
  }
  *in_keep_list=0;
  free(remove_list);
 } else {
  new_nr_of_channels=0;
  /*{{{  Build the keep_list by checking all channels using is_in_one_of_the_ranges*/
  for (channel=1; channel<=tinfo->nr_of_channels; channel++) {
   if (!is_in_one_of_the_ranges(channel, local_arg->remove_ranges)) {
    new_nr_of_channels++;
   }
  }
  if (new_nr_of_channels==tinfo->nr_of_channels) {
   TRACEMS(tinfo->emethods, 1, "remove_channel: Not removing any channel: Bypassing...\n");
   return tinfo->tsdata;
  }

  if ((local_arg->keep_list=(int *)malloc((new_nr_of_channels+1)*sizeof(int)))==NULL) {
   ERREXIT(tinfo->emethods, "remove_channel: Error allocating keep_list memory\n");
  }

  in_keep_list=local_arg->keep_list;
  for (channel=1; channel<=tinfo->nr_of_channels; channel++) {
   if (!is_in_one_of_the_ranges(channel, local_arg->remove_ranges)) {
    *in_keep_list++ = channel;
   }
  }
  *in_keep_list=0;
  /*}}}  */
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
 /*{{{  Count the remaining channels and allocate data*/
 for (stringlen=0, in_keep_list=local_arg->keep_list; in_keep_list!=NULL && *in_keep_list!=0; in_keep_list++) {
  stringlen+=strlen(tinfo->channelnames[*in_keep_list-1])+1;
 }
 /* This is only needed for KEEPCHANNELS mode, but it's correct in any case: */
 new_nr_of_channels=in_keep_list-local_arg->keep_list;
 if (new_nr_of_channels==0) {
  TRACEMS(tinfo->emethods, 0, "remove_channel: Rejecting epoch because no channel would be kept.\n");
  return NULL;
 }
 /* Nonmultiplexed: Make newarray large enough to hold all shifts */
 newarray.nr_of_vectors=new_nr_of_channels*tsdata_steps;
 newarray.nr_of_elements=tinfo->nr_of_points;
 newarray.element_skip=tinfo->itemsize;
 if (array_allocate(&newarray)==NULL || 
     (new_channelnames=(char **)malloc(new_nr_of_channels*sizeof(char *)))==NULL ||
     (in_channelnames=(char *)malloc(stringlen))==NULL ||
     (new_probepos=(double *)malloc(new_nr_of_channels*3*sizeof(double)))==NULL) {
  ERREXIT(tinfo->emethods, "remove_channel: Error allocating memory.\n");
 }
 /*}}}  */
 
 /*{{{  Transfer channel names and probe positions*/
 for (channel=0, in_keep_list=local_arg->keep_list; *in_keep_list!=0; in_keep_list++, channel++) {
  int const old_channel= *in_keep_list-1;
  int i;
  strcpy(in_channelnames, tinfo->channelnames[old_channel]);
  new_channelnames[channel]=in_channelnames;
  in_channelnames+=strlen(in_channelnames)+1;
  for (i=0; i<3; i++) {
   new_probepos[3*channel+i]=tinfo->probepos[3*old_channel+i];
  }
 }
 /*}}}  */

 /*{{{  Transfer the data*/
 for (tsdata_step=0; tsdata_step<tsdata_steps; tinfo->tsdata+=tsdata_stepwidth, tsdata_step++) {
 tinfo_array(tinfo, &myarray);
 for (item=0; item<tinfo->itemsize; item++) {
  array_use_item(&myarray, item);
  array_use_item(&newarray, item);

  for (in_keep_list=local_arg->keep_list; *in_keep_list!=0; in_keep_list++) {
   myarray.current_vector= *in_keep_list-1;
   do {
    array_write(&newarray, array_scan(&myarray));
   } while (myarray.message==ARRAY_CONTINUE);
  }
 }
 }
 tinfo->tsdata=orig_tsdata;
 /*}}}  */

 /*{{{  Free old data and write the new to *tinfo*/
 free_pointer((void **)&tinfo->probepos);
 free_pointer((void **)&tinfo->channelnames[0]);
 free_pointer((void **)&tinfo->channelnames);
 tinfo->probepos=new_probepos;
 tinfo->channelnames=new_channelnames;
 tinfo->nr_of_channels=new_nr_of_channels;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tsdata_steps*tinfo->itemsize;
 tinfo->multiplexed=FALSE;
 /*}}}  */

 if (args[ARGS_KEEPCHANNELS].is_set || args[ARGS_BYNAME].is_set) {
  free_pointer((void **)&local_arg->keep_list);
 }

 return newarray.start;
}
/*}}}  */

/*{{{  remove_channel_exit(transform_info_ptr tinfo) {*/
METHODDEF void
remove_channel_exit(transform_info_ptr tinfo) {
 struct remove_channel_storage *local_arg=(struct remove_channel_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->keep_list);
 free_pointer((void **)&local_arg->remove_ranges);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_remove_channel(transform_info_ptr tinfo) {*/
GLOBAL void
select_remove_channel(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &remove_channel_init;
 tinfo->methods->transform= &remove_channel;
 tinfo->methods->transform_exit= &remove_channel_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="remove_channel";
 tinfo->methods->method_description=
  "Transform method to remove a single channel or a range of\n"
  " channels from the input data. Channel numbers start with 1.\n";
 tinfo->methods->local_storage_size=sizeof(struct remove_channel_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
