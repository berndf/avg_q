/*
 * Copyright (C) 2016 Bernd Feige
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
#include "cfs.h"
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
/*}}}  */

char **mainargv;
enum { CFSFILENAME=0, END_OF_ARGS
} args;
#define MAINARG(x) mainargv[optind+x]

#define MAXBUF 512

LOCAL void
print_var(TDataType vType, TpStr buf) {
 switch ((enum var_storage_enum)vType) {
  case INT1:
   printf("INT1: %d", *(int8_t *)buf);
   break;
  case WRD1:
   printf("WRD1: %d", *(uint8_t *)buf);
   break;
  case INT2:
   printf("INT2: %d", *(int16_t *)buf);
   break;
  case WRD2:
   printf("WRD1: %d", *(uint16_t *)buf);
   break;
  case INT4:
   printf("INT4: %d", *(int32_t *)buf);
   break;
  case RL4:
   printf("RL4: %g", *(float *)buf);
   break;
  case RL8:
   printf("RL8: %g", *(double *)buf);
   break;
  case LSTR:
   /* LSTR does NOT have a count byte at the start but is \0-terminated... */
   printf("LSTR: >%s<", buf);
   break;
 }
}

int 
main(int argc, char **argv) {
 TFileHead filehead;
 char *filename;
 FILE *cfsfile;
 long pos, vardatapos;
 int errflag=0, c;
 char readbuf[MAXBUF+1];
 TVarDesc *filevardescp, *DSvardescp;
 uint32_t *DSTable;

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
  fprintf(stderr, "Usage: %s [options] cfs_filename\n"
   " Options are:\n"
  , argv[0]);
  exit(1);
 }

 filename=MAINARG(CFSFILENAME);
 cfsfile=fopen(filename,"rb");
 if(cfsfile==NULL) {
  fprintf(stderr, "%s: Can't open file %s\n", argv[0], filename);
  exit(1);
 }

 if (read_struct((char *)&filehead, sm_TFileHead, cfsfile)==0) {
  fprintf(stderr, "%s: Header read error on file %s\n", argv[0], filename);
  exit(1);
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&filehead, sm_TFileHead);
#endif
 printf("CFS file %s:\n\n", filename);
 print_structcontents((char *)&filehead, sm_TFileHead, smd_TFileHead, stdout);

 printf("\n------- Fixed channel descriptions -------\n");
 pos=sm_TFileHead[0].offset;
 for (int channel=0; channel<filehead.dataChans; channel++) {
  TFilChInfo ch_info;
  fseek(cfsfile,pos,SEEK_SET);
  if (read_struct((char *)&ch_info, sm_TFilChInfo, cfsfile)==0) {
   fprintf(stderr, "%s: Header read error on file %s\n", argv[0], filename);
   exit(1);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&ch_info, sm_TFilChInfo);
#endif
  printf("\n");
  print_structcontents((char *)&ch_info, sm_TFilChInfo, smd_TFilChInfo, stdout);
  pos+=sm_TFilChInfo[0].offset;
 }

 printf("\n------- File variable descriptions -------\n");
 /* The variable data storage starts after file and data variable descriptors */
 vardatapos=pos+(filehead.filVars+filehead.datVars)*sm_TVarDesc[0].offset;
 /* Length is filehead.filVars+1 because "vSize" is actually offset and
  * a final entry is needed so that all vardata sizes are known
  */
 filevardescp=(TVarDesc *)malloc((filehead.filVars+1)*sizeof(TVarDesc));
 for (int filevar=0; filevar<filehead.filVars+1; filevar++) {
  fseek(cfsfile,pos,SEEK_SET);
  if (read_struct((char *)&filevardescp[filevar], sm_TVarDesc, cfsfile)==0) {
   fprintf(stderr, "%s: Header read error on file %s\n", argv[0], filename);
   exit(1);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&filevardescp[filevar], sm_TVarDesc);
#endif
  pos+=sm_TVarDesc[0].offset;
 }
 for (int filevar=0; filevar<filehead.filVars; filevar++) {
  /* Keep in mind that "vSize" actually means offset... Here's actually size: */
  int const size=filevardescp[filevar+1].vSize-filevardescp[filevar].vSize;
  printf("\n");
  print_structcontents((char *)&filevardescp[filevar], sm_TVarDesc, smd_TVarDesc, stdout);
  fseek(cfsfile,vardatapos+filevardescp[filevar].vSize,SEEK_SET);
  if (size>MAXBUF) {
   printf("Error: size=%d > MAXBUF (%d)\n", size, MAXBUF);
   exit(1);
  }
  fread(readbuf,1,size,cfsfile);
  printf("Variable data (%d bytes): ",size);
  print_var(filevardescp[filevar].vType,readbuf);
  printf("\n");
 }

 printf("\n------- DS variable descriptions -------\n");
 /* Offsets ("vSize") start with 0 again, base at the end of file var space */
 vardatapos+=filevardescp[filehead.filVars].vSize;
 DSvardescp=(TVarDesc *)malloc((filehead.datVars+1)*sizeof(TVarDesc));
 for (int dsvar=0; dsvar<filehead.datVars+1; dsvar++) {
  fseek(cfsfile,pos,SEEK_SET);
  if (read_struct((char *)&DSvardescp[dsvar], sm_TVarDesc, cfsfile)==0) {
   fprintf(stderr, "%s: Header read error on file %s\n", argv[0], filename);
   exit(1);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&DSvardescp[dsvar], sm_TVarDesc);
#endif
  pos+=sm_TVarDesc[0].offset;
 }
 for (int dsvar=0; dsvar<filehead.datVars; dsvar++) {
  /* Keep in mind that "vSize" actually means offset... Here's actually size: */
  int const size=DSvardescp[dsvar+1].vSize-DSvardescp[dsvar].vSize;
  printf("\n");
  print_structcontents((char *)&DSvardescp[dsvar], sm_TVarDesc, smd_TVarDesc, stdout);
  fseek(cfsfile,vardatapos+DSvardescp[dsvar].vSize,SEEK_SET);
  if (size>MAXBUF) {
   printf("Error: size=%d > MAXBUF (%d)\n", size, MAXBUF);
   exit(1);
  }
  fread(readbuf,1,size,cfsfile);
  printf("Variable data (%d bytes): ",size);
  print_var(DSvardescp[dsvar].vType,readbuf);
  printf("\n");
 }

 printf("\n------- Data sections -------\n");
 DSTable=(uint32_t *)malloc(filehead.dataSecs*sizeof(uint32_t));
 fseek(cfsfile,filehead.tablePos,SEEK_SET);
 if (fread((char *)DSTable, sizeof(uint32_t), filehead.dataSecs, cfsfile)!=filehead.dataSecs) {
  fprintf(stderr, "%s: DSTable read error on file %s\n", argv[0], filename);
  exit(1);
 }
 for (int datasec=0; datasec<filehead.dataSecs; datasec++) {
  /* Note that the actual data usually comes first in the file, then its header */
  TDataHead datahead;
  long ChInfoPos;
#ifndef LITTLE_ENDIAN
  Intel_int32(&DSTable[datasec])
#endif
  printf("\nDataSec offset: %d\n", DSTable[datasec]);
  fseek(cfsfile,DSTable[datasec],SEEK_SET);
  if (read_struct((char *)&datahead, sm_TDataHead, cfsfile)==0) {
   fprintf(stderr, "%s: DS header read error on file %s\n", argv[0], filename);
   exit(1);
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&datahead, sm_TDataHead);
#endif
  print_structcontents((char *)&datahead, sm_TDataHead, smd_TDataHead, stdout);
  /* Channel info starts after TDataHead */
  ChInfoPos=DSTable[datasec]+sm_TDataHead[0].offset;
  for (int channel=0; channel<filehead.dataChans; channel++) {
   TDSChInfo ch_info;
   fseek(cfsfile,ChInfoPos,SEEK_SET);
   if (read_struct((char *)&ch_info, sm_TDSChInfo, cfsfile)==0) {
    fprintf(stderr, "%s: Ds channel header read error on file %s\n", argv[0], filename);
    exit(1);
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&ch_info, sm_TDSChInfo);
#endif
   printf("- Channel %d:\n", channel);
   print_structcontents((char *)&ch_info, sm_TDSChInfo, smd_TDSChInfo, stdout);
   ChInfoPos+=sm_TDSChInfo[0].offset;
  }
 }
 fclose(cfsfile);

 return 0;
}
