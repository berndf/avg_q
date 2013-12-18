/*
 * Copyright (C) 2005-2007,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * convolve is a transform method heavily based upon sliding_average,
 * used to build simple detectors of a given wave characteristic.
 * 						-- Bernd Feige 19.05.2005
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
 ARGS_CLOSE=0, 
 ARGS_EVERY, 
 ARGS_FROMEPOCH, 
 ARGS_CONVOLVEFILE,
 ARGS_SSTEP, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Close and reopen the file for each epoch", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Read every epoch in convolve_file in turn", "e", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Start with epoch number fromepoch (>=1)", "f", 1, NULL},
 {T_ARGS_TAKES_FILENAME, "convolve_file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"},
 {T_ARGS_TAKES_STRING_WORD, "sliding_step", "", ARGDESC_UNUSED, (const char *const *)"2.5ms"}
};

typedef struct sliding_data_struct {
 struct transform_info_struct side_tinfo;
 struct transform_methods_struct side_method;
 growing_buf side_argbuf;

 array sidearray;

 /*{{{  Struct for passing information to single_convolve*/
 DATATYPE *fromstart;
 int fromskip;
 DATATYPE *tostart;
 int toskip;

 int ssize;
 float sstep;
 int inpoints;

 int allocated_outpoints;
 int outpoints; /* Set on return */
 /*}}}  */
} sliding_data;

/* This is used for xdata only: */
LOCAL void
single_sliding_average(sliding_data *sdata) {
 int point, spoint, j;
 int const rightover=sdata->ssize/2;
 /* Mend the special case that ssize==1; 
  * else, a point too much is left out at the start: */
 int const leftover=(sdata->ssize==1 ? 0 : sdata->ssize-rightover);
 int const allocated_outpoints=sdata->allocated_outpoints;
 float s;
 DATATYPE mean=0.0;

 /*{{{  Slowly increase the number of averaged points to ssize*/
 for (s=sdata->sstep+leftover, point=spoint=0; point<sdata->ssize; point++) {
  mean+=sdata->fromstart[point*sdata->fromskip];
  s--;
  while (s<=0) {
   /* Protect against memory overflow. This can happen as a rounding problem. */
   if (spoint<allocated_outpoints)
   sdata->tostart[spoint*sdata->toskip]=mean/(point+1);
   s+=sdata->sstep; spoint++;
  }
 }
 /*}}}  */
 /*{{{  Slide the window continuously*/
 for (; point<sdata->inpoints; point++) {
  mean+= sdata->fromstart[point*sdata->fromskip]-sdata->fromstart[(point-sdata->ssize)*sdata->fromskip];
  s--;
  while (s<=0.0) {
   /* Protect against memory overflow. This can happen as a rounding problem. */
   if (spoint<allocated_outpoints)
   sdata->tostart[spoint*sdata->toskip]=mean/sdata->ssize;
   s+=sdata->sstep; spoint++;
  }
 }
 /*}}}  */
 /*{{{  Slowly take the last 'rightover' points out of the average*/
 if (rightover>0)
 for (j=sdata->ssize-1; j>=rightover; j--, point++) {
  mean-= sdata->fromstart[(point-sdata->ssize)*sdata->fromskip];
  s--;
  while (s<=0) {
   /* Protect against memory overflow. This can happen as a rounding problem. */
   if (spoint<allocated_outpoints)
   sdata->tostart[spoint*sdata->toskip]=mean/j;
   s+=sdata->sstep; spoint++;
  }
 }
 /*}}}  */
 sdata->outpoints=spoint;
}
LOCAL void
single_convolve(sliding_data *sdata) {
 int point, spoint, j;
 int const rightover=sdata->ssize/2;
 /* Mend the special case that ssize==1; 
  * else, a point too much is left out at the start: */
 int const leftover=(sdata->ssize==1 ? 0 : sdata->ssize-rightover);
 int const allocated_outpoints=sdata->allocated_outpoints;
 int const template_vector=sdata->sidearray.current_vector;
 array a;
 float s;
 DATATYPE val;

 a.nr_of_vectors=a.element_skip=sdata->fromskip;

 /*{{{  Slowly increase the number of averaged points to ssize*/
 for (s=sdata->sstep+leftover, point=spoint=0; point<sdata->ssize; point++) {
  a.start=sdata->fromstart;
  a.nr_of_elements=point+1;
  array_setreadwrite(&a);
  a.current_vector=a.current_element=sdata->sidearray.current_element=0;
  sdata->sidearray.current_vector=template_vector;
  val=array_multiply(&a, &sdata->sidearray, MULT_VECTOR);
  s--;
  while (s<=0) {
   /* Protect against memory overflow. This can happen as a rounding problem. */
   if (spoint<allocated_outpoints)
   /* Note that in contrast to sliding_average, here we always divide the
    * sum product by ssize, not by the actual number of products, so that
    * the partial sums at start and end are not disproportionately pronounced! */
   sdata->tostart[spoint*sdata->toskip]=val/sdata->ssize;
   s+=sdata->sstep; spoint++;
  }
 }
 /*}}}  */
 /*{{{  Slide the window continuously*/
 for (; point<sdata->inpoints; point++) {
  a.start=sdata->fromstart+(point-sdata->ssize+1)*sdata->fromskip;
  array_setreadwrite(&a);
  a.current_vector=a.current_element=sdata->sidearray.current_element=0;
  sdata->sidearray.current_vector=template_vector;
  val=array_multiply(&a, &sdata->sidearray, MULT_VECTOR);
  s--;
  while (s<=0.0) {
   /* Protect against memory overflow. This can happen as a rounding problem. */
   if (spoint<allocated_outpoints)
   sdata->tostart[spoint*sdata->toskip]=val/sdata->ssize;
   s+=sdata->sstep; spoint++;
  }
 }
 /*}}}  */
 /*{{{  Slowly take the last 'rightover' points out of the average*/
 if (rightover>0)
 for (j=sdata->ssize-1; j>=rightover; j--, point++) {
  a.start=sdata->fromstart+(point-sdata->ssize+1)*sdata->fromskip;
  a.nr_of_elements=j;
  array_setreadwrite(&a);
  a.current_vector=a.current_element=sdata->sidearray.current_element=0;
  sdata->sidearray.current_vector=template_vector;
  val=array_multiply(&a, &sdata->sidearray, MULT_VECTOR);
  s--;
  while (s<=0) {
   /* Protect against memory overflow. This can happen as a rounding problem. */
   if (spoint<allocated_outpoints)
   sdata->tostart[spoint*sdata->toskip]=val/sdata->ssize;
   s+=sdata->sstep; spoint++;
  }
 }
 /*}}}  */
 sdata->outpoints=spoint;
}

/*{{{  convolve_init(transform_info_ptr tinfo) {*/
METHODDEF void
convolve_init(transform_info_ptr tinfo) {
 sliding_data *local_arg=(sliding_data *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;

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
 growing_buf_appendstring(&local_arg->side_argbuf, args[ARGS_CONVOLVEFILE].arg.s);

 if (!local_arg->side_argbuf.can_be_freed || !setup_method(side_tinfo, &local_arg->side_argbuf)) {
  ERREXIT(tinfo->emethods, "convolve_init: Error setting readasc arguments.\n");
 }

 (*side_tinfo->methods->transform_init)(side_tinfo);

 side_tinfo->tsdata=NULL;	/* We still have to fetch the data... */

 local_arg->sstep=gettimefloat(tinfo, args[ARGS_SSTEP].arg.s);
 if (local_arg->sstep<=0) {
  ERREXIT(tinfo->emethods, "convolve_init: Sliding_step<=0!\n");
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  convolve(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
convolve(transform_info_ptr tinfo) {
 sliding_data *local_arg=(sliding_data *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;
 int channel, itempart;
 int const new_outpoints=(int)(tinfo->nr_of_points/local_arg->sstep);
 DATATYPE *slidedata, *slide_xdata=NULL;
 float size_factor;

 if (side_tinfo->tsdata==NULL || args[ARGS_EVERY].is_set) {
  /*{{{  (Try to) Read the next convolve epoch */
  if (side_tinfo->tsdata!=NULL) free_tinfo(side_tinfo);
  side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo);
  if (side_tinfo->nr_of_channels!=1 && side_tinfo->nr_of_channels!=tinfo->nr_of_channels) {
   ERREXIT(tinfo->emethods, "convolve: convolve_file needs either 1 or as many channels as the input epoch.\n");
  }
  tinfo_array(side_tinfo, &local_arg->sidearray);
  /*}}}  */
 }

 /* Rejecting epochs is our choice if more epochs come in than present in
  * the convolve file */
 if (side_tinfo->tsdata==NULL) {
  TRACEMS(tinfo->emethods, 0, "convolve: Rejecting epoch since corresponding convolve_file epoch could not be read!\n");
  return NULL;
 }

 local_arg->ssize=side_tinfo->nr_of_points;
 if (local_arg->ssize>tinfo->nr_of_points) {
  TRACEMS2(tinfo->emethods, 0, "convolve_init: Specified sliding_size (%d)>nr_of_points, reducing to %d\n", MSGPARM(local_arg->ssize), MSGPARM(tinfo->nr_of_points));
  local_arg->ssize=tinfo->nr_of_points;
 }

 multiplexed(tinfo);
 /*{{{  Allocate the output memory*/
 if ((slidedata=(DATATYPE *)malloc(tinfo->nr_of_channels*new_outpoints*tinfo->itemsize*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "convolve: Error allocating slidedata memory\n");
 }
 if (tinfo->xdata!=NULL) {
  /* Store xchannelname at the end of xdata, as in create_xaxis(); otherwise we'll have 
   * a problem after free()'ing xdata... */
  if ((slide_xdata=(DATATYPE *)malloc(new_outpoints*sizeof(DATATYPE)+strlen(tinfo->xchannelname)+1))==NULL) {
   ERREXIT(tinfo->emethods, "convolve: Error allocating slide_xdata memory\n");
  }
 }
 /*}}}  */
 /*{{{  Perform the sliding average on the points for each item in each channel*/
 local_arg->fromskip=local_arg->toskip=tinfo->nr_of_channels*tinfo->itemsize;
 local_arg->inpoints=tinfo->nr_of_points;
 /* Inform single_convolve about the size of the allocated array */
 local_arg->allocated_outpoints=new_outpoints;
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  for (itempart=0; itempart<tinfo->itemsize; itempart++) {
   local_arg->fromstart=tinfo->tsdata+channel*tinfo->itemsize+itempart;
   local_arg->tostart  =    slidedata+channel*tinfo->itemsize+itempart;
   local_arg->sidearray.current_vector=(local_arg->sidearray.nr_of_vectors==1 ? 0 : channel);
   single_convolve(local_arg);
  }
 }
 /*}}}  */
 if (tinfo->xdata!=NULL) {
  /*{{{  Perform the sliding average on xdata as well*/
  local_arg->fromstart=tinfo->xdata;
  local_arg->tostart  = slide_xdata;
  local_arg->fromskip = local_arg->toskip=1;
  single_sliding_average(local_arg);
  /*}}}  */
  strcpy((char *)(slide_xdata+new_outpoints),tinfo->xchannelname);
  free(tinfo->xdata);
  tinfo->xdata=slide_xdata;
  tinfo->xchannelname=(char *)(slide_xdata+new_outpoints);
 }
 /* local_arg->outpoints should be equal to new_outpoints! */
 if (local_arg->outpoints!=new_outpoints) {
  TRACEMS2(tinfo->emethods, 0, "convolve: Calculated %d outpoints, got %d.\n", MSGPARM(new_outpoints), MSGPARM(local_arg->outpoints));
 }
 size_factor=((DATATYPE)new_outpoints)/tinfo->nr_of_points;
 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start;
  while (intrig->code!=0) {
   intrig->position=(int)rint(intrig->position*size_factor);
   intrig++;
  }
 }
 tinfo->sfreq*= size_factor;	/* Adjust the sampling rate */
 tinfo->beforetrig=(int)rint(tinfo->beforetrig*size_factor);
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->nr_of_points=new_outpoints;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tinfo->itemsize;

 return slidedata;
}
/*}}}  */

/*{{{  convolve_exit(transform_info_ptr tinfo) {*/
METHODDEF void
convolve_exit(transform_info_ptr tinfo) {
 sliding_data *local_arg=(sliding_data *)tinfo->methods->local_storage;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;
 if (side_tinfo->methods->init_done) 
  (*side_tinfo->methods->transform_exit)(side_tinfo);
 free_tinfo(side_tinfo);
 free_methodmem(side_tinfo);
 growing_buf_free(&local_arg->side_argbuf);
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_convolve(transform_info_ptr tinfo) {*/
GLOBAL void
select_convolve(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &convolve_init;
 tinfo->methods->transform= &convolve;
 tinfo->methods->transform_exit= &convolve_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="convolve";
 tinfo->methods->method_description=
  "Convolution method very similar to sliding_average but sliding\n"
  " arbitrary waveform(s) across the current data instead of an (implied)\n"
  " block or rectangular function.\n"
  " sstep can be specified in points (also fractional) or as time by\n"
  " appending `s' or `ms'.\n";
 tinfo->methods->local_storage_size=sizeof(sliding_data);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
