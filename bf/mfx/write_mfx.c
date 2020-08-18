/*
 * Copyright (C) 2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_mfx.c module to write data to an MFX file
 *	-- Bernd Feige 27.01.1997
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <limits.h>
#include <mfx_file.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

#ifdef FLOAT_DATATYPE
#define MFX_DATATYPE MFX_FLOATS
#endif
#ifdef DOUBLE_DATATYPE
#define MFX_DATATYPE MFX_DOUBLES
#endif

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_CREATE_TRIGGERCHANNEL,
 ARGS_CONTINUOUS,
 ARGS_OFILE,
 ARGS_CONVFACTOR,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Create a TRIGGER channel", "T", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Continuous output", "c", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_DOUBLE, "conv_factor", "", 1, NULL}
};

/*{{{  struct write_mfx_storage {*/
struct write_mfx_storage {
 MFX_FILE *mfxfile;	/* Output file */
};
/*}}}  */

/*{{{  write_mfx_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_mfx_init(transform_info_ptr tinfo) {
 struct write_mfx_storage *local_arg=(struct write_mfx_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int NoOfChannels=tinfo->nr_of_channels;
 int channel;

 local_arg->mfxfile=mfx_open(args[ARGS_OFILE].arg.s, "r+b", MFX_DATATYPE);
 if (!args[ARGS_APPEND].is_set || local_arg->mfxfile==NULL) {   /* target does not exist*/
  /*{{{  Create file*/
  if (local_arg->mfxfile!=NULL) mfx_close(local_arg->mfxfile);
  if ((local_arg->mfxfile=mfx_create(args[ARGS_OFILE].arg.s, (args[ARGS_CREATE_TRIGGERCHANNEL].is_set ? NoOfChannels+1 : NoOfChannels), MFX_DATATYPE))==NULL) {
   ERREXIT1(tinfo->emethods, "write_mfx_init: Can't open file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
  if (mfx_describefile(local_arg->mfxfile, tinfo->comment, 1.0/tinfo->sfreq, (args[ARGS_CONTINUOUS].is_set ? 0 : tinfo->nr_of_points), -tinfo->beforetrig/tinfo->sfreq)!=MFX_NOERR) {
   ERREXIT1(tinfo->emethods, "write_mfx_init: mfx_describefile error %s\n", MSGPARM(mfx_errors[mfx_lasterr]));
  }
  for (channel=0; channel<NoOfChannels; channel++) {
   double * const pos_mfx=local_arg->mfxfile->channelheaders[channel].position;
   double * const pos_tinfo=tinfo->probepos+3*channel;
   int i;
   const Bool is_electric=(*tinfo->channelnames[channel]!='M');
   if (mfx_describechannel(local_arg->mfxfile, channel+1, tinfo->channelnames[channel], (is_electric ? "uV" : "fT"),
    (is_electric ? DT_EEGTYPE : DT_MEGTYPE), SHRT_MIN/args[ARGS_CONVFACTOR].arg.d, SHRT_MAX/args[ARGS_CONVFACTOR].arg.d)!=MFX_NOERR) {
    ERREXIT1(tinfo->emethods, "write_mfx_init: mfx_describechannel error %s\n", MSGPARM(mfx_errors[mfx_lasterr]));
   }
   for (i=0; i<3; i++) {
    pos_mfx[i]=pos_tinfo[i];
   }
  }
  if (args[ARGS_CREATE_TRIGGERCHANNEL].is_set) {
   if (mfx_describechannel(local_arg->mfxfile, channel+1, "TRIGGER", "T", DT_TRIGGER, -32768.0, 32767.0)!=MFX_NOERR) {
    ERREXIT1(tinfo->emethods, "write_mfx_init: mfx_describechannel error %s\n", MSGPARM(mfx_errors[mfx_lasterr]));
   }
  }
  /*}}}  */
 } else {
  /*{{{  Append to file*/
  mfx_seek(local_arg->mfxfile, 0L, SEEK_END);
  /*}}}  */
 }
 if ((local_arg->mfxfile->selected_channels=(int *)malloc(NoOfChannels*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "write_mfx_init: Error allocating selection array\n");
 }
 for (channel=0; channel<NoOfChannels; channel++) {
  local_arg->mfxfile->selected_channels[channel]=channel+1;
 }
 local_arg->mfxfile->nr_of_channels_selected=NoOfChannels;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_mfx(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_mfx(transform_info_ptr tinfo) {
 struct write_mfx_storage *local_arg=(struct write_mfx_storage *)tinfo->methods->local_storage;

 /*{{{  Assert that epoch size didn't change & itemsize==1*/
 if (tinfo->itemsize!=1) {
  ERREXIT(tinfo->emethods, "write_mfx: Only itemsize=1 is supported.\n");
 }
 if (local_arg->mfxfile->nr_of_channels_selected!=tinfo->nr_of_channels) {
  ERREXIT2(tinfo->emethods, "write_mfx: nr_of_channels was %d, now %d\n", MSGPARM(local_arg->mfxfile->nr_of_channels_selected), MSGPARM(tinfo->nr_of_channels));
 }
 /*}}}  */

 multiplexed(tinfo);
 if (tinfo->data_type==FREQ_DATA) tinfo->nr_of_points=tinfo->nroffreq;
 if (mfx_write((void *)tinfo->tsdata, tinfo->nr_of_points, local_arg->mfxfile)!=MFX_NOERR) {
  ERREXIT1(tinfo->emethods, "write_mfx_init: mfx_write error %s\n", MSGPARM(mfx_errors[mfx_lasterr]));
 }

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_mfx_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_mfx_exit(transform_info_ptr tinfo) {
 struct write_mfx_storage *local_arg=(struct write_mfx_storage *)tinfo->methods->local_storage;
 mfx_close(local_arg->mfxfile);
 local_arg->mfxfile=NULL;
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_mfx(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_mfx(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_mfx_init;
 tinfo->methods->transform= &write_mfx;
 tinfo->methods->transform_exit= &write_mfx_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_mfx";
 tinfo->methods->method_description=
  "Put-epoch method to write data to an MFX (Munster File Exchange) file\n";
 tinfo->methods->local_storage_size=sizeof(struct write_mfx_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
