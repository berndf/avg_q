/*
 * Copyright (C) 1996-1999,2001,2004,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * extspec C program to extract values for a single channel from an 'asc'
 * file in a format suitable to feed, eg, into gnuplot.
 * Column 1: x value; Column 2: z value; Columns 3..itemsize+2: itemparts
 *					-- Bernd Feige 11.11.1993
 */

/*{{{}}}*/
/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "transform.h"
#include "bf.h"
#include "array.h"
#include "stat.h"
/*}}}  */

struct transform_info_struct tinfostruc;
struct transform_methods_struct method;
struct external_methods_struct emethod;

int
main(int argc, char *argv[]) {
 char *channelname, *filename, **inarg=argv+1;
 int argcount=argc-1, errcount=0, channel, point, itempart;
 int xrange_select=FALSE, zrange_select=FALSE, binomial=FALSE, allchannels=FALSE;
 DATATYPE minx, maxx, minz, maxz, *item_address;
 transform_info_ptr tinfo;
 array point_array;
 growing_buf args;
#define READASC_ARGS_SIZE 1024
 char readasc_args[READASC_ARGS_SIZE];

 /*{{{  Process arguments*/
 tinfo= &tinfostruc;

 while (*inarg!=NULL && **inarg=='-') {
  if (strcmp(*inarg, "-binomial")==0) {
   binomial=TRUE;
  } else if (strcmp(*inarg, "-zrange")==0) {
   if (argcount>=3) {
    inarg++; minz=atof(*inarg);
    inarg++; maxz=atof(*inarg);
    zrange_select=TRUE;
    argcount-=2;
   } else {
    fprintf(stderr, "%s: Not enough arguments for -zrange\n", argv[0]);
    errcount++;
   }
  } else {
   fprintf(stderr, "%s: Unknown option %s\n", argv[0], *inarg);
   errcount++;
  }
  inarg++; argcount--;
 }

 if ((argcount!=2 && argcount!=4) || errcount>0) {
  fprintf(stderr, "Usage: %s [options] ascfile channelname [minx maxx]\n"
    "\t`ALL' can be used as a special `channel name'. - Options:\n"
    "\t-binomial: Calculate probability from binomial distribution\n"
    "\t-zrange minz maxz: Specify the z range to output\n"
    , argv[0]);
  exit(1);
 }

 filename=inarg[0];
 channelname=inarg[1];
 allchannels=(strcmp(channelname, "ALL")==0);
 if (argcount==4) {
  xrange_select=TRUE;
  minx=atof(inarg[2]);
  maxx=atof(inarg[3]);
 }
 /*}}}  */

 clear_external_methods(&emethod);
 tinfostruc.methods= &method;
 tinfostruc.emethods= &emethod;

 select_readasc(tinfo);
 trafo_std_defaults(tinfo);
 snprintf(readasc_args, READASC_ARGS_SIZE, "%s", filename);
 growing_buf_settothis(&args, readasc_args);
 setup_method(tinfo, &args);
 (*method.transform_init)(tinfo);

 while ((*method.transform)(tinfo)!=NULL) {
  if (!zrange_select || (tinfo->z_value>=minz && tinfo->z_value<=maxz)) {
  /*{{{  Output one data block*/
  channel=0;
  if (!allchannels) {
   /*{{{  Find channel number*/
   for (; channel<tinfo->nr_of_channels; channel++) {
    if (strcmp(channelname, tinfo->channelnames[channel])==0) break;
   }
   if (channel==tinfo->nr_of_channels) {
    ERREXIT1(&emethod, "extspec: Can't find channel name %s\n", MSGPARM(channelname));
   }
   /*}}}  */
  }
  tinfo_array(tinfo, &point_array);

  do {
   if (allchannels && tinfo->nr_of_channels>1) printf("%s:\n", tinfo->channelnames[channel]);
  /*{{{  Output one channel*/
  array_reset(&point_array);
  point_array.current_vector=channel;
  for (point=0; point<tinfo->nr_of_points; point++) {
   if (xrange_select && (tinfo->xdata[point]<minx || tinfo->xdata[point]>maxx)) continue;
   printf("%g %g", tinfo->xdata[point], tinfo->z_value);
   point_array.current_element=point;
   item_address=ARRAY_ELEMENT(&point_array);
   for (itempart=0; itempart<tinfo->itemsize; itempart++) {
    printf(" %.10g", item_address[itempart]);
   }
   if (binomial) {
    /*{{{  Output the probability given that [1] and [2] are +,- counts*/
    const DATATYPE k1=item_address[1], k2=item_address[2];
    double p, log10p;

    /* p is the probability for max(k1, k2) of the two counts k1, k2 to deviate
     * from mean(k1, k2) by this many or more within n events if both mutually
     * exclusive cases counted are equally probable. The betai term gives the
     * probability that k>=k1 (cumulative binomial probability, NR p.229) */
    if (k1>k2) {
     p=2.0*betai(k1, k2+1.0, 0.5);
     if (p<FLT_MIN) log10p= -log10(FLT_MIN);	/* Cutoff at p<FLT_MIN */
     else            log10p= -log10(p);		/* Positive */
    } else if (k1<k2) {
     p=2.0*betai(k2, k1+1.0, 0.5);
     if (p<FLT_MIN) log10p=  log10(FLT_MIN);	/* Cutoff at p<FLT_MIN */
     else	     log10p=  log10(p);		/* Negative */
    } else {
     p=1.0; log10p=0.0;
    }
    printf(" %.10g %.10g %.10g", p, log10p, (k1-k2)/(float)(k1+k2));
    /*}}}  */
   }
   printf("\n");
  }
  printf("\n");
  /*}}}  */
  } while (allchannels && ++channel<tinfo->nr_of_channels);
  /*}}}  */
  }
  free_tinfo(tinfo);
 }
 (*method.transform_exit)(tinfo);

 return 0;
}
