/*
 * Copyright (C) 1996-1999,2001,2003,2010,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * collapse_channels collapses all input channels to one output "channel"
 * 						-- Bernd Feige 24.11.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum collapse_choices {
 COLLAPSE_BY_AVERAGING,
 COLLAPSE_BY_SUMMATION,
 COLLAPSE_BY_HIGHEST,
 COLLAPSE_BY_LOWEST,
};
LOCAL const char * const collapse_choice[]={
 "-a", "-s", "-h", "-l", NULL
};
enum ARGS_ENUM {
 ARGS_COLLAPSE=0, 
 ARGS_RANGES, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SELECTION, "Collapse channels by averaging, summation, highest or lowest value", " ", 0, collapse_choice},
 {T_ARGS_TAKES_SENTENCE, "[channelnames1:name1 [channelnames2:name2 ...]]", " ", ARGDESC_UNUSED, NULL}
};

static char const * const collapsed_channelname="collapsed";

/*{{{  Definition of collapse_channels_storage*/
struct collapse_channels_storage {
 enum collapse_choices collapse_choice;
 char **name_lists;
 char **channelnames;
 int **ranges;
 int nr_of_ranges;
 int channelnames_len;
};
/*}}}  */

/*{{{  collapse_channels_init(transform_info_ptr tinfo) {*/
METHODDEF void
collapse_channels_init(transform_info_ptr tinfo) {
 struct collapse_channels_storage *local_arg=(struct collapse_channels_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 local_arg->collapse_choice=(args[ARGS_COLLAPSE].is_set ? args[ARGS_COLLAPSE].arg.i : COLLAPSE_BY_AVERAGING);

 if (args[ARGS_RANGES].is_set) {
  /*{{{  Count and check arguments (=ranges)*/
  growing_buf buf, tokenbuf;
  int range=0, range_len=0;
  char *in_rangenames, *in_channelnames;
  growing_buf_init(&buf);
  growing_buf_takethis(&buf, args[ARGS_RANGES].arg.s);
  buf.delim_protector='\\';
  growing_buf_init(&tokenbuf);
  growing_buf_allocate(&tokenbuf, 0);

  local_arg->nr_of_ranges=growing_buf_count_tokens(&buf);
  local_arg->channelnames_len=0;
  growing_buf_get_firsttoken(&buf,&tokenbuf);
  while (tokenbuf.current_length>0) {
   char *colon=strchr(tokenbuf.buffer_start, ':');
   if (colon==NULL) {
    ERREXIT(tinfo->emethods, "collapse_channels_init: Range syntax is channels:name\n");
   }
   range_len+=colon-tokenbuf.buffer_start+1;
   local_arg->channelnames_len+=strlen(colon+1)+1;
   growing_buf_get_nexttoken(&buf,&tokenbuf);
  }

  if ((local_arg->ranges=(int **)calloc(local_arg->nr_of_ranges, sizeof(int *)))==NULL 
    ||(in_rangenames=(char *)malloc(range_len))==NULL
    ||(in_channelnames=(char *)malloc(local_arg->channelnames_len))==NULL
    ||(local_arg->name_lists=(char **)malloc(local_arg->nr_of_ranges*sizeof(char *)))==NULL
    ||(local_arg->channelnames=(char **)malloc(local_arg->nr_of_ranges*sizeof(char *)))==NULL) {
   ERREXIT(tinfo->emethods, "collapse_channels_init: Error allocating memory\n");
  }

  growing_buf_get_firsttoken(&buf,&tokenbuf);
  while (tokenbuf.current_length>0) {
   char const * const colon=strchr(tokenbuf.buffer_start, ':');
   strncpy(in_rangenames, tokenbuf.buffer_start, colon-tokenbuf.buffer_start);
   local_arg->name_lists[range]=in_rangenames;
   in_rangenames+=(colon-tokenbuf.buffer_start);
   *in_rangenames++ ='\0';
   strcpy(in_channelnames, colon+1);
   local_arg->channelnames[range]=in_channelnames;
   in_channelnames+=strlen(in_channelnames)+1;
   growing_buf_get_nexttoken(&buf,&tokenbuf);
   range++;
  }
  growing_buf_free(&buf);
  growing_buf_free(&tokenbuf);
  /*}}}  */
 } else {
  /* No arguments: Collapse over all channels */
  local_arg->nr_of_ranges=1;
  local_arg->ranges=NULL;
  local_arg->channelnames_len=strlen(collapsed_channelname)+1;
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  collapse_channels(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
collapse_channels(transform_info_ptr tinfo) {
 struct collapse_channels_storage *local_arg=(struct collapse_channels_storage *)tinfo->methods->local_storage;
 char **new_channelnames, *in_channelnames;
 double *new_probepos;
 int itempart, itemparts=tinfo->itemsize-tinfo->leaveright;
 array inarray, newarray;

 if (local_arg->ranges!=NULL) {
  int range;
  for (range=0; range<local_arg->nr_of_ranges; range++) {
   free_pointer((void **)&local_arg->ranges[range]);
   if ((local_arg->ranges[range]=expand_channel_list(tinfo, local_arg->name_lists[range]))==NULL) {
    ERREXIT1(tinfo->emethods, "collapse_channels: Error expanding channel list %s\n", MSGPARM(local_arg->name_lists[range]));
   }
  }
 }

 /*{{{  Prepare and allocate memory*/
 tinfo_array(tinfo, &inarray);
 array_transpose(&inarray);	/* elements=channels */
 newarray.nr_of_elements=local_arg->nr_of_ranges; /* New number of channels */
 newarray.nr_of_vectors=inarray.nr_of_vectors;
 newarray.element_skip=tinfo->itemsize;
 if (array_allocate(&newarray)==NULL ||
     (new_channelnames=(char **)malloc(newarray.nr_of_elements*sizeof(char *)))==NULL ||
     (in_channelnames=(char *)malloc(local_arg->channelnames_len))==NULL ||
     (new_probepos=(double *)malloc(newarray.nr_of_elements*3*sizeof(double)))==NULL) {
  ERREXIT(tinfo->emethods, "collapse_channels: Error allocating memory.\n");
 }
 /*}}}  */
  
 /*{{{  Transfer channel names and probe positions*/
 {int range;
 for (range=0; range<local_arg->nr_of_ranges; range++) {
  char const *channelname;
  int i, old_channel;

  if (local_arg->ranges==NULL) {
   channelname=collapsed_channelname;
   old_channel=0;
  } else {
   channelname=local_arg->channelnames[range];
   /* The position of the first channel in a range is used as the output position */
   old_channel=local_arg->ranges[range][0]-1;
  }
  strcpy(in_channelnames, channelname);
  new_channelnames[range]=in_channelnames;
  in_channelnames+=strlen(in_channelnames)+1;
  for (i=0; i<3; i++) {
   new_probepos[3*range+i]=tinfo->probepos[3*old_channel+i];
  }
 }
 }
 /*}}}  */

 /*{{{  Collect the data to the new array*/
 for (itempart=0; itempart<tinfo->itemsize; itempart++) {
  array_use_item(&inarray, itempart);
  array_use_item(&newarray, itempart);

  do {
   do {
    DATATYPE accu=0.0;
    int chans_in_range=0, *in_range, channel=0;
    switch (local_arg->collapse_choice) {
     case COLLAPSE_BY_HIGHEST:
      accu= -FLT_MAX;
      break;
     case COLLAPSE_BY_LOWEST:
      accu=  FLT_MAX;
      break;
     default:
      break;
    }

    if (local_arg->ranges!=NULL) in_range=local_arg->ranges[newarray.current_element];
    inarray.current_vector=newarray.current_vector;
    /*{{{  Sum over the channels in this range*/
    do {
     DATATYPE hold;
     if (local_arg->ranges==NULL) {
      /* Cycle through all channels if used without arguments */
      if (++channel>tinfo->nr_of_channels) channel=0;
     } else {
      /* Read the given channels */
      channel= *in_range++;
     }
     if (channel==0) break;
     inarray.current_element=channel-1;
     hold=READ_ELEMENT(&inarray);
     switch (local_arg->collapse_choice) {
      case COLLAPSE_BY_SUMMATION:
      case COLLAPSE_BY_AVERAGING:
       accu+=hold;
       break;
      case COLLAPSE_BY_HIGHEST:
       if (hold>accu) accu=hold;
       break;
      case COLLAPSE_BY_LOWEST:
       if (hold<accu) accu=hold;
       break;
      default:
       break;
     }
     chans_in_range++;
    } while (1);
    /*}}}  */
    if (local_arg->collapse_choice==COLLAPSE_BY_AVERAGING
     && itempart<itemparts) accu/=chans_in_range;
    array_write(&newarray, accu);
   } while (newarray.message==ARRAY_CONTINUE);
  } while (newarray.message==ARRAY_ENDOFVECTOR);
 }
 /*}}}  */

 /*{{{  Free old data and write the new to *tinfo*/
 free_pointer((void **)&tinfo->probepos);
 free_pointer((void **)&tinfo->channelnames[0]);
 free_pointer((void **)&tinfo->channelnames);
 tinfo->probepos=new_probepos;
 tinfo->channelnames=new_channelnames;
 tinfo->nr_of_channels=newarray.nr_of_elements;
 tinfo->length_of_output_region=newarray.nr_of_elements*newarray.element_skip*newarray.nr_of_vectors;
 tinfo->multiplexed=TRUE;
 /*}}}  */

 return newarray.start;
}
/*}}}  */

/*{{{  collapse_channels_exit(transform_info_ptr tinfo) {*/
METHODDEF void
collapse_channels_exit(transform_info_ptr tinfo) {
 struct collapse_channels_storage *local_arg=(struct collapse_channels_storage *)tinfo->methods->local_storage;

 if (local_arg->ranges!=NULL) {
  int i;
  for (i=0; i<local_arg->nr_of_ranges; i++) {
   free_pointer((void **)&local_arg->ranges[i]);
  }
  free_pointer((void **)&local_arg->ranges);
  free_pointer((void **)&local_arg->name_lists[0]);
  free_pointer((void **)&local_arg->name_lists);
  free_pointer((void **)&local_arg->channelnames[0]);
  free_pointer((void **)&local_arg->channelnames);
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_collapse_channels(transform_info_ptr tinfo) {*/
GLOBAL void
select_collapse_channels(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &collapse_channels_init;
 tinfo->methods->transform= &collapse_channels;
 tinfo->methods->transform_exit= &collapse_channels_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="collapse_channels";
 tinfo->methods->method_description=
  "Transform method to create a number of output channels by averaging across\n"
  " subsets of existing channels.\n";
 tinfo->methods->local_storage_size=sizeof(struct collapse_channels_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
