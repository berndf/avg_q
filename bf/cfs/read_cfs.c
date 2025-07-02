/*
 * Copyright (C) 2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_cfs.c module to read data from Cambridge Electronic Design (tm) CFS files.
 *	-- Bernd Feige 29.02.2016
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#ifdef __GNUC__
#include <unistd.h>
#endif
#ifdef __MINGW32__
#include <fcntl.h>
#include <io.h>
#endif
#include <ctype.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
#include "cfs.h"
/*}}}  */

#define MAX_COMMENTLEN 1024

enum ARGS_ENUM {
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_IFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL},
};

/*{{{  Definition of read_cfs_storage*/
struct read_cfs_storage {
 FILE *infile;
 int current_epoch;
 long fromepoch;
 long epochs;
 TFileHead filehead;
 TFilChInfo *file_ch_info;
 TDSChInfo *ds_ch_info;
 uint32_t *DSTable;
};
/*}}}  */

/*{{{  read_cfs_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_cfs_init(transform_info_ptr tinfo) {
 struct read_cfs_storage *local_arg=(struct read_cfs_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 long pos;

 if (strcmp(args[ARGS_IFILE].arg.s, "stdin")==0) {
  local_arg->infile=stdin;
#ifdef __MINGW32__
  if (_setmode( _fileno( stdin ), _O_BINARY ) == -1) {
   ERREXIT(tinfo->emethods, "read_cfs_init: Can't set binary mode for stdin\n");
  }
#endif
 } else if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_cfs_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 if (read_struct((char *)&local_arg->filehead, sm_TFileHead, local_arg->infile)==0) {
  ERREXIT1(tinfo->emethods, "read_cfs_init: Header read error on file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->filehead, sm_TFileHead);
#endif
 local_arg->file_ch_info=(TFilChInfo *)malloc(local_arg->filehead.dataChans*sizeof(TFilChInfo));
 /* Also allocate the data section (epoch) specific channel information here: */
 local_arg->ds_ch_info=(TDSChInfo *)malloc(local_arg->filehead.dataChans*sizeof(TDSChInfo));
 pos=sm_TFileHead[0].offset;
 for (int channel=0; channel<local_arg->filehead.dataChans; channel++) {
  fseek(local_arg->infile,pos,SEEK_SET);
  if (read_struct((char *)&local_arg->file_ch_info[channel], sm_TFilChInfo, local_arg->infile)==0) {
   ERREXIT1(tinfo->emethods, "read_cfs_init: Header read error on file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)local_arg->file_ch_info[channel]&, sm_TFilChInfo);
#endif
  pos+=sm_TFilChInfo[0].offset;
 }
 local_arg->DSTable=(uint32_t *)malloc(local_arg->filehead.dataSecs*sizeof(uint32_t));
 fseek(local_arg->infile,local_arg->filehead.tablePos,SEEK_SET);
 if (fread((char *)local_arg->DSTable, sizeof(uint32_t), local_arg->filehead.dataSecs, local_arg->infile)!=local_arg->filehead.dataSecs) {
  ERREXIT1(tinfo->emethods, "read_cfs_init: DSTable read error on file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifndef LITTLE_ENDIAN
 for (int datasec=0; datasec<local_arg->filehead.dataSecs; datasec++) {
  Intel_int32(&local_arg->DSTable[datasec])
 }
#endif

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : local_arg->filehead.dataSecs);
 /*}}}  */

 local_arg->current_epoch=local_arg->fromepoch-1;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_cfs(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_cfs(transform_info_ptr tinfo) {
 struct read_cfs_storage *local_arg=(struct read_cfs_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 TDataHead datahead;
 long ChInfoPos;
 char *data_section;
 char *in_buffer;
 long channelmem_required=0;

 if (local_arg->epochs--==0) return NULL;
 /* Read the data section headers */
 fseek(local_arg->infile,local_arg->DSTable[local_arg->current_epoch],SEEK_SET);
 if (read_struct((char *)&datahead, sm_TDataHead, local_arg->infile)==0) {
  ERREXIT1(tinfo->emethods, "read_cfs: DS header read error on file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&datahead, sm_TDataHead);
#endif
 ChInfoPos=local_arg->DSTable[local_arg->current_epoch]+sm_TDataHead[0].offset;
 for (int channel=0; channel<local_arg->filehead.dataChans; channel++) {
  fseek(local_arg->infile,ChInfoPos,SEEK_SET);
  if (read_struct((char *)&local_arg->ds_ch_info[channel], sm_TDSChInfo, local_arg->infile)==0) {
   ERREXIT1(tinfo->emethods, "read_cfs: DS channel header read error on file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->ds_ch_info[channel], sm_TDSChInfo);
#endif
  ChInfoPos+=sm_TDSChInfo[0].offset;
 }

 /* Now read the data section */
 data_section=(char *)malloc(datahead.dataSz);
 fseek(local_arg->infile,datahead.dataSt,SEEK_SET);
 if (fread(data_section, 1, datahead.dataSz, local_arg->infile)!=datahead.dataSz) {
  ERREXIT1(tinfo->emethods, "read_cfs: DS read error on file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 
 tinfo->sfreq=1.0/local_arg->ds_ch_info[0].scaleX;
 tinfo->nr_of_points=local_arg->ds_ch_info[0].dataPoints;
 tinfo->beforetrig=rint(local_arg->ds_ch_info[0].offsetX/local_arg->ds_ch_info[0].scaleX);
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->nr_of_channels=local_arg->filehead.dataChans;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_cfs: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 tinfo->multiplexed=FALSE;
 myarray.nr_of_elements=tinfo->nr_of_points;
 myarray.nr_of_vectors=tinfo->nr_of_channels;
 tinfo->xdata=NULL;
 if (array_allocate(&myarray)==NULL
  || (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL) {
  ERREXIT(tinfo->emethods, "read_cfs: Error allocating data\n");
 }
 /*}}}  */

 snprintf(tinfo->comment, MAX_COMMENTLEN, "read_cfs %s; ", args[ARGS_IFILE].arg.s);
 in_buffer=tinfo->comment+strlen(tinfo->comment);
 strncpy(in_buffer, local_arg->filehead.dateStr, 8);
 in_buffer[8]=' ';
 strncpy(in_buffer+9, local_arg->filehead.timeStr, 8);
 in_buffer[17]='\0';

 for (int channel=0; channel<local_arg->filehead.dataChans; channel++) {
  char *indata=data_section+local_arg->ds_ch_info[channel].dataOffset;
  int const element_skip=local_arg->file_ch_info[channel].dSpacing;
  do {
   DATATYPE val=0.0;
   switch ((enum var_storage_enum)local_arg->file_ch_info[channel].dType) {
    case INT1:
     val=*(int8_t *)indata;
     break;
    case WRD1:
     val=*(uint8_t *)indata;
     break;
    case INT2:
#ifndef LITTLE_ENDIAN
     Intel_int16((int16_t *)indata);
#endif
     val=*(int16_t *)indata;
     break;
    case WRD2:
#ifndef LITTLE_ENDIAN
     Intel_int16((int16_t *)indata);
#endif
     val=*(uint16_t *)indata;
     break;
    case INT4:
#ifndef LITTLE_ENDIAN
     Intel_int32((int16_t *)indata);
#endif
     val=*(int32_t *)indata;
     break;
    case RL4:
#ifndef LITTLE_ENDIAN
     Intel_float((int16_t *)indata);
#endif
     val=*(float *)indata;
     break;
    case RL8:
#ifndef LITTLE_ENDIAN
     Intel_double((int16_t *)indata);
#endif
     val=*(double *)indata;
     break;
    case LSTR:
     /* LSTR does NOT have a count byte at the start but is \0-terminated... */
     break;
   }
   array_write(&myarray,val*local_arg->ds_ch_info[channel].scaleY+local_arg->ds_ch_info[channel].offsetY);
   indata+=element_skip;
  } while (myarray.message==ARRAY_CONTINUE);
 }

 free(data_section);

 /* Transfer channel names. */
 for (int channel=0; channel<tinfo->nr_of_channels; channel++) {
  channelmem_required+=strlen(local_arg->file_ch_info[channel].chanName+1)+1;
 }
 in_buffer=(char *)malloc(channelmem_required);
 tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *));
 if (in_buffer==NULL || tinfo->channelnames==NULL) {
  ERREXIT(tinfo->emethods, "read_cfs: Error allocating channelnames array\n");
 }
 for (int channel=0; channel<tinfo->nr_of_channels; channel++) {
  tinfo->channelnames[channel]=in_buffer;
  strcpy(in_buffer, local_arg->file_ch_info[channel].chanName+1);
  in_buffer=in_buffer+strlen(in_buffer)+1;
 }
 tinfo->probepos=NULL;
 create_channelgrid(tinfo); /* Create defaults for probe pos */

 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;
 local_arg->current_epoch++;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_cfs_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_cfs_exit(transform_info_ptr tinfo) {
 struct read_cfs_storage *local_arg=(struct read_cfs_storage *)tinfo->methods->local_storage;

 if (local_arg->infile!=NULL && local_arg->infile!=stdin) fclose(local_arg->infile);
 local_arg->infile=NULL;
 free_pointer((void **)&local_arg->DSTable);
 free_pointer((void **)&local_arg->ds_ch_info);
 free_pointer((void **)&local_arg->file_ch_info);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_cfs(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_cfs(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_cfs_init;
 tinfo->methods->transform= &read_cfs;
 tinfo->methods->transform_exit= &read_cfs_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_cfs";
 tinfo->methods->method_description=
  "Get_epoch method to read Cambridge Electronic Design (tm) CFS data.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_cfs_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
