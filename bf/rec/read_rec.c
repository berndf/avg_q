/*
 * Copyright (C) 1996-2003,2005,2007,2009-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
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
 ARGS_CONTINUOUS, 
 ARGS_TRIGTRANSFER, 
 ARGS_TRIGFILE,
 ARGS_TRIGLIST, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.rec"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"0s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"30s"},
};

/*{{{  Definition of read_rec_storage*/
struct read_rec_storage {
 FILE *infile;
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 uint8_t **recordbuf;
 float *sampling_step;
 int *samples_per_record;
 int total_samples_per_record;
 int max_samples_per_record;
 int bytes_per_sample;	/* 2 for EDF, 3 for BDF */
 long current_record;
 int current_sample;
 long current_point;
 long points_in_file;
 char **channelnames;
 int channelnames_length;
 char *comment;
 DATATYPE *rec_offset;
 DATATYPE *rec_factor;

 long bytes_in_header;
 long nr_of_records;
 long filesize;

 int nr_of_channels;
 int nr_of_signals; /* nr_of_channels minus annotation channels */
 long beforetrig;
 long aftertrig;
 long offset;
 long epochlength;
 long fromepoch;
 long epochs;
 float sfreq;
};
/*}}}  */

/*{{{  actual_fieldlength(char *thisentry, char *nextentry) {*/
LOCAL int 
actual_fieldlength(char *thisentry, char *nextentry) {
 char *endchar=nextentry-1;
 /* VERY strange... In their example file, they don't fill with blanks only,
  * but also zeroes are lurking somewhere... Have to skip any of these... 
  * Additionally we stop at characters 20 or 21 which structure annotations. */
 while (endchar>=thisentry && (*endchar=='\0' || strchr(" \t\r\n\x14\x15", *endchar)!=NULL)) endchar--;
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

LOCAL Bool
is_annotation(struct read_rec_storage *local_arg, int channel) {
 return strncmp(local_arg->channelnames[channel]+1,"DF Annotations",14)==0;
}
/* This returns the trigger code, 0 for no more annotations */
LOCAL int
parse_annotation(char **in_annotationp, char *end_annotation, DATATYPE sfreq, long *trigpointp, long *durationp, growing_buf *descriptionp) {
 char *inin_annotation;
 if (*in_annotationp>end_annotation || **in_annotationp=='\0') return 0;
 if (**in_annotationp=='+'  || **in_annotationp=='-') {
  /* Start of a new offset/duration block 
   * Otherwise the old offset and duration are reused! */
  *trigpointp=strtod(*in_annotationp,&inin_annotation)*sfreq;
  if (*inin_annotation=='\x15') {
   *durationp=strtod((char *)inin_annotation+1,&inin_annotation)*sfreq;
  } else {
   *durationp=0;
  }
  inin_annotation++; /* Skip the '\x14' */
 }
 while (*inin_annotation!='\x14') {
  growing_buf_appendchar(descriptionp,*inin_annotation);
  inin_annotation++;
 }
 growing_buf_appendchar(descriptionp,'\0');
 *in_annotationp=inin_annotation+1; /* Skip the '\x14' */
 if (*in_annotationp<=end_annotation && **in_annotationp=='\0') (*in_annotationp)++;
 return 1;
}

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_rec_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_rec_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_rec_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_rec_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_rec_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  if (local_arg->nr_of_channels==local_arg->nr_of_signals) {
   TRACEMS(tinfo->emethods, 0, "read_rec_build_trigbuffer: No trigger source known.\n");
  } else {
   long startrecord=0;
   growing_buf description;
   growing_buf_init(&description);
   growing_buf_allocate(&description,0);
   /* We have to read the whole file... */
   while (startrecord<local_arg->nr_of_records) {
    long const filepos=local_arg->bytes_in_header+startrecord*local_arg->total_samples_per_record*local_arg->bytes_per_sample;
    long samples_read;
    int channel;
    fseek(local_arg->infile, filepos, SEEK_SET);
    samples_read=fread(local_arg->recordbuf[0], local_arg->bytes_per_sample, local_arg->total_samples_per_record, local_arg->infile);
    /* Keep on readin' until an error occurs... That should be EOF... */
    if (samples_read!=local_arg->total_samples_per_record) {
     if (samples_read!=0) {
      TRACEMS1(tinfo->emethods, 0, "read_rec warning: REC file %s appears to be truncated.\n", MSGPARM(args[ARGS_IFILE].arg.s));
     }
     break;
    }
    for (channel=0; channel<local_arg->nr_of_channels; channel++) {
     if (is_annotation(local_arg,channel)) {
      char * in_annotation=(char *)local_arg->recordbuf[channel];
      /* Last byte of annotation! */
      char * const end_annotation=in_annotation+local_arg->bytes_per_sample*local_arg->samples_per_record[channel]-1;
      int code;
      long trigpoint=0L, duration=0L;
      while (TRUE) {
       growing_buf_clear(&description);
       if ((code=parse_annotation(&in_annotation,end_annotation,tinfo->sfreq,&trigpoint,&duration,&description))==0) break;
       /* We skip entries without description, such as time-keeping annotations */
       //printf("%ld %ld >%s<\n", trigpoint, duration, description.buffer_start);
       if (description.current_length>1) {
	push_trigger(&local_arg->triggers, trigpoint, code, strdup(description.buffer_start));
	if (duration>0) push_trigger(&local_arg->triggers, trigpoint+duration, -code, strdup(description.buffer_start));
       }
      }
     }
    }
    startrecord++;
   }
   growing_buf_free(&description);
  }
 }
}
/*}}}  */
/*{{{  read_rec_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_rec_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_rec_build_trigbuffer(tinfo);
 }
 {
 int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);

 if (local_arg->current_trigger<nevents) {
  struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+local_arg->current_trigger;
  *position=intrig->position;
   code    =intrig->code;
  if (descriptionp!=NULL) *descriptionp=intrig->description;
 }
 }
 local_arg->current_trigger++;
 return code;
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
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
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
 /* BDF: 3 bytes per sample, 24 bit; EDF (version='0'): 2 bytes, 16 bit */
 local_arg->bytes_per_sample=(fileheader.version[0]=='\xff' ? 3 : 2);
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
  local_arg->nr_of_signals++;

  local_arg->samples_per_record[channel]=atoi(channelheader.samples_per_record[channel]);
  local_arg->total_samples_per_record+=local_arg->samples_per_record[channel];
  if (local_arg->samples_per_record[channel]>local_arg->max_samples_per_record) local_arg->max_samples_per_record=local_arg->samples_per_record[channel];
 }
 if ((local_arg->channelnames=(char **)malloc(local_arg->nr_of_channels*sizeof(char *)))==NULL ||
     (innames=(char *)malloc(local_arg->channelnames_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating channelnames memory\n");
 }
 if ((local_arg->rec_offset=(DATATYPE *)malloc(local_arg->nr_of_channels*sizeof(DATATYPE)))==NULL ||
     (local_arg->rec_factor=(DATATYPE *)malloc(local_arg->nr_of_channels*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating offset+factor memory\n");
 }
 if ((local_arg->recordbuf=(uint8_t **)malloc(local_arg->nr_of_channels*sizeof(uint8_t *)))==NULL ||
     (local_arg->recordbuf[0]=(uint8_t *)malloc(local_arg->total_samples_per_record*local_arg->bytes_per_sample))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating recordbuf memory\n");
 }
 if ((local_arg->sampling_step=(float *)malloc(local_arg->nr_of_channels*sizeof(float)))==NULL) {
  ERREXIT(tinfo->emethods, "read_rec_init: Error allocating sampling_step array\n");
 }
 /* We're at the start, need to load a fresh data record */
 local_arg->current_sample=0;

 setlocale(LC_NUMERIC, "C"); /* Make fractional numbers be read correctly */
 local_arg->nr_of_signals=0;
 for (channel=0; channel<local_arg->nr_of_channels; channel++) {
  const DATATYPE physmin=atof(channelheader.physmin[channel]), physmax=atof(channelheader.physmax[channel]);
  const short digmin=atoi(channelheader.digmin[channel]), digmax=atoi(channelheader.digmax[channel]);
  int const n=actual_fieldlength(channelheader.label[channel], channelheader.label[channel+1]);

  local_arg->channelnames[channel]=innames;
  strncpy(innames, channelheader.label[channel], n);
  innames[n]='\0';
  innames+=n+1;

  if (!is_annotation(local_arg,channel)) local_arg->nr_of_signals++;

  local_arg->rec_factor[channel]=(physmax-physmin)/(digmax-digmin);
  local_arg->rec_offset[channel]=physmin-digmin*local_arg->rec_factor[channel];

  if (channel>=1) {
   local_arg->recordbuf[channel]=local_arg->recordbuf[channel-1]+local_arg->bytes_per_sample*local_arg->samples_per_record[channel-1];
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
 if (local_arg->filesize!=local_arg->nr_of_records*local_arg->total_samples_per_record*local_arg->bytes_per_sample+local_arg->bytes_in_header) {
  long const nr_of_records=(local_arg->filesize-local_arg->bytes_in_header)/local_arg->bytes_per_sample/local_arg->total_samples_per_record;
  if (local_arg->nr_of_records==nr_of_records) {
   TRACEMS1(tinfo->emethods, 1, "read_rec_init: %d excess bytes in file.\n", MSGPARM(local_arg->filesize-local_arg->nr_of_records*local_arg->total_samples_per_record*local_arg->bytes_per_sample-local_arg->bytes_in_header));
  } else {
   TRACEMS2(tinfo->emethods, 0, "read_rec_init: File size is incompatible with %d records, correcting to %d\n", MSGPARM(local_arg->nr_of_records), MSGPARM(nr_of_records));
   local_arg->nr_of_records=nr_of_records;
  }
 }
 local_arg->points_in_file=tinfo->points_in_file=local_arg->nr_of_records*local_arg->max_samples_per_record;
 /*}}}  */

 TRACEMS3(tinfo->emethods, 1, "read_rec_init: Opened REC file %s with %d channels, Sfreq=%d.\n", MSGPARM(args[ARGS_IFILE].arg.s), MSGPARM(local_arg->nr_of_channels), MSGPARM(local_arg->sfreq));
 
 /*{{{  Process arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_rec_init: Error allocating triglist memory\n");
   }
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   if (local_arg->points_in_file==0) {
    ERREXIT(tinfo->emethods, "read_rec: Unable to determine the number of samples in the input file!\n");
   }
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }

 read_rec_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_record= -1; /* No record is currently loaded */
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_rec(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_rec(transform_info_ptr tinfo) {
 struct read_rec_storage *local_arg=(struct read_rec_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char *innamebuf;
 int channel, point, signal;
 array myarray;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->nr_of_signals;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_rec: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Find the next window that fits into the actual data*/
 /* This is just for the Continuous option (no trigger file): */
 file_end_point=local_arg->current_point-1;
 do {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Simulate a trigger at current_point+beforetrig */
   file_start_point=file_end_point+1;
   trigger_point=file_start_point+tinfo->beforetrig;
   file_end_point=trigger_point+tinfo->aftertrig-1;
   if (local_arg->points_in_file>0 && file_end_point>=local_arg->points_in_file) return NULL;
   local_arg->current_trigger++;
   local_arg->current_point+=tinfo->nr_of_points;
   tinfo->condition=0;
  } else 
  do {
   tinfo->condition=read_rec_read_trigger(tinfo, &trigger_point, &description);
   if (tinfo->condition==0) return NULL;	/* No more triggers in file */
   file_start_point=trigger_point-tinfo->beforetrig+local_arg->offset;
   file_end_point=trigger_point+tinfo->aftertrig-1-local_arg->offset;
   
   if (local_arg->trigcodes==NULL) {
    not_correct_trigger=FALSE;
   } else {
    int trigno=0;
    not_correct_trigger=TRUE;
    while (local_arg->trigcodes[trigno]!=0) {
     if (local_arg->trigcodes[trigno]==tinfo->condition) {
      not_correct_trigger=FALSE;
      break;
     }
     trigno++;
    }
   }
  } while (not_correct_trigger || file_start_point<0 || (local_arg->points_in_file>0 && file_end_point>=local_arg->points_in_file));
 } while (--local_arg->fromepoch>0);
 if (description==NULL) {
  TRACEMS3(tinfo->emethods, 1, "read_rec: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_rec: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
 }
 /*}}}  */

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_rec_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_rec_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 myarray.nr_of_elements=tinfo->nr_of_channels;
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
  long samples_read;
  /*{{{  Read a new record if necessary*/
  ldiv_t const d=ldiv(file_start_point+point, local_arg->max_samples_per_record);
  long const startrecord=d.quot;
  local_arg->current_sample=d.rem;
  if (startrecord!=local_arg->current_record) {
   long const filepos=local_arg->bytes_in_header+startrecord*local_arg->total_samples_per_record*local_arg->bytes_per_sample;
   fseek(local_arg->infile, filepos, SEEK_SET);
   samples_read=fread(local_arg->recordbuf[0], local_arg->bytes_per_sample, local_arg->total_samples_per_record, local_arg->infile);
   /* Keep on readin' until an error occurs... That should be EOF... */
   if (samples_read!=local_arg->total_samples_per_record) {
    if (samples_read!=0) {
     TRACEMS1(tinfo->emethods, 0, "read_rec warning: REC file %s appears to be truncated.\n", MSGPARM(args[ARGS_IFILE].arg.s));
    }
    array_free(&myarray);
    return NULL;
   }
   local_arg->current_record=startrecord;
  }
  /*}}}  */
  for (channel=0; channel<local_arg->nr_of_channels; channel++) {
   if (!is_annotation(local_arg,channel)) {
    int32_t sample;
    uint8_t * const samplepos=local_arg->recordbuf[channel]+(int)(local_arg->current_sample*local_arg->sampling_step[channel])*local_arg->bytes_per_sample;
    if (local_arg->bytes_per_sample==3) {
     memcpy(&sample,samplepos,3);
     ((uint8_t *)&sample)[3]=((((uint8_t *)&sample)[2]&'\x80') ? '\xff' : '\x00');
#   ifndef LITTLE_ENDIAN
     Intel_int32((uint32_t *)&sample);
#   endif
    } else {
     int16_t sample16=*(int16_t *)samplepos;
#   ifndef LITTLE_ENDIAN
     Intel_int16((uint16_t *)&sample16);
#   endif
     sample=(int32_t)sample16;
    }
    array_write(&myarray, local_arg->rec_offset[channel]+local_arg->rec_factor[channel]*sample);
   }
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
 for (signal=channel=0; channel<local_arg->nr_of_channels; channel++) {
  if (!is_annotation(local_arg,channel)) {
   tinfo->channelnames[signal]=innamebuf+(local_arg->channelnames[channel]-local_arg->channelnames[0]);
   signal++;
  }
 }
 create_channelgrid(tinfo);
 strcpy(tinfo->comment, local_arg->comment);
 /*}}}  */

 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->leaveright=0;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

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
 free_pointer((void **)&local_arg->rec_offset);
 free_pointer((void **)&local_arg->rec_factor);
 free_pointer((void **)&local_arg->comment);
 free_pointer((void **)&local_arg->trigcodes);
 growing_buf_free(&local_arg->triggers);

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
