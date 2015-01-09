/*
 * Copyright (C) 1996,1998,1999,2001,2003,2004,2006,2008,2011,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * sliding_average is a transform method serving two purposes by combinations
 * of the two parameters (int)sliding_size and (float)sliding_step:
 *
 * - At first, it implements a moving average filter over sliding_size data
 * points. Care is taken that if sliding_step==1, the output is of exactly
 * the same size as the input no matter what sliding_size is. This is done
 * by allowing the window to contain only sliding_size/2 the points at the left
 * and right ends of the data. Thus, the output data points always represent
 * a window of the original data centered around this point.
 *
 * - Second, the floating-point sliding_step allows resampling of the data
 * (or rather, of the moving average output) at any rate, including
 * expansion of the original data. Thus, sliding_size=1 and sliding_step=0.5
 * resamples the original data at twice the rate.
 *
 * 						-- Bernd Feige 22.07.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_MEDIAN=0, 
 ARGS_SSIZE, 
 ARGS_SSTEP, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Use sliding Median instead of average", "M", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "sliding_size", "", ARGDESC_UNUSED, (const char *const *)"10ms"},
 {T_ARGS_TAKES_STRING_WORD, "sliding_step", "", ARGDESC_UNUSED, (const char *const *)"2.5ms"}
};

typedef struct sliding_data_struct {
 void (*single_sliding_function)(struct sliding_data_struct *sdata);

 /*{{{  Struct for passing information to single_sliding_function*/
 DATATYPE *fromstart;
 int fromskip;
 DATATYPE *tostart;
 int toskip;

 int ssize;
 int leftover;
 int rightover;
 float sstep;
 int inpoints;

 int allocated_outpoints;
 /*}}}  */

 DATATYPE sfreq; /* To see whether sfreq changed */
 unsigned long *indx;
} sliding_data;

LOCAL void
single_sliding_average(sliding_data *sdata) {
 int spoint;

 /* Sliding window: eg ssize=5
  * |L|L|R|R|R|
  *      ^ middle_point (Always first R) */
 /* Optimization: Treat three different cases.
  * The second and third implementations are mathematically equivalent. */
 if (sdata->ssize==1) {
  for (spoint=0; spoint<sdata->allocated_outpoints; spoint++) { /* Loop across target points */
   int const middle_point=(int)floorf(spoint*sdata->sstep);
   sdata->tostart[spoint*sdata->toskip]=sdata->fromstart[middle_point*sdata->fromskip];
  }
 } else if (sdata->sstep>sdata->ssize/2) {
  /* Compute the whole average anew */
  for (spoint=0; spoint<sdata->allocated_outpoints; spoint++) { /* Loop across target points */
   int const middle_point=(int)floorf(spoint*sdata->sstep);
   int const new_left_point= (middle_point-sdata->leftover>0 ? middle_point-sdata->leftover : 0);
   int const new_right_point=(middle_point+sdata->rightover<sdata->inpoints ? middle_point+sdata->rightover : sdata->inpoints)-1;
   DATATYPE sum=0.0;
   int point;
   for (point=new_left_point; point<=new_right_point; point++) {
    sum+=sdata->fromstart[point*sdata->fromskip];
   }
   sdata->tostart[spoint*sdata->toskip]=sum/(new_right_point-new_left_point+1);
  }
 } else {
  /* Sliding-Window algorithm subtracting points falling out on the left 
   * and adding incoming points to the right */
  int left_point= -1, right_point= -1;
  DATATYPE sum=0.0;
  for (spoint=0; spoint<sdata->allocated_outpoints; spoint++) { /* Loop across target points */
   int const middle_point=(int)floorf(spoint*sdata->sstep);
   int const new_left_point= (middle_point-sdata->leftover>0 ? middle_point-sdata->leftover : 0);
   int const new_right_point=(middle_point+sdata->rightover<sdata->inpoints ? middle_point+sdata->rightover : sdata->inpoints)-1;
   int point;

   if (new_left_point>0) { /* Nothing to subtract at the start */
    for (point=left_point; point<new_left_point; point++) {
     sum-=sdata->fromstart[point*sdata->fromskip];
    }
   }
   for (point=right_point+1; point<=new_right_point; point++) {
    sum+=sdata->fromstart[point*sdata->fromskip];
   }
   left_point=new_left_point;
   right_point=new_right_point;
   sdata->tostart[spoint*sdata->toskip]=sum/(new_right_point-new_left_point+1);
  }
 }
}
#define MEDIAN(a) \
  (i1=((a)->nr_of_elements-1)/2, i2=(a)->nr_of_elements/2,\
  array_index(a, sdata->indx),\
  (a)->current_element=sdata->indx[i1],\
  d1=READ_ELEMENT(a),\
  (a)->current_element=sdata->indx[i2],\
  d2=READ_ELEMENT(a),\
  (d1+d2)/2.0)
LOCAL void
single_sliding_median(sliding_data *sdata) {
 int spoint;
 unsigned long i1, i2;
 array a;
 DATATYPE d1, d2;

 a.current_vector=0;
 a.nr_of_vectors=a.element_skip=sdata->fromskip;

 for (spoint=0; spoint<sdata->allocated_outpoints; spoint++) { /* Loop across target points */
  int const middle_point=(int)floorf(spoint*sdata->sstep);
  int const new_left_point= (middle_point-sdata->leftover>0 ? middle_point-sdata->leftover : 0);
  int const new_right_point=(middle_point+sdata->rightover<sdata->inpoints ? middle_point+sdata->rightover : sdata->inpoints)-1;
  a.start=sdata->fromstart+new_left_point*sdata->fromskip;
  a.nr_of_elements=new_right_point-new_left_point+1;
  array_setreadwrite(&a);
  sdata->tostart[spoint*sdata->toskip]=MEDIAN(&a);
 }
}

LOCAL void
init_sdata(transform_info_ptr tinfo) {
 sliding_data *sdata=(sliding_data *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 sdata->sfreq=tinfo->sfreq;
 free_pointer((void **)&sdata->indx);

 sdata->ssize=gettimeslice(tinfo, args[ARGS_SSIZE].arg.s);
 sdata->sstep=gettimefloat(tinfo, args[ARGS_SSTEP].arg.s);
 if (sdata->ssize>tinfo->nr_of_points) {
  TRACEMS2(tinfo->emethods, 0, "sliding_average_init: Specified sliding_size (%d)>nr_of_points, reducing to %d\n", MSGPARM(sdata->ssize), MSGPARM(tinfo->nr_of_points));
  sdata->ssize=tinfo->nr_of_points;
 }
 if (sdata->ssize<1) {
  ERREXIT(tinfo->emethods, "sliding_average_init: Sliding_size<1!\n");
 }
 if (sdata->sstep<=0) {
  ERREXIT(tinfo->emethods, "sliding_average_init: Sliding_step<=0!\n");
 }
 if (sdata->ssize==1 && sdata->sstep==1.0) {
  TRACEMS(tinfo->emethods, 0, "sliding_average: ssize==sstep==1, bypassing!\n");
 } else {
  if (args[ARGS_MEDIAN].is_set) {
   sdata->single_sliding_function=single_sliding_median;
   if ((sdata->indx=(unsigned long *)malloc(sdata->ssize*sizeof(unsigned long)))==NULL) {
    ERREXIT(tinfo->emethods, "sliding_average_init: Error allocating memory\n");
   }
  } else {
   sdata->single_sliding_function=single_sliding_average;
  }
 }
 /* Sliding window: eg ssize=5
  * |L|L|R|R|R| */
 sdata->rightover=(sdata->ssize-1)/2+1;
 sdata->leftover=sdata->ssize-sdata->rightover;
}

/*{{{  sliding_average_init(transform_info_ptr tinfo) {*/
METHODDEF void
sliding_average_init(transform_info_ptr tinfo) {
 sliding_data *sdata=(sliding_data *)tinfo->methods->local_storage;

 sdata->indx=NULL;
 init_sdata(tinfo);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  sliding_average(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
sliding_average(transform_info_ptr tinfo) {
 sliding_data *sdata=(sliding_data *)tinfo->methods->local_storage;
 int channel, itempart;
 DATATYPE *slidedata, *slide_xdata=NULL;
 DATATYPE size_factor;

 /* Re-init if sfreq changed. This is important for data entered in time rather
  * than in point units. */
 if (tinfo->sfreq!=sdata->sfreq) init_sdata(tinfo);

 if (sdata->ssize==1 && sdata->sstep==1.0) {
  /* Bypass the trivial operation */
  return tinfo->tsdata;
 }

 sdata->allocated_outpoints=(int)rint(tinfo->nr_of_points/sdata->sstep);
 if (sdata->allocated_outpoints==0) {
  /* Zero size result data set! Reject this epoch. */
  return NULL;
 }

 multiplexed(tinfo);

 /*{{{  Allocate the output memory*/
 if ((slidedata=(DATATYPE *)malloc(tinfo->nr_of_channels*sdata->allocated_outpoints*tinfo->itemsize*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "sliding_average: Error allocating slidedata memory\n");
 }
 if (tinfo->xdata!=NULL) {
  /* Store xchannelname at the end of xdata, as in create_xaxis(); otherwise we'll have 
   * a problem after free()'ing xdata... */
  if ((slide_xdata=(DATATYPE *)malloc(sdata->allocated_outpoints*sizeof(DATATYPE)+strlen(tinfo->xchannelname)+1))==NULL) {
   ERREXIT(tinfo->emethods, "sliding_average: Error allocating slide_xdata memory\n");
  }
 }
 /*}}}  */
 /*{{{  Perform the sliding average on the points for each item in each channel*/
 sdata->fromskip=sdata->toskip=tinfo->nr_of_channels*tinfo->itemsize;
 sdata->inpoints=tinfo->nr_of_points;
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  for (itempart=0; itempart<tinfo->itemsize; itempart++) {
   sdata->fromstart=tinfo->tsdata+channel*tinfo->itemsize+itempart;
   sdata->tostart  =    slidedata+channel*tinfo->itemsize+itempart;
   (*sdata->single_sliding_function)(sdata);
  }
 }
 /*}}}  */
 if (tinfo->xdata!=NULL) {
  /*{{{  Perform the sliding average on xdata as well*/
  sdata->fromstart=tinfo->xdata;
  sdata->tostart  = slide_xdata;
  sdata->fromskip = sdata->toskip=1;
  (*sdata->single_sliding_function)(sdata);
  /*}}}  */
  strcpy((char *)(slide_xdata+sdata->allocated_outpoints),tinfo->xchannelname);
  free(tinfo->xdata);
  tinfo->xdata=slide_xdata;
  tinfo->xchannelname=(char *)(slide_xdata+sdata->allocated_outpoints);
 }
 size_factor=((DATATYPE)sdata->allocated_outpoints)/tinfo->nr_of_points;
 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start;
  while (intrig->code!=0) {
   intrig->position=(int)rint(intrig->position*size_factor);
   intrig++;
  }
 }
 tinfo->sfreq*= size_factor;	/* Adjust the sampling rate */
 tinfo->nr_of_points=sdata->allocated_outpoints;
 tinfo->beforetrig=(int)rint(tinfo->beforetrig*size_factor);
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tinfo->itemsize;

 return slidedata;
}
/*}}}  */

/*{{{  sliding_average_exit(transform_info_ptr tinfo) {*/
METHODDEF void
sliding_average_exit(transform_info_ptr tinfo) {
 sliding_data *sdata=(sliding_data *)tinfo->methods->local_storage;

 free_pointer((void **)&sdata->indx);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_sliding_average(transform_info_ptr tinfo) {*/
GLOBAL void
select_sliding_average(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &sliding_average_init;
 tinfo->methods->transform= &sliding_average;
 tinfo->methods->transform_exit= &sliding_average_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="sliding_average";
 tinfo->methods->method_description=
  "Sliding average method (block filter) for smoothing and resampling.\n"
  " Both arguments can be specified in points (ssize is rounded to integer,\n"
  " while sstep may be fractional) or as time by appending `s' or `ms'.\n";
 tinfo->methods->local_storage_size=sizeof(sliding_data);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
