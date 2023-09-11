/*
 * Copyright (C) 2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <read_struct.h>
#ifdef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "growing_buf.h"
#include "transform.h"
#include "bf.h"

#include "sigma_header.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { SIGMAFILE=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]
#undef ERREXIT
#define ERREXIT(dummy, message) fprintf(stderr, message); exit(1)

int 
main(int argc, char **argv) {
 sigma_file_header fileheader;
 sigma_channel_header channelheader;
 FILE *infile;
 char * filename;
 int errflag=0, c;
 int32_t pos;
 growing_buf buf;

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
  fprintf(stderr, "Usage: %s [options] sigma_filename\n"
  , argv[0]);
  exit(1);
 }

 filename=MAINARG(SIGMAFILE);
 infile=fopen(filename,"rb");
 if(infile==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }
 /*}}}  */

 printf("Sigma file %s\n\n", filename);

 /*{{{  Read the file header and build comment*/
 if (read_struct((char *)&fileheader, sm_sigma_file, infile)==0) {
  ERREXIT(tinfo->emethods, "dump_sigma_header: Error reading file header.\n");
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader, sm_sigma_file);
#endif
 fseek(infile, fileheader.pos_channelheader, SEEK_SET);
 if (read_struct((char *)&fileheader.n_channels, sm_sigma_file1, infile)==0) {
  ERREXIT(tinfo->emethods, "dump_sigma_header: Error reading file header.\n");
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader.n_channels, sm_sigma_file1);
#endif
 print_structcontents((char *)&fileheader, sm_sigma_file, smd_sigma_file, stdout);
 print_structcontents((char *)&fileheader.n_channels, sm_sigma_file1, smd_sigma_file1, stdout);

 growing_buf_init(&buf);
 growing_buf_allocate(&buf,fileheader.pos_channelheader-fileheader.pos_patinfo+1);
 fseek(infile, fileheader.pos_patinfo, SEEK_SET);
 fread((void *)buf.buffer_start,1,fileheader.pos_channelheader-fileheader.pos_patinfo,infile);
 printf("%s\n",buf.buffer_start);

 pos=fileheader.pos_channelheader+39-4;
 printf("Starting at %d\n", pos);
 fseek(infile, pos, SEEK_SET);
 for (int channel=0; channel<fileheader.n_channels; channel++) {
  if (read_struct((char *)&channelheader, sm_sigma_channel, infile)==0) {
   ERREXIT(tinfo->emethods, "dump_sigma_header: Error reading channel header.\n");
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&channelheader, sm_sigma_channel);
#endif
  print_structcontents((char *)&channelheader, sm_sigma_channel, smd_sigma_channel, stdout);
 }
 printf("Position after channel headers: %ld\n", ftell(infile));

 fclose(infile);
 growing_buf_free(&buf);

 return 0;
}
