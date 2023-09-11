/*
 * Copyright (C) 2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "nke_format.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { EEGFILENAME=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

int 
main(int argc, char **argv) {
 char *filename;
 FILE *infile;
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

 filename=MAINARG(EEGFILENAME);
 infile=fopen(filename,"rb");
 if(infile==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }

 struct nke_file_header file_header;
 if (read_struct((char *)&file_header, sm_nke_file_header, infile)==0) {
  fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&file_header, sm_nke_file_header);
#endif
 print_structcontents((char *)&file_header, sm_nke_file_header, smd_nke_file_header, stdout);

 for (int ctl_block=0; ctl_block<file_header.ctl_block_cnt; ctl_block++) {
  printf("\nControl block %d:\n", ctl_block+1);
  fseeko(infile, sm_nke_file_header[0].offset+ctl_block*sm_nke_control_block[0].offset, SEEK_SET);
  struct nke_control_block control_block;
  if (read_struct((char *)&control_block, sm_nke_control_block, infile)==0) {
   fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&control_block, sm_nke_control_block);
#endif
  print_structcontents((char *)&control_block, sm_nke_control_block, smd_nke_control_block, stdout);
  struct nke_data_block1 data_block1;
  fseeko(infile, control_block.address, SEEK_SET);
  if (read_struct((char *)&data_block1, sm_nke_data_block1, infile)==0) {
   fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&data_block1, sm_nke_data_block1);
#endif
  unsigned long	wfmblock_address=data_block1.wfmblock_address;
  for (int dta_block=0; dta_block<data_block1.datablock_cnt; dta_block++) {
   printf("\nData block %d:\n", dta_block+1);
   if (dta_block==0) {
    print_structcontents((char *)&data_block1, sm_nke_data_block1, smd_nke_data_block1, stdout);
   } else {
    struct nke_data_block data_block;
    fseeko(infile, control_block.address+sm_nke_data_block1[0].offset+(dta_block-1)*sm_nke_data_block[0].offset, SEEK_SET);
    if (read_struct((char *)&data_block, sm_nke_data_block, infile)==0) {
     fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
    }
#ifndef LITTLE_ENDIAN
    change_byteorder((char *)&data_block, sm_nke_data_block);
#endif
    print_structcontents((char *)&data_block, sm_nke_data_block, smd_nke_data_block, stdout);
    wfmblock_address=data_block.wfmblock_address;
   }

   printf("\nWFM block:\n");
   fseeko(infile, wfmblock_address, SEEK_SET);
   struct nke_wfm_block wfm_block;
   if (read_struct((char *)&wfm_block, sm_nke_wfm_block, infile)==0) {
    fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&wfm_block, sm_nke_wfm_block);
#endif
   wfm_block.sfreq&=0x3fff; /* Patch-up sfreq in place */
   print_structcontents((char *)&wfm_block, sm_nke_wfm_block, smd_nke_wfm_block, stdout);

   for (int channel=0; channel<wfm_block.channels; channel++) {
    printf("\nChannel number %d:\n", channel+1);
    fseeko(infile, wfmblock_address+sm_nke_wfm_block[0].offset+channel*sm_nke_channel_block[0].offset, SEEK_SET);
    struct nke_channel_block channel_block;
    if (read_struct((char *)&channel_block, sm_nke_channel_block, infile)==0) {
     fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
    }
#ifndef LITTLE_ENDIAN
    change_byteorder((char *)&channel_block, sm_nke_channel_block);
#endif
    print_structcontents((char *)&channel_block, sm_nke_channel_block, smd_nke_channel_block, stdout);
   }
  }
 }
 fclose(infile);

 growing_buf logfilename;

 growing_buf_init(&logfilename);
 growing_buf_takethis(&logfilename,filename);
 char *const dot=strrchr(logfilename.buffer_start,'.');
 if (dot!=NULL) {
  logfilename.current_length=dot-logfilename.buffer_start+1;
 }
 long const savelength=logfilename.current_length;
 growing_buf_appendstring(&logfilename,".log");
 FILE *logfile=fopen(logfilename.buffer_start, "rb");
 if (logfile==NULL) {
  logfilename.current_length=savelength;
  growing_buf_appendstring(&logfilename,".LOG");
  logfile=fopen(logfilename.buffer_start, "rb");
 }
 if (logfile!=NULL) {
  printf("\n\nLog file %s:\n", logfilename.buffer_start);
  struct nke_file_header logfile_header;
  if (read_struct((char *)&logfile_header, sm_nke_file_header, infile)==0) {
   fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&logfile_header, sm_nke_file_header);
#endif
  print_structcontents((char *)&logfile_header, sm_nke_file_header, smd_nke_file_header, stdout);

  /* log_block_cnt can be at most 22; Afterwards "subevents" may follow with milliseconds information */
  Bool read_subevents=TRUE;
  for (int lg_block=0; lg_block<logfile_header.ctl_block_cnt; lg_block++) {
   printf("\nLog block %d:\n", lg_block+1);
   fseeko(logfile, sm_nke_file_header[0].offset+lg_block*sm_nke_control_block[0].offset, SEEK_SET);
   struct nke_control_block clog_block,sclog_block;
   if (read_struct((char *)&clog_block, sm_nke_control_block, logfile)==0) {
    fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&clog_block, sm_nke_control_block);
#endif
   print_structcontents((char *)&clog_block, sm_nke_control_block, smd_nke_control_block, stdout);
   fseeko(logfile, clog_block.address, SEEK_SET);
   printf("Log header:\n");
   struct nke_log_header log_header;
   if (read_struct((char *)&log_header, sm_nke_log_header, logfile)==0) {
    fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&log_header, sm_nke_log_header);
#endif
   print_structcontents((char *)&log_header, sm_nke_log_header, smd_nke_log_header, stdout);
   if (read_subevents) {
    printf("\n\"subevents\" log block %d:\n", lg_block+1);
    if (fseeko(logfile, sm_nke_file_header[0].offset+(lg_block+22)*sm_nke_control_block[0].offset, SEEK_SET)) {
     printf("Not reading subevents.\n");
     read_subevents=FALSE;
    } else {
     if (read_struct((char *)&sclog_block, sm_nke_control_block, logfile)==0) {
      fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
     }
#ifndef LITTLE_ENDIAN
     change_byteorder((char *)&sclog_block, sm_nke_control_block);
#endif
     print_structcontents((char *)&sclog_block, sm_nke_control_block, smd_nke_control_block, stdout);
     printf("Log header:\n");
     fseeko(logfile, sclog_block.address, SEEK_SET);
     struct nke_log_header slog_header;
     if (read_struct((char *)&slog_header, sm_nke_log_header, logfile)==0) {
      fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
     }
#ifndef LITTLE_ENDIAN
     change_byteorder((char *)&slog_header, sm_nke_log_header);
#endif
     print_structcontents((char *)&slog_header, sm_nke_log_header, smd_nke_log_header, stdout);
     if (slog_header.log_block_cnt!=log_header.log_block_cnt) {
      printf("Not reading subevents.\n");
      read_subevents=FALSE;
     }
    }
   }
   for (int dta_block=0; dta_block<log_header.log_block_cnt; dta_block++) {
    printf("\nData block %d:\n", dta_block+1);
    fseeko(logfile, clog_block.address+sm_nke_log_header[0].offset+dta_block*sm_nke_log_block[0].offset, SEEK_SET);
    struct nke_log_block log_block;
    if (read_struct((char *)&log_block, sm_nke_log_block, logfile)==0) {
     fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
    }
#ifndef LITTLE_ENDIAN
    change_byteorder((char *)&log_block, sm_nke_log_block);
#endif
    print_structcontents((char *)&log_block, sm_nke_log_block, smd_nke_log_block, stdout);
    /* Format of timestamp[:6] is HHMMSS starting from recording start */
    float marker_time_s=
     36000L*(log_block.timestamp[0]-'0')+
      3600L*(log_block.timestamp[1]-'0')+
       600L*(log_block.timestamp[2]-'0')+
        60L*(log_block.timestamp[3]-'0')+
        10L*(log_block.timestamp[4]-'0')+
            (log_block.timestamp[5]-'0');
    if (read_subevents) {
     printf("\nSub data block %d:\n", dta_block+1);
     fseeko(logfile, sclog_block.address+sm_nke_log_header[0].offset+dta_block*sm_nke_log_block[0].offset, SEEK_SET);
     struct nke_log_block slog_block;
     if (read_struct((char *)&slog_block, sm_nke_log_block, logfile)==0) {
      fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
     }
#ifndef LITTLE_ENDIAN
     change_byteorder((char *)&slog_block, sm_nke_log_block);
#endif
     print_structcontents((char *)&slog_block, sm_nke_log_block, smd_nke_log_block, stdout);
     marker_time_s+=
        0.1*(slog_block.timestamp[4]-'0')+
       0.01*(slog_block.timestamp[5]-'0')+
      0.001*(slog_block.timestamp[6]-'0');
    }
    printf("This corresponds to %g seconds.\n", marker_time_s);
   }
  }
  fclose(logfile);
 }
 growing_buf_free(&logfilename);

 return 0;
}
