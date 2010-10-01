/*
 * Copyright (C) 1996,1998,1999,2001 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * baseline_subtract is a simple transform method to subtract the baseline mean
 * from the data. It works for both TIME_DATA and FREQ_DATA.
 * 						-- Bernd Feige 22.10.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  baseline_subtract_init(transform_info_ptr tinfo) {*/
METHODDEF void
baseline_subtract_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  baseline_subtract(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
baseline_subtract(transform_info_ptr tinfo) {
 int point, channel, freq, itempart;
 int points=tinfo->nr_of_points, itemsize=tinfo->itemsize;
 int freqs=1, basepoints, channelskip, pointskip, datastart;
 int itemstep=tinfo->leaveright+1;
 DATATYPE mean;

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
  ERREXIT1(tinfo->emethods, "baseline_subtract: Have only %d basepoints!\n", MSGPARM(basepoints));
 }
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  for (freq=0; freq<freqs; freq++) {
   for (itempart=0; itempart<itemsize; itempart+=itemstep) {
    datastart=channel*channelskip+freq*itemsize+itempart;
    /*{{{  Calculate baseline mean*/
    mean=0;
    for (point=0; point<basepoints; point++) {
     mean+=tinfo->tsdata[datastart+point*pointskip];
    }
    mean/=basepoints;
    /*}}}  */
    /*{{{  Subtract the baseline from the data points*/
    for (point=0; point<points; point++) {
     tinfo->tsdata[datastart+point*pointskip]-=mean;
    }
    /*}}}  */
   }
  }
 }
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  baseline_subtract_exit(transform_info_ptr tinfo) {*/
METHODDEF void
baseline_subtract_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_baseline_subtract(transform_info_ptr tinfo) {*/
GLOBAL void
select_baseline_subtract(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &baseline_subtract_init;
 tinfo->methods->transform= &baseline_subtract;
 tinfo->methods->transform_exit= &baseline_subtract_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="baseline_subtract";
 tinfo->methods->method_description=
  "Transform method to subtract the baseline mean from the data.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
