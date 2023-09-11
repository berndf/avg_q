/*
 * Copyright (C) 1997,2007,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "neurofile.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { DSCFILENAME=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

int 
main(int argc, char **argv) {
 struct sequence seq;
 char *filename;
 FILE *dsc;
 int channel;
 int errflag=0, c;

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
  fprintf(stderr, "Usage: %s [options] dsc_filename\n"
   " Options are:\n"
  , argv[0]);
  exit(1);
 }

 filename=MAINARG(DSCFILENAME);
 dsc=fopen(filename,"rb");
 if(dsc==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }

 if (read_struct((char *)&seq, sm_sequence, dsc)==0) {
  fprintf(stderr, "%s: Header read error on file %s\n", argv[0], filename);
  exit(1);
 }
 fclose(dsc);
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&seq, sm_sequence);
#endif
 printf("NeuroFile File %s:\n\n", filename);
 print_structcontents((char *)&seq, sm_sequence, smd_sequence, stdout);
 printf(" From fastrate=%d follows that sfreq=%g Hz\n", seq.fastrate, neurofile_map_sfreq(seq.fastrate));

 for (channel=0; channel<seq.nfast; channel++) {
  printf("Channel number %d: Name %s, Coord %d %d\n", channel+1, seq.elnam[channel], seq.coord[0][channel], seq.coord[1][channel]);
 }

 return 0;
}
