/*
 * Copyright (C) 1994,1998,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * distance_correct takes a bf 'asc' file as input, which should contain
 * a N data sets with N channels of complex 'phase align' data, the first
 * data set for reference channel 1 and so on.
 * distance_correct writes an analogous 'asc' file containing the
 * real part only, corrected for simple dependance on distance from the
 * reference channel by fitting a gauss curve to all maps for a certain
 * frequency and subtracting that.
 *						-- Bernd Feige, March 94
 */

#include <stdio.h>
#include <stdlib.h>
#define _XOPEN_SOURCE
#define _ALL_SOURCE
#include <math.h>
#include "transform.h"
#include "bf.h"
#include "array.h"
#include "optimize.h"

/*{{{}}}*/
/*{{{  square(double x) {*/
LOCAL double
square(double x) {
 return x*x;
}
/*}}}  */

/*{{{  channel_distance(transform_info_ptr tinfo, int c1, int c2) {*/
LOCAL double
channel_distance(transform_info_ptr tinfo, int c1, int c2) {
 const double *pos1=tinfo->probepos+3*c1, *pos2=tinfo->probepos+3*c2;
 return sqrt(square(pos1[0]-pos2[0])+square(pos1[1]-pos2[1])+square(pos1[2]-pos2[2]));
}
/*}}}  */

/*{{{  distfun(OPT_DTYPE x, OPT_DTYPE b) {*/
struct minfunc_struct {
 int nr_of_points;
 OPT_DTYPE *points;
};

LOCAL OPT_DTYPE
distfun(OPT_DTYPE x, OPT_DTYPE b) {
 return exp(-square(x/b));
}
/*}}}  */

/*{{{  minfunc(optimize_struct *ostructp) {*/
LOCAL OPT_DTYPE
minfunc(optimize_struct *ostructp) {
 struct minfunc_struct *mfs=(struct minfunc_struct *)ostructp->fixed_args;
 int i, N=mfs->nr_of_points;
 OPT_DTYPE x= *((OPT_DTYPE *)ostructp->opt_args);
 OPT_DTYPE *points=mfs->points, sumsq=0.0;

 if (x<=0) return 1e5;
 for (i=0; i<N; i++) {
  sumsq+=square(distfun(points[2*i], x)-points[2*i+1]);
 }

 return sumsq;
}
/*}}}  */

int main(int argc, char *argv[]) {
 int i=1, start=0, end=0, freqno;
 DATATYPE *new_tsdata, hold;
 array D;
 OPT_DTYPE ax, bx, cx, fa, fb, fc, xmin, sumsq, weight, sumweight, ave, min_power, optimize_arg, *points;
 optimize_struct ostruct;
 struct minfunc_struct mfs;
 struct transform_info_struct firsttinfo;
 transform_info_ptr tinfo, tinfo_next;
 struct transform_methods_struct methods[2];
 struct external_methods_struct emethod;


 tinfo= &firsttinfo;
 /*{{{  Process command line*/
 while (*argv[1]=='-') {
  switch (argv[1][1]) {
  }
  argv++; argc--;
 }
 if (argc!=3) {
  fprintf(stderr, "Usage: %s ascdatafile outfile\n"
    , argv[0]);
  return 1;
 }

 if (start<0) start=0;
 if (end<=0) end=start;
 /*}}}  */

 clear_external_methods(&emethod);
 firsttinfo.methods= methods;
 firsttinfo.emethods= &emethod;

 /*{{{  Read all data sets*/
 select_readasc(tinfo);
 (*tinfo->methods->transform_defaults)(tinfo);
 firsttinfo.filename=argv[1];
 firsttinfo.epochs= -1; firsttinfo.fromepoch=1;
 (*tinfo->methods->transform_init)(tinfo);
 if (start!=0) {
  while (i<start) {
   (*tinfo->methods->transform)(tinfo);
   i++;
  }
 }
 while ((end==0 || i<=end) && (*tinfo->methods->transform)(tinfo)!=NULL) {
  if ((tinfo_next=malloc(sizeof(struct transform_info_struct)))==NULL) {
   ERREXIT(&emethod, "similarity: Error malloc'ing tinfo struct\n");
  }
  *(tinfo_next)=firsttinfo;
  tinfo_next->previous=tinfo;
  tinfo_next->next=NULL;
  tinfo->next=tinfo_next;
  tinfo=tinfo_next;
  i++;
 }
 /* The last tinfo structure was allocated in vein, throw away */
 tinfo=tinfo->previous;
 free(tinfo->next);
 tinfo->next=NULL;
 (*firsttinfo.methods->transform_exit)(&firsttinfo);
 /*}}}  */

 points=malloc(2*firsttinfo.nr_of_channels*sizeof(OPT_DTYPE));
 for (freqno=0; freqno<firsttinfo.nr_of_channels; freqno++) {
  /*{{{  Construct symmetrical function*/
  sumweight=ave=0.0;
  min_power=1e10;
  for (i=0, tinfo= &firsttinfo; tinfo!=NULL; tinfo=tinfo->next, i++) {
   tinfo_array(tinfo, &D);
   array_transpose(&D);
   array_reset(&D);

   D.current_vector=freqno;
   D.current_element=i;
   hold=array_readelement(&D);
   D.current_element=0;
   do {
    points[D.current_element*2]=100*channel_distance(tinfo, i, D.current_element);
    points[D.current_element*2+1]=array_scan(&D)/hold;
   } while (D.message==ARRAY_CONTINUE);
   ostruct.function= &minfunc;
   mfs.nr_of_points=firsttinfo.nr_of_channels;
   mfs.points=points;
   ostruct.fixed_args=(void *)&mfs;
   ostruct.num_opt_args=1;
   ostruct.opt_args=(void *)&optimize_arg;
   ax=0.1; bx=12;
   opt_mnbrak(&ostruct, &ax, &bx, &cx, &fa, &fb, &fc);
   sumsq=opt_brent(&ostruct, ax, bx, cx, 1e-4, &xmin);
#   ifdef VERBOSE
   printf("%d %g %g %g\n", i, xmin, sumsq, hold);
#   endif
   weight= 1.0/sumsq;
   sumweight+=weight;
   ave+=weight*xmin;
   if (hold<min_power) min_power=hold;
  }
  ave/=sumweight;
#  ifdef VERBOSE
  printf("Consensus: %g %g\n", ave, min_power);
#  endif
  /*}}}  */
  /*{{{  Subtract symmetrical function*/
  for (i=0, tinfo= &firsttinfo; tinfo!=NULL; tinfo=tinfo->next, i++) {
   tinfo_array(tinfo, &D);
   array_transpose(&D);
   array_reset(&D);
   D.current_vector=freqno;
   D.current_element=0;
   do {
    hold= min_power*distfun(100*channel_distance(tinfo, i, D.current_element), ave);
    array_write(&D, array_readelement(&D)-hold);
   } while (D.message==ARRAY_CONTINUE);
  }
  /*}}}  */
 }
 free(points);

 /*{{{  Write all data sets*/
 tinfo= &firsttinfo;

 select_extract_item(tinfo);	/* Output real part only */
 (*tinfo->methods->transform_defaults)(tinfo);
 tinfo->methods->local_storage=(void *)0;
 (*tinfo->methods->transform_init)(tinfo);
 tinfo->methods++;

 select_writeasc(tinfo);
 (*tinfo->methods->transform_defaults)(tinfo);
 tinfo->outfname=argv[2];
 tinfo->methods->local_storage=(void *)1;
 (*tinfo->methods->transform_init)(tinfo);

 for (; tinfo!=NULL; tinfo=tinfo->next) {
  tinfo->outfptr=firsttinfo.outfptr;
  tinfo->methods=methods;
  if ((new_tsdata=(*tinfo->methods->transform)(tinfo))==NULL) {
   ERREXIT(tinfo->emethods, "extract_item failed !\n");
  }
  free(tinfo->tsdata);
  tinfo->tsdata=new_tsdata;
  tinfo->methods++;
  (*tinfo->methods->transform)(tinfo);
 }

 tinfo= &firsttinfo;
 tinfo->methods=methods;
 (*tinfo->methods->transform_exit)(tinfo);
 tinfo->methods++;
 (*tinfo->methods->transform_exit)(tinfo);
 /*}}}  */

 return 0;
}
