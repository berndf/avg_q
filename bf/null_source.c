/*
 * Copyright (C) 1999,2003,2004,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * null_source.c `dummy' method to just create zero'ed data.
 *	-- Bernd Feige 23.03.1999
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

#define MAX_COMMENTLEN 1024

enum ARGS_ENUM {
 ARGS_ITEMSIZE=0,
 ARGS_SFREQ, 
 ARGS_EPOCHS, 
 ARGS_NROFCHANNELS,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "Items: Specify the number of items (default 1)", "I", 1, NULL},
 {T_ARGS_TAKES_DOUBLE, "sampling_freq", "", 100, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_epochs", "", 1, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_channels", "", 30, NULL},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"}
};

/*{{{  Definition of null_source_storage*/
struct null_source_storage {
 int nr_of_channels;
 int itemsize;
 long beforetrig;
 long aftertrig;
 long epochs;
 float sfreq;
};
/*}}}  */

/*{{{  null_source_init(transform_info_ptr tinfo) {*/
METHODDEF void
null_source_init(transform_info_ptr tinfo) {
 struct null_source_storage *local_arg=(struct null_source_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /*{{{  Process options*/
 local_arg->itemsize=(args[ARGS_ITEMSIZE].is_set ? args[ARGS_ITEMSIZE].arg.i : 1);
 tinfo->sfreq=local_arg->sfreq=args[ARGS_SFREQ].arg.d;
 local_arg->epochs=args[ARGS_EPOCHS].arg.i;
 local_arg->nr_of_channels=args[ARGS_NROFCHANNELS].arg.i;
 if (local_arg->itemsize<=0 || 
     local_arg->sfreq<=0 || 
     local_arg->epochs<=0 || 
     local_arg->nr_of_channels<=0) {
  ERREXIT(tinfo->emethods, "null_source_init: Invalid parameters (all must be >0).\n");
 }
 /*}}}  */
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  null_source(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
null_source(transform_info_ptr tinfo) {
 struct null_source_storage *local_arg=(struct null_source_storage *)tinfo->methods->local_storage;
 array myarray;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->nr_of_channels;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "null_source: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=local_arg->itemsize;
 tinfo->multiplexed=FALSE;
 myarray.nr_of_elements=tinfo->nr_of_points;
 myarray.nr_of_vectors=tinfo->nr_of_channels;
 if (array_allocate(&myarray)==NULL || (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL) {
  ERREXIT(tinfo->emethods, "null_source: Error allocating data\n");
 }
 /*}}}  */

 snprintf(tinfo->comment, MAX_COMMENTLEN, "null_source");

 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, free_tinfo will free someone else's data ! */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */

 tinfo->z_label=NULL;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  null_source_exit(transform_info_ptr tinfo) {*/
METHODDEF void
null_source_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_null_source(transform_info_ptr tinfo) {*/
GLOBAL void
select_null_source(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &null_source_init;
 tinfo->methods->transform= &null_source;
 tinfo->methods->transform_exit= &null_source_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="null_source";
 tinfo->methods->method_description=
  "Null get-epoch method. This method generates all-zero data sets.\n";
 tinfo->methods->local_storage_size=sizeof(struct null_source_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
