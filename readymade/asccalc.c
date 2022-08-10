/*
 * Copyright (C) 1996,1998,2000,2001,2004,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * asccalc.c - Program to do simple calculations with data sets within an
 *  ascii input file, producing an ascii result file on stdout
 *						-- Bernd Feige 24.02.1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "transform.h"
#include "bf.h"

#define MAX_METHODARG_LENGTH 1024
#define MAX_COMMENT_LENGTH 1024

/*{{{  Global variables*/
char datacomment[MAX_COMMENT_LENGTH];

struct transform_info_struct lefttinfo, righttinfo;
struct transform_methods_struct method;
struct external_methods_struct emethod;
/*}}}  */

/*{{{}}}*/
/*{{{  readdataset(transform_info_ptr tinfo, char *filename, int which)*/
LOCAL void
readdataset(transform_info_ptr tinfo, char *filename, int which) {
 DATATYPE *readasc_out;
 growing_buf args;
 char readasc_args[MAX_METHODARG_LENGTH];

 select_readasc(tinfo);
 trafo_std_defaults(tinfo);
 snprintf(readasc_args, MAX_METHODARG_LENGTH, "-f %d -e %d %s", which, 1, filename);
 growing_buf_settothis(&args, readasc_args);
 setup_method(tinfo, &args);
 (*method.transform_init)(tinfo);
 readasc_out=(*method.transform)(tinfo);
 (*method.transform_exit)(tinfo);
 free_methodmem(tinfo);
 if (readasc_out==NULL) ERREXIT1(&emethod, "Error reading data set %d.\n", MSGPARM(which));
 nonmultiplexed(tinfo);
}
/*}}}  */

/*{{{  getchannelnr(transform_info_ptr tinfo, char *channelname)*/
LOCAL int
getchannelnr(transform_info_ptr tinfo, char *channelname) {
 int i;
 for (i=0; strcmp(tinfo->channelnames[i], channelname)!=0 && i<tinfo->nr_of_channels; i++);
 if (i==tinfo->nr_of_channels)
  ERREXIT1(&emethod, "Unknown channel name %s\n", MSGPARM(channelname));
 return i;
}
/*}}}  */

int main(int argc, char *argv[]) {
 int *channelmap;
 char *leftchannel, *rightchannel;
 long offsetl, offsetr;
 int left, right, channel, point, itempart;
 char operator;

 if (argc<3) {
  fprintf(stderr, "Usage: %s ascdatafile NxM [channel1=channel2 ...]\n"
	" where N and M are dataset numbers within the file and x is an operator [+-*/%%];\n"
        " (Note: %% denotes amplitude subtraction useful for tuple data)\n"
	" The operation takes place between channel1 of data set N and channel2 of data\n"
	" set M. The trivial channel map is valid for all channels not explicitly\n"
	" assigned.\n",
	argv[0]);
  exit(1);
 }

 clear_external_methods(&emethod);
 lefttinfo.methods=righttinfo.methods= &method;
 lefttinfo.emethods=righttinfo.emethods= &emethod;

 sscanf(argv[2], "%d%c%d", &left, &operator, &right);

 if (operator==0 && left>0) {
  /* If operator is left out, just output the corresponding data set */
  readdataset(&lefttinfo, argv[1], left);
 } else {
  /*{{{  Perform the arithmetic operation on each element pair*/
  if (left<=0 || right<=0) {
   fprintf(stderr, "asccalc: Invalid dataset number.\n");
   exit(2);
  }
  if (strchr("+-*/%", operator)==NULL) {
   fprintf(stderr, "asccalc: Unknown operator '%c'.\n", operator);
   exit(3);
  }

  readdataset(&lefttinfo, argv[1], left);
  readdataset(&righttinfo, argv[1], right);

  if (lefttinfo.nr_of_points!=righttinfo.nr_of_points || 
      lefttinfo.nr_of_channels!=righttinfo.nr_of_channels) {
   ERREXIT(&emethod, "The data sets are not the same size.\n");
  }

  if ((channelmap=malloc(sizeof(int)*lefttinfo.nr_of_channels))==NULL) {
   ERREXIT(&emethod, "Error malloc'ing the channel map list.\n");
  }
  for (channel=0; channel<lefttinfo.nr_of_channels; channel++) {
   channelmap[channel]=channel;
  }
  for (channel=3; channel<argc; channel++) {
   if ((rightchannel=strchr(argv[channel], '='))==NULL) {
    ERREXIT(&emethod, "Invalid channel equation.\n");
   }
   *rightchannel++='\0';
   leftchannel=argv[channel];
   channelmap[getchannelnr(&lefttinfo, leftchannel)]=getchannelnr(&righttinfo, rightchannel);
  }

  for (channel=0; channel<lefttinfo.nr_of_channels; channel++) {
   for (point=0; point<lefttinfo.nr_of_points; point++) {
    for (itempart=0; itempart<lefttinfo.itemsize; itempart++) {
     offsetl=(channel*lefttinfo.nr_of_points+point)*lefttinfo.itemsize+itempart;
     offsetr=(channelmap[channel]*righttinfo.nr_of_points+point)*lefttinfo.itemsize+itempart;
     switch (operator) {
      case '+':
       lefttinfo.tsdata[offsetl]+=righttinfo.tsdata[offsetr];
       break;
      case '-':
       lefttinfo.tsdata[offsetl]-=righttinfo.tsdata[offsetr];
       break;
      case '*':
       lefttinfo.tsdata[offsetl]*=righttinfo.tsdata[offsetr];
       break;
      case '/':
       lefttinfo.tsdata[offsetl]/=righttinfo.tsdata[offsetr];
       break;
      case '%':
       if (itempart>0) {
        lefttinfo.tsdata[offsetl]=0;
       } else {
        lefttinfo.tsdata[offsetl]*=lefttinfo.tsdata[offsetl];
        righttinfo.tsdata[offsetr]*=righttinfo.tsdata[offsetr];
        for (itempart=1; itempart<lefttinfo.itemsize; itempart++) {
         lefttinfo.tsdata[offsetl]+=lefttinfo.tsdata[offsetl+itempart]*lefttinfo.tsdata[offsetl+itempart];
         lefttinfo.tsdata[offsetl+itempart]=0.0;
         righttinfo.tsdata[offsetr]+=righttinfo.tsdata[offsetr+itempart]*righttinfo.tsdata[offsetr+itempart];
        }
        lefttinfo.tsdata[offsetl]=sqrt(lefttinfo.tsdata[offsetl])-sqrt(righttinfo.tsdata[offsetr]);
       }
       break;
     }
    }
   }
  }
  /*}}}  */
 }

 {
 growing_buf args;
 char writeasc_args[MAX_METHODARG_LENGTH];
 strcpy(datacomment, lefttinfo.comment);
 snprintf(datacomment+strlen(datacomment), MAX_COMMENT_LENGTH-strlen(datacomment), "; asccalc %s", argv[2]);
 lefttinfo.comment=datacomment;
 select_writeasc(&lefttinfo);
 trafo_std_defaults(&lefttinfo);
 snprintf(writeasc_args, MAX_METHODARG_LENGTH, "%s", "stdout");
 growing_buf_settothis(&args, writeasc_args);
 setup_method(&lefttinfo, &args);
 (*method.transform_init)(&lefttinfo);
 (*method.transform)(&lefttinfo);
 (*method.transform_exit)(&lefttinfo);
 }

 return 0;
}
