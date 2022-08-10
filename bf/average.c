/*
 * Copyright (C) 1996-1999,2001,2003,2004,2009,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * average.c collect method to average epoch data or a transformation thereof
 *	-- Bernd Feige 11.02.1993
 *
 * The method call itself only sums up the incoming epochs and returns a
 * pointer to this sum; the division by nrofaverages only takes place
 * during the exit operation.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
#include "stat.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_MATCHBYNAME=0, 
 ARGS_SIGNTEST, 
 ARGS_SSIGNTEST,
 ARGS_TTEST,
 ARGS_LEAVE_TPARAMETERS,
 ARGS_WEIGHTED,
 ARGS_OUTPUT_NROFFILES,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Match channels to average by name rather than by position", "M", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Sign test against baseline, ie count - and + incidences", "s", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "As -s, but average +/- counts against all single baseline values", "ss", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "perform a t-test against zero", "t", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "output the accumulated parameters for the t test instead of t and p", "u", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "use the Nr_of_averages property of the incoming epochs as weights", "W", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Set the output Nr_of_averages to the number of averaged epochs rather than to the sum of the weights", "N", FALSE, NULL}
};

/*{{{  Global defines*/
enum statistical_test {
 STAT_NONE=0,
 STAT_SIGN, STAT_SINGLESIGN,
 STAT_TTEST,
 NROFTESTS
};
LOCAL int var_steps[NROFTESTS]={
 1, 3, 3, 3
};
LOCAL int matchbyname_addsteps[NROFTESTS]={
 1, 0, 0, 1
};

struct average_local_struct {
 struct transform_info_struct tinfo;
 enum statistical_test stat_test;
 int nr_of_averages;
 int nroffiles;
 int original_itemsize;
 int original_leaveright;
 long *channelaverages;
 long *channelweights;
};
/*}}}  */

/*{{{  average_init(transform_info_ptr tinfo)*/
METHODDEF void
average_init(transform_info_ptr tinfo) {
 struct average_local_struct *localp=(struct average_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 localp->nr_of_averages=0;
 localp->nroffiles=0;

 if (args[ARGS_SIGNTEST].is_set) {
  localp->stat_test=STAT_SIGN;
 } else if (args[ARGS_SSIGNTEST].is_set) {
  localp->stat_test=STAT_SINGLESIGN;
 } else if (args[ARGS_TTEST].is_set || args[ARGS_LEAVE_TPARAMETERS].is_set) {
  if (!args[ARGS_TTEST].is_set) {
   TRACEMS(tinfo->emethods, 0, "average_init: Option -u given without -t. Adding -t...\n");
  }
  localp->stat_test=STAT_TTEST;
 } else {
  localp->stat_test=STAT_NONE;
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  average(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
average(transform_info_ptr tinfo) {
 struct average_local_struct *localp=(struct average_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int *channelmap=NULL;
 int missing_channels=0;
 int const varstep=var_steps[localp->stat_test]+(args[ARGS_MATCHBYNAME].is_set ? matchbyname_addsteps[localp->stat_test] : 0);
 int channel, itempart, freq, nfreq;
 int basepoints=tinfo->beforetrig;
 array avg_tsdata, tsdata, baseline;

 if (tinfo->nrofaverages<=0 || !args[ARGS_WEIGHTED].is_set) tinfo->nrofaverages=1;
 if (localp->nr_of_averages==0) {
  /*{{{  First incoming epoch: Initialize working memory*/
  /* Make our own local copy of tinfo and its arrays */
  deepcopy_tinfo(&localp->tinfo, tinfo);

  localp->tinfo.length_of_output_region*=varstep;
  /* Allocate result space */
  if ((localp->tinfo.tsdata=(DATATYPE *)calloc(localp->tinfo.length_of_output_region, sizeof(DATATYPE)))==NULL) {
   ERREXIT(tinfo->emethods, "average: Error allocating epoch memory\n");
  }
  localp->original_itemsize=tinfo->itemsize;
  localp->tinfo.itemsize*=varstep;
  localp->original_leaveright=tinfo->leaveright;
  localp->tinfo.leaveright=(tinfo->leaveright+1)*varstep-1;
  localp->tinfo.z_value=0.0;
  if ((localp->channelaverages=(long *)calloc(tinfo->nr_of_channels, sizeof(long)))==NULL ||
      (localp->channelweights=(long *)calloc(tinfo->nr_of_channels, sizeof(long)))==NULL) {
   ERREXIT(tinfo->emethods, "average: Error allocating channel averages+weights\n");
  }
  /*}}}  */
 } else {
  if (tinfo->nr_of_points!=localp->tinfo.nr_of_points || tinfo->itemsize!=localp->original_itemsize) {
   ERREXIT(tinfo->emethods, "average: Varying epoch length.\n");
  }
  if (!args[ARGS_MATCHBYNAME].is_set && tinfo->nr_of_channels!=localp->tinfo.nr_of_channels) {
   ERREXIT(tinfo->emethods, "average: Varying number of channels. Hint: You might try -M\n");
  }
 }
 if (args[ARGS_MATCHBYNAME].is_set) {
  int *missmap=NULL;
 if ((channelmap=(int *)malloc(tinfo->nr_of_channels*sizeof(int)))==NULL ||
     (missmap=(int *)malloc((tinfo->nr_of_channels+1)*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "average: Error allocating channelmap\n");
 }
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  channelmap[channel]=find_channel_number(&localp->tinfo, tinfo->channelnames[channel]);
  if (channelmap[channel]== -1) {
   /* Enter the location this channel will have after add_channels_or_points */
   channelmap[channel]=localp->tinfo.nr_of_channels+missing_channels;
   missmap[missing_channels]=channel+1;
   missing_channels++;
  }
 }
 missmap[missing_channels]=0;	/* End of list */

 if (missing_channels>0) {
  DATATYPE *new_tsdata=add_channels_or_points(&localp->tinfo, tinfo, /* channel_list= */missmap, ADD_CHANNELS, /* zero_newdata= */TRUE);
  /* At this point, localp->tinfo.nr_of_channels is already enlarged: */
  long *new_channelaverages=(long *)realloc(localp->channelaverages, localp->tinfo.nr_of_channels*sizeof(long));
  long *new_channelweights=(long *)realloc(localp->channelweights, localp->tinfo.nr_of_channels*sizeof(long));
  if (new_tsdata==NULL || new_channelaverages==NULL || new_channelweights==NULL) {
   ERREXIT(tinfo->emethods, "average: Error adding new channels\n");
  }
  free_pointer((void **)&localp->tinfo.tsdata);
  localp->tinfo.tsdata=new_tsdata;
  localp->channelaverages=new_channelaverages;
  localp->channelweights=new_channelweights;
  for (channel=localp->tinfo.nr_of_channels-missing_channels; channel<localp->tinfo.nr_of_channels; channel++) {
   localp->channelaverages[channel]=localp->channelweights[channel]=0;
  }
  TRACEMS1(tinfo->emethods, 1, "average: Added %d channels!\n", MSGPARM(missing_channels));
 }
  free(missmap);
 }

 /*{{{  Add a new epoch, possibly with statistical analysis*/
 /*{{{  Set up tsdata array: elements=points, vectors=items*/
 if (tinfo->data_type==FREQ_DATA) {
  nfreq=tinfo->nroffreq;
  if (tinfo->windowsize>basepoints) {
   basepoints=0;
  } else if (tinfo->shiftwidth==0) {
   /* This means that only 1 shift is available altogether */
   basepoints=1;
  } else {
   basepoints=1+(basepoints-tinfo->windowsize)/tinfo->shiftwidth;
  }
 } else {
  nfreq=1;
  basepoints=tinfo->beforetrig;
 }
 /*}}}  */

 for (freq=0; freq<nfreq; freq++) {
  if (tinfo->data_type==FREQ_DATA) {
   /* Since we want to access the baseline as an average over shifts in the
    * FREQ_DATA case, we need to access the shifts as array elements */
   tinfo_array_setfreq(tinfo, &tsdata, freq);
   tinfo_array_setfreq(&localp->tinfo, &avg_tsdata, freq);
  } else {
   tinfo_array(tinfo, &tsdata);
   tinfo_array(&localp->tinfo, &avg_tsdata);
  }
  baseline=tsdata; baseline.nr_of_elements=basepoints;

  for (itempart=0; itempart<tinfo->itemsize; itempart++) {
   array_use_item(&tsdata, itempart);
   array_use_item(&avg_tsdata, itempart*varstep);
   array_use_item(&baseline, itempart);

   do {	/* For each channel==vector */
    int const target_channel=(channelmap==NULL ? tsdata.current_vector : channelmap[tsdata.current_vector]);
    avg_tsdata.current_vector=target_channel;

    do {
     DATATYPE * const tmpptr=ARRAY_ELEMENT(&avg_tsdata);
     if (itempart<tinfo->itemsize-tinfo->leaveright) {
      tmpptr[0]+=array_scan(&tsdata)*tinfo->nrofaverages;
     } else {
      /* Non-weighted sum for the leaveright (`sum-only') items */
      tmpptr[0]+=array_scan(&tsdata);
     }
     array_advance(&avg_tsdata);
    } while (tsdata.message==ARRAY_CONTINUE);

    switch (localp->stat_test) {
     case STAT_SINGLESIGN:
      /*{{{  Do single value comparisons*/
      array_previousvector(&tsdata);
      array_previousvector(&avg_tsdata);
      while (TRUE) {	/* For each element */
       int votesum=0, nr_of_votes=0;
       int const tsdata_current_element=tsdata.current_element;
       DATATYPE * const tmpptr=ARRAY_ELEMENT(&avg_tsdata);
       DATATYPE const hold=array_scan(&tsdata);
       DATATYPE votemean;

       do {	/* For each baseline element */
	if (baseline.current_element!=tsdata_current_element) {
	 /* Don't count a comparison of a baseline value with itself */
	 if (hold>array_scan(&baseline)) votesum++;
	 nr_of_votes++;
	} else {
	 array_advance(&baseline);
	}
       } while (baseline.message==ARRAY_CONTINUE);
       votemean=((DATATYPE)votesum)/nr_of_votes;

       tmpptr[1]+=votemean; tmpptr[2]+=(1.0-votemean);
       array_advance(&avg_tsdata);
       if (tsdata.message!=ARRAY_CONTINUE) break;
       array_previousvector(&baseline);
      }
      /*}}}  */
      break;
     case STAT_SIGN:
      /*{{{  Compare against average baseline*/
      {
      DATATYPE const baseval=array_mean(&baseline);	/* Baseline mean */
      array_previousvector(&tsdata);
      array_previousvector(&avg_tsdata);
      do {	/* For each element */
       DATATYPE * const tmpptr=ARRAY_ELEMENT(&avg_tsdata);
       DATATYPE const hold=array_scan(&tsdata);
       /* Correct the mean to NOT include THIS value if within baseline
	* This does absolutely nothing to the result.
	* DATATYPE corrected_baseval=(tsdata.current_element>=1 && tsdata.current_element<=basepoints ?
	*	(baseval*basepoints-hold)/(basepoints-1) : baseval);
	*/
       tmpptr[hold>baseval ? 1 : 2]++;
       array_advance(&avg_tsdata);
      } while (tsdata.message==ARRAY_CONTINUE);
      }
      /*}}}  */
      break;
     case STAT_TTEST:
      array_previousvector(&tsdata);
      array_previousvector(&avg_tsdata);
      do {	/* For each element */
       DATATYPE * const tmpptr=ARRAY_ELEMENT(&avg_tsdata);
       DATATYPE const hold=array_scan(&tsdata);
       tmpptr[1]+=hold; tmpptr[2]+=hold*hold;
       array_advance(&avg_tsdata);
      } while (tsdata.message==ARRAY_CONTINUE);
      break;
     default:
      break;
    }
    if (freq==0 && itempart==0) {
     localp->channelaverages[target_channel]++;
     localp->channelweights[target_channel]+=tinfo->nrofaverages;
    }
   } while (tsdata.message!=ARRAY_ENDOFSCAN);
  }
 }
 localp->tinfo.z_value+=tinfo->z_value*tinfo->nrofaverages;
 /*}}}  */

 if (channelmap!=NULL) free(channelmap);
 free_tinfo(tinfo); /* Free everything from the old epoch */

 localp->nr_of_averages+=tinfo->nrofaverages;
 tinfo->nrofaverages=localp->nr_of_averages;
 localp->nroffiles++;
 tinfo->leaveright=localp->tinfo.leaveright;
 return localp->tinfo.tsdata;
}
/*}}}  */

/*{{{  average_exit(transform_info_ptr tinfo)*/
METHODDEF void
average_exit(transform_info_ptr tinfo) {
 struct average_local_struct *localp=(struct average_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int nrofaverages=localp->nr_of_averages;
 int itempart, shift, nrofshifts=(tinfo->data_type==FREQ_DATA ? tinfo->nrofshifts : 1);
 int const varstep=var_steps[localp->stat_test]+(args[ARGS_MATCHBYNAME].is_set ? matchbyname_addsteps[localp->stat_test] : 0);
 array myarray;

 if (nrofaverages>0) {
  int const comment_len=20+(localp->tinfo.comment!=NULL ? strlen(localp->tinfo.comment) : 0);
  char *const new_comment=(char *)malloc(comment_len);
  if (new_comment==NULL) {
   ERREXIT(tinfo->emethods, "average_exit: Error allocating new comment\n");
  }
  snprintf(new_comment, comment_len, "%s; %d averages", (localp->tinfo.comment!=NULL ? localp->tinfo.comment : ""), localp->nroffiles);
  localp->tinfo.comment=new_comment;

  memcpy(tinfo, &localp->tinfo, sizeof(struct transform_info_struct));
  tinfo->nrofaverages=(args[ARGS_OUTPUT_NROFFILES].is_set ? localp->nroffiles : nrofaverages);

  /*{{{  Postprocess: Divide data by nrofaverages, calculate probability*/
  for (shift=0; shift<nrofshifts; shift++) {
   tinfo_array_setshift(tinfo, &myarray, shift);
   for (itempart=0; itempart<localp->original_itemsize; itempart++) {
    array_use_item(&myarray, itempart*varstep);
    do {
     do {
      DATATYPE *const element=ARRAY_ELEMENT(&myarray);
      if (itempart<localp->original_itemsize-localp->original_leaveright) element[0]/=localp->channelweights[myarray.current_vector];
      if (args[ARGS_TTEST].is_set && !args[ARGS_LEAVE_TPARAMETERS].is_set) {
       long const channelaverages=localp->channelaverages[myarray.current_vector];
       DATATYPE const datatemp=element[1];
       element[1]=datatemp/(channelaverages*sqrt((element[2]-datatemp*datatemp/channelaverages)/(channelaverages*(channelaverages-1))));
       element[2]=student_p(channelaverages-1, fabs(element[1]));
      }
      if (args[ARGS_MATCHBYNAME].is_set && matchbyname_addsteps[localp->stat_test]!=0) {
       element[varstep-1]=(args[ARGS_OUTPUT_NROFFILES].is_set ? localp->channelaverages[myarray.current_vector] : localp->channelweights[myarray.current_vector]);
      }
      array_advance(&myarray);
     } while (myarray.message==ARRAY_CONTINUE);
    } while (myarray.message==ARRAY_ENDOFVECTOR);
   }
  }
  /*}}}  */
  tinfo->z_value/=nrofaverages;
 }

 free_pointer((void **)&localp->channelaverages);
 free_pointer((void **)&localp->channelweights);
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_average(transform_info_ptr tinfo)*/
GLOBAL void
select_average(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &average_init;
 tinfo->methods->transform= &average;
 tinfo->methods->transform_exit= &average_exit;
 tinfo->methods->method_type=COLLECT_METHOD;
 tinfo->methods->method_name="average";
 tinfo->methods->method_description=
  "Collect method to average all incoming epochs (which should, of course,\n"
  " have equal types and sizes)\n";
 tinfo->methods->local_storage_size=sizeof(struct average_local_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
