/*
 * Copyright (C) 2017 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
#include <ctype.h>
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
#define MAINARG(x) mainargv[optind+x]
#undef ERREXIT
#define ERREXIT(dummy, message) fprintf(stderr, message); exit(1)

#define NUMBUF_LENGTH 81

/*{{{  copy_string(char *dest, char const *source) {*/
/* Function to copy a string the REC way: without the trailing \0 */
LOCAL void
copy_nstring(char *dest, char const *source, size_t n) {
 while(*source!='\0' && n>0) {
  *dest++ = *source++;
  n--;
 }
}
/*}}}  */

int 
main(int argc, char **argv) {
 REC_file_header fileheader;
 REC_channel_header channelheader;
 long channelheader_length,record_size=0;
 int channel, infile_ind;
 long nr_of_channels, bytes_in_header, nr_of_records, total_records, record;
 FILE *infile, *outfile;
 int samples_per_record;
 int total_samples_per_record;
 char * filename;
 void *recordbuf=NULL;
 int errflag=0, c;
 char numbuf[NUMBUF_LENGTH];

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

 if (argc-optind<2 || errflag>0) {
  fprintf(stderr, "Usage: %s [options] EDF_filename1 [emptyrecs] EDF_filename2 ... EDF_outfile\n"
  , argv[0]);
  exit(1);
 }
 /*}}}  */

 filename=MAINARG(argc-optind-1);
 outfile=fopen(filename,"wb");
 if(outfile==NULL) {
  fprintf(stderr, "%s: Can't open output file %s\n", argv[0], filename);
  exit(1);
 }
 for (infile_ind=0; infile_ind<argc-optind-1; infile_ind++) {
  char *infilename;
  filename=MAINARG(infile_ind);
  for (infilename=filename; isdigit(*infilename); infilename++);
  if (*infilename=='\0') {
   /* all-digit argument: output this number of empty records */
   if (recordbuf==NULL) {
    ERREXIT(tinfo->emethods, "join_edf: recordbuf not yet initialized.\n");
   }
   memset(recordbuf, '\0', record_size);
   nr_of_records=atoi(filename);
   printf("Empty: %d records of %ld bytes.\n", nr_of_records, record_size);
   for (record=0; record<nr_of_records; record++) {
    if (1!=(int)fwrite(recordbuf, record_size, 1, outfile)) {
     ERREXIT(tinfo->emethods, "join_edf: Error writing record\n");
    }
    total_records++;
   }
   continue;
  }
  infile=fopen(filename,"rb");
  if(infile==NULL) {
   fprintf(stderr, "%s: Can't open input file %s\n", argv[0], filename);
   exit(1);
  }

  /*{{{  Read the file header */
  if (read_struct((char *)&fileheader, sm_REC_file, infile)==0) {
   ERREXIT(tinfo->emethods, "join_edf: Error reading file header.\n");
  }
# ifndef LITTLE_ENDIAN
  change_byteorder((char *)&fileheader, sm_REC_file);
# endif
  nr_of_channels=atoi(fileheader.nr_of_channels);
  /*}}}  */
  /*{{{  Read the channel header*/
  /* Length of the file header is included in this number... */
  bytes_in_header=atoi(fileheader.bytes_in_header);
  nr_of_records=atoi(fileheader.nr_of_records);
  channelheader_length=bytes_in_header-sm_REC_file[0].offset;
  if ((channelheader.label=(char (*)[16])malloc(channelheader_length))==NULL) {
   ERREXIT(tinfo->emethods, "join_edf: Error allocating channelheader memory\n");
  }
  if (nr_of_channels!=(int)fread((void *)channelheader.label, CHANNELHEADER_SIZE_PER_CHANNEL, nr_of_channels, infile)) {
   ERREXIT(tinfo->emethods, "join_edf: Error reading channel headers\n");
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

  total_samples_per_record=0;
  for (channel=0; channel<nr_of_channels; channel++) {
   samples_per_record=atoi(channelheader.samples_per_record[channel]);
   total_samples_per_record+=samples_per_record;
  }
  record_size=total_samples_per_record*sizeof(int16_t);
  if (recordbuf==NULL) {
   /* Before copying the first record, allocate buffer, write headers to outfile */
   if ((recordbuf=malloc(record_size))==NULL) {
    ERREXIT(tinfo->emethods, "join_edf: Error allocating recordbuf memory\n");
   }
   /* Write the file header */
   if (write_struct((char *)&fileheader, sm_REC_file, outfile)==0) {
    ERREXIT(tinfo->emethods, "join_edf: Error writing file header.\n");
   }
   /* Write the channel header */
   if (nr_of_channels!=(int)fwrite((void *)channelheader.label, CHANNELHEADER_SIZE_PER_CHANNEL, nr_of_channels, outfile)) {
    ERREXIT(tinfo->emethods, "join_edf: Error writing channel headers\n");
   }
   total_records=0L;
  }
  printf("%s: %d records of %ld bytes.\n", filename, nr_of_records, record_size);

  for (record=0; record<nr_of_records; record++) {
   if (1!=(int)fread(recordbuf, record_size, 1, infile)) {
    ERREXIT(tinfo->emethods, "join_edf: Error reading record\n");
   }
   if (1!=(int)fwrite(recordbuf, record_size, 1, outfile)) {
    ERREXIT(tinfo->emethods, "join_edf: Error writing record\n");
   }
   total_records++;
  }
  free((void *)channelheader.label);
  fclose(infile);
 }
 if (recordbuf!=NULL) free(recordbuf);

 snprintf(numbuf, NUMBUF_LENGTH, "%ld", total_records);
 memset(&fileheader.nr_of_records, ' ', sizeof(fileheader.nr_of_records));
 copy_nstring(fileheader.nr_of_records, numbuf, sizeof(fileheader.nr_of_records));
 fseek(outfile, 0L, SEEK_SET);
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader, sm_REC_file);
#endif
 write_struct((char *)&fileheader, sm_REC_file, outfile);
 fclose(outfile);

 return 0;
}
