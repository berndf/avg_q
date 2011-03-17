/*
 * Copyright (C) 1996-1999,2001-2005,2009 Bernd Feige
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
 * scale_by.c method to scale the incoming epochs
 * - either by a constant factor
 * - or so that each output map represents a unity vector
 *	-- Bernd Feige 25.09.1994
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

/*{{{  Local definitions*/
enum scale_by_type {
 SCALE_BY_INVNORM=0,
 SCALE_BY_INVPOINTNORM, 
 SCALE_BY_INVSQUARENORM,
 SCALE_BY_INVPOINTSQUARENORM, 
 SCALE_BY_INVSUM, 
 SCALE_BY_INVPOINTSUM, 
 SCALE_BY_INVMAX, 
 SCALE_BY_INVPOINTMAX, 
 SCALE_BY_INVMAXABS, 
 SCALE_BY_INVPOINTMAXABS, 
 SCALE_BY_INVQUANTILE, 
 SCALE_BY_INVPOINTQUANTILE, 
 SCALE_BY_XDATA, 
 SCALE_BY_INVXDATA, 

 SCALE_BY_PI, 
 SCALE_BY_INVPI, 
 SCALE_BY_SFREQ, 
 SCALE_BY_INVSFREQ, 
 SCALE_BY_NR_OF_POINTS, 
 SCALE_BY_INVNR_OF_POINTS, 
 SCALE_BY_NR_OF_CHANNELS, 
 SCALE_BY_INVNR_OF_CHANNELS, 
 SCALE_BY_NROFAVERAGES, 
 SCALE_BY_INVNROFAVERAGES, 
 SCALE_BY_SQRTNROFAVERAGES, 
 SCALE_BY_INVSQRTNROFAVERAGES, 

 SCALE_BY_NORMALIZE,

 SCALE_BY_FACTOR
};
LOCAL char const * const scale_by_typenames[]={
 "invnorm",
 "invpointnorm",
 "invsquarenorm",
 "invpointsquarenorm",
 "invsum",
 "invpointsum",
 "invmax",
 "invpointmax",
 "invmaxabs",
 "invpointmaxabs",
 "invquantile",
 "invpointquantile",
 "xdata",
 "invxdata",

 "pi",
 "invpi",
 "sfreq",
 "invsfreq",
 "nr_of_points",
 "invnr_of_points",
 "nr_of_channels",
 "invnr_of_channels",
 "nrofaverages",
 "invnrofaverages",
 "sqrtnrofaverages",
 "invsqrtnrofaverages",

 /* This one is deprecated, only for detecting old use: */
 "1.0",
 NULL
};
/*}}}  */

enum ARGS_ENUM {
 ARGS_BYNAME=0,
 ARGS_ITEMPART,
 ARGS_TYPE,
 ARGS_FACTOR, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_SELECTION, "Special factors", " ", 0, scale_by_typenames},
 {T_ARGS_TAKES_DOUBLE, "factor", " ", 1, NULL}
};
/*}}}  */

struct scale_by_storage {
 int *channel_list;
 enum scale_by_type type;
 double factor;
 int fromitem;
 int toitem;
};

/*{{{  scale_by_init(transform_info_ptr tinfo)*/
METHODDEF void
scale_by_init(transform_info_ptr tinfo) {
 struct scale_by_storage *local_arg=(struct scale_by_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_TYPE].is_set) {
  local_arg->type=(enum scale_by_type)args[ARGS_TYPE].arg.i;
  if (local_arg->type==SCALE_BY_NORMALIZE) {
   ERREXIT(tinfo->emethods, "scale_by_init: Using '1.0' is deprecated. Please use 'invnorm' instead!\n");
  }
 } else {
  local_arg->type=SCALE_BY_FACTOR;
 }
 if (local_arg->type==SCALE_BY_FACTOR
  || local_arg->type==SCALE_BY_INVQUANTILE || local_arg->type==SCALE_BY_INVPOINTQUANTILE
  ) {
  if (!args[ARGS_FACTOR].is_set) {
   ERREXIT(tinfo->emethods, "scale_by_init: A factor must be given!\n");
  }
  local_arg->factor=args[ARGS_FACTOR].arg.d;
  if ((local_arg->type==SCALE_BY_INVQUANTILE || local_arg->type==SCALE_BY_INVPOINTQUANTILE)
   && (local_arg->factor<0 || local_arg->factor>1)) {
    ERREXIT(tinfo->emethods, "scale_by_init: factor must be between 0 and 1 for quantile!\n");
  }
 }

 local_arg->channel_list=NULL;
 if (args[ARGS_BYNAME].is_set) {
  local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_BYNAME].arg.s);
  if (local_arg->type==SCALE_BY_INVQUANTILE) {
   ERREXIT(tinfo->emethods, "scale_by_init: Channel name list does not work with invquantile!\n");
  }
 }

 local_arg->fromitem=0;
 local_arg->toitem=tinfo->itemsize-tinfo->leaveright-1;
 if (args[ARGS_ITEMPART].is_set) {
  local_arg->fromitem=local_arg->toitem=args[ARGS_ITEMPART].arg.i;
  if (local_arg->fromitem<0 || local_arg->fromitem>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "scale_by_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  scale_by(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
scale_by(transform_info_ptr tinfo) {
 struct scale_by_storage *local_arg=(struct scale_by_storage *)tinfo->methods->local_storage;
 DATATYPE factor;
 int itempart;
 array indata;

 tinfo_array(tinfo, &indata);
 switch (local_arg->type) {
  /* Operations which are done on maps */
  case SCALE_BY_XDATA:
  case SCALE_BY_INVXDATA:
   if (tinfo->xdata==NULL) create_xaxis(tinfo);
  case SCALE_BY_NORMALIZE:
  case SCALE_BY_INVNORM:
  case SCALE_BY_INVSQUARENORM:
  case SCALE_BY_INVSUM:
  case SCALE_BY_INVMAX:
  case SCALE_BY_INVMAXABS:
  case SCALE_BY_INVQUANTILE:
   array_transpose(&indata);	/* Vectors are maps */
   if (local_arg->channel_list!=NULL) {
    ERREXIT(tinfo->emethods, "scale_by: Channel subsets are not supported for map operations.\n");
   }
   break;

  /* Operations which are done on channels */
  case SCALE_BY_INVPOINTNORM:
  case SCALE_BY_INVPOINTSQUARENORM:
  case SCALE_BY_INVPOINTSUM:
  case SCALE_BY_INVPOINTMAX:
  case SCALE_BY_INVPOINTMAXABS:
  case SCALE_BY_INVPOINTQUANTILE:
  case SCALE_BY_FACTOR:
   break;

  /* Operations which involve a special but constant factor */
  case SCALE_BY_PI:
   local_arg->factor= M_PI;
   break;
  case SCALE_BY_INVPI:
   local_arg->factor= 1.0/M_PI;
   break;
  case SCALE_BY_SFREQ:
   local_arg->factor= tinfo->sfreq;
   break;
  case SCALE_BY_INVSFREQ:
   local_arg->factor= 1.0/tinfo->sfreq;
   break;
  case SCALE_BY_NR_OF_POINTS:
   local_arg->factor= (tinfo->data_type==FREQ_DATA ? tinfo->nroffreq : tinfo->nr_of_points);
   break;
  case SCALE_BY_INVNR_OF_POINTS:
   local_arg->factor= 1.0/(tinfo->data_type==FREQ_DATA ? tinfo->nroffreq : tinfo->nr_of_points);
   break;
  case SCALE_BY_NR_OF_CHANNELS:
   local_arg->factor= tinfo->nr_of_channels;
   break;
  case SCALE_BY_INVNR_OF_CHANNELS:
   local_arg->factor= 1.0/tinfo->nr_of_channels;
   break;
  case SCALE_BY_NROFAVERAGES:
   local_arg->factor= tinfo->nrofaverages;
   break;
  case SCALE_BY_INVNROFAVERAGES:
   local_arg->factor= 1.0/tinfo->nrofaverages;
   break;
  case SCALE_BY_SQRTNROFAVERAGES:
   local_arg->factor= sqrt(tinfo->nrofaverages);
   break;
  case SCALE_BY_INVSQRTNROFAVERAGES:
   local_arg->factor= 1.0/sqrt(tinfo->nrofaverages);
   break;
 }

 for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
  array_use_item(&indata, itempart);
  do {
   if (local_arg->channel_list!=NULL && !is_in_channellist(indata.current_vector+1, local_arg->channel_list)) {
    array_nextvector(&indata);
    continue;
   }
   switch (local_arg->type) {
    case SCALE_BY_NORMALIZE:
    case SCALE_BY_INVNORM:
    case SCALE_BY_INVPOINTNORM:
     factor=array_abs(&indata);
     if (factor==0.0) factor=1.0;
     factor=1.0/factor;
     array_previousvector(&indata);
     break;
    case SCALE_BY_INVSQUARENORM:
    case SCALE_BY_INVPOINTSQUARENORM:
     factor=array_square(&indata);
     if (factor==0.0) factor=1.0;
     factor=1.0/factor;
     array_previousvector(&indata);
     break;
    case SCALE_BY_INVSUM:
    case SCALE_BY_INVPOINTSUM:
     factor=array_sum(&indata);
     if (factor==0.0) factor=1.0;
     factor=1.0/factor;
     array_previousvector(&indata);
     break;
    case SCALE_BY_INVMAX:
    case SCALE_BY_INVPOINTMAX:
     factor=array_max(&indata);
     if (factor==0.0) factor=1.0;
     factor=1.0/factor;
     array_previousvector(&indata);
     break;
    case SCALE_BY_INVMAXABS:
    case SCALE_BY_INVPOINTMAXABS: {
     DATATYPE amax=fabs(array_scan(&indata)), hold;
     while (indata.message==ARRAY_CONTINUE) {
      hold=fabs(array_scan(&indata));
      if (hold>amax) amax=hold;
     }
     factor=amax;
     }
     if (factor==0.0) factor=1.0;
     factor=1.0/factor;
     array_previousvector(&indata);
     break;
    case SCALE_BY_INVQUANTILE:
    case SCALE_BY_INVPOINTQUANTILE:
     factor=array_quantile(&indata,local_arg->factor);
     if (factor==0.0) factor=1.0;
     factor=1.0/factor;
     break;
    case SCALE_BY_XDATA:
     factor=tinfo->xdata[indata.current_vector];
     break;
    case SCALE_BY_INVXDATA:
     factor=1.0/tinfo->xdata[indata.current_vector];
     break;
    case SCALE_BY_PI:
    case SCALE_BY_INVPI:
    case SCALE_BY_SFREQ:
    case SCALE_BY_INVSFREQ:
    case SCALE_BY_NR_OF_POINTS:
    case SCALE_BY_INVNR_OF_POINTS:
    case SCALE_BY_NR_OF_CHANNELS:
    case SCALE_BY_INVNR_OF_CHANNELS:
    case SCALE_BY_NROFAVERAGES:
    case SCALE_BY_INVNROFAVERAGES:
    case SCALE_BY_SQRTNROFAVERAGES:
    case SCALE_BY_INVSQRTNROFAVERAGES:
    case SCALE_BY_FACTOR:
     factor=local_arg->factor;
     break;
    default:
     continue;
   }
   array_scale(&indata, factor);
  } while (indata.message!=ARRAY_ENDOFSCAN);
 }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  scale_by_exit(transform_info_ptr tinfo)*/
METHODDEF void
scale_by_exit(transform_info_ptr tinfo) {
 struct scale_by_storage *local_arg=(struct scale_by_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_scale_by(transform_info_ptr tinfo)*/
GLOBAL void
select_scale_by(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &scale_by_init;
 tinfo->methods->transform= &scale_by;
 tinfo->methods->transform_exit= &scale_by_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="scale_by";
 tinfo->methods->method_description=
  "Method to scale (multiply) the incoming epochs either by\n"
  " - the inverse of each map's vector length -> normalize (factor==invnorm; '1.0' is deprecated!)\n"
  " - ~ of the time course's vector length (factor==invpointnorm)\n"
  " - ~ of each map's square vector length (factor==invsquarenorm)\n"
  " - ~ of the time course's square vector length (factor==invpointsquarenorm)\n"
  " - ~ of the sum of all values in each map (factor==invsum)\n"
  " - ~ of the sum of all points within each channel (factor==invpointsum)\n"
  " - ~ of the maximum of all values in each map (factor==invmax)\n"
  " - ~ of the maximum of all points within each channel (factor==invpointmax)\n"
  " - ~ of the absolute maximum of all values in each map (factor==invmaxabs)\n"
  " - ~ of the absolute maximum of all points within each channel (factor==invpointmaxabs)\n"
  " - ~ of the given quantile of all values in each map (eg median: factor==invquantile 0.5)\n"
  " - ~ of the given quantile of all points within each channel (eg median: factor==invpointquantile 0.5)\n"
  " - the x value for each point (factor==xdata)\n"
  " - the inverse x value for each point (factor==invxdata)\n"
  " - The following dataset-global values or their inverse:\n"
  "pi, sfreq, nr_of_points, nr_of_channels, nrofaverages, sqrtnrofaverages\n"
  " - a constant factor\n";
 tinfo->methods->local_storage_size=sizeof(struct scale_by_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
