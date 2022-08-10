/*
 * Copyright (C) 1996-2000,2003,2010,2013-2015,2019,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * histogram.c method to generate a histogram with defined resolution
 * for all selected channels.
 * The histogram boundaries are the same for all points and channels.
 * Different input points or frequencies are mapped onto different output
 * items, and the x axis will be the amplitude bin on output.
 *					-- Bernd Feige 24.10.1994
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
 ARGS_COLLAPSE_CHANNELS=0, 
 ARGS_COLLAPSE_POINTS, 
 ARGS_ASSIGN_OUTLIERS,
 ARGS_HIST_MIN,
 ARGS_HIST_MAX,
 ARGS_NBINS,
 ARGS_MINX,
 ARGS_MAXX,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Collapse over all channels", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Collapse over all points or frequencies (normally->different output items)", "p", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Outlier inclusion. Assign the two ends of the histogram to outliers", "o", FALSE, NULL},
 {T_ARGS_TAKES_DOUBLE, "hist_min", "", 0, NULL},
 {T_ARGS_TAKES_DOUBLE, "hist_max", "", 1, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_bins", "", 30, NULL},
 {T_ARGS_TAKES_DOUBLE, "minx", " ", 0, NULL},
 {T_ARGS_TAKES_DOUBLE, "maxx", " ", 1, NULL}
};

struct histogram_local_struct {
 struct transform_info_struct tinfo;
 struct hist_boundary boundaries;
 int from_x;
 int to_x;
};

LOCAL char * const collapsed_channelname="h_collapsed";

/*{{{  histogram_init(transform_info_ptr tinfo) {*/
METHODDEF void
histogram_init(transform_info_ptr tinfo) {
 struct histogram_local_struct *localp=(struct histogram_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 tinfo->nrofaverages=0;

 localp->tinfo.tsdata=NULL;

 localp->boundaries.histogram=NULL;
 localp->boundaries.hist_min=args[ARGS_HIST_MIN].arg.d;
 localp->boundaries.hist_max=args[ARGS_HIST_MAX].arg.d;
 localp->boundaries.nr_of_bins=args[ARGS_NBINS].arg.i;
 localp->boundaries.hist_resolution=(localp->boundaries.hist_max-localp->boundaries.hist_min)/localp->boundaries.nr_of_bins;
 localp->from_x=(int)rint(args[ARGS_MINX].is_set ? args[ARGS_MINX].arg.d : -1);
 localp->to_x=(int)rint(args[ARGS_MAXX].is_set ? args[ARGS_MAXX].arg.d : -1);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  histogram(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
histogram(transform_info_ptr tinfo) {
 struct histogram_local_struct *localp=(struct histogram_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 HIST_TYPE *histogram_data=localp->boundaries.histogram;
 DATATYPE hist_min=localp->boundaries.hist_min, hist_resolution=localp->boundaries.hist_resolution;
 int nr_of_bins=localp->boundaries.nr_of_bins;
 int from_x=localp->from_x, to_x=localp->to_x;
 int outliers=args[ARGS_ASSIGN_OUTLIERS].is_set;
 int channels=tinfo->nr_of_channels, itemsize=tinfo->itemsize;
 int channelskip, freqskip, channel, point, points, freq, nfreq;
 int out_binskip, out_freqskip, out_pointskip, out_channelskip, out_shifts;
 int start_freq, end_freq, start_point, end_point;
 int freq_offset, channel_offset, point_offset;
 array tsdata;

 if (itemsize>1) {
  ERREXIT(tinfo->emethods, "histogram: Can't handle multiple items in input data.\n");
 }

 /*{{{  Set up tsdata array: elements=points, vectors=items*/
 tsdata.vector_skip=1;
 tsdata.nr_of_vectors=itemsize;
 if (from_x<0) from_x=0;
 if (tinfo->data_type==FREQ_DATA) {
  nfreq=tinfo->nroffreq;
  freqskip=itemsize;
  channelskip=nfreq*itemsize;
  tsdata.element_skip=channels*channelskip;

  out_shifts=points=tinfo->nrofshifts;
  if (to_x<0 || to_x>nfreq) to_x=nfreq;
  start_freq=from_x; end_freq=to_x;
  start_point=0; end_point=points;
  if (args[ARGS_COLLAPSE_POINTS].is_set) {
   out_pointskip=0;
  } else {
   out_pointskip=(end_freq-start_freq)*nr_of_bins*(args[ARGS_COLLAPSE_CHANNELS].is_set ? 1 : channels);
  }
 } else {
  nfreq=1;
  freqskip=0;
  points=tinfo->nr_of_points;
  if (tinfo->multiplexed) {
   tsdata.element_skip=channels*itemsize;
   channelskip=itemsize;
  } else {
   tsdata.element_skip=itemsize;
   channelskip=tinfo->nr_of_points*itemsize;
  }

  if (to_x<0 || to_x>points) to_x=points;
  start_freq=0; end_freq=1;
  start_point=from_x; end_point=to_x;
  out_shifts=1;
  out_pointskip=(args[ARGS_COLLAPSE_POINTS].is_set ? 0: 1);
 }
 tsdata.nr_of_elements=points;
 array_setreadwrite(&tsdata);

 if (to_x<=from_x) {
  ERREXIT(tinfo->emethods, "histogram: minx>=maxx. Please respecify these parameters.\n");
 }
 out_binskip=(args[ARGS_COLLAPSE_POINTS].is_set ? 1 : to_x-from_x);
 out_channelskip=(args[ARGS_COLLAPSE_CHANNELS].is_set ? 0 : out_binskip*nr_of_bins);
 out_freqskip=1;
 /*}}}  */

 if (histogram_data==(HIST_TYPE *)NULL) {
  /*{{{  First incoming epoch: Initialize working memory*/
  memcpy(&localp->tinfo, tinfo, sizeof(struct transform_info_struct));
  /* Protect the memory spaces we may need: */
  tinfo->comment=NULL;
  tinfo->probepos=NULL;
  if (args[ARGS_COLLAPSE_CHANNELS].is_set) {
   localp->tinfo.length_of_output_region=out_binskip*nr_of_bins*out_shifts;
   localp->tinfo.nr_of_channels=1;
  } else {
   tinfo->channelnames=NULL;
   localp->tinfo.length_of_output_region=channels*out_channelskip*out_shifts;
  }
  if ((histogram_data=(HIST_TYPE *)calloc(localp->tinfo.length_of_output_region, sizeof(DATATYPE)))==NULL) {
   ERREXIT(tinfo->emethods, "histogram: Error allocating epoch memory\n");
  }
  localp->tinfo.tsdata=(DATATYPE *)(localp->boundaries.histogram=histogram_data);
  localp->tinfo.itemsize=out_binskip;
  localp->tinfo.leaveright=0;
  if (tinfo->data_type==FREQ_DATA) {
   localp->tinfo.nroffreq=nr_of_bins;
   /* nrofshifts remains unmodified */
  } else {
   localp->tinfo.nr_of_points=nr_of_bins;
  }
  localp->tinfo.multiplexed=FALSE;
  localp->to_x=to_x;
  localp->from_x=from_x;
  /*}}}  */
 }

 /*{{{  Register the input data*/
 freq_offset=0;
 for (freq=start_freq; freq<end_freq; freq++) {
  tsdata.start=tinfo->tsdata+freq*freqskip;
  channel_offset=freq_offset;
  for (channel=0; channel<channels; channel++) {
   array_setreadwrite(&tsdata);
   array_reset(&tsdata);
   tsdata.current_element=start_point;
   point_offset=channel_offset;
   for (point=start_point; point<end_point; point++) {
    int bin=(int)rint((array_scan(&tsdata)-hist_min)/hist_resolution);
    if (bin>=0 && bin<nr_of_bins) {
     histogram_data[point_offset+bin*out_binskip]++;
    } else if (outliers) {
     if (bin<0) histogram_data[point_offset]++;
     else histogram_data[point_offset+(nr_of_bins-1)*out_binskip]++;
    }
    point_offset+=out_pointskip;
   }
   tsdata.start+=channelskip;
   channel_offset+=out_channelskip;
  }
  freq_offset+=out_freqskip;
 }
 /*}}}  */

 free_tinfo(tinfo); /* Free everything from the old epoch */

 tinfo->nrofaverages++;
 return localp->tinfo.tsdata;
}
/*}}}  */

/*{{{  histogram_exit(transform_info_ptr tinfo) {*/
METHODDEF void
histogram_exit(transform_info_ptr tinfo) {
 struct histogram_local_struct *localp=(struct histogram_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE hist_min=localp->boundaries.hist_min, hist_resolution=localp->boundaries.hist_resolution;
 int nr_of_bins=localp->boundaries.nr_of_bins, i;

 /* This happens if an error occurred before histogram() could save tinfo: */
 if (localp->tinfo.tsdata==NULL) return;

 /*{{{  Transfer data from localp->tinfo*/
 localp->tinfo.nrofaverages=tinfo->nrofaverages;
 memcpy(tinfo, &localp->tinfo, sizeof(struct transform_info_struct));
 /*}}}  */

 /*{{{  Build the correct x axis*/
 tinfo->xchannelname="Amplitude";
 if ((tinfo->xdata=(DATATYPE *)malloc(nr_of_bins*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "histogram_exit: Error allocating xdata memory\n");
 }
 for (i=0; i<nr_of_bins; i++) {
  tinfo->xdata[i]=hist_min+i*hist_resolution;
 }
 /*}}}  */
 if (args[ARGS_COLLAPSE_CHANNELS].is_set) {
  if (tinfo->channelnames!=NULL) {
   free_pointer((void **)&tinfo->channelnames[0]);
   free_pointer((void **)&tinfo->channelnames);
  }
  if ((tinfo->channelnames=(char **)malloc(sizeof(char *)))==NULL
    ||(tinfo->channelnames[0]=(char *)malloc(strlen(collapsed_channelname)+1))==NULL) {
   ERREXIT(tinfo->emethods, "histogram_exit: Error allocating channelname memory\n");
  }
  strcpy(tinfo->channelnames[0], collapsed_channelname);
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_histogram(transform_info_ptr tinfo) {*/
GLOBAL void
select_histogram(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &histogram_init;
 tinfo->methods->transform= &histogram;
 tinfo->methods->transform_exit= &histogram_exit;
 tinfo->methods->method_type=COLLECT_METHOD;
 tinfo->methods->method_name="histogram";
 tinfo->methods->method_description=
  "Collect method to output a histogram of the received values\n";
 tinfo->methods->local_storage_size=sizeof(struct histogram_local_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
