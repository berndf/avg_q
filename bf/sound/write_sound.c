/*
 * Copyright (C) 1996,1998,1999,2001-2004,2008,2010,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_sound.c module to write data to some SOX (SOund eXchange) output format
 *	-- Bernd Feige 16.10.1996
 *
 * If (args[ARGS_OFILE].arg.s eq "stdout") tinfo->outfptr=stdout;
 * If (args[ARGS_OFILE].arg.s eq "stderr") tinfo->outfptr=stderr;
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if	defined(__STDC__) || defined(ARM)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "transform.h"
#include "bf.h"
#include "sox.h"
/*}}}  */

/*{{{  Argument defs*/
LOCAL const char *const bits_choice[]={
 "-8", "-16", "-32", "-float", "-double", "-ieee", NULL
};
LOCAL int bits_sizes[]={
 8, 16, 32, 32, 64, 64
};
LOCAL const char *const encoding_choice[]={
 "-u", "-s", "-ul", "-al",
 "-float", "-adpcm", "-ima_adpcm", "-gsm", 
 "-inv_ul", "-inv_al", NULL
};
LOCAL int encoding_codes[]={
 SOX_ENCODING_UNSIGNED, SOX_ENCODING_SIGN2, SOX_ENCODING_ULAW, SOX_ENCODING_ALAW,
 SOX_ENCODING_FLOAT, SOX_ENCODING_MS_ADPCM, SOX_ENCODING_IMA_ADPCM, SOX_ENCODING_GSM,
};
enum ARGS_ENUM {
 ARGS_HELP=0,
 ARGS_BITS,
 ARGS_ENCODING,
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Help - List supported formats and exit", "H", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "Select number of output bits", " ", 1, bits_choice},
 {T_ARGS_TAKES_SELECTION, "Select the output encoding", " ", 2, encoding_choice},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.wav"}
};
/*}}}  */

/*{{{  #defines*/
#define MESSAGE_BUFLEN 1024
/*}}}  */

struct write_sound_storage {
 sox_sample_t *outbuf;
 sox_format_t *outformat;
};

#define EXPORT

EXPORT Bool sox_init_done=FALSE;
EXPORT char *myname;
EXPORT external_methods_ptr sox_emethods;

LOCAL char msgbuf[MESSAGE_BUFLEN];
EXPORT void
avg_q_sox_output_message_handler(
    unsigned level,                       /* 1 = FAIL, 2 = WARN, 3 = INFO, 4 = DEBUG, 5 = DEBUG_MORE, 6 = DEBUG_MOST. */
    LSX_PARAM_IN_Z char const * filename, /* Source code __FILENAME__ from which message originates. */
    LSX_PARAM_IN_PRINTF char const * fmt, /* Message format string. */
    LSX_PARAM_IN va_list ap               /* Message format parameters. */
) {
 vsnprintf(msgbuf,MESSAGE_BUFLEN,fmt,ap);
 TRACEMS(sox_emethods,level-1,msgbuf);
}

/*{{{  write_sound_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_sound_init(transform_info_ptr tinfo) {
 struct write_sound_storage *local_arg=(struct write_sound_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const *ofile=args[ARGS_OFILE].arg.s;
 sox_signalinfo_t signal;
 sox_encodinginfo_t encodingstruct, *encoding=NULL;
 sox_globals_t *sox_globalsp;

 /* This is needed by the error handler: */
 myname=tinfo->methods->method_name;
 sox_emethods=tinfo->emethods;

 if (!sox_init_done) {
  /* SOX doesn't like sox_init() to be called twice --
   * This may occur for example if both read_sound and write_sound are present
   * in a script */
  if (sox_init()!=SOX_SUCCESS) {
   ERREXIT(tinfo->emethods, "write_sound_init: sox init failed.\n");
  }
  sox_init_done=TRUE;
 }
 sox_globalsp=sox_get_globals();
 sox_globalsp->output_message_handler=&avg_q_sox_output_message_handler;

 if (args[ARGS_HELP].is_set) {
  //st_list_formats();
  ERREXIT(tinfo->emethods, "write_sound: Help request by user.\n");
 }

 signal.rate=(sox_rate_t)tinfo->sfreq;
 signal.channels=tinfo->nr_of_channels;
 if (args[ARGS_ENCODING].is_set) {
  encodingstruct.encoding=encoding_codes[args[ARGS_ENCODING].arg.i];
  encodingstruct.bits_per_sample=(args[ARGS_BITS].is_set ? bits_sizes[args[ARGS_BITS].arg.i] : 16);
  encoding=&encodingstruct;
 }
 if ((local_arg->outformat=sox_open_write(ofile, &signal, encoding, NULL, NULL, NULL))==NULL) {
  ERREXIT1(tinfo->emethods, "write_sound_init: Can't open output file '%s'\n", MSGPARM(ofile));
 }

 //local_arg->outformat.comment=tinfo->comment;

 if ((local_arg->outbuf=(sox_sample_t *)malloc(tinfo->nr_of_points*tinfo->nr_of_channels*sizeof(sox_sample_t)))==NULL) {
  ERREXIT(tinfo->emethods, "write_sound_init: Error allocating outbuf memory.\n");
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_sound(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_sound(transform_info_ptr tinfo) {
 struct write_sound_storage *local_arg=(struct write_sound_storage *)tinfo->methods->local_storage;
 sox_sample_t *inoutbuf=local_arg->outbuf;
 array myarray;

 myname=tinfo->methods->method_name;

 tinfo_array(tinfo, &myarray);
 array_transpose(&myarray);	/* Write channels fastest */
 do {
  do {
   *inoutbuf++ = (int)rint(array_scan(&myarray));
  } while (myarray.message==ARRAY_CONTINUE);
 } while (myarray.message==ARRAY_ENDOFVECTOR);
 sox_write(local_arg->outformat, local_arg->outbuf, (size_t) tinfo->nr_of_points*tinfo->nr_of_channels);

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_sound_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_sound_exit(transform_info_ptr tinfo) {
 struct write_sound_storage *local_arg=(struct write_sound_storage *)tinfo->methods->local_storage;

 myname=tinfo->methods->method_name;
 sox_close(local_arg->outformat);
 /* It must be ensured that sox_quit() is called at most once and only after
  * all SOX users are finished - possibly atexit() with extra global variable
  * and such, so we just leave it alone
  *sox_quit(); */
 free_pointer((void **)&local_arg->outbuf);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_sound(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_sound(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_sound_init;
 tinfo->methods->transform= &write_sound;
 tinfo->methods->transform_exit= &write_sound_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_sound";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to any of the sound formats\n"
  " supported by the SOX ('SOund eXchange') package. The output file\n"
  " type is determined from the file extension.\n"
  " Note that SOX data has an order of magnitude of +-2e9.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_sound_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
