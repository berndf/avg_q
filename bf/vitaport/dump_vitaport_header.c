/*
 * Copyright (C) 1996-2003,2007,2010-2017,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <read_struct.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
#include "vitaport.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { VITAPORTFILE=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

#define TRIGCODE_FOR_STARTMARKER 1
#define TRIGCODE_FOR_ENDMARKER 2

int 
main(int argc, char **argv) {
 struct vitaport_fileheader fileheader;
 struct vitaportI_fileheader fileheaderI;
 struct vitaportII_fileheader fileheaderII;
 struct vitaportII_fileext fileext;
 enum vitaport_filetypes vitaport_filetype=VITAPORT1_FILE;
 char *filename;
 FILE *infile;
 int NoOfChannels, channel;
 uint16_t checksum;
 long sum_dlen=0;
 unsigned long max_sfac=1, min_sfac=SHRT_MAX, bytes_per_max_sfac=0;
 float ceil_sfreq=0;
 int errflag=0, c;
 int nr_of_files=0;
 long filestart;
 enum {EVENTLIST_NONE, EVENTLIST_AVG_Q} event_list=EVENTLIST_NONE;

 /*{{{  Process command line*/
 mainargv=argv;
 while ((c=getopt(argc, argv, "E"))!=EOF) {
  switch (c) {
   case 'E':
    event_list=EVENTLIST_AVG_Q;
    break;
   case '?':
   default:
    errflag++;
    continue;
  }
 }

 if (argc-optind!=END_OF_ARGS || errflag>0) {
  fprintf(stderr, "Usage: %s [options] vitaport_filename\n"
   " Options are:\n"
   "\t-E: Output the event table in avg_q format\n"
  , argv[0]);
  exit(1);
 }

 filename=MAINARG(VITAPORTFILE);
 infile=fopen(filename,"rb");
 if(infile==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }

again:
 /* This is repeated only for raw files. We want to silently stop reading
  * more files if EOF is encountered or the sync value is wrong. */
 filestart=ftell(infile);
 if (read_struct((char *)&fileheader, sm_vitaport_fileheader, infile)==0) {
  if (nr_of_files==0) {
   fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
  }
  exit(1);
 }
#ifdef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader, sm_vitaport_fileheader);
#endif
 if (event_list==EVENTLIST_NONE) {
  printf("File %s:\n\n", filename);
  print_structcontents((char *)&fileheader, sm_vitaport_fileheader, smd_vitaport_fileheader, stdout);
 }
 if (fileheader.Sync!=0x4afc) {
  if (nr_of_files==0) {
   fprintf(stderr, "%s: Sync value is %4x, not 4afc\n", argv[0], fileheader.Sync);
  }
  exit(1);
 }
 nr_of_files++;
 switch (fileheader.hdtyp) {
  case HDTYP_VITAPORT1:
   if (event_list==EVENTLIST_NONE) {
    printf(" - this is a VitaPort I file.\n\n");
   }
   if (read_struct((char *)&fileheaderI, sm_vitaportI_fileheader, infile)==0) {
    fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
    exit(1);
   }
#ifdef LITTLE_ENDIAN
   change_byteorder((char *)&fileheaderI, sm_vitaportI_fileheader);
#endif
   Intel_int16((uint16_t *)&fileheaderI.blckslh);	/* These is in low-high order, ie always reversed */
   if (event_list==EVENTLIST_NONE)
   print_structcontents((char *)&fileheaderI, sm_vitaportI_fileheader, smd_vitaportI_fileheader, stdout);
   break;
  case HDTYP_VITAPORT2:
  case HDTYP_VITAPORT2P1:
   if (event_list==EVENTLIST_NONE) {
    printf(" - this is a VitaPort II file.\n\n");
   }
   if (read_struct((char *)&fileheaderII, sm_vitaportII_fileheader, infile)==0) {
    fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
    exit(1);
   }
#ifdef LITTLE_ENDIAN
   change_byteorder((char *)&fileheaderII, sm_vitaportII_fileheader);
#endif
   convert_fromhex(&fileheaderII.Hour);
   convert_fromhex(&fileheaderII.Minute);
   convert_fromhex(&fileheaderII.Second);
   convert_fromhex(&fileheaderII.Day);
   convert_fromhex(&fileheaderII.Month);
   convert_fromhex(&fileheaderII.Year);
   if (event_list==EVENTLIST_NONE)
   print_structcontents((char *)&fileheaderII, sm_vitaportII_fileheader, smd_vitaportII_fileheader, stdout);
   switch (fileheader.chnoffs) {
    case 22:
     vitaport_filetype=VITAPORT1_FILE;
     if (event_list==EVENTLIST_NONE) printf(" - this is of file type VITAPORT1_FILE.\n\n");
     ceil_sfreq=400.0;	/* Isitso? */
     break;
    case 36:
     vitaport_filetype=((fileheader.hdlen&511)==0 ? VITAPORT2RFILE : VITAPORT2_FILE);
     if (event_list==EVENTLIST_NONE) printf(" - this is a %s file.\n\n", vitaport_filetype==VITAPORT2RFILE ? "raw" : "converted");
     if (read_struct((char *)&fileext, sm_vitaportII_fileext, infile)==0) {
      fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
      exit(1);
     }
#ifdef LITTLE_ENDIAN
     change_byteorder((char *)&fileext, sm_vitaportII_fileext);
#endif
     if (event_list==EVENTLIST_NONE)
     print_structcontents((char *)&fileext, sm_vitaportII_fileext, smd_vitaportII_fileext, stdout);
     ceil_sfreq=VITAPORT_CLOCKRATE/fileheaderII.scnrate;
     break;
    default:
     fprintf(stderr, "%s: Unknown Vitaport II channel offset %d\n", argv[0], fileheader.chnoffs);
     exit(1);
   }
   break;
  default:
   fprintf(stderr, "%s: Unknown header type value %d\n", argv[0], fileheader.hdtyp);
   exit(1);
 }
 NoOfChannels=fileheader.knum;
 if (event_list==EVENTLIST_NONE)
 printf("\nCHANNEL HEADERS"
        "\n---------------\n");
 for (channel=0; channel<NoOfChannels; channel++) {
  struct vitaport_channelheader channelheader;
  struct vitaportIIrchannelheader channelheaderII;
  unsigned int this_sfac,this_size;
  if (event_list==EVENTLIST_NONE)
  printf("\nChannel number %d:\n", channel+1);
  if (read_struct((char *)&channelheader, sm_vitaport_channelheader, infile)==0) {
   fprintf(stderr, "%s: Channel header read error on file %s\n", argv[0], filename);
   exit(1);
  }
#ifdef LITTLE_ENDIAN
  change_byteorder((char *)&channelheader, sm_vitaport_channelheader);
#endif
  if (event_list==EVENTLIST_NONE)
  print_structcontents((char *)&channelheader, sm_vitaport_channelheader, smd_vitaport_channelheader, stdout);

  this_sfac=channelheader.scanfc*channelheader.stofac;
  this_size=channelheader.dasize+1;
  if (this_sfac>max_sfac) {
   bytes_per_max_sfac=(bytes_per_max_sfac*this_sfac)/max_sfac;
   max_sfac=this_sfac;
  }
  if (this_sfac<min_sfac) {
   min_sfac=this_sfac;
  }
  bytes_per_max_sfac+=(this_size*max_sfac)/this_sfac;
  if (event_list==EVENTLIST_NONE) {
   printf(" ->sfreq=%gHz\n", ceil_sfreq/this_sfac);
  }

  switch (vitaport_filetype) {
   case VITAPORT1_FILE:
    break;
   case VITAPORT2_FILE:
   case VITAPORT2RFILE:
    if (read_struct((char *)&channelheaderII, sm_vitaportIIrchannelheader, infile)==0) {
     fprintf(stderr, "%s: Channel header read error on file %s\n", argv[0], filename);
     exit(1);
    }
#ifdef LITTLE_ENDIAN
    change_byteorder((char *)&channelheaderII, sm_vitaportIIrchannelheader);
#endif
    if (event_list==EVENTLIST_NONE) {
     print_structcontents((char *)&channelheaderII, sm_vitaportIIrchannelheader, smd_vitaportIIrchannelheader, stdout);
     printf(" ->NumSamples=%ld\n", channelheaderII.dlen/(channelheader.dasize+1));
    }
    sum_dlen+=channelheaderII.dlen;
    break;
  }
 }
 fread(&checksum, sizeof(checksum), 1, infile);
#ifdef LITTLE_ENDIAN
 Intel_int16((uint16_t *)&checksum);
#endif
 if (event_list==EVENTLIST_NONE) {
  printf("\nHeader checksum is %d; Position after header is %ld, hdlen is %d,\n"
         "sum_dlen is %ld, max_sfac %ld, min_sfac %ld, bytes_per_max_sfac %ld\n"
         "->NumSamples from file info: %ld\n",
	 checksum, ftell(infile), fileheader.hdlen,
	 fileheaderII.dlen, max_sfac, min_sfac, bytes_per_max_sfac,
	 (long)rint(((double)fileheaderII.dlen-fileheader.hdlen)*max_sfac/min_sfac/bytes_per_max_sfac));
 }
 if (vitaport_filetype==VITAPORT2_FILE) {
  if (event_list==EVENTLIST_NONE)
  printf("Skipping the data section of %ld Bytes...\n", sum_dlen);
  fseek(infile, sum_dlen, SEEK_CUR);
  while(1) {
   char label[VP_TABLEVAR_LENGTH+1];
   uint32_t length;
   struct vitaport_idiotic_multiplemarkertablenames *markertable_entry;
   label[VP_TABLEVAR_LENGTH]='\0';
   fread(label, 1, VP_TABLEVAR_LENGTH, infile);
   if (feof(infile)) break;
   fread(&length, sizeof(length), 1, infile);
#ifdef LITTLE_ENDIAN
   Intel_int32((uint32_t *)&length);
#endif
   for (markertable_entry=markertable_names; markertable_entry->markertable_name!=NULL && strcmp(label, markertable_entry->markertable_name)!=0; markertable_entry++);
   if (event_list==EVENTLIST_NONE) {
    printf("Field of length %d Bytes, Label: >%s<\n", length, label);
    if (markertable_entry->markertable_name!=NULL || 
        strcmp(label, VP_GLBMRKTABLE_NAME)==0) {
     uint32_t pointno;
     int tagno;
     for (tagno=0; tagno<length/(int)sizeof(uint32_t); tagno++) {
      fread(&pointno, sizeof(pointno), 1, infile);
#ifdef LITTLE_ENDIAN
      Intel_int32((uint32_t *)&pointno);
#endif
      if (pointno!=UINT32_MAX) printf(" %dms\n", pointno);
     }
     continue;
    } else if (strcmp(label, VP_CHANNELTABLE_NAME)==0) {
     int pass;
     for (channel=0; channel<NoOfChannels; channel++) {
      unsigned char display_channel;
      fread(&display_channel, 1, 1, infile);
      printf(" %d", display_channel);
     }
     printf("\n");
     for (pass=0; pass<2; pass++) {
      for (channel=0; channel<NoOfChannels; channel++) {
       uint32_t unknown;
       fread(&unknown, sizeof(unknown), 1, infile);
#ifdef LITTLE_ENDIAN
       Intel_int32((uint32_t *)&unknown);
#endif
       printf(" %d", unknown);
      }
      printf("\n");
     }
     continue;
    }
   }
   if (event_list==EVENTLIST_AVG_Q && markertable_entry->markertable_name!=NULL) {
    uint32_t pointno;
    int tagno;
    for (tagno=0; tagno<length/(int)sizeof(uint32_t); tagno++) {
     fread(&pointno, sizeof(pointno), 1, infile);
#ifdef LITTLE_ENDIAN
     Intel_int32((uint32_t *)&pointno);
#endif
     if (pointno!=UINT32_MAX) printf("%dms %d\n", pointno, pointno%2==0 ? TRIGCODE_FOR_STARTMARKER : TRIGCODE_FOR_ENDMARKER);
    }
    continue;
   }
   fseek(infile, length, SEEK_CUR);
  }
 } else if (vitaport_filetype==VITAPORT2RFILE) {
  /* Note that for raw files, dlen is the *full* file length, 
   * header inclusive */
  long togo=filestart+fileheaderII.dlen;
  /* Skip to the next 512-byte boundary: */
  togo=((togo>>9)+1)<<9;
  printf("Seeking to %ld\n", togo);
  fseek(infile, togo, SEEK_SET);
  goto again;
 }
 fclose(infile);

 return 0;
}
