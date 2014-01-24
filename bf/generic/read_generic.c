/*
 * Copyright (C) 1996-1999,2001-2004,2006-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_generic.c module to read data from generic (binary or limited ASCII) files.
 *	-- Bernd Feige 10.12.1996
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <ctype.h>
#include <setjmp.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

#define MAX_COMMENTLEN 1024

LOCAL char *datatype_choice[]={
 "uint8",
 "int8",
 "int16",
 "int32",
 "float32",
 "float64",
 "string",
 NULL
};
LOCAL int datatype_size[]={
 sizeof(uint8_t),
 sizeof(int8_t),
 sizeof(int16_t),
 sizeof(int32_t),
 sizeof(float),
 sizeof(double),
 0,	/* This entry for the `string' datatype tells us there is no fixed length... */
};
enum DATATYPE_ENUM {
 DT_UINT8=0,
 DT_INT8,
 DT_INT16,
 DT_INT32,
 DT_FLOAT32,
 DT_FLOAT64,
 DT_STRING,
};
enum ARGS_ENUM {
 ARGS_SWAPBYTEORDER=0,
 ARGS_POINTSFASTEST,
 ARGS_CONTINUOUS, 
 ARGS_TRIGTRANSFER, 
 ARGS_TRIGFILE,
 ARGS_TRIGLIST, 
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_SFREQ,
 ARGS_XCHANNELNAME,
 ARGS_NROFCHANNELS,
 ARGS_ITEMSIZE,
 ARGS_FILEOFFSET,
 ARGS_BLOCKGAP,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 ARGS_DATATYPE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Swap byte order relative to the current machine", "S", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Points vary fastest in the input file (`nonmultiplexed')", "P", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_DOUBLE, "sampling_freq: Specify the sampling frequency in Hz (default: 100)", "s", 100, NULL},
 {T_ARGS_TAKES_STRING_WORD, "xchannelname: Read the x axis data as the first `channel'", "x", ARGDESC_UNUSED, (const char *const *)"Point"},
 {T_ARGS_TAKES_LONG, "Channels: Specify the number of channels", "C", 30, NULL},
 {T_ARGS_TAKES_LONG, "Items: Specify the number of items", "I", 1, NULL},
 {T_ARGS_TAKES_LONG, "File_offset: Skip this many bytes (`string':Lines) at the start", "O", 0, NULL},
 {T_ARGS_TAKES_LONG, "Block_gap: Skip this many bytes between (point/channel) blocks", "B", 0, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_SELECTION, "data_type", "", DT_INT16, (const char *const *)datatype_choice}
};

/*{{{  Definition of read_generic_storage*/
struct read_generic_storage {
 FILE *infile;
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 int nr_of_channels;
 int itemsize;
 long points_in_file;
 long bytes_per_point;
 long current_point;
 long fileoffset;
 long block_gap;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 float sfreq;
 enum DATATYPE_ENUM datatype;
 growing_buf stringbuf;
 Bool new_line;
 Bool first_in_epoch;

 jmp_buf error_jmp;
};
/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_generic_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_generic_storage *local_arg=(struct read_generic_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_generic_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_generic_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_generic_storage *local_arg=(struct read_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_generic_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_generic_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
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
  TRACEMS(tinfo->emethods, 0, "read_generic_build_trigbuffer: No trigger source known.\n");
 }
}
/*}}}  */
/*{{{  read_generic_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_generic_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_generic_storage *local_arg=(struct read_generic_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_generic_build_trigbuffer(tinfo);
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

/*{{{  read_generic_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_generic_init(transform_info_ptr tinfo) {
 struct read_generic_storage *local_arg=(struct read_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 struct stat statbuff;
 char const * const filemode=(local_arg->datatype==DT_STRING ? "r" : "rb");

 growing_buf_init(&local_arg->triggers);

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 local_arg->nr_of_channels=(args[ARGS_NROFCHANNELS].is_set ? args[ARGS_NROFCHANNELS].arg.i : 1);
 local_arg->itemsize=(args[ARGS_ITEMSIZE].is_set ? args[ARGS_ITEMSIZE].arg.i : 1);
 tinfo->sfreq=local_arg->sfreq=(args[ARGS_SFREQ].is_set ? args[ARGS_SFREQ].arg.d : 100.0);
 local_arg->datatype=(enum DATATYPE_ENUM)args[ARGS_DATATYPE].arg.i;
 local_arg->fileoffset=(args[ARGS_FILEOFFSET].is_set ? args[ARGS_FILEOFFSET].arg.i : 0);
 local_arg->block_gap=(args[ARGS_BLOCKGAP].is_set ? args[ARGS_BLOCKGAP].arg.i : 0);
 /*}}}  */
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 if (strcmp(args[ARGS_IFILE].arg.s, "stdin")==0) local_arg->infile=stdin;
 else if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s, filemode))==NULL) {
  ERREXIT1(tinfo->emethods, "read_generic_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }

 if (local_arg->datatype==DT_STRING) {
  if (args[ARGS_SWAPBYTEORDER].is_set) {
   ERREXIT(tinfo->emethods, "read_generic_init: Swapping the byte order makes no sense with the `string' datatype.\n");
  }
  if (args[ARGS_TRIGFILE].is_set || args[ARGS_TRIGLIST].is_set) {
   ERREXIT(tinfo->emethods, "read_generic_init: Using triggers directly with the `string' datatype is not possible.\n");
  }
  /* Skip (local_arg->fileoffset) lines */
  while (local_arg->fileoffset!=0) {
   while (TRUE) {
    int const inchar=fgetc(local_arg->infile);
    if (inchar==EOF || inchar=='\n') break;
   }
   local_arg->fileoffset--;
  }
  growing_buf_init(&local_arg->stringbuf);
  growing_buf_allocate(&local_arg->stringbuf, 0);
  local_arg->new_line=TRUE;
 }

 /* Note that we can't use stat here as args[ARGS_IFILE].arg.s might be "stdin"... */
 fstat(fileno(local_arg->infile),&statbuff);
 /* Allow size to be less then fileoffset only if we read from a pipe... */
 if (statbuff.st_size<=local_arg->fileoffset && !S_ISFIFO(statbuff.st_mode)) {
  ERREXIT(tinfo->emethods, "read_generic_init: Input file length <= fileoffset!\n");
 }
 if (datatype_size[local_arg->datatype]==0) {
  /* We don't know bytes_per_point and how many samples may fit in the input file... */
  local_arg->bytes_per_point = 0;
  local_arg->points_in_file = 0;
 } else {
  local_arg->bytes_per_point=(local_arg->nr_of_channels*local_arg->itemsize+(args[ARGS_XCHANNELNAME].is_set ? 1 : 0))*datatype_size[local_arg->datatype]+local_arg->block_gap;
  if (statbuff.st_size==0) {
   local_arg->points_in_file = 0;
  } else {
   /* We add block_gap to the file length because this many bytes do not actually need
    * to be available at the end of the file for the last data point to be valid: */
   local_arg->points_in_file = (statbuff.st_size-local_arg->fileoffset+local_arg->block_gap)/local_arg->bytes_per_point;
  }
 }
 tinfo->points_in_file=local_arg->points_in_file;

 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_generic_init: Error allocating triglist memory\n");
   }
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   if (local_arg->points_in_file==0) {
    ERREXIT(tinfo->emethods, "read_generic: Unable to determine the number of samples in the input file!\n");
   }
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }

 read_generic_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_value: Central function for reading a number*/
enum ERROR_ENUM {
 ERR_NONE=0,
 ERR_READ,
 ERR_NUMBER
};
LOCAL DATATYPE
read_value(FILE *infile, 
 struct read_generic_storage *local_arg, 
 Bool swap_byteorder) {
 DATATYPE dat=0;
 enum ERROR_ENUM err=ERR_NONE;
 switch (local_arg->datatype) {
  case DT_UINT8: {
   unsigned char c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   dat=c;
   }
   break;
  case DT_INT8: {
   signed char c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   dat=c;
   }
   break;
  case DT_INT16: {
   int16_t c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   if (swap_byteorder) Intel_int16((uint16_t *)&c);
   dat=c;
   }
   break;
  case DT_INT32: {
   int32_t c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   if (swap_byteorder) Intel_int32((uint32_t *)&c);
   dat=c;
   }
   break;
  case DT_FLOAT32: {
   float c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   if (swap_byteorder) Intel_float(&c);
   dat=c;
   }
   break;
  case DT_FLOAT64: {
   double c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   if (swap_byteorder) Intel_double(&c);
   dat=c;
   }
   break;
  case DT_STRING: {
   /* Hmmm. fgetc() returns an int, which may be negative (error), so
    * we shouldn't assign it to a char since that might be unsigned -
    * But we need a char * for growing_buf_append and don't want to do
    * a byteorder-dependent hack here... Sigh... */
   int inchar;
   char ch;
   growing_buf_clear(&local_arg->stringbuf);
   while (TRUE) {
    inchar=fgetc(infile);
    if (inchar==EOF) {
     err=ERR_READ;
     break;
    }
    if (strchr("-+0123456789.eEdD", inchar)!=NULL) {
     ch=inchar;
     /* Fortran 'double' data is output with a 'D' instead of 'E';
      * C does not recognise this so we have to fix it... */
     if (ch=='d' || ch=='D') ch='e';
     growing_buf_append(&local_arg->stringbuf, &ch, 1);
     local_arg->new_line=FALSE;
    } else {
     if (inchar=='#' && local_arg->new_line) {
      while (TRUE) {
       inchar=fgetc(infile);
       if (inchar==EOF || inchar=='\n') break;
      }
      if (inchar==EOF) {
       err=ERR_READ;
       break;
      }
     }
     if (inchar=='\n') {
      local_arg->new_line=TRUE;
     } else {
      local_arg->new_line=FALSE;
     }
     if (local_arg->stringbuf.current_length>0) break;
    }
   }
   if (err==ERR_NONE) {
    char *endpointer;
    /* Properly 0-terminate the collected string: */
    ch='\0';
    growing_buf_append(&local_arg->stringbuf, &ch, 1);
    dat=strtod(local_arg->stringbuf.buffer_start, &endpointer);
    /* It is an error if strtod() doesn't accept all of the 'number chars' we
     * collected - eg '12+3' would be such a badly-formed number */
    if (endpointer-local_arg->stringbuf.buffer_start!=local_arg->stringbuf.current_length-1) err=ERR_NUMBER;
   }
   }
   break;
 }
 if (err!=ERR_NONE) {
  longjmp(local_arg->error_jmp, err);
 }
 return dat;
}
/*}}}  */

/*{{{  read_generic(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_generic(transform_info_ptr tinfo) {
 struct read_generic_storage *local_arg=(struct read_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 FILE *infile=local_arg->infile;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->nr_of_channels;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_generic: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
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
   tinfo->condition=read_generic_read_trigger(tinfo, &trigger_point, &description);
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
  TRACEMS3(tinfo->emethods, 1, "read_generic: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_generic: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
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
  read_generic_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_generic_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /* If bytes_per_point is unknown (DT_STRING...) we just continuously scan the data.
  * fromepoch won't work! */
 if (local_arg->bytes_per_point>0) {
  fseek(infile, local_arg->fileoffset+file_start_point*local_arg->bytes_per_point, SEEK_SET);
 }

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=local_arg->itemsize;
 if (args[ARGS_POINTSFASTEST].is_set) {
  tinfo->multiplexed=FALSE;
  myarray.nr_of_elements=tinfo->nr_of_points;
  myarray.nr_of_vectors=tinfo->nr_of_channels;
 } else {
  tinfo->multiplexed=TRUE;
  myarray.nr_of_elements=tinfo->nr_of_channels;
  myarray.nr_of_vectors=tinfo->nr_of_points;
 }
 tinfo->xdata=NULL;
 if (array_allocate(&myarray)==NULL
  || (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL
  || (args[ARGS_XCHANNELNAME].is_set && (tinfo->xdata=(DATATYPE *)malloc(tinfo->nr_of_points*sizeof(DATATYPE)))==NULL)) {
  ERREXIT(tinfo->emethods, "read_generic: Error allocating data\n");
 }
 /*}}}  */

 if (description==NULL) {
  snprintf(tinfo->comment, MAX_COMMENTLEN, "read_generic %s", args[ARGS_IFILE].arg.s);
 } else {
  snprintf(tinfo->comment, MAX_COMMENTLEN, "%s", description);
 }

 switch ((enum ERROR_ENUM)setjmp(local_arg->error_jmp)) {
  case ERR_NONE:
   if (args[ARGS_XCHANNELNAME].is_set && args[ARGS_POINTSFASTEST].is_set) {
    int i;
    for (i=0; i<tinfo->nr_of_points; i++) {
     tinfo->xdata[i]=read_value(infile, local_arg, args[ARGS_SWAPBYTEORDER].is_set);
    }
   }

   local_arg->first_in_epoch=TRUE;
   do {
    if (args[ARGS_XCHANNELNAME].is_set && !args[ARGS_POINTSFASTEST].is_set) {
     tinfo->xdata[myarray.current_vector]=read_value(infile, local_arg, args[ARGS_SWAPBYTEORDER].is_set);
    }
    do {
     DATATYPE * const item0_addr=ARRAY_ELEMENT(&myarray);
     int itempart;
     /* Multiple items are always stored side by side: */
     for (itempart=0; itempart<tinfo->itemsize; itempart++) {
      item0_addr[itempart]=read_value(infile, local_arg, args[ARGS_SWAPBYTEORDER].is_set);
     }
     array_advance(&myarray);
     local_arg->first_in_epoch=FALSE;
    } while (myarray.message==ARRAY_CONTINUE);
    if (local_arg->block_gap!=0) fseek(infile, local_arg->block_gap, SEEK_CUR);
   } while (myarray.message!=ARRAY_ENDOFSCAN);
   break;
  case ERR_READ:
   if (local_arg->points_in_file==0) {
    /* While we can avoid this error where we know points_in_file beforehand, the
     * following is needed for a graceful exit on EOF where we don't: */
    if (local_arg->first_in_epoch) {
     TRACEMS(tinfo->emethods, 1, "read_generic: *End of File*\n");
     array_free(&myarray);
     free_pointer((void **)&tinfo->comment);
     return NULL;
    }
   }
  case ERR_NUMBER:
   /* Note that a number_err is always a hard error... */
   ERREXIT(tinfo->emethods, "read_generic: Error reading data\n");
   break;
 }

 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, deepfree_tinfo will free someone else's data ! */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */

 tinfo->z_label=NULL;
 if (args[ARGS_XCHANNELNAME].is_set) tinfo->xchannelname=args[ARGS_XCHANNELNAME].arg.s;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_generic_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_generic_exit(transform_info_ptr tinfo) {
 struct read_generic_storage *local_arg=(struct read_generic_storage *)tinfo->methods->local_storage;

 if (local_arg->datatype==DT_STRING) growing_buf_free(&local_arg->stringbuf);
 if (local_arg->infile!=NULL && local_arg->infile!=stdin) fclose(local_arg->infile);
 local_arg->infile=NULL;
 free_pointer((void **)&local_arg->trigcodes);
 growing_buf_free(&local_arg->triggers);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_generic(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_generic(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_generic_init;
 tinfo->methods->transform= &read_generic;
 tinfo->methods->transform_exit= &read_generic_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_generic";
 tinfo->methods->method_description=
  "Generic get-epoch method. This is for files in which the data\n"
  " is stored in successive elements of a known type.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_generic_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
