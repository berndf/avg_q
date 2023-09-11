/*
 * Copyright (C) 1996-2003,2006,2007,2010,2012,2013,2017 Bernd Feige
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
#include <math.h>
#include <read_struct.h>
#ifdef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "vitaport.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { EVENTFILE=0, VITAPORTFILE, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

int 
main(int argc, char **argv) {
 struct vitaport_fileheader fileheader;
 struct vitaportI_fileheader fileheaderI;
 struct vitaportII_fileheader fileheaderII;
 struct vitaportII_fileext fileext;
 enum vitaport_filetypes vitaport_filetype=VITAPORT1_FILE;
 char *filename;
 FILE *infile;
 FILE *evtfile;
 int NoOfChannels, channel;
 int marker_channel= -1;
 long filelen_ms=0;
 uint16_t checksum;
 long sum_dlen=0;
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
  fprintf(stderr, "Usage: %s event_filename vitaport_filename\n"
  , argv[0]);
  exit(1);
 }

 filename=MAINARG(VITAPORTFILE);
 infile=fopen(filename,"r+b");
 if(infile==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }
 evtfile=fopen(MAINARG(EVENTFILE),"r");
 if(evtfile==NULL) {
  fprintf(stderr, "%s: Can't open event file %s\n", argv[0], MAINARG(EVENTFILE));
  exit(1);
 }

 if (read_struct((char *)&fileheader, sm_vitaport_fileheader, infile)==0) {
  fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
  exit(1);
 }
#ifdef LITTLE_ENDIAN
 change_byteorder((char *)&fileheader, sm_vitaport_fileheader);
#endif
 if (fileheader.Sync!=0x4afc) {
  fprintf(stderr, "%s: Sync value is %4x, not 4afc\n", argv[0], fileheader.Sync);
  exit(1);
 }
 switch (fileheader.hdtyp) {
  case HDTYP_VITAPORT1:
   if (read_struct((char *)&fileheaderI, sm_vitaportI_fileheader, infile)==0) {
    fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
    exit(1);
   }
#ifdef LITTLE_ENDIAN
   change_byteorder((char *)&fileheaderI, sm_vitaportI_fileheader);
#endif
   Intel_int16((uint16_t *)&fileheaderI.blckslh);	/* These is in low-high order, ie always reversed */
   break;
  case HDTYP_VITAPORT2:
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
   switch (fileheader.chnoffs) {
    case 22:
     vitaport_filetype=VITAPORT1_FILE;
     break;
    case 36:
     vitaport_filetype=VITAPORT2_FILE;
     if (read_struct((char *)&fileext, sm_vitaportII_fileext, infile)==0) {
      fprintf(stderr, "%s: Read error on file %s\n", argv[0], filename);
      exit(1);
     }
#ifdef LITTLE_ENDIAN
     change_byteorder((char *)&fileext, sm_vitaportII_fileext);
#endif
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
 for (channel=0; channel<NoOfChannels; channel++) {
  struct vitaport_channelheader channelheader;
  struct vitaportIIrchannelheader channelheaderII;
  long channellen_ms;
  if (read_struct((char *)&channelheader, sm_vitaport_channelheader, infile)==0) {
   fprintf(stderr, "%s: Channel header read error on file %s\n", argv[0], filename);
   exit(1);
  }
#ifdef LITTLE_ENDIAN
  change_byteorder((char *)&channelheader, sm_vitaport_channelheader);
#endif
  switch (vitaport_filetype) {
   case VITAPORT1_FILE:
    break;
   case VITAPORT2_FILE:
    if (read_struct((char *)&channelheaderII, sm_vitaportIIrchannelheader, infile)==0) {
     fprintf(stderr, "%s: Channel header read error on file %s\n", argv[0], filename);
     exit(1);
    }
#ifdef LITTLE_ENDIAN
    change_byteorder((char *)&channelheaderII, sm_vitaportIIrchannelheader);
#endif
    sum_dlen+=channelheaderII.dlen;
    break;
   case VITAPORT2RFILE:
    break;
  }
  if (strncmp(channelheader.kname, VP_MARKERCHANNEL_NAME, VP_CHANNELNAME_LENGTH)==0) {
   marker_channel=channel;
  }
  channellen_ms=(long int)rint(channelheaderII.dlen*1000.0/(channelheader.dasize+1)*fileheaderII.scnrate*channelheader.scanfc*channelheader.stofac/VITAPORT_CLOCKRATE);
  if (channellen_ms>filelen_ms) filelen_ms=channellen_ms;
 }
 fread(&checksum, sizeof(checksum), 1, infile);
#ifdef LITTLE_ENDIAN
 Intel_int16((uint16_t *)&checksum);
#endif
 if (vitaport_filetype==VITAPORT2_FILE) {
  long const tablepos=ftell(infile)+sum_dlen;
  long tagno, n_events, trigpoint;
  uint32_t length, utrigpoint;
  struct vitaport_idiotic_multiplemarkertablenames *markertable_entry;

  /* Count the events in evtfile. */
  for (n_events=0; read_trigger_from_trigfile(evtfile, (DATATYPE)1000, &trigpoint, NULL)!=0; n_events++);
  fseek(evtfile, 0, SEEK_SET);

  /* Try to keep a security space of at least 20 free events */
  for (markertable_entry=markertable_names; markertable_entry->markertable_name!=NULL && markertable_entry->idiotically_fixed_length<n_events+20; markertable_entry++);
  if (markertable_entry->markertable_name==NULL) {
   if ((markertable_entry-1)->idiotically_fixed_length<=n_events) {
    /* Drop the requirement for at least 20 free events */
    markertable_entry--;
   } else {
    /* No use... */
    fprintf(stderr, "%ld events wouldn't fit into the fixed-length VITAGRAPH marker tables!\nBlame the Vitaport people!\n", n_events);
    fclose(evtfile);
    fclose(infile);
    exit(1);
   }
  }

  printf("Truncating file at %ld+%ld=%ld; writing %ld events to >%s<...\n", ftell(infile), sum_dlen, tablepos, n_events, markertable_entry->markertable_name);

  ftruncate(fileno(infile), tablepos);
  fseek(infile, 0, SEEK_END);

#if 0
  fwrite(VP_CHANNELTABLE_NAME, 1, VP_TABLEVAR_LENGTH, infile);
  length=NoOfChannels*(1+2*sizeof(length));
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
  for (channel=0; channel<NoOfChannels; channel++) {
   unsigned char display_channel=channel;
   if (channel==marker_channel) display_channel=0xff;
   fwrite(&display_channel, 1, 1, infile);
  }
  for (channel=0; channel<NoOfChannels; channel++) {
   int32_t unknown= -1;
#ifdef LITTLE_ENDIAN
   Intel_int32((uint32_t *)&unknown);
#endif
   fwrite(&unknown, sizeof(unknown), 1, infile);
   fwrite(&unknown, sizeof(unknown), 1, infile);
  }
#endif

  fwrite(markertable_entry->markertable_name, 1, VP_TABLEVAR_LENGTH, infile);
  n_events=markertable_entry->idiotically_fixed_length;
  length=n_events*sizeof(length);
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
  for (tagno=0; tagno<n_events; tagno++) {
   /* By specifying a sampling rate of 1kHz, the 'point number' output will
    * actually be milliseconds as we need it IF THE POINT POSITIONS ARE GIVEN
    * IN TIME UNITS (s or ms) IN THE EVENT FILE */
   if (read_trigger_from_trigfile(evtfile, (DATATYPE)1000, &trigpoint, NULL)==0) break;
    utrigpoint=(uint32_t)trigpoint;
#ifdef LITTLE_ENDIAN
   Intel_int32((uint32_t *)&utrigpoint);
#endif
   fwrite(&utrigpoint, sizeof(utrigpoint), 1, infile);
  }
  for (; tagno<n_events; tagno++) {
   utrigpoint= -1;
#ifdef LITTLE_ENDIAN
   Intel_int32((uint32_t *)&utrigpoint);
#endif
   fwrite(&utrigpoint, sizeof(utrigpoint), 1, infile);
  }

  fwrite(VP_GLBMRKTABLE_NAME, 1, VP_TABLEVAR_LENGTH, infile);
  length=4*sizeof(length);
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
  length=0;
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
  length=filelen_ms;
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
  length=0;
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
  length=0;
#ifdef LITTLE_ENDIAN
  Intel_int32((uint32_t *)&length);
#endif
  fwrite(&length, sizeof(length), 1, infile);
 }
 fclose(evtfile);
 fclose(infile);

 return 0;
}
