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

 fseeko(infile, 0x0091L, SEEK_SET);
 int ctl_block_cnt = fgetc(infile);
 for (int ctl_block=0; ctl_block<ctl_block_cnt; ctl_block++) {
  printf("\nControl block %d:\n", ctl_block+1);
  fseeko(infile, 0x0092L+ctl_block*sm_nke_control_block[0].offset, SEEK_SET);
  struct nke_control_block control_block;
  if (read_struct((char *)&control_block, sm_nke_control_block, infile)==0) {
   fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&control_block, sm_nke_control_block);
#endif
  print_structcontents((char *)&control_block, sm_nke_control_block, smd_nke_control_block, stdout);
  // Read datablock_cnt which is a member of the first data block...
  fseeko(infile, control_block.address+17, SEEK_SET);
  int const data_block_cnt = fgetc(infile);
  for (int dta_block=0; dta_block<data_block_cnt; dta_block++) {
   printf("\nData block %d:\n", dta_block+1);
   fseeko(infile, control_block.address+dta_block*sm_nke_control_block[0].offset, SEEK_SET);
   struct nke_data_block data_block;
   if (read_struct((char *)&data_block, sm_nke_data_block, infile)==0) {
    fprintf(stderr, "%s: Can't read header in file >%s<\n", argv[0], filename);
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&data_block, sm_nke_data_block);
#endif
   print_structcontents((char *)&data_block, sm_nke_data_block, smd_nke_data_block, stdout);

   printf("\nWFM block:\n");
   fseeko(infile, data_block.wfmblock_address, SEEK_SET);
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
    fseeko(infile, data_block.wfmblock_address+sm_nke_wfm_block[0].offset+channel*sm_nke_channel_block[0].offset, SEEK_SET);
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

 return 0;
}
