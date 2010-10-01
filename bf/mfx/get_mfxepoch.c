/*
 * Copyright (C) 1996-1999,2001-2003,2008 Bernd Feige
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
/*
 * get_mfxepoch.c method to read epoch data from an mfx file
 *	-- Bernd Feige 11.02.1993
 *
 * Uses the filename, channels, trigcode and other tinfo entries to
 * open an mfx file (init) and get the single epochs of raw data.
 * As every get_epoch method, returns NULL on End Of File.
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "mfx_file.h"
/*}}}  */

#ifdef FLOAT_DATATYPE
#define MFX_DATATYPE MFX_FLOATS
#endif
#ifdef DOUBLE_DATATYPE
#define MFX_DATATYPE MFX_DOUBLES
#endif

enum ARGS_ENUM {
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_TRIGNAME,
 ARGS_IFILE,
 ARGS_CHANNELNAMES,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 ARGS_TRIGCODE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "trigname: Use channel trigname as trigger channel. 0=epoch mode", "t", ARGDESC_UNUSED, (const char *const *)"TRIGGER"},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.mfx"},
 {T_ARGS_TAKES_STRING_WORD, "channelnames", "", ARGDESC_UNUSED, (const char *const *)"A1-A37"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_LONG, "trigcode", "", 0, NULL}
};

/*{{{  Define the local storage struct*/
struct get_mfxepoch_storage {
 MFX_FILE *fileptr;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 char const *trigname;
};
/*}}}  */ 

/*{{{  get_mfxepoch_init(transform_info_ptr tinfo)*/
METHODDEF void
get_mfxepoch_init(transform_info_ptr tinfo) {
 struct get_mfxepoch_storage *local_arg=(struct get_mfxepoch_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int skipepochs;

 if ((local_arg->fileptr=mfx_open(args[ARGS_IFILE].arg.s, "rb", MFX_DATATYPE))==NULL) {
  ERREXIT2(tinfo->emethods, "get_mfxepoch_init: Can't open mfx file %s: %s\n", 
	   MSGPARM(args[ARGS_IFILE].arg.s), MSGPARM(mfx_errors[mfx_lasterr]));
 }
 if ((tinfo->nr_of_channels=mfx_cselect(local_arg->fileptr, args[ARGS_CHANNELNAMES].arg.s))==0) {
  ERREXIT1(tinfo->emethods, "get_mfxepoch_init: mfx_cselect error %s\n", 
	   MSGPARM(mfx_errors[mfx_lasterr]));
 }

 get_mfxinfo(local_arg->fileptr, tinfo);
 get_mfxinfo_free(tinfo); /* We don't need the channel names etc. here */

 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 local_arg->trigname=(args[ARGS_TRIGNAME].is_set ? args[ARGS_TRIGNAME].arg.s : "TRIGGER");
 if (strcmp(local_arg->trigname, "0")==0) {
  local_arg->trigname=NULL;
  local_arg->offset-= (long int)rint(local_arg->fileptr->fileheader.epoch_begin_latency*tinfo->sfreq);
  local_arg->beforetrig+=local_arg->offset;
  local_arg->aftertrig-=local_arg->offset;
 }

 skipepochs=local_arg->fromepoch-1;
 /*{{{  Skip triggers if fromepoch parameter demands it*/
 while (skipepochs>0) {
  mfx_seektrigger(local_arg->fileptr, local_arg->trigname, args[ARGS_TRIGCODE].arg.i);
  if (mfx_lasterr!=MFX_NOERR) {
   ERREXIT(tinfo->emethods, "get_mfxepoch_init: Error seeking to first trigger\n");
  }
  skipepochs--;
 }
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  get_mfxepoch(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
get_mfxepoch(transform_info_ptr tinfo) {
 struct get_mfxepoch_storage *local_arg=(struct get_mfxepoch_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE *newepoch;
 int again=FALSE;
 long epochlength;

 if (local_arg->epochs--==0) return NULL;

 do {
  again=FALSE;
  if ((newepoch=(DATATYPE *)mfx_getepoch(local_arg->fileptr, &epochlength, local_arg->beforetrig-local_arg->offset, local_arg->aftertrig+local_arg->offset, local_arg->trigname, args[ARGS_TRIGCODE].arg.i))==NULL) {
   /* MFX_READERR4 is the normal end-of-file 'error' */
   TRACEMS1(tinfo->emethods, (mfx_lasterr==MFX_READERR4 ? 1 : 0), "get_mfxepoch: mfx error >%s<\n", MSGPARM(mfx_errors[mfx_lasterr]));
   /* If the error was due to the beforetrig time being too long, try to
    * skip this epoch and try the next one */
   if (mfx_lasterr==MFX_NEGSEEK) {
    again=TRUE;
    mfx_seektrigger(local_arg->fileptr, local_arg->trigname, args[ARGS_TRIGCODE].arg.i);
    TRACEMS(tinfo->emethods, 1, "get_mfxepoch: Retrying...\n");
   }
  }
 } while (again);
 if (newepoch!=NULL) {
  /* TOUCH TINFO ONLY IF GETEPOCH WAS SUCCESSFUL */
  get_mfxinfo(local_arg->fileptr, tinfo);
  tinfo->z_label=NULL;
  tinfo->nr_of_points=epochlength;
  tinfo->length_of_output_region=epochlength*tinfo->nr_of_channels;
  tinfo->beforetrig=local_arg->beforetrig;
  tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
  tinfo->multiplexed=TRUE;
  tinfo->sfreq=1.0/(local_arg->fileptr)->fileheader.sample_period;
  tinfo->data_type=TIME_DATA;
  tinfo->itemsize=1;	/* Still, mfx doesn't support tuple data */
  tinfo->leaveright=0;
  tinfo->nrofaverages=1;
 }
 return newepoch;
}
/*}}}  */

/*{{{  get_mfxepoch_exit(transform_info_ptr tinfo)*/
METHODDEF void
get_mfxepoch_exit(transform_info_ptr tinfo) {
 struct get_mfxepoch_storage *local_arg=(struct get_mfxepoch_storage *)tinfo->methods->local_storage;
 mfx_close(local_arg->fileptr);
 local_arg->fileptr=NULL;
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_get_mfxepoch(transform_info_ptr tinfo)*/
GLOBAL void
select_get_mfxepoch(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &get_mfxepoch_init;
 tinfo->methods->transform= &get_mfxepoch;
 tinfo->methods->transform_exit= &get_mfxepoch_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="get_mfxepoch";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from mfx files.\n";
 tinfo->methods->local_storage_size=sizeof(struct get_mfxepoch_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
