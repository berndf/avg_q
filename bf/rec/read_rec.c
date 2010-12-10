/*
 * Copyright (C) 1996-2003,2005,2007,2009,2010 Bernd Feige
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
 * read_rec.c module to read data from REC format files [Kemp:92]
 * (Data is always continuous)
 *	-- Bernd Feige 17.06.1996
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <read_struct.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"

#include "RECfile.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 ARGS_EPOCHLENGTH,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.rec"},
 {T_ARGS_TAKES_STRING_WORD, "epochlength", "", ARGDESC_UNUSED, (const char *const *)"1s"}
};

/*{{{  Definition of read_rec_storage*/
struct read_rec_storage {
 FILE *infile;
 short **recordbuf;
 float *sampling_step;
 int *samples_per_record;
 int total_samples_per_record;
 int max_samples_per_record;
 int current_sample;
 char **channelnames;
 int channelnames_length;
 char *comment;
 DATATYPE *offset;
 DATATYPE *factor;

 long bytes_in_header;
 long nr_of_records;
 long filesize;

 int nr_of_channels;
 long beforetrig;
 long aftertrig;
 long epochlength;
 long epochs;
 float sfreq;
};
/*}}}  */

/*{{{  actual_fieldlength(char *thisentry, char *nextentry) {*/
LOCAL int 
actual_fieldlength(char *thisentry, char *nextentry) {
 char *endchar=nextentry-1;
 /* VERY strange... In their example file, they don't fill with blanks only,
  * but also zeroes are lurking somewhere... Have to skip any of these... */
 while (endchar>=thisentry && (*endchar=='\0' || strchr(" \t\r\n", *endchar)!=NULL)) endchar--;
 return (endchar-thisentry+1);
}
/*}}}  */
/*{{{  read_2_digit_integer(char * const start) {*/
/* This completely idiotic function is also just needed because the C
 * conversion functions need 0-terminated strings but we don't have room
 * for the zeros in the header... Need this to read date/time values. */
LOCAL int
read_2_digit_integer(char * const start) {
 int ret=0;
 signed char digit=start[0]-'0';
 if (digit>=0 && digit<=9) {
  ret=digit;
 }
 digit=start[1]-'0';
 if (digit>=0 && digit<=9) {
  ret=ret*10+digit;
 }
 return ret;
}
/*}}}  */

/*{{{  read_rec_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_rec_init(transform_info_ptr tinfo) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 REC_file_header fileheader;
 REC_channel_header channelheader;
 long channelheader_length;
 int patient_length, recording_length, channel, dd, mm, yy, hh, mi, ss;
 char *innames;
 struct stat statbuff;

 /*{{{  Process options*/
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s,"rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_rec_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 /*{{{  Read the file header and build comment*/
 if (read_struct((char *)&fileheader, sm_REC_file, local_arg->infile)==0) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error reading file header.\n");
 }
# ifndef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader, sm_REC_file);
# endif
 local_arg->nr_of_channels=atoi(fileheader.nr_of_channels);
 patient_length=actual_fieldlength(fileheader.patient, fileheader.recording);
 recording_length=actual_fieldlength(fileheader.recording, fileheader.startdate);
 if ((local_arg->comment=(char *)malloc(patient_length+recording_length+4+17))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating comment memory\n");
 }
 dd=read_2_digit_integer(fileheader.startdate);
 mm=read_2_digit_integer(fileheader.startdate+3);
 yy=read_2_digit_integer(fileheader.startdate+6);
 hh=read_2_digit_integer(fileheader.starttime);
 mi=read_2_digit_integer(fileheader.starttime+3);
 ss=read_2_digit_integer(fileheader.starttime+6);
 strncpy(local_arg->comment, fileheader.patient, patient_length);
 local_arg->comment[patient_length]='\0';
 strcat(local_arg->comment, "; ");
 strncat(local_arg->comment, fileheader.recording, recording_length);
 sprintf(local_arg->comment+patient_length+recording_length+2, " %02d/%02d/%02d,%02d:%02d:%02d", mm, dd, yy, hh, mi, ss);
 /*}}}  */
 /*{{{  Read the channel header*/
 /* Length of the file header is included in this number... */
 local_arg->bytes_in_header=atoi(fileheader.bytes_in_header);
 local_arg->nr_of_records=atoi(fileheader.nr_of_records);
 channelheader_length=local_arg->bytes_in_header-sm_REC_file[0].offset;
 if ((channelheader.label=(char (*)[16])malloc(channelheader_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating channelheader memory\n");
 }
 if (local_arg->nr_of_channels!=(int)fread((void *)channelheader.label, CHANNELHEADER_SIZE_PER_CHANNEL, local_arg->nr_of_channels, local_arg->infile)) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error reading channel headers\n");
 }
 channelheader.transducer=(char (*)[80])(channelheader.label+local_arg->nr_of_channels);
 channelheader.dimension=(char (*)[8])(channelheader.transducer+local_arg->nr_of_channels);
 channelheader.physmin=(char (*)[8])(channelheader.dimension+local_arg->nr_of_channels);
 channelheader.physmax=(char (*)[8])(channelheader.physmin+local_arg->nr_of_channels);
 channelheader.digmin=(char (*)[8])(channelheader.physmax+local_arg->nr_of_channels);
 channelheader.digmax=(char (*)[8])(channelheader.digmin+local_arg->nr_of_channels);
 channelheader.prefiltering=(char (*)[80])(channelheader.digmax+local_arg->nr_of_channels);
 channelheader.samples_per_record=(char (*)[8])(channelheader.prefiltering+local_arg->nr_of_channels);
 channelheader.reserved=(char (*)[32])(channelheader.samples_per_record+local_arg->nr_of_channels);
 /*}}}  */

 /*{{{  Allocate and fill local arrays*/
 if ((local_arg->samples_per_record=(int *)malloc(local_arg->nr_of_channels*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating samples_per_record memory\n");
 }
 local_arg->channelnames_length=0;
 local_arg->max_samples_per_record=local_arg->total_samples_per_record=0;
 for (channel=0; channel<local_arg->nr_of_channels; channel++) {
  local_arg->channelnames_length+=actual_fieldlength(channelheader.label[channel], channelheader.label[channel+1])+1;

  local_arg->samples_per_record[channel]=atoi(channelheader.samples_per_record[channel]);
  local_arg->total_samples_per_record+=local_arg->samples_per_record[channel];
  if (local_arg->samples_per_record[channel]>local_arg->max_samples_per_record) local_arg->max_samples_per_record=local_arg->samples_per_record[channel];
 }
 if ((local_arg->channelnames=(char **)malloc(local_arg->nr_of_channels*sizeof(char *)))==NULL ||
     (innames=(char *)malloc(local_arg->channelnames_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating channelnames memory\n");
 }
 if ((local_arg->offset=(DATATYPE *)malloc(local_arg->nr_of_channels*sizeof(DATATYPE)))==NULL ||
     (local_arg->factor=(DATATYPE *)malloc(local_arg->nr_of_channels*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating offset+factor memory\n");
 }
 if ((local_arg->recordbuf=(short **)malloc(local_arg->nr_of_channels*sizeof(short *)))==NULL ||
     (local_arg->recordbuf[0]=(short *)malloc(local_arg->total_samples_per_record*sizeof(short)))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating recordbuf memory\n");
 }
 if ((local_arg->sampling_step=(float *)malloc(local_arg->nr_of_channels*sizeof(float)))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating sampling_step array\n");
 }
 /* We're at the start, need to load a fresh data record */
 local_arg->current_sample=0;

 setlocale(LC_NUMERIC, "C"); /* Make fractional numbers be read correctly */
 for (channel=0; channel<local_arg->nr_of_channels; channel++) {
  const DATATYPE physmin=atof(channelheader.physmin[channel]), physmax=atof(channelheader.physmax[channel]);
  const short digmin=atoi(channelheader.digmin[channel]), digmax=atoi(channelheader.digmax[channel]);
  int const n=actual_fieldlength(channelheader.label[channel], channelheader.label[channel+1]);

  local_arg->channelnames[channel]=innames;
  strncpy(innames, channelheader.label[channel], n);
  innames[n]='\0';
  innames+=n+1;

  local_arg->factor[channel]=(physmax-physmin)/(digmax-digmin);
  local_arg->offset[channel]=physmin-digmin*local_arg->factor[channel];

  if (channel>=1) {
   local_arg->recordbuf[channel]=local_arg->recordbuf[channel-1]+local_arg->samples_per_record[channel-1];
  }
  
  local_arg->sampling_step[channel]=((float)local_arg->samples_per_record[channel])/local_arg->max_samples_per_record;
 }
 tinfo->sfreq=local_arg->sfreq=local_arg->max_samples_per_record/atof(fileheader.duration_s);
 setlocale(LC_NUMERIC, ""); /* Reset locale to environment */
 free((void *)channelheader.label);
 /*}}}  */

 /*{{{  Determine the file size*/
 if (ftell(local_arg->infile)!=local_arg->bytes_in_header) {
  TRACEMS2(tinfo->emethods, 0, "read_rec_init: Position after header is %d, bytes_in_header field was %d\n", MSGPARM(ftell(local_arg->infile)), MSGPARM(local_arg->bytes_in_header));
  local_arg->bytes_in_header=ftell(local_arg->infile);
 }
 fstat(fileno(local_arg->infile),&statbuff);
 local_arg->filesize = statbuff.st_size;
 if (local_arg->filesize!=local_arg->nr_of_records*local_arg->total_samples_per_record*(int)sizeof(short)+local_arg->bytes_in_header) {
  long const nr_of_records=(local_arg->filesize-local_arg->bytes_in_header)/sizeof(short)/local_arg->total_samples_per_record;
  if (local_arg->nr_of_records==nr_of_records) {
   TRACEMS1(tinfo->emethods, 1, "read_rec_init: %d excess bytes in file.\n", MSGPARM(local_arg->filesize-local_arg->nr_of_records*local_arg->total_samples_per_record*(int)sizeof(short)-local_arg->bytes_in_header));
  } else {
   TRACEMS2(tinfo->emethods, 0, "read_rec_init: File size is incompatible with %d records, correcting to %d\n", MSGPARM(local_arg->nr_of_records), MSGPARM(nr_of_records));
   local_arg->nr_of_records=nr_of_records;
  }
 }
 /*}}}  */
 
 /*{{{  Process arguments that can be in seconds*/
 local_arg->beforetrig=(args[ARGS_OFFSET].is_set ? -gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 local_arg->epochlength=gettimeslice(tinfo, args[ARGS_EPOCHLENGTH].arg.s);
 if (local_arg->epochlength==0) {
  /* Read the whole REC file */
  local_arg->epochlength=local_arg->nr_of_records*local_arg->max_samples_per_record;
 }
 if (local_arg->epochlength<=0) {
  ERREXIT1(tinfo->emethods, "read_rec_init: Invalid epoch length %d\n", MSGPARM(local_arg->epochlength));
 }
 local_arg->aftertrig=local_arg->epochlength-local_arg->beforetrig;
 /*}}}  */

 TRACEMS3(tinfo->emethods, 1, "read_rec_init: Opened REC file %s with %d channels, Sfreq=%d.\n", MSGPARM(args[ARGS_IFILE].arg.s), MSGPARM(local_arg->nr_of_channels), MSGPARM(local_arg->sfreq));

 /*{{{  Seek to fromepoch if necessary*/
 if (args[ARGS_FROMEPOCH].is_set && args[ARGS_FROMEPOCH].arg.i>1) {
  ldiv_t d=ldiv((args[ARGS_FROMEPOCH].arg.i-1)*local_arg->epochlength, local_arg->max_samples_per_record);
  long startrecord=d.quot;
  long skipbytes=startrecord*local_arg->total_samples_per_record*sizeof(short);
  fseek(local_arg->infile, skipbytes, SEEK_CUR);
  /* Tell read_rec that a record must be read and the current sample is d.rem */
  local_arg->current_sample= -d.rem;
 }
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_rec(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_rec(transform_info_ptr tinfo) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char *innamebuf;
 int channel, point;
 array myarray;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nrofaverages=1;

 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_vectors=tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
 myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->nr_of_channels;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_rec: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_rec: Error allocating data\n");
 }
 /* Byte order is multiplexed, ie channels fastest */
 tinfo->multiplexed=TRUE;
 /*}}}  */

 for (point=0; point<tinfo->nr_of_points; point++) {
  /*{{{  Read a new record if necessary*/
  if (local_arg->current_sample<=0 || local_arg->current_sample>=local_arg->max_samples_per_record) {
   long samples_read=fread(local_arg->recordbuf[0], sizeof(short), local_arg->total_samples_per_record, local_arg->infile);
   /* Keep on readin' until an error occurs... That should be EOF... */
   if (samples_read!=local_arg->total_samples_per_record) {
    if (samples_read!=0) {
     TRACEMS1(tinfo->emethods, 0, "read_rec warning: REC file %s appears to be truncated.\n", MSGPARM(args[ARGS_IFILE].arg.s));
    }
    array_free(&myarray);
    return NULL;
   }
   /*{{{  Swap byte order if necessary*/
#   ifndef LITTLE_ENDIAN
   {short *inrecbuf=local_arg->recordbuf[0], *recbufend=inrecbuf+local_arg->total_samples_per_record;
    while (inrecbuf<recbufend) Intel_short(inrecbuf++);
   }
#   endif
   /*}}}  */
   if (local_arg->current_sample>0) {
    local_arg->current_sample=0;
   } else {
    local_arg->current_sample= -local_arg->current_sample;
   }
  }
  /*}}}  */
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   array_write(&myarray, local_arg->offset[channel]+local_arg->factor[channel]*local_arg->recordbuf[channel][(int)(local_arg->current_sample*local_arg->sampling_step[channel])]);
  }
  local_arg->current_sample++;
 }
 
 /*{{{  Allocate and copy channelnames and comment; Set positions on a grid*/
 tinfo->xdata=NULL;
 tinfo->probepos=NULL;
 if ((tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
     (tinfo->comment=(char *)malloc(strlen(local_arg->comment)+1))==NULL ||
     (innamebuf=(char *)malloc(local_arg->channelnames_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec: Error allocating channelnames\n");
 }
 memcpy(innamebuf, local_arg->channelnames[0], local_arg->channelnames_length);
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  tinfo->channelnames[channel]=innamebuf+(local_arg->channelnames[channel]-local_arg->channelnames[0]);
 }
 create_channelgrid(tinfo);
 strcpy(tinfo->comment, local_arg->comment);
 /*}}}  */

 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->leaveright=0;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->data_type=TIME_DATA;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_rec_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_rec_exit(transform_info_ptr tinfo) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;

 fclose(local_arg->infile);
 if (local_arg->recordbuf!=NULL) {
  free_pointer((void **)&local_arg->recordbuf[0]);
  free_pointer((void **)&local_arg->recordbuf);
 }
 free_pointer((void **)&local_arg->samples_per_record);
 free_pointer((void **)&local_arg->sampling_step);
 if (local_arg->channelnames!=NULL) {
  free_pointer((void **)&local_arg->channelnames[0]);
  free_pointer((void **)&local_arg->channelnames);
 }
 free_pointer((void **)&local_arg->offset);
 free_pointer((void **)&local_arg->factor);
 free_pointer((void **)&local_arg->comment);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_rec(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_rec(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_rec_init;
 tinfo->methods->transform= &read_rec;
 tinfo->methods->transform_exit= &read_rec_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_rec";
 tinfo->methods->method_description=
  "Get-epoch method to read data from REC format files [Kemp:92]\n";
 tinfo->methods->local_storage_size=sizeof(struct read_rec_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
