/*
 * Copyright (C) 1996-1999,2001,2003,2004,2007,2008,2010-2015,2018,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_rec.c method to write data to a REC format file [Kemp:92]
 *	-- Bernd Feige 18.06.1996
 *
 * If (local_arg->outfname eq "stdout") local_arg->outfptr=stdout;
 * If (local_arg->outfname eq "stderr") local_arg->outfptr=stderr;
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <Intel_compat.h>
#include <read_struct.h>
#include "transform.h"
#include "bf.h"

#include "RECfile.h"
/*}}}  */

/* This must be > any single header string field */
#define NUMBUF_LENGTH 81

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_CLOSE,
 ARGS_BITS,
 ARGS_RESOLUTION,
 ARGS_SAMPLES_PER_RECORD,
 ARGS_PATIENT,
 ARGS_RECORDING,
 ARGS_DATA_FORMAT_VERSION,
 ARGS_DATETIME,
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close the file after writing each epoch (and open it again next time)", "c", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "bits: Pretend this number of digitization bits (default: 16)", "b", 16, NULL},
 {T_ARGS_TAKES_DOUBLE, "resolution: Digitize in steps this large (default: 1.0)", "r", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "samples_per_record: Output block size (default: epoch length)", "s", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "patient: Set the `patient' field value", "P", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "recording: Set the `recording' field value", "R", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "reServed: Set the `data_format_version' field value", "S", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "Datetime: Set date and time values, format: dd.mm.yy,hh.mi.ss", "D", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.rec"}
};

#define DEFAULT_BITS 16

/*{{{  struct write_rec_storage {*/
struct write_rec_storage {
 REC_file_header fheader;

 long nr_of_records;
 long samples_per_record;
 FILE *outfptr;
 short *outbuf;
 long current_sample;
 long digmin;
 long digmax;
 double resolution;
 Bool overflow_has_occurred;
 Bool appendmode;
};
/*}}}  */

/*{{{  Local open_file and close_file routines*/
/*{{{  copy_string(char *dest, char const *source) {*/
/* Function to copy a string the REC way: without the trailing \0 */
LOCAL void
copy_nstring(char *dest, char const *source, size_t n) {
 while(*source!='\0' && n>0) {
  *dest++ = *source++;
  n--;
 }
}
/*}}}  */

/*{{{  write_rec_open_file(transform_info_ptr tinfo) {*/
LOCAL void
write_rec_open_file(transform_info_ptr tinfo) {
 struct write_rec_storage *local_arg=(struct write_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *outfptr=NULL;
 
 if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) outfptr=stdout;
 else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) outfptr=stderr;
 local_arg->appendmode=args[ARGS_APPEND].is_set;
 if (local_arg->appendmode) {
  /*{{{  Append mode open if cofile exists and is of non-zero length*/
  if (outfptr==NULL) {
   if ((outfptr=fopen(args[ARGS_OFILE].arg.s, "r+b"))!=NULL) {
    fseek(outfptr, 0L, SEEK_END);
    if (ftell(outfptr)==0L) {
     local_arg->appendmode=FALSE;	/* If at start: write header */
    }
   }
  }
  /*}}}  */
 }
 if (outfptr==NULL) {
  /*{{{  Open the REC file in truncate mode*/
  local_arg->appendmode=FALSE;	/* write header */
  if ((outfptr=fopen(args[ARGS_OFILE].arg.s, "wb"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_rec_open_file: Can't open %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
  /*}}}  */
 }
 local_arg->outfptr=outfptr;
 if (local_arg->appendmode) {
  /* Read rather than write header in append mode */
  fseek(outfptr, 0L, SEEK_SET);
  if (read_struct((char *)&local_arg->fheader, sm_REC_file, outfptr)==0) {
   ERREXIT1(tinfo->emethods, "write_rec_open_file: Can't read header in file %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
# ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->fheader, sm_REC_file);
# endif
  local_arg->nr_of_records=atoi(local_arg->fheader.nr_of_records);
  fseek(outfptr, 0L, SEEK_END);
 } else {
  /*{{{  Write header*/
  REC_channel_header channelheader;
  const long channelheader_length=CHANNELHEADER_SIZE_PER_CHANNEL*tinfo->nr_of_channels;
  int channel;
  short dd, mm, yy, yyyy, hh, mi, ss;
  char numbuf[NUMBUF_LENGTH];

  setlocale(LC_NUMERIC, "C"); /* Print fractional numbers with decimal point */
#define fileheader local_arg->fheader
  memset(&fileheader, ' ', sizeof(REC_file_header));
  copy_nstring(fileheader.version, "0", sizeof(fileheader.version));
  if (args[ARGS_PATIENT].is_set) copy_nstring(fileheader.patient, args[ARGS_PATIENT].arg.s, sizeof(fileheader.patient));
  if (args[ARGS_RECORDING].is_set) copy_nstring(fileheader.recording, args[ARGS_RECORDING].arg.s, sizeof(fileheader.recording));
  if (args[ARGS_DATETIME].is_set) {
   char * const comma=strchr(args[ARGS_DATETIME].arg.s, ',');
   if (comma==NULL) {
    ERREXIT(tinfo->emethods, "write_rec_open_file: Datetime format is dd.mm.yy,hh.mi.ss !\n");
   }
   copy_nstring(fileheader.startdate, args[ARGS_DATETIME].arg.s, sizeof(fileheader.startdate));
   copy_nstring(fileheader.starttime, comma+1, sizeof(fileheader.starttime));
  } else if (tinfo->comment!=NULL) {
   if (!args[ARGS_RECORDING].is_set) copy_nstring(fileheader.recording, tinfo->comment, sizeof(fileheader.recording));
   if (comment2time(tinfo->comment, &dd, &mm, &yy, &yyyy, &hh, &mi, &ss)) {
    snprintf(numbuf, NUMBUF_LENGTH, "%02d.%02d.%02d", dd, mm, yy);
    copy_nstring(fileheader.startdate, numbuf, sizeof(fileheader.startdate));
    snprintf(numbuf, NUMBUF_LENGTH, "%02d.%02d.%02d", hh, mi, ss);
    copy_nstring(fileheader.starttime, numbuf, sizeof(fileheader.starttime));
   }
  }
  if (*fileheader.startdate==' ') {
   /* Date/time weren't set otherwise: */
   copy_nstring(fileheader.startdate, "00.00.00", sizeof(fileheader.startdate));
   copy_nstring(fileheader.starttime, "00.00.00", sizeof(fileheader.starttime));
  }
  snprintf(numbuf, NUMBUF_LENGTH, "%ld", sm_REC_file[0].offset+channelheader_length);
  copy_nstring(fileheader.bytes_in_header, numbuf, sizeof(fileheader.bytes_in_header));
  copy_nstring(fileheader.data_format_version, (args[ARGS_DATA_FORMAT_VERSION].is_set ? args[ARGS_DATA_FORMAT_VERSION].arg.s : "write_rec file"), sizeof(fileheader.data_format_version));
  local_arg->nr_of_records=0;
  copy_nstring(fileheader.nr_of_records, "-1", sizeof(fileheader.nr_of_records));
  snprintf(numbuf, NUMBUF_LENGTH, "%g", ((double)local_arg->samples_per_record)/tinfo->sfreq);
  copy_nstring(fileheader.duration_s, numbuf, sizeof(fileheader.duration_s));
  snprintf(numbuf, NUMBUF_LENGTH, "%d", tinfo->nr_of_channels);
  copy_nstring(fileheader.nr_of_channels, numbuf, sizeof(fileheader.nr_of_channels));
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&fileheader, sm_REC_file);
#endif
  write_struct((char *)&fileheader, sm_REC_file, outfptr);
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&fileheader, sm_REC_file);
#endif
#undef fileheader

  if ((channelheader.label=(char (*)[16])malloc(channelheader_length))==NULL) {
   ERREXIT(tinfo->emethods, "write_rec_open_file: Error allocating channelheader memory\n");
  }
  channelheader.transducer=(char (*)[80])(channelheader.label+tinfo->nr_of_channels);
  channelheader.dimension=(char (*)[8])(channelheader.transducer+tinfo->nr_of_channels);
  channelheader.physmin=(char (*)[8])(channelheader.dimension+tinfo->nr_of_channels);
  channelheader.physmax=(char (*)[8])(channelheader.physmin+tinfo->nr_of_channels);
  channelheader.digmin=(char (*)[8])(channelheader.physmax+tinfo->nr_of_channels);
  channelheader.digmax=(char (*)[8])(channelheader.digmin+tinfo->nr_of_channels);
  channelheader.prefiltering=(char (*)[80])(channelheader.digmax+tinfo->nr_of_channels);
  channelheader.samples_per_record=(char (*)[8])(channelheader.prefiltering+tinfo->nr_of_channels);
  channelheader.reserved=(char (*)[32])(channelheader.samples_per_record+tinfo->nr_of_channels);
  memset(channelheader.label, ' ', channelheader_length);
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   copy_nstring(channelheader.dimension[channel], "uV", sizeof(channelheader.dimension[channel]));
   copy_nstring(channelheader.label[channel], tinfo->channelnames[channel], sizeof(channelheader.label[channel]));
   snprintf(numbuf, NUMBUF_LENGTH, "%g", (double)local_arg->digmin*local_arg->resolution);
   copy_nstring(channelheader.physmin[channel], numbuf, sizeof(channelheader.physmin[channel]));
   snprintf(numbuf, NUMBUF_LENGTH, "%g", (double)local_arg->digmax*local_arg->resolution);
   copy_nstring(channelheader.physmax[channel], numbuf, sizeof(channelheader.physmax[channel]));
   snprintf(numbuf, NUMBUF_LENGTH, "%ld", local_arg->digmin);
   copy_nstring(channelheader.digmin[channel], numbuf, sizeof(channelheader.digmin[channel]));
   snprintf(numbuf, NUMBUF_LENGTH, "%ld", local_arg->digmax);
   copy_nstring(channelheader.digmax[channel], numbuf, sizeof(channelheader.digmax[channel]));
   snprintf(numbuf, NUMBUF_LENGTH, "%ld", local_arg->samples_per_record);
   copy_nstring(channelheader.samples_per_record[channel], numbuf, sizeof(channelheader.samples_per_record[channel]));
  }
  setlocale(LC_NUMERIC, ""); /* Reset locale to environment */
  if (tinfo->nr_of_channels!=(int)fwrite((void *)channelheader.label, CHANNELHEADER_SIZE_PER_CHANNEL, tinfo->nr_of_channels, local_arg->outfptr)) {
   ERREXIT(tinfo->emethods, "write_rec_open_file: Error writing channel headers\n");
  }
  free(channelheader.label);
  /*}}}  */
 }
}
/*}}}  */

/*{{{  write_rec_close_file(transform_info_ptr tinfo) {*/
LOCAL void
write_rec_close_file(transform_info_ptr tinfo) {
 struct write_rec_storage *local_arg=(struct write_rec_storage *)tinfo->methods->local_storage;
 char numbuf[NUMBUF_LENGTH];

 snprintf(numbuf, NUMBUF_LENGTH, "%ld", local_arg->nr_of_records);
 memset(&local_arg->fheader.nr_of_records, ' ', sizeof(local_arg->fheader.nr_of_records));
 copy_nstring(local_arg->fheader.nr_of_records, numbuf, sizeof(local_arg->fheader.nr_of_records));
 fseek(local_arg->outfptr, 0L, SEEK_SET);
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->fheader, sm_REC_file);
#endif
 write_struct((char *)&local_arg->fheader, sm_REC_file, local_arg->outfptr);
 if (local_arg->outfptr!=stdout && local_arg->outfptr!=stderr) {
  fclose(local_arg->outfptr);
 } else {
  fflush(local_arg->outfptr);
 }
}
/*}}}  */
/*}}}  */

/*{{{  write_rec_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_rec_init(transform_info_ptr tinfo) {
 struct write_rec_storage *local_arg=(struct write_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int bits=(args[ARGS_BITS].is_set ? args[ARGS_BITS].arg.i : DEFAULT_BITS);
 local_arg->resolution=(args[ARGS_RESOLUTION].is_set ? args[ARGS_RESOLUTION].arg.d : 1.0);

 local_arg->samples_per_record=args[ARGS_SAMPLES_PER_RECORD].is_set ? gettimeslice(tinfo, args[ARGS_SAMPLES_PER_RECORD].arg.s) : (tinfo->data_type==FREQ_DATA ? tinfo->nroffreq : tinfo->nr_of_points);
 local_arg->current_sample=0L;
 if ((local_arg->outbuf=(short *)malloc(tinfo->nr_of_channels*local_arg->samples_per_record*sizeof(short)))==NULL) {
  ERREXIT(tinfo->emethods, "write_rec_init: Error allocating buffer memory.\n");
 }
 if (bits<1) {
  ERREXIT1(tinfo->emethods, "write_rec_init: Invalid number of bits %d\n", MSGPARM(bits));
 }
 if (local_arg->resolution<=0.0) {
  ERREXIT1(tinfo->emethods, "write_rec_init: A resolution<=0.0 is not allowed\n", MSGPARM(bits));
 }
 local_arg->digmin=(-1L<<(bits-1));
 local_arg->digmax=(~local_arg->digmin);
 local_arg->overflow_has_occurred=FALSE;

 if (args[ARGS_CLOSE].is_set==FALSE) write_rec_open_file(tinfo);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_rec(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_rec(transform_info_ptr tinfo) {
 struct write_rec_storage *local_arg=(struct write_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *outfptr=local_arg->outfptr;
 short *inbuf;
 array myarray;

 if (tinfo->itemsize!=1) {
  ERREXIT(tinfo->emethods, "write_rec: Only itemsize=1 is supported.\n");
 }
 if (args[ARGS_CLOSE].is_set) {
  write_rec_open_file(tinfo);
  outfptr=local_arg->outfptr;
 }
 /*{{{  Write epoch*/
 tinfo_array(tinfo, &myarray);
 /* We read the data point by point, i.e. channels fastest */
 array_transpose(&myarray);
 /* Within the record, writing is non-interlaced, ie points fastest */
 do {
  do {
   inbuf=local_arg->outbuf+local_arg->samples_per_record*myarray.current_element+local_arg->current_sample;
   *inbuf= (short int)rint(array_scan(&myarray)/local_arg->resolution);
   if (!local_arg->overflow_has_occurred && (*inbuf<local_arg->digmin || *inbuf>local_arg->digmax)) {
    TRACEMS(tinfo->emethods, 0, "write_rec: Some output values exceed digitization bits!\n");
    local_arg->overflow_has_occurred=TRUE;
   }
#ifndef LITTLE_ENDIAN
   Intel_int16(inbuf);
#endif
  } while (myarray.message==ARRAY_CONTINUE);
  if (++local_arg->current_sample>=local_arg->samples_per_record) {
   if ((int)fwrite(local_arg->outbuf, sizeof(short), tinfo->nr_of_channels*local_arg->samples_per_record, outfptr)!=tinfo->nr_of_channels*local_arg->samples_per_record) {
    ERREXIT(tinfo->emethods, "write_rec: Write error on data file.\n");
   }
   /* If nr_of_records was -1 (can only happen while appending),
    * leave it that way */
   if (local_arg->nr_of_records>=0) local_arg->nr_of_records++;
   local_arg->current_sample=0L;
  }
 } while (myarray.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
 if (args[ARGS_CLOSE].is_set) write_rec_close_file(tinfo);
 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_rec_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_rec_exit(transform_info_ptr tinfo) {
 struct write_rec_storage *local_arg=(struct write_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_CLOSE].is_set==FALSE) write_rec_close_file(tinfo);
 free_pointer((void **)&local_arg->outbuf);

 local_arg->outfptr=NULL;
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_rec(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_rec(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_rec_init;
 tinfo->methods->transform= &write_rec;
 tinfo->methods->transform_exit= &write_rec_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_rec";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to a REC format file [Kemp:92]\n";
 tinfo->methods->local_storage_size=sizeof(struct write_rec_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
