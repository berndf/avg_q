/*
 * Copyright (C) 1996-1999,2003,2004,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * swap_fc is a simple transform method swapping frequencies for channels
 * in frequency data with a single channel. nr_of_points will be the former
 * nrofshifts and, of course, nr_of_channels will be nroffreq.
 * 						-- Bernd Feige 28.01.1996
 * FREQ_DATA is completely nonmultiplexed, ie Channels-Shifts-Frequencies
 * slower from right to left. For a single channel, we can have the frequencies
 * as channels and the output will be multiplexed.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

struct swap_fc_storage {
 int current_epoch;
};

/*{{{  swap_fc_init(transform_info_ptr tinfo) {*/
METHODDEF void
swap_fc_init(transform_info_ptr tinfo) {
 struct swap_fc_storage *local_arg=(struct swap_fc_storage *)tinfo->methods->local_storage;

 local_arg->current_epoch=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  swap_fc(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
swap_fc(transform_info_ptr tinfo) {
 struct swap_fc_storage *local_arg=(struct swap_fc_storage *)tinfo->methods->local_storage;
#define BUFFER_SIZE 80
 char *in_channelnames, buffer[BUFFER_SIZE];
 char *in_xchannelname, *in_buffer, labbuf[BUFFER_SIZE];
 int channel, stringlen;

 if (tinfo->nr_of_channels!=1) {
  if (tinfo->data_type!=FREQ_DATA && tinfo->nr_of_points==1) {
   char **new_channelnames;
   const char *channelname="swapped", *xchannelname="Freq[Hz]";
   const char *z_label="Time[s]";
   /* Swap `swapped' data back to spectra */
   if ((new_channelnames=(char **)malloc(1*sizeof(char *)))==NULL ||
       (in_channelnames=(char *)malloc(strlen(channelname)+1))==NULL ||
       (tinfo->z_label=(char *)malloc(strlen(z_label)+1))==NULL ||
       (tinfo->xdata=(DATATYPE *)malloc(tinfo->nr_of_channels*sizeof(DATATYPE)))==NULL ||
       (tinfo->xchannelname=(char *)malloc(strlen(xchannelname)+1))==NULL) {
    ERREXIT(tinfo->emethods, "swap_fc (back): Error allocating memory\n");
   }
   for (channel=0; channel<tinfo->nr_of_channels; channel++) {
    char *endptr;
    tinfo->xdata[channel]=strtod(tinfo->channelnames[channel], &endptr);
    if (endptr==tinfo->channelnames[channel]) tinfo->xdata[channel]=channel;
   }
   strcpy(tinfo->z_label, z_label);
   /* The z value is evaluated as time in s given that epochs are successive
    * sampling points: */
   tinfo->z_value=local_arg->current_epoch/tinfo->sfreq;
   strcpy(in_channelnames, channelname);
   strcpy(tinfo->xchannelname, xchannelname);
   new_channelnames[0]=in_channelnames;
   tinfo->channelnames=new_channelnames;
   tinfo->nr_of_points=tinfo->nr_of_channels;
   tinfo->nr_of_channels=1;
   local_arg->current_epoch++;
   return tinfo->tsdata;
  } else {
   ERREXIT(tinfo->emethods, "swap_fc: Can swap either a single channel or a single point.\n");
  }
 }

 if (tinfo->data_type!=FREQ_DATA) {
  /* Create a single output point */
  tinfo->nrofshifts=1;
  tinfo->shiftwidth=1;
  tinfo->nroffreq=tinfo->nr_of_points;
  tinfo->basefreq=tinfo->sfreq/2/tinfo->nr_of_points;
 }
 
 /*{{{  Create the channel names as frequency measures*/
 if (tinfo->xchannelname!=NULL) {
  in_buffer=labbuf;
  if ((in_xchannelname=strchr(tinfo->xchannelname, '['))!=NULL) {
   for (in_xchannelname++; *in_xchannelname!='\0' && *in_xchannelname!=']' && in_buffer-labbuf<BUFFER_SIZE-1; in_xchannelname++) {
    *in_buffer++ = *in_xchannelname;
   }
  }
  *in_buffer='\0';
 } else {
  strcpy(labbuf, "Hz");
 }
 /* First count the number of bytes needed... */
 for (stringlen=0, channel=0; channel<tinfo->nroffreq; channel++) {
  DATATYPE const value=(tinfo->xdata==NULL ? channel*tinfo->basefreq : tinfo->xdata[channel]);
  snprintf(buffer, BUFFER_SIZE, "%.2f%s", value, labbuf);
  stringlen+=strlen(buffer)+1;
 }
 if ((tinfo->channelnames=(char **)malloc(tinfo->nroffreq*sizeof(char *)))==NULL ||
     (in_channelnames=(char *)malloc(stringlen))==NULL) {
  ERREXIT(tinfo->emethods, "swap_fc: Error allocating memory\n");
 }
 for (channel=0; channel<tinfo->nroffreq; channel++) {
  DATATYPE const value=(tinfo->xdata==NULL ? channel*tinfo->basefreq : tinfo->xdata[channel]);
  snprintf(buffer, BUFFER_SIZE, "%.2f%s", value, labbuf);
  tinfo->channelnames[channel]=in_channelnames;
  strcpy(in_channelnames, buffer);
  in_channelnames+=strlen(in_channelnames)+1;
 }
 /*}}}  */

 tinfo->data_type=TIME_DATA;
 if (tinfo->nrofshifts>1) {
  tinfo->sfreq/=tinfo->shiftwidth;
  tinfo->beforetrig=(int)rint((tinfo->beforetrig-tinfo->windowsize)/tinfo->shiftwidth);
 }
 tinfo->nr_of_points=tinfo->nrofshifts;
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->nr_of_channels=tinfo->nroffreq;
 tinfo->multiplexed=TRUE;

 tinfo->probepos=NULL;
 create_channelgrid(tinfo);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  swap_fc_exit(transform_info_ptr tinfo) {*/
METHODDEF void
swap_fc_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_swap_fc(transform_info_ptr tinfo) {*/
GLOBAL void
select_swap_fc(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &swap_fc_init;
 tinfo->methods->transform= &swap_fc;
 tinfo->methods->transform_exit= &swap_fc_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="swap_fc";
 tinfo->methods->method_description=
  "Transform method to swap frequencies and channels in an epoch with 1 channel;\n"
  " nrofshifts will become the new number of points in the output.\n"
  " If swap_fc is applied to single-channel time domain data, it is assumed that\n"
  " the data comes from an ASC file containing spectra and a single output point\n"
  " is generated for each incoming epoch.\n"
  " If the input epoch contains only one point but multiple channels, the reverse\n"
  " operation is attempted.\n";
 tinfo->methods->local_storage_size=sizeof(struct swap_fc_storage);
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
