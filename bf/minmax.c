/*
 * Copyright (C) 1996-2000 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * minmax.c collect method to output the minimum and maximum values of
 * the input. The output will have twice the number of items of the
 * input, min and max for each input item.
 *					-- Bernd Feige 17.06.1995
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_COLLAPSECHANNELS=0, 
 ARGS_COLLAPSEPOINTS, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Collapse over all channels", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Collapse over all points", "p", FALSE, NULL}
};

struct minmax_local_struct {
 struct transform_info_struct tinfo;
 int nr_of_averages;
};

/*{{{  minmax_init(transform_info_ptr tinfo) {*/
METHODDEF void
minmax_init(transform_info_ptr tinfo) {
 struct minmax_local_struct *localp=(struct minmax_local_struct *)tinfo->methods->local_storage;
 localp->nr_of_averages=0;

 if (localp==(struct minmax_local_struct *)NULL) {
  ERREXIT(tinfo->emethods, "minmax_init: local_storage must contain the minmax arguments.\n");
 }
 localp->tinfo.tsdata=NULL;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  minmax(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
minmax(transform_info_ptr tinfo) {
 struct minmax_local_struct *localp=(struct minmax_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE *minmax_data=localp->tinfo.tsdata;
 array tsdata;
 int item, fe, first_epoch=FALSE;
 int element_skip, vector_skip;

 if (tinfo->data_type!=TIME_DATA) {
  ERREXIT(tinfo->emethods, "minmax: Only TIME_DATA is supported.\n");
 }
 if (localp->nr_of_averages==0) {
  /*{{{  First incoming epoch: Initialize working memory*/
  first_epoch=TRUE;

  /* Make our own local copy of tinfo and its arrays */
  deepcopy_tinfo(&localp->tinfo, tinfo);

  localp->tinfo.itemsize=2*tinfo->itemsize;
  if (args[ARGS_COLLAPSECHANNELS].is_set) localp->tinfo.nr_of_channels=1;
  if (args[ARGS_COLLAPSEPOINTS].is_set) localp->tinfo.nr_of_points=1;
  localp->tinfo.length_of_output_region=localp->tinfo.itemsize*localp->tinfo.nr_of_channels*localp->tinfo.nr_of_points;
  if ((minmax_data=(DATATYPE *)malloc(localp->tinfo.length_of_output_region*sizeof(DATATYPE)))==NULL) {
   ERREXIT(tinfo->emethods, "minmax: Error allocating epoch memory\n");
  }
  localp->tinfo.tsdata=minmax_data;
  localp->tinfo.leaveright=2*tinfo->leaveright;
  localp->tinfo.multiplexed=FALSE;
  /*}}}  */
 }
 vector_skip=(args[ARGS_COLLAPSECHANNELS].is_set ? 0 : localp->tinfo.nr_of_points*localp->tinfo.itemsize);
 element_skip=(args[ARGS_COLLAPSEPOINTS].is_set ? 0 : localp->tinfo.itemsize);

 /*{{{  minmax the data*/
 tinfo_array(tinfo, &tsdata);
 fe=first_epoch;
 do { /* Repeat this once for the first epoch, so that it will minmax correctly */
  for (item=0; item<tinfo->itemsize; item++) {
   array_use_item(&tsdata, item);
   do {
    int out_offset=tsdata.current_vector*vector_skip+tsdata.current_element*element_skip+item*2;
    DATATYPE val=array_scan(&tsdata);
    if (fe) {
     /* No matter which collapse mode is set, this will initialize minmax_data to
      * a value that really occurs */
     minmax_data[out_offset]=minmax_data[out_offset+1]=val;
    } else {
     if (val<minmax_data[out_offset]) minmax_data[out_offset]=val;
     if (val>minmax_data[out_offset+1]) minmax_data[out_offset+1]=val;
    }
   } while (tsdata.message!=ARRAY_ENDOFSCAN);
  }
 } while ((fe= !fe)==FALSE);
 /*}}}  */
 
 free_tinfo(tinfo); /* Free everything from the old epoch */

 localp->nr_of_averages++;
 tinfo->nrofaverages=localp->nr_of_averages;
 tinfo->leaveright=localp->tinfo.leaveright;
 return localp->tinfo.tsdata;
}
/*}}}  */

/*{{{  minmax_exit(transform_info_ptr tinfo) {*/
METHODDEF void
minmax_exit(transform_info_ptr tinfo) {
 struct minmax_local_struct *localp=(struct minmax_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int collapsed=args[ARGS_COLLAPSECHANNELS].is_set;

 /*{{{  Transfer data from localp->tinfo*/
 localp->tinfo.nrofaverages=tinfo->nrofaverages;
 memcpy(tinfo, &localp->tinfo, sizeof(struct transform_info_struct));
 /*}}}  */

 if (collapsed) {
  tinfo->channelnames[0]="m_collapsed";
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_minmax(transform_info_ptr tinfo) {*/
GLOBAL void
select_minmax(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &minmax_init;
 tinfo->methods->transform= &minmax;
 tinfo->methods->transform_exit= &minmax_exit;
 tinfo->methods->method_type=COLLECT_METHOD;
 tinfo->methods->method_name="minmax";
 tinfo->methods->method_description=
  "Collect method to output the extrema of the received values.\n"
  " For each input item, two items (min and max) are output as the result.\n";
 tinfo->methods->local_storage_size=sizeof(struct minmax_local_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
