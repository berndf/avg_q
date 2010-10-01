/*
 * Copyright (C) 1996-2000,2003,2004,2007,2009,2010 Bernd Feige
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
 * subtract.c method to subtract a given epoch from the transform epochs
 *	-- Bernd Feige 28.04.1993
 *
 * This method needs a side_tinfo to be set with a selected get_epoch
 * method; It will use this method to read an epoch when initialized and
 * then subtract this epoch from each epoch to be processed.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "stat.h"
#include "growing_buf.h"
/*}}}  */

enum subtract_types {
 SUBTRACT_TYPE_ADD=0,
 SUBTRACT_TYPE_MEAN,
 SUBTRACT_TYPE_WMEAN,
 SUBTRACT_TYPE_SUBTRACT,
 SUBTRACT_TYPE_MULTIPLY,
 SUBTRACT_TYPE_DIVIDE,
 SUBTRACT_TYPE_COMPLEXMULTIPLY,
 SUBTRACT_TYPE_COMPLEXDIVIDE,
};
LOCAL const char *const type_choice[]={
 "-a",
 "-mean",
 "-wmean",
 "-s",
 "-m",
 "-d",
 "-cm",
 "-cd",
 NULL
};
enum ARGS_ENUM {
 ARGS_CLOSE=0, 
 ARGS_EVERY, 
 ARGS_RECYCLE_CHANNELS, 
 ARGS_RECYCLE_POINTS, 
 ARGS_TTEST,
 ARGS_LEAVE_TPARAMETERS,
 ARGS_FROMEPOCH, 
 ARGS_TYPE,
 ARGS_SUBTRACTFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Close and reopen the file for each epoch", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Read every epoch in subtract_file in turn", "e", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Recycle channels of subtract_file if it has less channels than epoch", "C", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Recycle points of subtract_file if it has less points than epoch", "P", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "t-comparisons. Use the sum and sum of squares to calculate t and p", "t", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "output the accumulated parameters for the t test instead of t and p", "u", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Start with epoch number fromepoch (>=1)", "f", 1, NULL},
 {T_ARGS_TAKES_SELECTION, "Add, mean, weighted mean, subtract (default), multiply, divide, complex multiply or divide", " ", 3, type_choice},
 {T_ARGS_TAKES_FILENAME, "subtract_file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"}
};

/*{{{  #defines and Local functions*/
#define min(x, y) ((x)<(y) ? (x) : (y));
LOCAL double
square(double x) {
 return x*x;
}
/*}}}  */

struct subtract_storage {
 enum subtract_types type;
 struct transform_info_struct side_tinfo;
 struct transform_methods_struct side_method;
 growing_buf side_argbuf;
 Bool ttest;
};
/*}}}  */

/*{{{  subtract_init(transform_info_ptr tinfo)*/
METHODDEF void
subtract_init(transform_info_ptr tinfo) {
 struct subtract_storage *local_arg=(struct subtract_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;

 local_arg->ttest=args[ARGS_TTEST].is_set;
 if (args[ARGS_LEAVE_TPARAMETERS].is_set && !local_arg->ttest) {
  TRACEMS(tinfo->emethods, 0, "subtract_init: Option -u given without -t. Adding -t...\n");
  local_arg->ttest=TRUE;
 }
 if (args[ARGS_TYPE].is_set) {
  local_arg->type=(enum subtract_types)args[ARGS_TYPE].arg.i;
 } else {
  local_arg->type=SUBTRACT_TYPE_SUBTRACT;
 }
 if (local_arg->type!=SUBTRACT_TYPE_SUBTRACT && local_arg->ttest) {
  ERREXIT(tinfo->emethods, "subtract_init: T-test is only useful in subtract mode.\n");
 }
 side_tinfo->emethods=tinfo->emethods;
 side_tinfo->methods= &local_arg->side_method;
 select_readasc(side_tinfo);
 growing_buf_init(&local_arg->side_argbuf);
 growing_buf_allocate(&local_arg->side_argbuf, 0);
 if (args[ARGS_CLOSE].is_set) {
  growing_buf_appendstring(&local_arg->side_argbuf, "-c ");
 }
 if (args[ARGS_FROMEPOCH].is_set) {
#define BUFFER_SIZE 80
  char buffer[BUFFER_SIZE];
  snprintf(buffer, BUFFER_SIZE, "-f %ld ", args[ARGS_FROMEPOCH].arg.i);
  growing_buf_appendstring(&local_arg->side_argbuf, buffer);
 }
 growing_buf_appendstring(&local_arg->side_argbuf, args[ARGS_SUBTRACTFILE].arg.s);

 if (!local_arg->side_argbuf.can_be_freed || !setup_method(side_tinfo, &local_arg->side_argbuf)) {
  ERREXIT(tinfo->emethods, "subtract_init: Error setting readasc arguments.\n");
 }

 (*side_tinfo->methods->transform_init)(side_tinfo);

 side_tinfo->tsdata=NULL;	/* We still have to fetch the data... */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  subtract(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
subtract(transform_info_ptr tinfo) {
 struct subtract_storage *local_arg=(struct subtract_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;
 int channels1, channels2, points1, points2, channel1, channel2, point1, point2;
 int itempart, offset1, offset2;
 int itemskip=1+tinfo->leaveright;
 int side_itemskip=1+side_tinfo->leaveright;

 if (side_tinfo->tsdata==NULL || args[ARGS_EVERY].is_set) {
  /*{{{  (Try to) Read the next subtract epoch */
  if (side_tinfo->tsdata!=NULL) free_tinfo(side_tinfo);
  if ((side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo))!=NULL) {
   multiplexed(side_tinfo);
  }
  /*}}}  */
 }

 if (side_tinfo->tsdata==NULL) {
  ERREXIT(tinfo->emethods, "subtract: Last subtract epoch couldn't be read.\n");
 }
 if (local_arg->ttest) {
  if (itemskip!=3 && itemskip!=4) {
   ERREXIT(tinfo->emethods, "subtract: Not 3 or 4 values per item in tinfo to calculate t comparison.\n");
  }
  if (side_itemskip!=3 && side_itemskip!=4) {
   ERREXIT(tinfo->emethods, "subtract: Not 3 or 4 values per item in side_tinfo to calculate t comparison.\n");
  }
 } else
 if (tinfo->itemsize!=side_tinfo->itemsize || itemskip!=side_itemskip) {
  ERREXIT(tinfo->emethods, "subtract: Item counts of input epochs don't match.\n");
 }
 if (local_arg->type==SUBTRACT_TYPE_COMPLEXMULTIPLY || local_arg->type==SUBTRACT_TYPE_COMPLEXDIVIDE) {
  if (tinfo->itemsize%2!=0) {
   ERREXIT(tinfo->emethods, "subtract: Item size must be divisible by 2 for complex operations.\n");
  }
  itemskip=side_itemskip=2;
 }
 if (args[ARGS_RECYCLE_CHANNELS].is_set && tinfo->nr_of_channels>side_tinfo->nr_of_channels) {
  channels1=tinfo->nr_of_channels;
  channels2=side_tinfo->nr_of_channels;
 } else {
  channels1=channels2=min(tinfo->nr_of_channels, side_tinfo->nr_of_channels);
 }
 if (args[ARGS_RECYCLE_POINTS].is_set && tinfo->nr_of_points>side_tinfo->nr_of_points) {
  points1=tinfo->nr_of_points;
  points2=side_tinfo->nr_of_points;
 } else {
  points1=points2=min(tinfo->nr_of_points, side_tinfo->nr_of_points);
 }
 multiplexed(tinfo);
 for (point1=point2=0; point1<points1; point1++, point2++) {
  /* Flip point2 back to the start if it hits the end */
  if (point2==points2) point2=0;
  for (channel1=channel2=0; channel1<channels1; channel1++, channel2++) {
   /* Flip channel2 back to the start if it hits the end */
   if (channel2==channels2) channel2=0;
   for (itempart=0; itempart<tinfo->itemsize; itempart+=itemskip) {
    offset1=(point1*tinfo->nr_of_channels+channel1)*tinfo->itemsize+itempart;
    offset2=(point2*side_tinfo->nr_of_channels+channel2)*side_tinfo->itemsize+itempart;
    switch (local_arg->type) {
     case SUBTRACT_TYPE_ADD:
      tinfo->tsdata[offset1]+=side_tinfo->tsdata[offset2];
      break;
     case SUBTRACT_TYPE_MEAN:
      tinfo->tsdata[offset1]=(tinfo->tsdata[offset1]+side_tinfo->tsdata[offset2])/2;
      break;
     case SUBTRACT_TYPE_WMEAN:
      tinfo->tsdata[offset1]=(tinfo->tsdata[offset1]*tinfo->nrofaverages+side_tinfo->tsdata[offset2]*side_tinfo->nrofaverages)/(tinfo->nrofaverages+side_tinfo->nrofaverages);
      break;
     case SUBTRACT_TYPE_SUBTRACT:
      tinfo->tsdata[offset1]-=side_tinfo->tsdata[offset2];
      break;
     case SUBTRACT_TYPE_MULTIPLY:
      tinfo->tsdata[offset1]*=side_tinfo->tsdata[offset2];
      break;
     case SUBTRACT_TYPE_DIVIDE:
      tinfo->tsdata[offset1]/=side_tinfo->tsdata[offset2];
      break;
     case SUBTRACT_TYPE_COMPLEXMULTIPLY: {
      complex c1, c2, c3;
      /* Note that we directly conjugate c2 before multiplication->c3=c1*c2' */
      c1.Re=     tinfo->tsdata[offset1]; c1.Im=       tinfo->tsdata[offset1+1];
      c2.Re=side_tinfo->tsdata[offset1]; c2.Im= -side_tinfo->tsdata[offset1+1];
      c3=c_mult(c1, c2);
      tinfo->tsdata[offset1]=c3.Re; tinfo->tsdata[offset1+1]=c3.Im;
      }
      break;
     case SUBTRACT_TYPE_COMPLEXDIVIDE: {
      complex c1, c2, c3;
      /* Note that we directly conjugate c2 before multiplication->c3=c1/c2' */
      c1.Re=     tinfo->tsdata[offset1]; c1.Im=       tinfo->tsdata[offset1+1];
      c2.Re=side_tinfo->tsdata[offset1]; c2.Im= -side_tinfo->tsdata[offset1+1];
      c3=c_mult(c1, c_inv(c2));
      tinfo->tsdata[offset1]=c3.Re; tinfo->tsdata[offset1+1]=c3.Im;
      }
      break;
    }
    if (local_arg->ttest) {
     /*{{{  Calculate t and p values*/
     int const n1=(int const)(itemskip==4 ? tinfo->tsdata[offset1+3] : tinfo->nrofaverages);
     int const n2=(int const)(side_itemskip==4 ? side_tinfo->tsdata[offset2+3] : side_tinfo->nrofaverages);
     /* tsdata[offset+2] is sum(x^2), tsdata[offset+1] is sum(x) */
     /* stddevq is really n*square(sigma) */
     double const stddevq1=(tinfo->tsdata[offset1+2]-square((double)tinfo->tsdata[offset1+1])/n1);
     double const stddevq2=(side_tinfo->tsdata[offset2+2]-square((double)side_tinfo->tsdata[offset2+1])/n2);
     if (args[ARGS_LEAVE_TPARAMETERS].is_set) {
      long const N=n1+n2-1; /* The degrees of freedom for a t test against zero will be N-1 */
      /* The output will nominally have N averages, so we create
       * tweaked values that will recreate the t value for the paired
       * t-test when interpreted as sum and sumsquared and tested against zero. */
      tinfo->tsdata[offset1+1]=(tinfo->tsdata[offset1+1]/n1-side_tinfo->tsdata[offset2+1]/n2)*N;
      tinfo->tsdata[offset1+2]=((stddevq1+stddevq2)*(n1+n2)/(n1*n2))*N+square((double)tinfo->tsdata[offset1+1])/N;
      if (itemskip==4) {
       tinfo->tsdata[offset1+3]=N;
      }
     } else {
      double const stddev=sqrt((stddevq1+stddevq2)/(n1+n2-2));
      double const tval=(tinfo->tsdata[offset1+1]/n1-side_tinfo->tsdata[offset2+1]/n2)/stddev*sqrt(((double)n1*n2)/(n1+n2));
      tinfo->tsdata[offset1+1]=tval;
      tinfo->tsdata[offset1+2]=student_p(n1+n2-2, (float)fabs(tval));
     }
     /*}}}  */
    }
   }
  }
 }

 if (args[ARGS_TTEST].is_set && itemskip!=4) {
  /* Set the output nrofaverages to the necessary value needed for
   * 'calc ttest' to compute the correct values, but also to allow z score
   * computation from the t value by 'scale_by -i 1 invsqrtnrofaverages' */
  tinfo->nrofaverages=tinfo->nrofaverages+side_tinfo->nrofaverages-1;
 } else if (local_arg->type==SUBTRACT_TYPE_MEAN) {
  tinfo->nrofaverages=2;
 } else if (local_arg->type==SUBTRACT_TYPE_WMEAN) {
  tinfo->nrofaverages+=side_tinfo->nrofaverages;
 }
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  subtract_exit(transform_info_ptr tinfo)*/
METHODDEF void
subtract_exit(transform_info_ptr tinfo) {
 struct subtract_storage *local_arg=(struct subtract_storage *)tinfo->methods->local_storage;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;
 if (side_tinfo->methods->init_done) 
  (*side_tinfo->methods->transform_exit)(side_tinfo);
 free_tinfo(side_tinfo);
 free_methodmem(side_tinfo);
 growing_buf_free(&local_arg->side_argbuf);
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_subtract(transform_info_ptr tinfo)*/
GLOBAL void
select_subtract(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &subtract_init;
 tinfo->methods->transform= &subtract;
 tinfo->methods->transform_exit= &subtract_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="subtract";
 tinfo->methods->method_description=
  "Transform method to subtract a given asc-file epoch from\n"
  "all incoming epochs to transform.\n";
 tinfo->methods->local_storage_size=sizeof(struct subtract_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
