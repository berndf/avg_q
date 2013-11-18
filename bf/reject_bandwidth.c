/*
 * Copyright (C) 1996-1999,2001,2003,2004 Bernd Feige
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
 * reject_bandwidth is a rejection method to include into an average()
 * process chain (usually just after the get_epoch method) to reject epochs
 * with a bandwidth (max value-min value) exceeding the specification for
 * any given channel. The default is 3500e-15 (3500 fT), following the
 * default used by Scott Hampson on the BTi system, for all channels unless
 * explicitly specified (see below).
 * If tinfo->reject_arg is not NULL, reject_bandwidth_init first tries to
 * interpret it as a number, which is then taken as the new bandwidth limit
 * default in T. If there are additional characters (possibly starting with
 * a colon ':'), they are interpreted as the name of a text file with
 *  channel_number	bandwidth_limit
 *  ...
 * where channel_number is the number of the channel IN THE CURRENT SELECTION
 * starting with 1.
 * If the first character in a line is a hash ('#') or channel_number exceeds
 * the number of channels available, the line is ignored.
 *
 * If the leading default number is left out but a filename given, the
 * default remains at the built-in value.
 *
 * Example:
 *  args[ARGS_BANDWIDTH].arg.s=="2500e-15 basefile" && basefile=="
 *  5	3000e-15"
 * would set the bandwidth limits of all channels to 2500e-15 and the limit
 * for channel 5 to 3000e-15.
 * 						-- Bernd Feige 20.07.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_MAXONLY=0, 
 ARGS_REMOVE_CHANNELS, 
 ARGS_INVERT, 
 ARGS_BYNAME, 
 ARGS_ITEMPART, 
 ARGS_BANDWIDTH, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Maximum only. Reject where a channel exceeds a threshold", "m", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Channel mode. Remove individual channels due to the criterion", "C", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Invert. Pass only epochs that would have been rejected", "I", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Select channels by name rather than by number in the bandwidth file", "n", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: reject on this item # (>=0; default: 0)", "i", 0, NULL},
 {T_ARGS_TAKES_SENTENCE, "[bandwidth] [filename]", " ", ARGDESC_UNUSED, NULL}
};

struct reject_bandwidth_storage {
 DATATYPE *bandwidths;
};

/*{{{  reject_bandwidth_init(transform_info_ptr tinfo) {*/
METHODDEF void
reject_bandwidth_init(transform_info_ptr tinfo) {
 struct reject_bandwidth_storage *local_arg=(struct reject_bandwidth_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE bandwidth=3500.0e-15;	/* Standard bandwidth limit is 3500 fT */
 char *endptr, *filearg=NULL;
 int channel;
 int nr_of_tokens;
 growing_buf buf, tokenbuf;
 growing_buf_init(&buf);
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf, 0L);
 
 if ((local_arg->bandwidths=(DATATYPE *)malloc(sizeof(DATATYPE)*tinfo->nr_of_channels))==NULL) {
  ERREXIT(tinfo->emethods, "reject_bandwidth_init: Error allocating bandwidth array.\n");
 }
 if (args[ARGS_BANDWIDTH].is_set) {
  double b;
  growing_buf_takethis(&buf, args[ARGS_BANDWIDTH].arg.s);
  nr_of_tokens=growing_buf_count_tokens(&buf);
  if (nr_of_tokens>2) {
   ERREXIT(tinfo->emethods, "reject_bandwidth_init: Too many arguments.\n");
  }
  if (growing_buf_get_firsttoken(&buf,&tokenbuf)) {
   b=strtod(tokenbuf.buffer_start, &endptr);
   if (*endptr!='\0') {
    /* The first argument was not a number: must be a file name */
    filearg=tokenbuf.buffer_start;
    if (nr_of_tokens>1) {
     ERREXIT(tinfo->emethods, "reject_bandwidth_init: Two args, but the first is not a number!\n");
    }
   } else {
    bandwidth=b;
    if (growing_buf_get_nexttoken(&buf,&tokenbuf)) filearg=tokenbuf.buffer_start;
   }
  }
 }
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  local_arg->bandwidths[channel]=bandwidth;
 }
 if (filearg!=NULL) {
  /*{{{  Try reading bandwidth values from a bandwidth file*/
  FILE *bandwidth_file;
  growing_buf linebuf;
  char *inbuf;
  growing_buf_init(&linebuf);
  growing_buf_allocate(&linebuf, 0L);

  if ((bandwidth_file=fopen(filearg, "r"))==NULL) {
   ERREXIT1(tinfo->emethods, "reject_bandwidth_init: Can't open file %s\n", MSGPARM(filearg));
  }
  while (TRUE) {
   growing_buf_read_line(bandwidth_file, &linebuf);
   if (feof(bandwidth_file)) break;
   if (*linebuf.buffer_start=='#') continue;	/* Allow comments */
   nr_of_tokens=growing_buf_count_tokens(&linebuf);
   if (args[ARGS_BYNAME].is_set) {
    if (nr_of_tokens!=2) {
     ERREXIT(tinfo->emethods, "reject_bandwidth_init: Invalid line in bandwidth file\n");
    }
    growing_buf_get_firsttoken(&linebuf,&tokenbuf);
    if ((channel=find_channel_number(tinfo, tokenbuf.buffer_start))<0) {
     ERREXIT1(tinfo->emethods, "reject_bandwidth_init: Unknown channel name >%s<\n", MSGPARM(tokenbuf.buffer_start));
    }
    growing_buf_get_nexttoken(&linebuf,&tokenbuf);
    inbuf=tokenbuf.buffer_start;
   } else {
    channel=(int)strtod(linebuf.buffer_start, &endptr)-1;
    if (channel<0 || endptr==linebuf.buffer_start) {
     ERREXIT(tinfo->emethods, "reject_bandwidth_init: Malformed channel number in bandwidth file\n");
    }
    inbuf=endptr;
   }
   bandwidth=strtod(inbuf, &endptr);
   if (endptr==inbuf) {
    ERREXIT(tinfo->emethods, "reject_bandwidth_init: Malformed bandwidth in bandwidth file\n");
   }
   if (channel<tinfo->nr_of_channels)
    local_arg->bandwidths[channel]=bandwidth;
  }
  fclose(bandwidth_file);
  growing_buf_free(&linebuf);
  /*}}}  */
 }
 growing_buf_free(&tokenbuf);
 growing_buf_free(&buf);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  reject_bandwidth(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
reject_bandwidth(transform_info_ptr tinfo) {
 struct reject_bandwidth_storage *local_arg=(struct reject_bandwidth_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel;
 DATATYPE item, maxitem=FLT_MAX, minitem= -FLT_MAX;
 DATATYPE *newtsdata;
 Bool should_be_rejected=FALSE;
 array myarray;
 growing_buf buf;	/* Used to store info about which channel exceeded the criteria */

 if (args[ARGS_REMOVE_CHANNELS].is_set) {
  growing_buf_init(&buf);
  growing_buf_allocate(&buf, 0);
 }

 tinfo_array(tinfo, &myarray);	/* The channels are the vectors */
 array_use_item(&myarray, args[ARGS_ITEMPART].is_set ? args[ARGS_ITEMPART].arg.i : 0);
 channel=0;
 do {
  minitem=FLT_MAX; maxitem= -FLT_MAX;
  do {
   item=array_scan(&myarray);
   if (item>maxitem) maxitem=item;
   if (item<minitem) minitem=item;
  } while (myarray.message==ARRAY_CONTINUE);
  if (args[ARGS_MAXONLY].is_set) {
   should_be_rejected=maxitem>local_arg->bandwidths[channel];
  } else {
   should_be_rejected=maxitem-minitem>local_arg->bandwidths[channel];
  }
  if (should_be_rejected) {
   if (args[ARGS_REMOVE_CHANNELS].is_set) {
    growing_buf_append(&buf, (char *)&channel, sizeof(int));
   } else {
    break;
   }
  }
  channel++;
 } while (myarray.message==ARRAY_ENDOFVECTOR);

 newtsdata=tinfo->tsdata;

 if (args[ARGS_REMOVE_CHANNELS].is_set) {
  int const channels_to_reject=buf.current_length/sizeof(int);

  if ((!args[ARGS_INVERT].is_set && channels_to_reject==tinfo->nr_of_channels)
   || ( args[ARGS_INVERT].is_set && channels_to_reject==0)) {
   /* No channels to keep: reject complete epoch */
   TRACEMS(tinfo->emethods, 1, "reject_bandwidth -C: Had to reject the whole epoch!\n");
   newtsdata=NULL;
  } else if ((!args[ARGS_INVERT].is_set && channels_to_reject==0)
          || ( args[ARGS_INVERT].is_set && channels_to_reject==tinfo->nr_of_channels)) {
   /* All channels to keep: No need to copy the data... */
  } else {
   char **new_channelnames, *in_channelnames;
   int in_buffer, stringlen, new_channel;
   double *new_probepos;
   array newarray;

   /* By making the last value an impossible channel number, we avoid having
    * to test for in_buffer>=channels_to_reject below */
   channel= -1;
   growing_buf_append(&buf, (char *)&channel, sizeof(int));

   /*{{{  Allocate data*/
   for (stringlen=0, channel=0, in_buffer=0, new_channel=0; channel<tinfo->nr_of_channels; channel++) {
    Bool const take_this_channel=(channel!= ((int *)buf.buffer_start)[in_buffer]);
    if (take_this_channel ^ args[ARGS_INVERT].is_set) {
     stringlen+=strlen(tinfo->channelnames[channel])+1;
     new_channel++;
    }
    if (!take_this_channel) {
     in_buffer++;
    }
   }
   newarray.nr_of_vectors=new_channel;
   newarray.nr_of_elements=tinfo->nr_of_points;
   newarray.element_skip=tinfo->itemsize;
   if (array_allocate(&newarray)==NULL || 
       (new_channelnames=(char **)malloc(newarray.nr_of_vectors*sizeof(char *)))==NULL ||
       (in_channelnames=(char *)malloc(stringlen))==NULL ||
       (new_probepos=(double *)malloc(newarray.nr_of_vectors*3*sizeof(double)))==NULL) {
    ERREXIT(tinfo->emethods, "reject_bandwidth: Error allocating memory for new epoch.\n");
   }
   /*}}}  */

   /*{{{  Transfer channel names and probe positions*/
   for (channel=0, in_buffer=0, new_channel=0; channel<tinfo->nr_of_channels; channel++) {
    int i;
    Bool const take_this_channel=(channel!=((int *)buf.buffer_start)[in_buffer]);
    if (take_this_channel ^ args[ARGS_INVERT].is_set) {
     strcpy(in_channelnames, tinfo->channelnames[channel]);
     new_channelnames[new_channel]=in_channelnames;
     in_channelnames+=strlen(in_channelnames)+1;
     for (i=0; i<3; i++) {
      new_probepos[3*new_channel+i]=tinfo->probepos[3*channel+i];
     }
     new_channel++;
    }
    if (!take_this_channel) {
     in_buffer++;
    }
   }
   /*}}}  */

   /*{{{  Transfer the data*/
   array_reset(&myarray);
   for (item=0; item<tinfo->itemsize; item++) {
    array_use_item(&myarray, (int)item);
    array_use_item(&newarray, (int)item);

    for (channel=0, in_buffer=0; channel<tinfo->nr_of_channels; channel++) {
     Bool const take_this_channel=(channel!= ((int *)buf.buffer_start)[in_buffer]);
     if (take_this_channel ^ args[ARGS_INVERT].is_set) {
      myarray.current_vector= channel;
      do {
       array_write(&newarray, array_scan(&myarray));
      } while (myarray.message==ARRAY_CONTINUE);
     }
     if (!take_this_channel) {
      in_buffer++;
     }
    }
   }
   /*}}}  */

   /*{{{  Free old data and write the new to *tinfo*/
   free_pointer((void **)&tinfo->probepos);
   free_pointer((void **)&tinfo->channelnames[0]);
   free_pointer((void **)&tinfo->channelnames);
   tinfo->probepos=new_probepos;
   tinfo->channelnames=new_channelnames;
   tinfo->nr_of_channels=newarray.nr_of_vectors;
   tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tinfo->itemsize;
   tinfo->multiplexed=FALSE;
   /*}}}  */
   newtsdata=newarray.start;
  }
  growing_buf_free(&buf);
 } else {
 /* The xor operation inverts the value of should_be_rejected if invert is set */
 if (should_be_rejected ^ args[ARGS_INVERT].is_set) {
#define BUFFER_SIZE 128
  char buffer[BUFFER_SIZE];
  int const epochno=tinfo->accepted_epochs+tinfo->rejected_epochs+1;
  if (args[ARGS_INVERT].is_set) {
   /* If the epoch would have been accepted, no specific channel is at fault.
    * We also cannot output reasonable min/max values, because they must be
    * calculated on the basis of single channels for the bandwidth. */
   snprintf(buffer, BUFFER_SIZE, "reject_bandwidth: inv rejected at %d\n", epochno);
  } else {
   snprintf(buffer, BUFFER_SIZE, "reject_bandwidth: rejected at %d on channel %s, min=%g, max=%g\n", epochno, tinfo->channelnames[channel], minitem, maxitem);
  }
  TRACEMS(tinfo->emethods, 1, buffer);

  newtsdata=NULL;
 }
 }

 return newtsdata;
}
/*}}}  */

/*{{{  reject_bandwidth_exit(transform_info_ptr tinfo) {*/
METHODDEF void
reject_bandwidth_exit(transform_info_ptr tinfo) {
 struct reject_bandwidth_storage *local_arg=(struct reject_bandwidth_storage *)tinfo->methods->local_storage;
 free(local_arg->bandwidths);
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_reject_bandwidth(transform_info_ptr tinfo) {*/
GLOBAL void
select_reject_bandwidth(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &reject_bandwidth_init;
 tinfo->methods->transform= &reject_bandwidth;
 tinfo->methods->transform_exit= &reject_bandwidth_exit;
 tinfo->methods->method_type=REJECT_METHOD;
 tinfo->methods->method_name="reject_bandwidth";
 tinfo->methods->method_description=
  "Rejection method to reject any epoch with a bandwidth (max-min value) of\n"
  "any channel exceeding the specified value(s).\n"
  "If a bandwidth value is given, it is used as default for all channels.\n"
  "If not specified, it is 3500e-15 (3500 fT).\n"
  "If a filename is given, explicit assignments of bandwidths for a number of\n"
  "channels of the form `channel_number bandwidth_limit<NEWLINE>...' are read\n"
  "from the file and override the default for these channels.\n";
 tinfo->methods->local_storage_size=sizeof(struct reject_bandwidth_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
