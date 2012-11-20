/*
 * Copyright (C) 2000,2002,2003,2007,2011 Bernd Feige
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

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <string.h>
#include <read_struct.h>
#ifdef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"

#include "RECfile.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { EDFFILE=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]
#undef ERREXIT
#define ERREXIT(dummy, message) fprintf(stderr, message); exit(1)

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

int 
main(int argc, char **argv) {
 REC_file_header fileheader;
 REC_channel_header channelheader;
 long channelheader_length;
 int channel, dd, mm, yy, hh, mi, ss;
 long nr_of_channels, bytes_in_header, nr_of_records;
 FILE *infile;
 int samples_per_record;
 int total_samples_per_record;
 int max_samples_per_record;
 char * filename;
 int errflag=0, c;
 struct stat statbuff;

 /*{{{  Process command line*/
 mainargv=argv;
 while ((c=getopt(argc, argv, ""))!=EOF) {
  switch (c) {
   case '?':
   default:
    errflag++;
    continue;
  }
 }

 if (argc-optind!=END_OF_ARGS || errflag>0) {
  fprintf(stderr, "Usage: %s [options] EDF_filename\n"
  , argv[0]);
  exit(1);
 }

 filename=MAINARG(EDFFILE);
 infile=fopen(filename,"rb");
 if(infile==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }
 /*}}}  */

 printf("EDF file %s\n\n", filename);

 /*{{{  Read the file header and build comment*/
 if (read_struct((char *)&fileheader, sm_REC_file, infile)==0) {
  ERREXIT(tinfo->emethods, "dump_edf_header: Error reading file header.\n");
 }
# ifndef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader, sm_REC_file);
# endif
 print_structcontents((char *)&fileheader, sm_REC_file, smd_REC_file, stdout);
 nr_of_channels=atoi(fileheader.nr_of_channels);
 dd=read_2_digit_integer(fileheader.startdate);
 mm=read_2_digit_integer(fileheader.startdate+3);
 yy=read_2_digit_integer(fileheader.startdate+6);
 hh=read_2_digit_integer(fileheader.starttime);
 mi=read_2_digit_integer(fileheader.starttime+3);
 ss=read_2_digit_integer(fileheader.starttime+6);
 printf("Date: %02d/%02d/%02d,%02d:%02d:%02d\n", mm, dd, yy, hh, mi, ss);
 /*}}}  */
 /*{{{  Read the channel header*/
 /* Length of the file header is included in this number... */
 bytes_in_header=atoi(fileheader.bytes_in_header);
 nr_of_records=atoi(fileheader.nr_of_records);
 channelheader_length=bytes_in_header-sm_REC_file[0].offset;
 if ((channelheader.label=(char (*)[16])malloc(channelheader_length))==NULL) {
  ERREXIT(tinfo->emethods, "dump_edf_header: Error allocating channelheader memory\n");
 }
 if (nr_of_channels!=(int)fread((void *)channelheader.label, CHANNELHEADER_SIZE_PER_CHANNEL, nr_of_channels, infile)) {
  ERREXIT(tinfo->emethods, "dump_edf_header: Error reading channel headers\n");
 }
 channelheader.transducer=(char (*)[80])(channelheader.label+nr_of_channels);
 channelheader.dimension=(char (*)[8])(channelheader.transducer+nr_of_channels);
 channelheader.physmin=(char (*)[8])(channelheader.dimension+nr_of_channels);
 channelheader.physmax=(char (*)[8])(channelheader.physmin+nr_of_channels);
 channelheader.digmin=(char (*)[8])(channelheader.physmax+nr_of_channels);
 channelheader.digmax=(char (*)[8])(channelheader.digmin+nr_of_channels);
 channelheader.prefiltering=(char (*)[80])(channelheader.digmax+nr_of_channels);
 channelheader.samples_per_record=(char (*)[8])(channelheader.prefiltering+nr_of_channels);
 channelheader.reserved=(char (*)[32])(channelheader.samples_per_record+nr_of_channels);
 /*}}}  */

 /*{{{  Allocate and fill local arrays*/
 max_samples_per_record=total_samples_per_record=0;
 for (channel=0; channel<nr_of_channels; channel++) {
  One_REC_channel_header header;

  printf("\nChannel number %d (Offset within record: %d):\n", channel+1, total_samples_per_record*sizeof(int16_t));
  strncpy(header.label, channelheader.label[channel], 16);
  strncpy(header.transducer, channelheader.transducer[channel], 80);
  strncpy(header.dimension, channelheader.dimension[channel], 8);
  strncpy(header.physmin, channelheader.physmin[channel], 8);
  strncpy(header.physmax, channelheader.physmax[channel], 8);
  strncpy(header.digmin, channelheader.digmin[channel], 8);
  strncpy(header.digmax, channelheader.digmax[channel], 8);
  strncpy(header.prefiltering, channelheader.prefiltering[channel], 80);
  strncpy(header.samples_per_record, channelheader.samples_per_record[channel], 8);
  strncpy(header.reserved, channelheader.reserved[channel], 32);
  print_structcontents((char *)&header, sm_REC_channel_header, smd_REC_channel_header, stdout);

  samples_per_record=atoi(channelheader.samples_per_record[channel]);
  total_samples_per_record+=samples_per_record;
  if (samples_per_record>max_samples_per_record) max_samples_per_record=samples_per_record;
 }
 printf("\nMax samples per record: %d\n", max_samples_per_record);
 printf("Total samples per record: %d -> Size of 1 record %d bytes.\n", total_samples_per_record, total_samples_per_record*sizeof(int16_t));
 printf("Position after header is %ld, bytes_in_header field was %ld\n", ftell(infile), bytes_in_header);

 stat(filename,&statbuff);
 fstat(fileno(infile),&statbuff);
 printf("File size is %ld, expected for %ld records is %ld\n", statbuff.st_size, nr_of_records, nr_of_records*total_samples_per_record*sizeof(int16_t)+bytes_in_header);
 free((void *)channelheader.label);
 /*}}}  */

 fclose(infile);

 return 0;
}
