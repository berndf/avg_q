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

/*{{{  #defines*/
enum ARGS_ENUM {
 ARGS_HELP=0,
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 ARGS_EPOCHLENGTH,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Help - List supported formats and exit", "H", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.wav"},
 {T_ARGS_TAKES_STRING_WORD, "epochlength", "", ARGDESC_UNUSED, (const char *const *)"0"}
};
/*}}}  */

struct read_sound_storage {
 long beforetrig;
 long aftertrig;
 int fromepoch;
 int epochs;
 int current_epoch;
 long epochlength;
 size_t buflen;
 sox_sample_t *inbuf;
 sox_format_t *informat;
};

#define IMPORT extern

/* These global variables are defined in write_sound.c together with
 * the other SOX bindings: */
IMPORT Bool sox_init_done;
IMPORT char *myname;
IMPORT external_methods_ptr sox_emethods;
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
 char const *ifile=args[ARGS_IFILE].arg.s;
 sox_globals_t *sox_globalsp;

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

 if (args[ARGS_HELP].is_set) {
  //st_list_formats();
  ERREXIT(tinfo->emethods, "read_sound: Help request by user.\n");
 }

 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);

 if ((local_arg->informat = sox_open_read(ifile, NULL, NULL, NULL))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound_init: Can't open file.\n");
 }

 tinfo->points_in_file=local_arg->informat->signal.length/local_arg->informat->signal.channels;

 tinfo->sfreq=local_arg->informat->signal.rate;

 /*{{{  Process arguments that can be in seconds*/
 local_arg->beforetrig=(args[ARGS_OFFSET].is_set ? -gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 local_arg->epochlength=gettimeslice(tinfo, args[ARGS_EPOCHLENGTH].arg.s);
 if (local_arg->epochlength==0) {
  /* Read the whole file as one epoch. */
  local_arg->epochlength=tinfo->points_in_file;
 }
 local_arg->aftertrig=local_arg->epochlength-local_arg->beforetrig;
 /*}}}  */

 local_arg->buflen=local_arg->epochlength*local_arg->informat->signal.channels;
 if ((local_arg->inbuf=(sox_sample_t *)malloc(local_arg->buflen*sizeof(sox_sample_t)))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound_init: Error allocating inbuf memory.\n");
 }

 /*{{{  Seek to fromepoch if necessary*/
 if (args[ARGS_FROMEPOCH].is_set) {
  uint64_t seek= (args[ARGS_FROMEPOCH].arg.i-1)*local_arg->epochlength*local_arg->informat->signal.channels;
  /* Move the file pointer to the desired starting position */
  if (sox_seek(local_arg->informat, seek, SOX_SEEK_SET) != SOX_SUCCESS) {
   ERREXIT(tinfo->emethods, "read_sound_init: sox init failed.\n");
  }
  local_arg->current_epoch=args[ARGS_FROMEPOCH].arg.i-1;
 } else {
  local_arg->current_epoch=0;
 }
 /*}}}  */

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
 array myarray;
 sox_sample_t *ininbuf=local_arg->inbuf;

 myname=tinfo->methods->method_name;

 if (local_arg->epochs-- ==0) return NULL;
 if (local_arg->buflen!=sox_read(local_arg->informat, local_arg->inbuf, local_arg->buflen)) return NULL;

 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nrofaverages=1;

 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_vectors=tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
 myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->informat->signal.channels;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_sound: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
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

 tinfo->file_start_point=local_arg->current_epoch*tinfo->nr_of_points;
 tinfo->tsdata=myarray.start;
 tinfo->sfreq=local_arg->informat->signal.rate;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;
 local_arg->current_epoch++;

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
