/*
 * Copyright (C) 1996-1999,2002,2003,2008,2010 Bernd Feige
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
 * read_sound.c module to read data from some SOX (SOund eXchange) input format
 *	-- Bernd Feige 18.10.1996
 *
 * If (tinfo->filename eq "stdin") tinfo->fileptr=stdin;
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
#include "st_i.h"
/*}}}  */

/*{{{  Patch of code from sox/util.c needed by the DSP drivers (eg oss.c): */
#include <signal.h>
static ft_t ft_queue[2];
static void
sigint(int s) {
    if (s == SIGINT) {
	if (ft_queue[0])
	    ft_queue[0]->file.eof = 1;
	if (ft_queue[1])
	    ft_queue[1]->file.eof = 1;
    }
}
void
sigintreg(ft_t ft) {
    if (ft_queue[0] == 0)
	ft_queue[0] = ft;
    else
	ft_queue[1] = ft;
    signal(SIGINT, sigint);
}
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
 long epochlength;
 long buflen;
 st_sample_t *inbuf;
 struct st_soundstream informat;
};

/*{{{  SOX routines and #defines*/
#define IMPORT
#define READBINARY "rb"
/* From sox.c: */
#define LASTCHAR        '/'

/* These global variables are defined in write_sound.c together with
 * the other SOX bindings: */
IMPORT char *myname;
IMPORT external_methods_ptr sox_emethods;
IMPORT int st_gettype(ft_t formp);
IMPORT int filetype(int fd);

/* This is my own global helper function: */
IMPORT int filetype(int fd);
IMPORT void st_list_formats(void);
/*}}}  */

/*{{{  read_sound_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_sound_init(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const *ifile=args[ARGS_IFILE].arg.s;

 /* This is needed by the error handling kludge: */
 myname=tinfo->methods->method_name;
 sox_emethods=tinfo->emethods;

 if (args[ARGS_HELP].is_set) {
  st_list_formats();
  ERREXIT(tinfo->emethods, "read_sound: Help request by user.\n");
 }

 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);

 if ((local_arg->informat.filetype = strrchr(ifile, LASTCHAR)))
  local_arg->informat.filetype++;
 else
  local_arg->informat.filetype = ifile;
 if ((local_arg->informat.filetype = strrchr(local_arg->informat.filetype, '.')))
  local_arg->informat.filetype++;
 else /* Default to "auto" */
  local_arg->informat.filetype = "auto";

 if (local_arg->informat.filetype!=NULL) {
  if (strcmp(local_arg->informat.filetype, "ossdsp")==0) {
   ifile="/dev/dsp";
  } else if (strcmp(local_arg->informat.filetype, "sunau")==0) {
   ifile="/dev/audio";
  }
 }

 local_arg->informat.filename = ifile;
 if (strcmp(ifile, "stdin")==0)
  local_arg->informat.fp = stdin;
 else if ((local_arg->informat.fp = fopen(ifile, READBINARY)) == NULL)
  st_fail("Can't open input file '%s': %s", 
   ifile, strerror(errno));

 local_arg->informat.seekable  = (filetype(fileno(local_arg->informat.fp)) == S_IFREG);
 local_arg->informat.comment = local_arg->informat.filename;

 st_gettype(&local_arg->informat);
 local_arg->informat.info.channels= -1;
 local_arg->informat.info.encoding = -1;
 local_arg->informat.info.size = -1;
 local_arg->informat.length = -1;
 (* local_arg->informat.h->startread)(&local_arg->informat);
 /* A SOX peculiarity is that the driver might not set the number of channels
  * if it is 1: */ 
 if (local_arg->informat.info.channels== -1) local_arg->informat.info.channels=1;
 tinfo->points_in_file=local_arg->informat.length;

 tinfo->sfreq=local_arg->informat.info.rate;

 /*{{{  Process arguments that can be in seconds*/
 local_arg->beforetrig=(args[ARGS_OFFSET].is_set ? -gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 local_arg->epochlength=gettimeslice(tinfo, args[ARGS_EPOCHLENGTH].arg.s);
 if (local_arg->epochlength==0) {
  /* Read the whole file as one epoch. Within SOX, there is no predefined way
   * to determine the length of the file in samples; We take the route to
   * read the file twice. For this, the file must be seekable. */
  if (local_arg->informat.seekable) {
   st_sample_t *inbuf=(st_sample_t *)malloc(local_arg->informat.info.channels*sizeof(long));
   if (inbuf==NULL) {
    ERREXIT(tinfo->emethods, "read_sound_init: Error allocating temp inbuf memory.\n");
   }
   while (local_arg->buflen!=(*local_arg->informat.h->read)(&local_arg->informat, inbuf, (long)local_arg->informat.info.channels)) {
    local_arg->epochlength++;
   }
   free(inbuf);
   (*local_arg->informat.h->stopread)(&local_arg->informat);
   fseek(local_arg->informat.fp, 0L, SEEK_SET);
   (* local_arg->informat.h->startread)(&local_arg->informat);
   if (local_arg->informat.info.channels== -1) local_arg->informat.info.channels=1;
  } else {
   ERREXIT(tinfo->emethods, "read_sound_init: Can determine the sound length only if the file is seekable.\n");
  }
 }
 local_arg->aftertrig=local_arg->epochlength-local_arg->beforetrig;
 /*}}}  */

 local_arg->buflen=local_arg->epochlength*local_arg->informat.info.channels;
 if ((local_arg->inbuf=(st_sample_t *)malloc(local_arg->buflen*sizeof(long)))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound_init: Error allocating inbuf memory.\n");
 }

 /*{{{  Seek to fromepoch if necessary*/
 if (args[ARGS_FROMEPOCH].is_set) {
  int i;
  for (i=1; i<args[ARGS_FROMEPOCH].arg.i; i++) {
   (*local_arg->informat.h->read)(&local_arg->informat, local_arg->inbuf, (long) local_arg->buflen);
  }
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
 st_sample_t *ininbuf=local_arg->inbuf;

 myname=tinfo->methods->method_name;

 if (local_arg->epochs-- ==0) return NULL;
 if (local_arg->buflen!=(*local_arg->informat.h->read)(&local_arg->informat, local_arg->inbuf, (long) local_arg->buflen)) return NULL;

 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nrofaverages=1;

 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_vectors=tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
 myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->informat.info.channels;
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
 if ((tinfo->comment=(char *)malloc(strlen(local_arg->informat.comment)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_sound: Error allocating comment\n");
 }
 strcpy(tinfo->comment, local_arg->informat.comment);
 /*}}}  */

 tinfo->tsdata=myarray.start;
 tinfo->sfreq=local_arg->informat.info.rate;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_sound_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_sound_exit(transform_info_ptr tinfo) {
 struct read_sound_storage *local_arg=(struct read_sound_storage *)tinfo->methods->local_storage;

 myname=tinfo->methods->method_name;
 (*local_arg->informat.h->stopread)(&local_arg->informat);
 if (local_arg->informat.fp!=stdin) fclose(local_arg->informat.fp);
 local_arg->informat.fp=NULL;
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
