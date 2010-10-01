/*
 * Copyright (C) 1996-2004,2006,2008,2010 Bernd Feige
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
 * writeasc.c module to write data to an ascii file
 *	-- Bernd Feige 19.01.1993
 *
 * If (args[ARGS_OFILE].arg.s eq "stdout") tinfo->outfptr=stdout;
 * If (args[ARGS_OFILE].arg.s eq "stderr") tinfo->outfptr=stderr;
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "ascfile.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_BINARY, 
 ARGS_CLOSE,
 ARGS_LINKED, 
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Binary file format", "b", TRUE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close the file after writing each epoch (and open it again next time)", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Write all linked datasets", "L", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"}
};

/*{{{  #defines*/
#define OUTBUFSIZE (16*1024)
#define MAX_CHANNEL_LEN 1024
/*}}}  */

struct writeasc_storage {
 FILE *outfptr;
 DATATYPE sfreq;
 int beforetrig;
 int leaveright;
};

/*{{{  Local open_file and close_file routines*/
/*{{{  writeasc_open_file(transform_info_ptr tinfo) {*/
LOCAL void
writeasc_open_file(transform_info_ptr tinfo) {
 struct writeasc_storage *local_arg=(struct writeasc_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *outfptr=NULL;
 Bool append_mode=args[ARGS_APPEND].is_set;
 
 if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) outfptr=stdout;
 else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) outfptr=stderr;
 if (append_mode) {
  /*{{{  Append mode open*/
  if (outfptr==NULL) {
   if ((outfptr=fopen(args[ARGS_OFILE].arg.s, "a+b"))==NULL) {
    ERREXIT1(tinfo->emethods, "writeasc_init: Can't append to %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
   }
  }
  fseek(outfptr, 0L, 2);
  /* It is important here to overwrite the append_mode flag only for THIS open
   * operation, because writeasc_open_file will get called repeatedly if -c is
   * also set, and then the file will already be there the second time and we want
   * to append to it... */
  if (ftell(outfptr)==0L) append_mode=FALSE;	/* If at start: write header */
  /*}}}  */
 }
 if (outfptr==NULL) {
  /*{{{  Truncate mode open*/
  if ((outfptr=fopen(args[ARGS_OFILE].arg.s, "wb"))==NULL) {
   ERREXIT1(tinfo->emethods, "writeasc_init: Can't open %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
  /*}}}  */
 }
 local_arg->outfptr=outfptr;
 if (append_mode) {
  /* Don't write header in append mode.
   * Also set the local_arg values to (nearly) impossible numbers in order to
   * force these values to be assigned in the epoch header */
  local_arg->sfreq= -123456789;
  local_arg->beforetrig= -123456789;
  local_arg->leaveright= -123456789;
 } else {
 if (args[ARGS_BINARY].is_set) {
  /*{{{  Write binary header*/
  char outbuf[OUTBUFSIZE], *inoutbuf;
  unsigned short shortbuf=ASC_BF_MAGIC;
  fwrite((void *)&shortbuf, 2, 1, outfptr);
  inoutbuf=outbuf;
  snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Sfreq=%f\n", (float)tinfo->sfreq);
  inoutbuf+=strlen(inoutbuf);
  if (tinfo->beforetrig>0) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "BeforeTrig=%d\n", tinfo->beforetrig);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->leaveright>0) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Leaveright=%d\n", tinfo->leaveright);
   inoutbuf+=strlen(inoutbuf);
  }
  shortbuf=inoutbuf-outbuf;
  fwrite((void *)&shortbuf, 2, 1, outfptr);
  fwrite((void *)outbuf, (int)shortbuf, 1, outfptr);
  /*}}}  */
 } else {
  /*{{{  Write ascii header*/
  fprintf(outfptr, "Sfreq=%f\n", (float)tinfo->sfreq);
  if (tinfo->beforetrig>0) fprintf(outfptr, "BeforeTrig=%d\n", tinfo->beforetrig);
  if (tinfo->leaveright>0) fprintf(outfptr, "Leaveright=%d\n", tinfo->leaveright);
  /*}}}  */
 }
 local_arg->sfreq=tinfo->sfreq;
 local_arg->beforetrig=tinfo->beforetrig;
 local_arg->leaveright=tinfo->leaveright;
 }
}
/*}}}  */

/*{{{  writeasc_close_file(transform_info_ptr tinfo) {*/
LOCAL void
writeasc_close_file(transform_info_ptr tinfo) {
 struct writeasc_storage *local_arg=(struct writeasc_storage *)tinfo->methods->local_storage;
 if (local_arg->outfptr!=stdout && local_arg->outfptr!=stderr) {
  fclose(local_arg->outfptr);
 } else {
  fflush(local_arg->outfptr);
 }
 local_arg->outfptr=NULL;
}
/*}}}  */
/*}}}  */

/*{{{  writeasc_init(transform_info_ptr tinfo) {*/
METHODDEF void
writeasc_init(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;

 if (!args[ARGS_CLOSE].is_set) writeasc_open_file(tinfo);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  writeasc(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
writeasc(transform_info_ptr calltinfo) {
 struct writeasc_storage *local_arg=(struct writeasc_storage *)calltinfo->methods->local_storage;
 transform_argument *args=calltinfo->methods->arguments;
 int i, channel, itempart;
 long tsdata_step, tsdata_steps, tsdata_stepwidth;
 FILE *outfptr=local_arg->outfptr;
 /* Note that 'tinfo' instead of 'tinfoptr' is used here so that we don't have to modify 
  * all of the older code which stored only one epoch */
 transform_info_ptr tinfo=calltinfo;

 if (args[ARGS_CLOSE].is_set) {
  writeasc_open_file(calltinfo);
  outfptr=local_arg->outfptr;
 }
 if (args[ARGS_LINKED].is_set) {
  /* Go to the start: */
  for (; tinfo->previous!=NULL; tinfo=tinfo->previous);
 }
 for (; tinfo!=NULL; tinfo=tinfo->next) {
  DATATYPE * const orig_tsdata=tinfo->tsdata;
 if (tinfo->xdata==NULL) create_xaxis(tinfo);
 if (tinfo->data_type==FREQ_DATA) {
  tinfo->nr_of_points=tinfo->nroffreq;
  tsdata_steps=tinfo->nrofshifts;
  tsdata_stepwidth=tinfo->nr_of_channels*tinfo->nroffreq*tinfo->itemsize;
 } else {
  tsdata_steps=1;
  tsdata_stepwidth=0;
 }
 for (tsdata_step=0; tsdata_step<tsdata_steps; tinfo->tsdata+=tsdata_stepwidth, tsdata_step++) {
  if (tinfo->data_type==FREQ_DATA && tsdata_steps>1) {
   /* Multiple data sets to be written: Construct a different z_value for each. */
   if (tinfo->basetime==0.0) {
    /* Assigning the same latency to all shifts would not be optimal... */
    tinfo->z_label="Nr";
    tinfo->z_value=tsdata_step+1;
   } else {
    tinfo->z_label="Lat[ms]";
    tinfo->z_value=(tsdata_step*tinfo->basetime+(tinfo->windowsize-tinfo->beforetrig)/tinfo->sfreq)*1000;
   }
  }
 if (args[ARGS_BINARY].is_set) {
  /*{{{  Write binary*/
  char outbuf[OUTBUFSIZE], *inoutbuf;
  int len;

  fwrite((void *)&tinfo->nr_of_channels, sizeof(int), 1, outfptr);
  fwrite((void *)&tinfo->nr_of_points, sizeof(int), 1, outfptr);
  if (tinfo->itemsize<=0) tinfo->itemsize=1;
  fwrite((void *)&tinfo->itemsize, sizeof(int), 1, outfptr);
  fwrite((void *)&tinfo->multiplexed, sizeof(int), 1, outfptr);

  *outbuf=(char)0;
  inoutbuf=outbuf;
  if (tinfo->nrofaverages>0) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Nr_of_averages=%d;", tinfo->nrofaverages);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->sfreq!=local_arg->sfreq) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Sfreq=%f;", (float)tinfo->sfreq);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->beforetrig!=local_arg->beforetrig) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "BeforeTrig=%d;", tinfo->beforetrig);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->leaveright!=local_arg->leaveright) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Leaveright=%d;", tinfo->leaveright);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->condition!=0) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Condition=%d;", tinfo->condition);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->triggers.buffer_start!=NULL) {
   struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start;
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "Triggers=%ld", intrig->position);
   inoutbuf+=strlen(inoutbuf);
   intrig++;
   while (intrig->code!=0) {
    snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), ",%ld:%d", intrig->position, intrig->code);
    inoutbuf+=strlen(inoutbuf);
    intrig++;
   }
   *inoutbuf++=';';
  }
  if (tinfo->comment!=NULL) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "%s", tinfo->comment);
   inoutbuf+=strlen(inoutbuf);
  }
  if (tinfo->z_label!=NULL) {
   snprintf(inoutbuf, OUTBUFSIZE-(inoutbuf-outbuf), "%s%s=%g", ZAXIS_DELIMITER, tinfo->z_label, tinfo->z_value);
   inoutbuf+=strlen(inoutbuf);
  }
  len=inoutbuf-outbuf+1;
  len+=strlen(tinfo->xchannelname)+1;
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   len+=strlen(tinfo->channelnames[channel])+1;
  }
  fwrite((void *)&len, sizeof(int), 1, outfptr);

  fwrite((void *)outbuf, strlen(outbuf)+1, 1, outfptr);
  fwrite((void *)tinfo->xchannelname, strlen(tinfo->xchannelname)+1, 1, outfptr);
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   fwrite((void *)tinfo->channelnames[channel], strlen(tinfo->channelnames[channel])+1, 1, outfptr);
  }
  fwrite((void *)tinfo->probepos, sizeof(double), 3*tinfo->nr_of_channels, outfptr);
  fwrite((void *)tinfo->xdata, sizeof(DATATYPE), tinfo->nr_of_points, outfptr);

  fwrite((void *)tinfo->tsdata, sizeof(DATATYPE), tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize, outfptr);
  /* 6 ints: channels/points/items/multiplexed/len_of_string_section/skipback */
  len+=6*sizeof(int)+3*tinfo->nr_of_channels*sizeof(double)+tinfo->nr_of_points*(1+tinfo->nr_of_channels*tinfo->itemsize)*sizeof(DATATYPE);
  fwrite((void *)&len, sizeof(int), 1, outfptr);
  /*}}}  */
 } else {
  /*{{{  Write ascii*/
  array myarray;

  if (tinfo->nrofaverages>0) fprintf(outfptr, "Nr_of_averages=%d;", tinfo->nrofaverages);
  if (tinfo->sfreq!=local_arg->sfreq) fprintf(outfptr, "Sfreq=%f;", (float)tinfo->sfreq);
  if (tinfo->beforetrig!=local_arg->beforetrig) fprintf(outfptr, "BeforeTrig=%d;", tinfo->beforetrig);
  if (tinfo->leaveright!=local_arg->leaveright) fprintf(outfptr, "Leaveright=%d;", tinfo->leaveright);
  if (tinfo->condition!=0) fprintf(outfptr, "Condition=%d;", tinfo->condition);
  if (tinfo->triggers.buffer_start!=NULL) {
   struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start;
   fprintf(outfptr, "Triggers=%ld", intrig->position);
   intrig++;
   while (intrig->code!=0) {
    fprintf(outfptr, ",%ld:%d", intrig->position, intrig->code);
    intrig++;
   }
   fprintf(outfptr, ";");
  }
  if (tinfo->comment!=NULL) fprintf(outfptr, "%s", tinfo->comment);
  if (tinfo->z_label!=NULL) fprintf(outfptr, "%s%s=%g", ZAXIS_DELIMITER, tinfo->z_label, tinfo->z_value);
  fprintf(outfptr,"\nchannels=%d, points=%d", tinfo->nr_of_channels, tinfo->nr_of_points);
  if (tinfo->itemsize<=1) {
   tinfo->itemsize=1;	/* Make sure the itemsize is at least 1 */
  } else {
   fprintf(outfptr, ", itemsize=%d", tinfo->itemsize);
  }
  fprintf(outfptr, "\n%s", tinfo->xchannelname);
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   char buffer[MAX_CHANNEL_LEN+1], *inbuf;
   strncpy(buffer, tinfo->channelnames[channel], MAX_CHANNEL_LEN);
   buffer[MAX_CHANNEL_LEN+1]='\0';
   /* Some characters can become hazardous, so we change them... */
   for (inbuf=buffer; *inbuf!='\0'; inbuf++) {
    if (strchr(" \t\n", *inbuf)!=NULL) *inbuf='_';
   }
   fprintf(outfptr, "\t%s", buffer);
  }
  for (i=0; i<3; i++) {
   fprintf(outfptr, "\n-");
   for (channel=0; channel<tinfo->nr_of_channels; channel++) {
    fprintf(outfptr, "\t%g", tinfo->probepos[channel*3+i]);
   }
  }
  fprintf(outfptr, "\n");

  tinfo_array(tinfo, &myarray);
  array_transpose(&myarray); /* Outer loop is for points */
  i=0;
  do {
   fprintf(outfptr, "%g", tinfo->xdata[i]);
   do {
    if (tinfo->itemsize==1) {
     fprintf(outfptr, "\t%g", array_scan(&myarray));
    } else {
     DATATYPE *itemptr=ARRAY_ELEMENT(&myarray);
     fprintf(outfptr, "\t(%g", *itemptr++);
     for (itempart=1; itempart<tinfo->itemsize; itempart++) {
      fprintf(outfptr, "\t%g", *itemptr++);
     }
     fprintf(outfptr, ")");
     array_advance(&myarray);
    }
   } while (myarray.message==ARRAY_CONTINUE);
   fprintf(outfptr, "\n");
   i++;
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  /*}}}  */
 }
 } /* FREQ_DATA shifts loop */
 tinfo->tsdata=orig_tsdata;
  if (!args[ARGS_LINKED].is_set) {
   break;
  }
 } /* Linked epochs loop */
 if (args[ARGS_CLOSE].is_set) writeasc_close_file(tinfo);
 return calltinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  writeasc_exit(transform_info_ptr tinfo) {*/
METHODDEF void
writeasc_exit(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;

 if (!args[ARGS_CLOSE].is_set) writeasc_close_file(tinfo);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_writeasc(transform_info_ptr tinfo) {*/
GLOBAL void
select_writeasc(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &writeasc_init;
 tinfo->methods->transform= &writeasc;
 tinfo->methods->transform_exit= &writeasc_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="writeasc";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to an ascii (asc) file.\n";
 tinfo->methods->local_storage_size=sizeof(struct writeasc_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
