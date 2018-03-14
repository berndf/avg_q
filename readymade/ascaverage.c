/*
 * Copyright (C) 1996-1998,2000,2001,2003,2004 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * ascaverage reads data sets from files using the readasc method
 * and outputs the result to stdout.	Bernd Feige 19.02.1993
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "stat.h"

extern char *optarg;
extern int optind, opterr, optopt;
/*}}}  */

#define MAX_FILENAME_LENGTH 1024
#define MAX_METHODARG_LENGTH 1024
#define MAX_COMMENT_LENGTH 1024

/*{{{  Global variables*/
char datacomment[MAX_COMMENT_LENGTH];

struct transform_info_struct firsttinfo, secondtinfo;
transform_info_ptr tinfoptr;
struct transform_methods_struct method;
struct external_methods_struct emethod;
/*}}}  */

/*{{{  readall(transform_info_ptr tinfo, char *filename)*/
LOCAL int
readall(transform_info_ptr start_tinfo, char *filename) {
 transform_info_ptr tinfo, tinfo_next;
 growing_buf args;
 char readasc_args[MAX_METHODARG_LENGTH];
 int epochs_loaded=0;

 tinfo=start_tinfo;
 snprintf(readasc_args, MAX_METHODARG_LENGTH, "%s", filename);
 growing_buf_settothis(&args, readasc_args);
 setup_method(tinfo, &args);
 (*method.transform_init)(tinfo);
 while ((*method.transform)(tinfo)!=NULL) {
  epochs_loaded++;
  nonmultiplexed(tinfo);
  if ((tinfo_next=calloc(1,sizeof(struct transform_info_struct)))==NULL) {
   ERREXIT(&emethod, "ascaverage: Error malloc'ing tinfo struct\n");
  }
  tinfo_next->methods= &method;
  tinfo_next->emethods= &emethod;
  tinfo_next->previous=tinfo;
  tinfo_next->next=NULL;
  tinfo->next=tinfo_next;
  tinfo=tinfo_next;
 }
 (*method.transform_exit)(start_tinfo);
 /* The last tinfo structure was allocated in vein, throw away */
 tinfo=tinfo->previous;
 free(tinfo->next);
 tinfo->next=NULL;
 return epochs_loaded;
}
/*}}}  */

enum ttest_options {
 /* CALC_DIFF_TTEST: Calculate the difference for the paired t-test myself
  *		     (against baseline).
  * DIFF_TTEST: Use the values as they appear in the file (assume the `pairing'
  *		has been done already)
  */
 NO_TTEST, CALC_DIFF_TTEST, DIFF_TTEST
};

enum output_nrofavg_options {
 OUTPUT_NROFFILES=0, 
 OUTPUT_NROFAVERAGES, 
 OUTPUT_SUM_WEIGHTS,
};
LOCAL char *output_nrofavg_names[]={
 "Nr_of_files", 
 "Nr_of_averages", 
 "Sum_weights",
};

int main(int argc, char *argv[]) {
 int i, c, nroffiles=argc-1;
 int channel, point, itempart, newdatasets;
 int channels, points, itemsize, datasets=0, offset, dataset_size, itemstep=1;
 int errflag=0, sumsquared=FALSE, sum_only= -1, leave_testparameters=FALSE;
 int nr_of_averages=0, use_nrofavg_as_weight=FALSE, binary_output=FALSE;
 enum output_nrofavg_options output_nrofavg_is=OUTPUT_SUM_WEIGHTS;
 enum ttest_options ttest_option=NO_TTEST;
 char nextfname[MAX_FILENAME_LENGTH], outfname[MAX_FILENAME_LENGTH]="stdout";
 FILE *weightfile=NULL;
 double (*statfun)(double)=NULL;
 DATATYPE datatemp, *avg_buf, *out_tsdata, weight, sum_weights=0;
 growing_buf args;
 char writeasc_args[MAX_METHODARG_LENGTH];

 /*{{{  Process options*/
 while ((c=getopt(argc, argv, "BdDNstTuWo:S:w:"))!=EOF) {
  nroffiles--;
  switch (c) {
   case 's':
    sumsquared=TRUE;
    break;
   case 'T':
    statfun= &log;
   case 't':
    ttest_option=CALC_DIFF_TTEST;
    break;
   case 'D':
    statfun= &log;
   case 'd':
    ttest_option=DIFF_TTEST;
    break;
   case 'u':
    leave_testparameters=TRUE;
    break;
   case 'B':
    binary_output=TRUE;
    break;
   case 'S':
    nroffiles--;
    sum_only=atoi(optarg);
    break;
   case 'w':
    nroffiles--;
    if ((weightfile=fopen(optarg, "r"))==NULL) {
     fprintf(stderr, "%s: Can't open weightfile %s\n", argv[0], optarg);
     exit(2);
    }
    break;
   case 'W':
    use_nrofavg_as_weight=TRUE;
    break;
   case 'N':
    output_nrofavg_is=OUTPUT_NROFFILES;
    break;
   case 'o':
    nroffiles--;
    strcpy(outfname, optarg);
    break;
   case '?':
   default:
    errflag++;
    continue;
  }
 }
 if (errflag>0 || (weightfile==NULL && nroffiles<1) || (weightfile!=NULL && nroffiles>0)) {
  fprintf(stderr, "Usage: %s ascdatafile1 ascdatafile2 ...\n"
    "Options:\n"
    " -B writes output in binary format\n"
    " -s outputs the square root of the averaged squares\n"
    " -t performs a t-test against baseline for each value\n"
    " -T does the same with the log values\n"
    " -d interprets each value itself as a difference and performs a t-test\n"
    " -D does the same with the log values\n"
    " -u outputs the accumulated parameters for the t test instead of t and p\n"
    " -N forces the output Nr_of_averages property to be the number of files;\n"
    "    useful with -u, since these are the correct degrees of freedom for the test.\n"
    " -o outfile: Write the result to file outfile rather than to stdout\n"
    " -S sum_only: Leaves the sum_only rightmost values as sums, don't form average\n"
    " -w filename: reads file names to process and associated weights from file\n"
    " -W uses the Nr_of_averages property of the incoming files as weights\n"
    , argv[0]);
  exit(1);
 }
 if (leave_testparameters && ttest_option==NO_TTEST) {
  fprintf(stderr,
   "To leave the test parameters without transforming them into t and p values,\n"
   "we need to do some test first (option -[tTdD]), don't we?\n");
  exit(1);
 }
 /*}}}  */

 clear_external_methods(&emethod);
 secondtinfo.methods= &method;
 secondtinfo.emethods= &emethod;
 select_readasc(&secondtinfo);
 trafo_std_defaults(&secondtinfo);

 for (;;) {
  /*{{{  Decide about the next file name and weight*/
  if (weightfile==NULL) {
   if (optind>=argc) break;
   strcpy(nextfname, argv[optind++]);
   weight=1;	/* This can be changed by the -W option, after the file is read */
  } else {
   fscanf(weightfile, "%s %lf", nextfname, &weight);
   if (feof(weightfile) || weight==0.0) break;
   nroffiles++;
   /* Output Nr_of_averages property will be sum_weights only if each of
    * the input weights is integer */
   if (weight-(int)weight!=0) output_nrofavg_is=OUTPUT_NROFAVERAGES;
  }
  /*}}}  */
  /*{{{  Add the next file*/
  newdatasets=readall(&secondtinfo, nextfname);
  if (datasets==0) {
   /*{{{  First file: set parameters/allocate memory*/
   datasets=newdatasets;
   itemsize=secondtinfo.itemsize;
   if (sum_only== -1) {
    /* Don't touch a sum_only value specified with -S */
    sum_only=secondtinfo.leaveright;
    if (sum_only>0) fprintf(stderr, "ascaverage: sum_only automatically set to %d\n", sum_only);
   }
   points=secondtinfo.nr_of_points;
   channels=secondtinfo.nr_of_channels;

   firsttinfo=secondtinfo;
   if (ttest_option!=NO_TTEST) {
    if (ttest_option==CALC_DIFF_TTEST) {
     /*{{{  Allocate avg_buf memory*/
     if ((avg_buf=malloc(channels*points*itemsize*sizeof(DATATYPE)))==NULL) {
      ERREXIT(&emethod, "ascaverage: Error allocating avg_buf memory\n");
     }
     /*}}}  */
    }
    itemstep=3;
    firsttinfo.itemsize*=itemstep;
   }
   dataset_size=points*channels*firsttinfo.itemsize;
   if ((out_tsdata=calloc(datasets*dataset_size, sizeof(DATATYPE)))==NULL) {
    ERREXIT(&emethod, "ascaverage: Error allocating memory\n");
   }
   /*}}}  */
  } else if (newdatasets!=datasets) {
   ERREXIT2(&emethod, "ascaverage: Not %d data sets in file %s\n", MSGPARM(datasets), MSGPARM(nextfname));
  }
  nr_of_averages+=(secondtinfo.nrofaverages>0 ? secondtinfo.nrofaverages : 1);
  if (use_nrofavg_as_weight) {
   weight=secondtinfo.nrofaverages;	/* Beware, no check for 0 */
  }
  sum_weights+=weight;
  fprintf(stderr, "ascaverage: Successfully read %s, weight %g (Nr_of_averages=%d)\n", nextfname, weight, secondtinfo.nrofaverages);

  if (ttest_option==CALC_DIFF_TTEST) {
   /*{{{  Average baseline*/
   for (channel=0; channel<channels; channel++) {
    for (point=0; point<points; point++) {
     for (itempart=0; itempart<itemsize; itempart++) {
      offset=(channel*points+point)*itemsize+itempart;
      avg_buf[offset]=0.0;
      for (i=0, tinfoptr= &secondtinfo; tinfoptr!=NULL && tinfoptr->z_value<=0; tinfoptr=tinfoptr->next, i++) {
       datatemp=tinfoptr->tsdata[offset];
       if (statfun!=NULL) datatemp=(*statfun)(datatemp);
       avg_buf[offset]+=datatemp;
      }
      avg_buf[offset]/=i;
     }
    }
   }
   /*}}}  */
  }

  for (tinfoptr= &secondtinfo, firsttinfo.tsdata=out_tsdata, i=1;
  	tinfoptr!=NULL;
        firsttinfo.tsdata+=dataset_size, i++) {
   transform_info_ptr const save_tinfoptr=tinfoptr;
   
   if (tinfoptr->nr_of_channels!=channels) {
    ERREXIT2(&emethod, "ascaverage: Not the same number of channels in dataset %d in file %s\n", MSGPARM(i), MSGPARM(nextfname));
   }
   if (tinfoptr->nr_of_points!=points) {
    ERREXIT2(&emethod, "ascaverage: Not the same number of points in dataset %d in file %s\n", MSGPARM(i), MSGPARM(nextfname));
   }
   if (tinfoptr->itemsize!=itemsize) {
    ERREXIT2(&emethod, "ascaverage: Not the same itemsize in dataset %d in file %s\n", MSGPARM(i), MSGPARM(nextfname));
   }
   for (channel=0; channel<channels; channel++) {
    for (point=0; point<points; point++) {
     offset=channel*points+point;
     for (itempart=0; itempart<itemsize-sum_only; itempart++) {
      datatemp=tinfoptr->tsdata[offset*itemsize+itempart];
      if (sumsquared) datatemp*=datatemp;
      firsttinfo.tsdata[offset*firsttinfo.itemsize+itempart*itemstep]+=weight*datatemp;

      if (ttest_option!=NO_TTEST) {
       datatemp=tinfoptr->tsdata[offset*itemsize+itempart];
       if (statfun!=NULL) datatemp=(*statfun)(datatemp);
       if (ttest_option==CALC_DIFF_TTEST) datatemp-=avg_buf[offset*itemsize+itempart];
       firsttinfo.tsdata[offset*firsttinfo.itemsize+itempart*itemstep+1]+=datatemp;
       firsttinfo.tsdata[offset*firsttinfo.itemsize+itempart*itemstep+2]+=datatemp*datatemp;
      }
     }
     /*{{{  Process the sum_only part*/
     for (; itempart<itemsize; itempart++) {
      datatemp=tinfoptr->tsdata[offset*itemsize+itempart];
      firsttinfo.tsdata[offset*firsttinfo.itemsize+itempart*itemstep]+=datatemp;
     }
     /*}}}  */
    }
   }
   tinfoptr=tinfoptr->next;
   if (optind<argc-1) {	/* Leave the last file to read its comments */
    deepfree_tinfo(save_tinfoptr);
    if (save_tinfoptr!=&secondtinfo) free(save_tinfoptr);
   }
  }
  /*}}}  */
 }
 if (weightfile!=NULL) fclose(weightfile);

 /*{{{  Postprocess and write the result*/
 i=firsttinfo.itemsize;
 firsttinfo=secondtinfo;
 firsttinfo.itemsize=i;
 fprintf(stderr, "ascaverage: %d files processed, Sum_weights=%g, Nr_of_averages=%d;\nascaverage: Output Nr_of_averages is >%s<\n", nroffiles, sum_weights, nr_of_averages, output_nrofavg_names[output_nrofavg_is]);
 switch (output_nrofavg_is) {
  case OUTPUT_NROFFILES:
   firsttinfo.nrofaverages=nroffiles;
   break;
  case OUTPUT_NROFAVERAGES:
   firsttinfo.nrofaverages=nr_of_averages;
   break;
  case OUTPUT_SUM_WEIGHTS:
   firsttinfo.nrofaverages=(int)sum_weights;
   break;
 }
 select_writeasc(&firsttinfo);
 snprintf(writeasc_args, MAX_METHODARG_LENGTH, "%s %s", (binary_output ? "-b" : ""), outfname);
 growing_buf_settothis(&args, writeasc_args);
 setup_method(&firsttinfo, &args);
 (*method.transform_init)(&firsttinfo);
 for (tinfoptr= &secondtinfo, firsttinfo.tsdata=out_tsdata;
 	tinfoptr!=NULL;
 	tinfoptr=tinfoptr->next, firsttinfo.tsdata+=dataset_size) {
  firsttinfo.z_value=tinfoptr->z_value;
  for (channel=0; channel<channels; channel++) {
   for (point=0; point<points; point++) {
    for (itempart=0; itempart<itemsize-sum_only; itempart++) {
     offset=(channel*points+point)*firsttinfo.itemsize+itempart*itemstep;

     if (ttest_option!=NO_TTEST && !leave_testparameters) {
      datatemp=firsttinfo.tsdata[offset+1];
      firsttinfo.tsdata[offset+1]/=nroffiles*sqrt((firsttinfo.tsdata[offset+2]-datatemp*datatemp/nroffiles)/(nroffiles*(nroffiles-1)));
      firsttinfo.tsdata[offset+2]=student_p(nroffiles-1, fabs(firsttinfo.tsdata[offset+1]));
     }

     if (sumsquared) {
      firsttinfo.tsdata[offset]=sqrt(firsttinfo.tsdata[offset]/sum_weights);
     } else {
      firsttinfo.tsdata[offset]/=sum_weights;
     }
    }
   }
  }
  snprintf(datacomment, MAX_COMMENT_LENGTH, "%s; %d%s averages", tinfoptr->comment, nroffiles, weightfile==NULL ? "" : " weighted");
  if (sumsquared) strcat(datacomment, " -s");
  if (ttest_option==CALC_DIFF_TTEST) strcat(datacomment, statfun!=NULL ? " -T" : " -t");
  else if (ttest_option==DIFF_TTEST) strcat(datacomment, statfun!=NULL ? " -D" : " -d");
  firsttinfo.comment=datacomment;
  (*method.transform)(&firsttinfo);
 }
 (*method.transform_exit)(&firsttinfo);
 /*}}}  */

 return 0;
}
