/*
 * Copyright (C) 1996,1998,1999,2001-2004,2008,2010 Bernd Feige
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
#include "st_i.h"
/*}}}  */

/*{{{  Argument defs*/
LOCAL const char *const bits_choice[]={
 "-8", "-16", "-32", "-float", "-double", "-ieee", NULL
};
LOCAL int bits_sizes[]={
 ST_SIZE_8BIT, ST_SIZE_16BIT, ST_SIZE_32BIT, ST_SIZE_32BIT, ST_SIZE_64BIT, ST_SIZE_64BIT
};
LOCAL const char *const encoding_choice[]={
 "-u", "-s", "-ul", "-al",
 "-float", "-adpcm", "-ima_adpcm", "-gsm", 
 "-inv_ul", "-inv_al", NULL
};
LOCAL int encoding_codes[]={
 ST_ENCODING_UNSIGNED, ST_ENCODING_SIGN2, ST_ENCODING_ULAW, ST_ENCODING_ALAW,
 ST_ENCODING_FLOAT, ST_ENCODING_ADPCM, ST_ENCODING_IMA_ADPCM, ST_ENCODING_GSM,
 ST_ENCODING_INV_ULAW, ST_ENCODING_INV_ALAW
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
 st_sample_t *outbuf;
 struct st_soundstream outformat;
};

/*{{{  SOX routines and #defines*/
#define EXPORT
#define WRITEBINARY "wb"
/* From sox.c: */
#define LASTCHAR        '/'

/* We need to replicate some global things from util.c here, otherwise we
 * would not be able to incorporate sox's error and report mechanism. 
 * Nothing should be needed from util.c, otherwise linking will fail. */
EXPORT float volume = 1.0;	/* expansion coefficient */
EXPORT int dovolume = 0;
EXPORT int verbose = 0;	/* be noisy on stderr */
EXPORT char *myname;
LOCAL char msgbuf[MESSAGE_BUFLEN];
EXPORT external_methods_ptr sox_emethods;

/* This one is needed by both write_sound and read_sound: */
EXPORT int filetype(int fd) {
 struct stat st;
 fstat(fd, &st);
 return st.st_mode & S_IFMT;
}
LOCAL int strcmpcase(char *s1, char *s2) {
 while(*s1 && *s2 && (tolower(*s1) == tolower(*s2))) {
  s1++, s2++;
 }
 return *s1 - *s2;
}
EXPORT void
st_report(const char *fmt, ...)
{
 va_list args;

 if (! verbose)
	 return;

 va_start(args, fmt);
 vsnprintf(msgbuf, MESSAGE_BUFLEN, fmt, args);
 /* Set this to tracelevel 2, so that we have a chance to avoid the 'reports' */
 TRACEMS2(sox_emethods, 2, "%s: %s\n", MSGPARM(myname), MSGPARM(msgbuf));
 va_end(args);
}
EXPORT void
st_warn(const char *fmt, ...)
{
 va_list args;

 va_start(args, fmt);
 vsnprintf(msgbuf, MESSAGE_BUFLEN, fmt, args);
 va_end(args);
 TRACEMS2(sox_emethods, 0, "%s: %s\n", MSGPARM(myname), MSGPARM(msgbuf));
}
EXPORT void
st_fail(const char *fmt, ...)
{
 va_list args;

 va_start(args, fmt);
 vsnprintf(msgbuf, MESSAGE_BUFLEN, fmt, args);
 va_end(args);
 /* Note that util.c calls cleanup() here, which is def'd in sox.c. 
  * We don't have such a cleanup mechanism in avg_q (yet). */ 
 ERREXIT2(sox_emethods, "%s: %s\n", MSGPARM(myname), MSGPARM(msgbuf));
}
EXPORT void
st_fail_errno(ft_t ft, int st_errno, const char *fmt, ...)
{
 va_list args;

 ft->st_errno = st_errno;

 va_start(args, fmt);
 vsnprintf(ft->st_errstr, 256, fmt, args);
 va_end(args);
 ft->st_errstr[255] = '\0';
}
/*
 * Check that we have a known format suffix string.
 */
EXPORT int
st_gettype(ft_t formp)
{
 char **list;
 int i;

 if (! formp->filetype)
st_fail("Must give file type for %s file, either as suffix or with -t option",
formp->filename);
 for(i = 0; st_formats[i].names; i++) {
  for(list = st_formats[i].names; *list; list++) {
   char *s1 = *list, *s2 = formp->filetype;
   if (! strcmpcase(s1, s2))
    break; /* not a match */
  }
  if (! *list)
   continue;
  /* Found it! */
  formp->h = &st_formats[i];
  return ST_SUCCESS;
 }
 if (! strcmpcase(formp->filetype, "snd")) {
  verbose = 1;
  st_report("File type '%s' is used to name several different formats.", formp->filetype);
  st_report("If the file came from a Macintosh, it is probably");
  st_report("a .ub file with a sample rate of 11025 (or possibly 5012 or 22050).");
  st_report("Use the sequence '-t .ub -r 11025 file.snd'");
  st_report("If it came from a PC, it's probably a Soundtool file.");
  st_report("Use the sequence '-t .sndt file.snd'");
  st_report("If it came from a NeXT, it's probably a .au file.");
  st_fail("Use the sequence '-t .au file.snd'\n");
 }
 st_fail("File type '%s' of %s file is not known!",
  formp->filetype, formp->filename);
 return ST_EFMT;
}
EXPORT int
st_is_bigendian(void)
{
    int b;
    char *p;

    b = 1;
    p = (char *) &b;
    if (!*p)
        return 1;
    else
        return 0;
}

EXPORT int
st_is_littleendian(void)
{
    int b;
    char *p;

    b = 1;
    p = (char *) &b;
    if (*p)
        return 1;
    else
        return 0;
}
/*
 * st_parsesamples
 *
 * Parse a string for # of samples.  If string ends with a 's'
 * then string is interrepted as a user calculated # of samples.
 * If string contains ':' or '.' or if it ends with a 't' then its
 * treated as an amount of time.  This is converted into seconds and
 * fraction of seconds and then use the sample rate to calculate
 * # of samples.
 * Returns ST_EOF on error.
 */
EXPORT int
st_parsesamples(st_rate_t rate, char *str, st_size_t *samples, char def)
{
    int found_samples = 0, found_time = 0;
    int time;
    long long_samples;
    float frac = 0;

    if (strchr(str, ':') || strchr(str, '.') || str[strlen(str)-1] == 't')
        found_time = 1;
    else if (str[strlen(str)-1] == 's')
        found_samples = 1;

    if (found_time || (def == 't' && !found_samples))
    {
        *samples = 0;

        while(1)
        {
            if (sscanf(str, "%d", &time) != 1)
                return ST_EOF;
            *samples += time;

            while (*str != ':' && *str != '.' && *str != 0)
                str++;

            if (*str == '.' || *str == 0)
                break;

            /* Skip past ':' */
            str++;
            *samples *= 60;
        }

        if (*str == '.')
        {
            if (sscanf(str, "%f", &frac) != 1)
                return ST_EOF;
        }

        *samples *= rate;
        *samples += (unsigned int)rint(rate * frac);
        return ST_SUCCESS;
    }
    if (found_samples || (def == 's' && !found_time))
    {
        if (sscanf(str, "%ld", &long_samples) != 1)
            return ST_EOF;
        *samples = long_samples;
        return ST_SUCCESS;
    }
    return ST_EOF;
}
/*}}}  */

/*{{{  Own global functions*/
EXPORT void
st_list_formats(void) {
 int i;
 TRACEMS(sox_emethods, -1, "Supported SOX library formats:\n");
 for (i = 0; st_formats[i].names != NULL; i++) {
  /* only print the first name */
  snprintf(msgbuf, MESSAGE_BUFLEN, "%s ", st_formats[i].names[0]);
  TRACEMS(sox_emethods, -1, msgbuf);
 }
 TRACEMS(sox_emethods, -1, "\n");
}
/*}}}  */

/*{{{  write_sound_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_sound_init(transform_info_ptr tinfo) {
 struct write_sound_storage *local_arg=(struct write_sound_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const *ofile=args[ARGS_OFILE].arg.s;

 /* This is needed by the error handling kludge: */
 myname=tinfo->methods->method_name;
 sox_emethods=tinfo->emethods;

 if (args[ARGS_HELP].is_set) {
  st_list_formats();
  ERREXIT(tinfo->emethods, "write_sound: Help request by user.\n");
 }

 if ((local_arg->outformat.filetype = strrchr(ofile, LASTCHAR)))
  local_arg->outformat.filetype++;
 else
  local_arg->outformat.filetype = ofile;
 if ((local_arg->outformat.filetype = strrchr(local_arg->outformat.filetype, '.'))) {
  local_arg->outformat.filetype++;
 }

 if (local_arg->outformat.filetype!=NULL) {
  if (strcmp(local_arg->outformat.filetype, "ossdsp")==0) {
   ofile="/dev/dsp";
  } else if (strcmp(local_arg->outformat.filetype, "sunau")==0) {
   ofile="/dev/audio";
  }
 }

 local_arg->outformat.filename = ofile;
 /*
  * There are two choices here:
  * 1) stomp the old file - normal shell "> file" behavior
  * 2) fail if the old file already exists - csh mode
  */
 if (strcmp(ofile, "stdout")==0) {
  local_arg->outformat.fp = stdout;
 } else if (strcmp(ofile, "stderr")==0) {
  local_arg->outformat.fp = stderr;
 } else {
#ifdef unix
   /* 
   * Remove old file if it's a text file, but 
    * preserve Unix /dev/sound files.  I'm not sure
   * this needs to be here, but it's not hurting
   * anything.
   */
  if ((local_arg->outformat.fp = fopen(ofile, WRITEBINARY)) && 
      (filetype(fileno(local_arg->outformat.fp)) == S_IFREG)) {
   fclose(local_arg->outformat.fp);
   unlink(ofile);
   creat(ofile, 0666);
   local_arg->outformat.fp = fopen(ofile, WRITEBINARY);
  }
#else
  local_arg->outformat.fp = fopen(ofile, WRITEBINARY);
#endif
  if (local_arg->outformat.fp == NULL)
   ERREXIT1(tinfo->emethods, "write_sound_init: Can't open output file '%s'\n", MSGPARM(ofile));
 }

 local_arg->outformat.seekable = (filetype(fileno(local_arg->outformat.fp)) == S_IFREG); 
 local_arg->outformat.info.rate=(st_rate_t)tinfo->sfreq;
 local_arg->outformat.info.size=(args[ARGS_BITS].is_set ? bits_sizes[args[ARGS_BITS].arg.i] : ST_SIZE_16BIT);
 local_arg->outformat.info.encoding=(args[ARGS_ENCODING].is_set ? encoding_codes[args[ARGS_ENCODING].arg.i] : -1);
 local_arg->outformat.info.channels=tinfo->nr_of_channels;
 local_arg->outformat.comment=tinfo->comment;

 st_gettype(&local_arg->outformat);

 (*local_arg->outformat.h->startwrite)(&local_arg->outformat);
 
 if ((local_arg->outbuf=(st_sample_t *)malloc(tinfo->nr_of_points*tinfo->nr_of_channels*sizeof(long)))==NULL) {
  ERREXIT(tinfo->emethods, "write_sound_init: Error allocating outbuf memory.\n");
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_sound(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_sound(transform_info_ptr tinfo) {
 struct write_sound_storage *local_arg=(struct write_sound_storage *)tinfo->methods->local_storage;
 st_sample_t *inoutbuf=local_arg->outbuf;
 array myarray;

 myname=tinfo->methods->method_name;

 tinfo_array(tinfo, &myarray);
 array_transpose(&myarray);	/* Write channels fastest */
 do {
  do {
   *inoutbuf++ = (int)rint(array_scan(&myarray));
  } while (myarray.message==ARRAY_CONTINUE);
 } while (myarray.message==ARRAY_ENDOFVECTOR);
 (*local_arg->outformat.h->write)(&local_arg->outformat, local_arg->outbuf, (long) tinfo->nr_of_points*tinfo->nr_of_channels);

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_sound_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_sound_exit(transform_info_ptr tinfo) {
 struct write_sound_storage *local_arg=(struct write_sound_storage *)tinfo->methods->local_storage;

 myname=tinfo->methods->method_name;
 (*local_arg->outformat.h->stopwrite)(&local_arg->outformat);
 if (local_arg->outformat.fp!=stdout && local_arg->outformat.fp!=stderr) {
  fclose(local_arg->outformat.fp);
 } else {
  fflush(local_arg->outformat.fp);
 }
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
