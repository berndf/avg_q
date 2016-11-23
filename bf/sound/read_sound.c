/*
 * Copyright (C) 1996-1999,2002,2003,2008,2010,2013,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_sound.c module to read data from some SOX (SOund eXchange) input format
 *	-- Bernd Feige 18.10.1996
 *
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "transform.h"
#include "bf.h"
#include "sox.h"
/*}}}  */

/*{{{  Argument defs*/
enum ARGS_ENUM {
 ARGS_HELP=0,
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
 {T_ARGS_TAKES_NOTHING, "Help - List supported formats and exit", "H", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.wav"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
};
/*}}}  */

/*{{{  #defines*/
#define MESSAGE_BUFLEN 1024
/*}}}  */

/*{{{  Definition of read_sound_storage*/
struct read_sound_storage {
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 long points_in_file;
 long current_point;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 size_t buflen;
 sox_sample_t *inbuf;
 sox_format_t *informat;
};
/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_sound_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_sound_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_sound_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_sound_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_sound_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
   free_pointer((void **)&description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  TRACEMS(tinfo->emethods, 0, "read_sound_build_trigbuffer: No trigger source known.\n");
 }
}
/*}}}  */
/*{{{  read_sound_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_sound_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_sound_build_trigbuffer(tinfo);
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

#define IMPORT extern

/* These global variables are defined in write_sound.c together with
 * the other SOX bindings: */
IMPORT Bool sox_init_done;
IMPORT char const *myname;
IMPORT external_methods_ptr sox_emethods;
IMPORT void sox_list_formats(external_methods_ptr emethods);

IMPORT void
avg_q_sox_output_message_handler(
    unsigned level,                       /* 1 = FAIL, 2 = WARN, 3 = INFO, 4 = DEBUG, 5 = DEBUG_MORE, 6 = DEBUG_MOST. */
    LSX_PARAM_IN_Z char const * filename, /* Source code __FILENAME__ from which message originates. */
    LSX_PARAM_IN_PRINTF char const * fmt, /* Message format string. */
    LSX_PARAM_IN va_list ap               /* Message format parameters. */
);

/*{{{  read_sound_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_sound_init(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const *ifile=args[ARGS_IFILE].arg.s, *use_ifile="default", *filetype;
 sox_globals_t *sox_globalsp;
 sox_format_handler_t const *handler;

 /* This is needed by the error handler: */
 myname=tinfo->methods->method_name;
 sox_emethods=tinfo->emethods;

 if (!sox_init_done) {
  /* SOX doesn't like sox_init() to be called twice --
   * This may occur for example if both read_sound and write_sound are present
   * in a script */
  if (sox_init()!=SOX_SUCCESS) {
   ERREXIT(tinfo->emethods, "read_sound_init: sox init failed.\n");
  }
  sox_init_done=TRUE;
 }
 sox_globalsp=sox_get_globals();
 sox_globalsp->output_message_handler=&avg_q_sox_output_message_handler;

 handler=sox_write_handler(ifile,NULL,&filetype);
 if (handler!=NULL) {
  use_ifile=ifile;
 } else {
  filetype=lsx_find_file_extension(ifile);
  handler=sox_write_handler(NULL,filetype,NULL);
 }

 /*{{{  Process options*/
 if (args[ARGS_HELP].is_set) {
  sox_list_formats(tinfo->emethods);
  ERREXIT(tinfo->emethods, "read_sound: Help request by user.\n");
 }

 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 if ((local_arg->informat = sox_open_read(use_ifile, NULL, NULL, filetype))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound_init: Can't open file.\n");
 }
 tinfo->sfreq=local_arg->informat->signal.rate;
 local_arg->points_in_file=local_arg->informat->signal.length/local_arg->informat->signal.channels;

 /*{{{  Parse arguments that can be in seconds*/
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
    ERREXIT(tinfo->emethods, "read_sound_init: Error allocating triglist memory\n");
   }
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   if (local_arg->points_in_file==0) {
    ERREXIT(tinfo->emethods, "read_sound: Unable to determine the number of samples in the input file!\n");
   }
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }

 local_arg->buflen=(local_arg->beforetrig+local_arg->aftertrig)*local_arg->informat->signal.channels;
 if ((local_arg->inbuf=(sox_sample_t *)malloc(local_arg->buflen*sizeof(sox_sample_t)))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound_init: Error allocating inbuf memory.\n");
 }

 read_sound_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_sound(transform_info_ptr tinfo) {*/
/*
 * The method read_sound() returns a pointer to the allocated tsdata memory
 * location or NULL if End Of File encountered. EOF within the data set
 * results in an ERREXIT; NULL is returned ONLY if the first read resulted
 * in an EOF condition. This allows programs to look for multiple data sets
 * in a single file.
 */

METHODDEF DATATYPE *
read_sound(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 sox_sample_t *ininbuf=local_arg->inbuf;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 myname=tinfo->methods->method_name;

 if (local_arg->epochs-- ==0) return NULL;

 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->informat->signal.channels;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_sound: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
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
   tinfo->condition=read_sound_read_trigger(tinfo, &trigger_point, &description);
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
  TRACEMS3(tinfo->emethods, 1, "read_sound: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_sound: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
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
  read_sound_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_sound_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /* Move the file pointer to the desired starting position */
 if (sox_seek(local_arg->informat, file_start_point*local_arg->informat->signal.channels, SOX_SEEK_SET) != SOX_SUCCESS) {
  ERREXIT(tinfo->emethods, "read_sound_init: sox seek failed.\n");
 }
 if (local_arg->buflen!=sox_read(local_arg->informat, local_arg->inbuf, local_arg->buflen)) return NULL;


 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 myarray.nr_of_elements=tinfo->nr_of_channels;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_sound: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_sound: Error allocating data\n");
 }
 /* Byte order is multiplexed, ie channels fastest */
 tinfo->multiplexed=TRUE;
 /*}}}  */

 do {
  do {
   array_write(&myarray, (DATATYPE) *ininbuf++);
  } while (myarray.message==ARRAY_CONTINUE);
 } while (myarray.message==ARRAY_ENDOFVECTOR);

 /*{{{  Allocate and copy comment; Set positions on a grid*/
 tinfo->z_label=NULL;
 tinfo->xdata=NULL;
 tinfo->channelnames=NULL;
 tinfo->probepos=NULL;
 create_channelgrid(tinfo);
 if ((tinfo->comment=(char *)malloc(strlen(local_arg->informat->handler.description)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound: Error allocating comment\n");
 }
 strcpy(tinfo->comment, local_arg->informat->handler.description);
 /*}}}  */

 tinfo->file_start_point=file_start_point;
 tinfo->sfreq=local_arg->informat->signal.rate;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_sound_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_sound_exit(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;

 myname=tinfo->methods->method_name;
 sox_close(local_arg->informat);
 /* It must be ensured that sox_quit() is called at most once and only after
  * all SOX users are finished - possibly atexit() with extra global variable
  * and such, so we just leave it alone
  *sox_quit(); */
 free_pointer((void **)&local_arg->inbuf);

 free_pointer((void **)&local_arg->trigcodes);

 if (local_arg->triggers.buffer_start!=NULL) {
  clear_triggers(&local_arg->triggers);
  growing_buf_free(&local_arg->triggers);
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_sound(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_sound(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_sound_init;
 tinfo->methods->transform= &read_sound;
 tinfo->methods->transform_exit= &read_sound_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_sound";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from any of the sound formats\n"
  " supported by the SOX ('SOund eXchange') package.\n"
  " Note that SOX data has an order of magnitude of +-2e9.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_sound_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
