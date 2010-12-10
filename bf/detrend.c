/*
 * Copyright (C) 1996-1999,2001-2003,2010 Bernd Feige
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
 * detrend.c method for detrending the data set tinfo.
 * Multiple data sets connected by tinfo->next are supported as well as
 * frequency data sets consisting of a number of frequency shifts.
 * (The latter are detrended in the TIME domain for each frequency)
 * 						-- Bernd Feige 30.07.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_ORDER0=0,
 ARGS_INTERPOLATE,
 ARGS_USE_XVALUES,
 ARGS_LATENCY, 
 ARGS_BYNAME,
 ARGS_ITEMPART,
 ARGS_OMITRANGES,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "0-Order polynomial. Demean instead of detrend", "0", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Interpolate: Subtract the interpolation line between the first and the last point", "I", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Use x start and end values for omit ranges", "x", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "latency: Subtracts the constant value found at that latency from all points", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_SENTENCE, "Arguments: omit_start1 omit_end1 omit_start2 ...", " ", ARGDESC_UNUSED, NULL}
};

/*{{{  struct detrend_storage {*/
struct detrend_storage {
 int *channel_list;
 int fromitem;
 int toitem;
 int nr_of_omitargs;
 long last_points;
 long *detrend_omitranges;
 long detrend_where;
 long N;
 /* These have integer values but are chosen to be double because they
  * may accumulate to large values */
 double sumx;
 double sumxx;
};
/*}}}  */

/*{{{  detrend_init(transform_info_ptr tinfo) {*/
METHODDEF void
detrend_init(transform_info_ptr tinfo) {
 struct detrend_storage *local_arg=(struct detrend_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 local_arg->detrend_omitranges=NULL;
 if (args[ARGS_LATENCY].is_set) {
  local_arg->detrend_where=gettimeslice(tinfo, args[ARGS_LATENCY].arg.s);
 } else {
  /*{{{  Actual detrending requested: Prepare omitted ranges*/
  growing_buf buf;
  Bool havearg=FALSE;

  growing_buf_init(&buf);
  if (args[ARGS_OMITRANGES].is_set && args[ARGS_OMITRANGES].arg.s!=NULL) {
   growing_buf_takethis(&buf, args[ARGS_OMITRANGES].arg.s);
   havearg=growing_buf_firsttoken(&buf);
   local_arg->nr_of_omitargs=buf.nr_of_tokens;
  } else {
   local_arg->nr_of_omitargs=0;
  }
  if (local_arg->nr_of_omitargs>0) {
   int current_omitarg=0;
   if ((local_arg->detrend_omitranges=(long *)malloc(local_arg->nr_of_omitargs*sizeof(long)))==NULL) {
    ERREXIT(tinfo->emethods, "detrend_init: Error allocating omitranges\n");
   }
   while (havearg) {
    if (args[ARGS_USE_XVALUES].is_set) {
     local_arg->detrend_omitranges[current_omitarg]=decode_xpoint(tinfo, buf.current_token);
    } else {
     local_arg->detrend_omitranges[current_omitarg]=gettimeslice(tinfo, buf.current_token)+tinfo->beforetrig;
    }
    if (local_arg->detrend_omitranges[current_omitarg]<0) {
     ERREXIT(tinfo->emethods, "detrend_init: Omitrange boundary<beforetrig.\n");
    }
    if (current_omitarg>0 && local_arg->detrend_omitranges[current_omitarg]<=local_arg->detrend_omitranges[current_omitarg-1]) {
     ERREXIT(tinfo->emethods, "detrend_init: Omitranges are not monotonically increasing!\n");
    }
    current_omitarg++;
    havearg=growing_buf_nexttoken(&buf);
   }
  }
  growing_buf_free(&buf);
  /*}}}  */
 }

 local_arg->channel_list=NULL;
 if (args[ARGS_BYNAME].is_set) {
  local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_BYNAME].arg.s);
 }

 local_arg->fromitem=0;
 local_arg->toitem=tinfo->itemsize-tinfo->leaveright-1;
 if (args[ARGS_ITEMPART].is_set) {
  local_arg->fromitem=local_arg->toitem=args[ARGS_ITEMPART].arg.i;
  if (local_arg->fromitem<0 || local_arg->fromitem>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "detrend_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 /* This indicates that x statistics must be recalculated: */
 local_arg->last_points= -1;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  detrend(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
detrend(transform_info_ptr tinfo) {
 struct detrend_storage *local_arg=(struct detrend_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int point, channel, itempart, channelstart, points;
 int ds, ds_skip=0, point_skip, channel_skip;	/* Multiple data sets in FREQ_DATA */
 DATATYPE linreg_const, linreg_fact;
 DATATYPE *data;

 if (tinfo->data_type==FREQ_DATA) {
  if (tinfo->nrofshifts>1) {
   /* Detrend across shifts for each frequency */
   points=tinfo->nrofshifts;
   ds=tinfo->nroffreq;	/* Number of data sets */
   ds_skip=tinfo->itemsize;	/* Skip from one freq to the next */
   channel_skip=ds*ds_skip;	/* Skip from one channel to the next */
   point_skip=tinfo->nr_of_channels*channel_skip;	/* From one point to the next */
  } else {
   /* Detrend across frequencies */
   points=tinfo->nroffreq;
   ds=tinfo->nrofshifts;	/* Number of data sets */
   point_skip=tinfo->itemsize;	/* From one point to the next */
   channel_skip=points*point_skip;	/* Skip from one freq to the next */
   ds_skip=tinfo->nr_of_channels*channel_skip;	/* Skip from one shift to the next */
  }
 } else {
  nonmultiplexed(tinfo);
  ds=1;
  points=tinfo->nr_of_points;
  point_skip=tinfo->itemsize;
  channel_skip=points*point_skip;
 }
 if (args[ARGS_LATENCY].is_set && local_arg->detrend_where>=points) {
  ERREXIT2(tinfo->emethods, "detrend: latency %d >= nr_of_points %d\n", MSGPARM(local_arg->detrend_where), MSGPARM(points));
 }

 /* Do this whenever the number of points seen changes (and at the start, where last_points is -1): */
 if (points!=local_arg->last_points) {
  /*{{{  Pre-calculate sum(x) and sum(x*x) for the non-excluded ranges*/
  /* Though this is not really needed in the ORDER0 case, we need local_arg->N */
  int current_omitarg=0;
  local_arg->N=0;
  local_arg->sumx=local_arg->sumxx=0.0;
  for (point=0; point<points; point++) {
   if (current_omitarg>=local_arg->nr_of_omitargs || current_omitarg%2==0) {
    /* Use section */
    if (current_omitarg<local_arg->nr_of_omitargs && point>=local_arg->detrend_omitranges[current_omitarg]) {
     /* This point already belongs to an omitted range */
     current_omitarg++;
    } else {
     local_arg->N++;
     local_arg->sumx+=point;
     local_arg->sumxx+=point*(double)point;
    }
   } else {
    /* Skip section */
    if (point>=local_arg->detrend_omitranges[current_omitarg]) {
     /* The next point should be used again */
     current_omitarg++;
    }
   }
  }
  local_arg->last_points=points;
  /*}}}  */
 }

  data=tinfo->tsdata;
  for (; ds>0; ds--) {
   /*{{{  Detrend one data set*/
   for (channel=channelstart=0; channel<tinfo->nr_of_channels; channel++, channelstart+=channel_skip) {
    if (local_arg->channel_list!=NULL && !is_in_channellist(channel+1, local_arg->channel_list)) {
     continue;
    }
    for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
     if (args[ARGS_LATENCY].is_set) {
      linreg_const=data[channelstart+local_arg->detrend_where*point_skip+itempart];
      linreg_fact=0.0;
     } else if (args[ARGS_ORDER0].is_set) {
      double sumd=0.0;
      int current_omitarg=0;
      for (point=0; point<points; point++) {
       if (current_omitarg>=local_arg->nr_of_omitargs || current_omitarg%2==0) {
   	/* Use section */
        if (current_omitarg<local_arg->nr_of_omitargs && point>=local_arg->detrend_omitranges[current_omitarg]) {
   	 current_omitarg++;
   	} else {
   	 sumd+=data[channelstart+point*point_skip+itempart];
   	}
       } else {
   	/* Skip section */
   	if (point>=local_arg->detrend_omitranges[current_omitarg]) {
   	 current_omitarg++;
   	}
       }
      }
      linreg_fact=0;
      linreg_const=sumd/local_arg->N;
     } else if (args[ARGS_INTERPOLATE].is_set) {
      linreg_const=data[channelstart+itempart];
      linreg_fact=(data[channelstart+(points-1)*point_skip+itempart]-data[channelstart+itempart])/(points-1);
     } else {
      double sumd=0.0, sumxd=0.0;
      int current_omitarg=0;
      for (point=0; point<points; point++) {
       if (current_omitarg>=local_arg->nr_of_omitargs || current_omitarg%2==0) {
   	/* Use section */
        if (current_omitarg<local_arg->nr_of_omitargs && point>=local_arg->detrend_omitranges[current_omitarg]) {
   	 current_omitarg++;
   	} else {
   	 DATATYPE d=data[channelstart+point*point_skip+itempart];
   	 sumd+=d;
   	 sumxd+=point*d;
   	}
       } else {
   	/* Skip section */
   	if (point>=local_arg->detrend_omitranges[current_omitarg]) {
   	 current_omitarg++;
   	}
       }
      }
      linreg_fact=(local_arg->N*sumxd-sumd*local_arg->sumx)/(local_arg->N*local_arg->sumxx-local_arg->sumx*local_arg->sumx);
      linreg_const=(sumd-linreg_fact*local_arg->sumx)/local_arg->N;
     }
     for (point=0; point<points; point++) {
      data[channelstart+point*point_skip+itempart]-=linreg_const+point*linreg_fact;
     }
    }
   }
   /*}}}  */
   data+=ds_skip;
  }
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  detrend_exit(transform_info_ptr tinfo) {*/
METHODDEF void
detrend_exit(transform_info_ptr tinfo) {
 struct detrend_storage *local_arg=(struct detrend_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->detrend_omitranges);
 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_detrend(transform_info_ptr tinfo) {*/
GLOBAL void
select_detrend(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &detrend_init;
 tinfo->methods->transform= &detrend;
 tinfo->methods->transform_exit= &detrend_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="detrend";
 tinfo->methods->method_description=
  "Method to de-trend the given data set; This is done by subtracting a linear\n"
  " regression line from each time course. It is possible to specify any number\n"
  " of ranges to omit from the fit by start and end times relative to the epoch\n"
  " trigger, eg: detrend 0ms 500ms\n";
 tinfo->methods->local_storage_size=sizeof(struct detrend_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
