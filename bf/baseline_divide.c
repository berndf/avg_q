/*
 * Copyright (C) 1996,1998,1999,2001,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * baseline_divide is a simple transform method to divide the data by
 * the baseline mean. It works for both TIME_DATA and FREQ_DATA; for
 * tuple data (itemsize>1), it divides the vectors by the mean baseline
 * AMPLITUDE.
 * 						-- Bernd Feige 24.08.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  baseline_divide_init(transform_info_ptr tinfo) {*/
METHODDEF void
baseline_divide_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  baseline_divide(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
baseline_divide(transform_info_ptr tinfo) {
 int point, channel, freq, itempart;
 int points=tinfo->nr_of_points, itemsize=tinfo->itemsize;
 int freqs=1, basepoints, channelskip, pointskip, datastart;
 int itemstep=tinfo->leaveright+1, items=itemsize/itemstep;
 DATATYPE mean, power;

 basepoints=tinfo->beforetrig;
 if (tinfo->data_type==FREQ_DATA) {
  freqs=tinfo->nroffreq;
  points=tinfo->nrofshifts;
  basepoints=1+(basepoints-tinfo->windowsize)/tinfo->shiftwidth;
  pointskip=freqs*tinfo->nr_of_channels*itemsize;
  channelskip=freqs*itemsize;
 } else {
  nonmultiplexed(tinfo);
  pointskip=itemsize;
  channelskip=points*itemsize;
 }
 if (basepoints<=0) {
  ERREXIT1(tinfo->emethods, "baseline_divide: Have only %d basepoints!\n", MSGPARM(basepoints));
 }
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  for (freq=0; freq<freqs; freq++) {
   datastart=channel*channelskip+freq*itemsize;
   /*{{{  Calculate baseline mean*/
   mean=0;
   for (point=0; point<basepoints; point++) {
    if (items>1) {
     power=0;
     for (itempart=0; itempart<itemsize; itempart+=itemstep) {
      power+=tinfo->tsdata[datastart+point*pointskip+itempart]*tinfo->tsdata[datastart+point*pointskip+itempart];
     }
     mean+=sqrt((double)power);
    } else {
     mean+=tinfo->tsdata[datastart+point*pointskip];
    }
   }
   mean/=basepoints;
   /*}}}  */
   /*{{{  Divide data points by baseline*/
   for (point=0; point<points; point++) {
    for (itempart=0; itempart<itemsize; itempart+=itemstep) {
     tinfo->tsdata[datastart+point*pointskip+itempart]/=mean;
    }
   }
   /*}}}  */
  }
 }
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  baseline_divide_exit(transform_info_ptr tinfo) {*/
METHODDEF void
baseline_divide_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_baseline_divide(transform_info_ptr tinfo) {*/
GLOBAL void
select_baseline_divide(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &baseline_divide_init;
 tinfo->methods->transform= &baseline_divide;
 tinfo->methods->transform_exit= &baseline_divide_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="baseline_divide";
 tinfo->methods->method_description=
  "Transform method to divide the data by the baseline mean.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
