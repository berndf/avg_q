/*
 * Copyright (C) 2022,2023 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_nke.c module to write data to a Nihon Kohden 2110/1100 file
 *	-- Bernd Feige 10.12.2022
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <math.h>
#include <float.h>
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "nke_format.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL}
};
LOCAL char const *const file_signature="EEG-2100  V02.00";

/*{{{  struct write_nke_storage {*/
struct write_nke_storage {
 growing_buf triggers;
 FILE *outfile;
};
/*}}}  */

LOCAL void write_21e(char const *eegfilename, transform_info_ptr tinfo) {
 struct write_nke_storage *local_arg=(struct write_nke_storage *)tinfo->methods->local_storage;
 growing_buf filename;
 growing_buf_init(&filename);
 growing_buf_takethis(&filename,eegfilename);
 char *const dot=strrchr(filename.buffer_start,'.');
 if (dot!=NULL) {
  filename.current_length=dot-filename.buffer_start+1;
 }
 growing_buf_appendstring(&filename,".21e");
 FILE *file21e=fopen(filename.buffer_start, "wb");
 if (file21e!=NULL) {
  fprintf(file21e,"[ELECTRODE]\r\n");
  if (tinfo->channelnames==NULL) create_channelgrid(tinfo);
  for (int channel=0; channel<tinfo->nr_of_channels; channel++) {
   fprintf(file21e,"%04d=%s\r\n", channel, tinfo->channelnames[channel]);
  }
  fprintf(file21e,"[REFERENCE]\r\n");
  fclose(file21e);
 } else {
  TRACEMS(tinfo->emethods, 1, "write_nke write_21e: Cannot write .21e file\n");
 }
 growing_buf_free(&filename);
}

/*{{{  write_nke_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_nke_init(transform_info_ptr tinfo) {
 struct write_nke_storage *local_arg=(struct write_nke_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 write_21e(args[ARGS_OFILE].arg.s, tinfo);

 if ((local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, "wb"))==NULL) {
  ERREXIT1(tinfo->emethods, "write_nke_init: Can't create EEG file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 struct nke_file_header file_header;
 strncpy((char *)&file_header.signature1,file_signature,sm_nke_file_header[1].length);
 file_header.addfilename1[0]='\0';
 file_header.addfilename2[0]='\0';
 strncpy((char *)&file_header.datetime,"202207111531080",sm_nke_file_header[4].length);
 file_header.dummy2[0]='\0';
 strncpy((char *)&file_header.swversion,"JE-921A     ",sm_nke_file_header[6].length);
 strncpy((char *)&file_header.signature2,file_signature,sm_nke_file_header[7].length);
 file_header.ctl_block_cnt=1;
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&file_header, sm_nke_file_header);
#endif
 if (write_struct((char *)&file_header, sm_nke_file_header, local_arg->outfile)==0) {
  ERREXIT1(tinfo->emethods, "write_nke_init: Can't write header in file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 struct nke_control_block control_block;
 control_block.address=1024;
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&control_block, sm_nke_control_block);
#endif
 if (write_struct((char *)&control_block, sm_nke_control_block, local_arg->outfile)==0) {
  ERREXIT1(tinfo->emethods, "write_nke_init: Can't write header in file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 {int const fill_len=control_block.address-ftell(local_arg->outfile);
 for (int i=0; i<fill_len; i++) {
  fputc('\0', local_arg->outfile);
 }
 }
 struct nke_data_block1 data_block1;
 data_block1.dummy1=0;
 strncpy((char *)&data_block1.name,"EEG-1100INTEL   ",sm_nke_data_block1[2].length);
 data_block1.datablock_cnt=1;
 data_block1.wfmblock_address=6142;
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&data_block1, sm_nke_data_block1);
#endif
 if (write_struct((char *)&data_block1, sm_nke_data_block1, local_arg->outfile)==0) {
  ERREXIT1(tinfo->emethods, "write_nke_init: Can't write header in file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 {int const fill_len=data_block1.wfmblock_address-ftell(local_arg->outfile);
 for (int i=0; i<fill_len; i++) {
  fputc('\0', local_arg->outfile);
 }
 }
 struct nke_wfm_block wfm_block;
 wfm_block.sfreq=tinfo->sfreq;
 wfm_block.channels=tinfo->nr_of_channels;
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&wfm_block, sm_nke_wfm_block);
#endif
 if (write_struct((char *)&wfm_block, sm_nke_wfm_block, local_arg->outfile)==0) {
  ERREXIT1(tinfo->emethods, "write_nke_init: Can't write header in file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 for (int channel=0; channel<tinfo->nr_of_channels; channel++) {
  struct nke_channel_block channel_block;
  channel_block.chan=channel;
  channel_block.dummy3=5100000;
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&channel_block, sm_nke_channel_block);
#endif
  if (write_struct((char *)&channel_block, sm_nke_channel_block, local_arg->outfile)==0) {
   ERREXIT1(tinfo->emethods, "write_nke_init: Can't write header in file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
 }
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_nke(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_nke(transform_info_ptr tinfo) {
 struct write_nke_storage *local_arg=(struct write_nke_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_synamps_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_nke_exit(transform_info_ptr tinfo) {
 struct write_nke_storage *local_arg=(struct write_nke_storage *)tinfo->methods->local_storage;

 fclose(local_arg->outfile);
 local_arg->outfile=NULL;

 tinfo->methods->init_done=FALSE;
}
/*}}}  */
/*{{{  select_write_nke(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_nke(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_nke_init;
 tinfo->methods->transform= &write_nke;
 tinfo->methods->transform_exit= &write_nke_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_nke";
 tinfo->methods->method_description=
  "Put-epoch method to write data to a Nihon Kohden 2110/1100 file\n";
 tinfo->methods->local_storage_size=sizeof(struct write_nke_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
