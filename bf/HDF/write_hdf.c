/*
 * Copyright (C) 2008-2015,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_hdf.c method to write data to a HDF file
 *	-- Bernd Feige 21.10.1996
 *
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "transform.h"
#include "bf.h"

#include "mfhdf.h"
/*}}}  */

/*{{{  Arguments*/
LOCAL const char *const compression_choice[]={
 "-rle",
 "-nbit",
 "-skphuff",
 "-deflate",
 NULL
};
LOCAL comp_coder_t compression_types[]={
 COMP_CODE_RLE,
 COMP_CODE_NBIT,
 COMP_CODE_SKPHUFF,
 COMP_CODE_DEFLATE,
};
enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_CONTINUOUS,
 ARGS_COMPRESS, 
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Continuous output (unlimited first dimension)", "c", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "Choose output compression", " ", 3, compression_choice},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.hdf"}
};
/*}}}  */

#ifdef FLOAT_DATATYPE
#define DF_DATATYPE DFNT_FLOAT32
#endif
#ifdef DOUBLE_DATATYPE
#define DF_DATATYPE DFNT_FLOAT64
#endif

#ifndef MAX_NC_NAME
#define MAX_NC_NAME H4_MAX_NC_NAME
#endif

/* MAXRANK_FOR_MYDATA defines what is the maximum dimensionality acceptable to us */
#define MAXRANK_FOR_MYDATA 3

/*{{{  Definition of write_hdf_storage*/
/* This structure has to be defined to carry the sampling freq over epochs */
struct write_hdf_storage {
 int32 fileid;
 int32 sdsid;
 int32 pointno;
 growing_buf triggers;
};
/*}}}  */

/*{{{  write_hdf_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_hdf_init(transform_info_ptr tinfo) {
 struct write_hdf_storage *local_arg=(struct write_hdf_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 Bool append=args[ARGS_APPEND].is_set;

 local_arg->pointno=0;
 if (append) {
  struct stat statbuff;
  if (stat(args[ARGS_OFILE].arg.s, &statbuff)!=0) {
   append=FALSE;
  }
 }
 local_arg->fileid=SDstart(args[ARGS_OFILE].arg.s, append ? DFACC_RDWR : DFACC_CREATE);
 if (local_arg->fileid==FAIL) {
  ERREXIT1(tinfo->emethods, "write_hdf_init: Error opening file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 if (args[ARGS_CONTINUOUS].is_set) {
  /*{{{  Look for a compatible dataset to append to (otherwise, a new one is created)*/
  Bool again;
  int current_epoch=0;
  int32 rank, dimid, dimsize, dimnt, dimnattr, dims[MAXRANK_FOR_MYDATA], nt, nattr[MAXRANK_FOR_MYDATA];
  char dimname[MAX_NC_NAME];
  do {
   again=FALSE;
   local_arg->sdsid=SDselect(local_arg->fileid, current_epoch);
   if (local_arg->sdsid==FAIL) break;
   SDgetinfo(local_arg->sdsid, NULL, &rank, dims, &nt, nattr);
   dimid = SDgetdimid(local_arg->sdsid, 0);
   SDdiminfo(dimid, dimname, &dimsize, &dimnt, &dimnattr);
   /* dimsize==0 is the indication for UNLIMITED length */
   if (dimsize==0 && !SDiscoordvar(local_arg->sdsid) && nt==DF_DATATYPE && rank<=MAXRANK_FOR_MYDATA) {
    again=!(((rank==2 && tinfo->itemsize==1) || 
             (rank==3 && tinfo->itemsize>1 && dims[2]==tinfo->itemsize))
         && dims[1]==tinfo->nr_of_channels);
    if (!again) local_arg->pointno=dims[0]; /* Start appending at the end, of course... */
   } else {
    again=TRUE;
   }
   current_epoch++;
  } while (again);
  /* Initialize the buffer to record triggers in */
  growing_buf_init(&local_arg->triggers);
  /*}}}  */
 } else {
  /* This means that a new dataset will be created */
  local_arg->sdsid=FAIL;
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  record_triggers(transform_info_ptr tinfo) {*/
LOCAL void
record_triggers(transform_info_ptr tinfo) {
 /* In order to write the full trigger list using write_triggers() at the end, we
  * record triggers from all epochs encountered in continuous mode */
 if (tinfo->triggers.buffer_start!=NULL) {
  struct write_hdf_storage *local_arg=(struct write_hdf_storage *)tinfo->methods->local_storage;
  struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
  if (local_arg->triggers.current_length==0) {
   push_trigger(&local_arg->triggers,0L,-1,NULL);
  }
  while (intrig->code!=0) {
   push_trigger(&local_arg->triggers,local_arg->pointno+intrig->position,intrig->code,intrig->description);
   intrig++;
  }
 }
}
/*}}}  */

/*{{{  write_triggers(transform_info_ptr tinfo, growing_buf *triggersp, int32 sdsid) {*/
LOCAL void
write_triggers(transform_info_ptr tinfo, growing_buf *triggersp, int32 sdsid) {
 if (triggersp->buffer_start!=NULL) {
  /* Write triggers array as a data set attribute */
  struct trigger * const intrig=(struct trigger *)triggersp->buffer_start;
  int32 * trigentries;
  int n_entries=1;

  while (intrig[n_entries].code!=0) n_entries++;
  if (n_entries<=1) return;
  if ((trigentries=(int32 *)malloc(2*n_entries*sizeof(int32)))==NULL) {
   ERREXIT(tinfo->emethods, "write_hdf: Error allocating trigger array\n");
  }
  n_entries=0;
  do {
   trigentries[2*n_entries]  =intrig[n_entries].position;
   trigentries[2*n_entries+1]=intrig[n_entries].code;
   n_entries++;
  } while (intrig[n_entries].code!=0);
  if (intrig[0].position>0) {
   SDsetattr(sdsid, "Triggers", DFNT_INT32, 2*n_entries, trigentries);
  } else {
   SDsetattr(sdsid, "Triggers", DFNT_INT32, 2*(n_entries-1), trigentries+2);
  }
  free(trigentries);
 }
}
/*}}}  */

/*{{{  write_hdf(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_hdf(transform_info_ptr tinfo) {
 struct write_hdf_storage *local_arg=(struct write_hdf_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel;
 char const *pointsname=(tinfo->data_type==TIME_DATA ? "Time" : "Frequency"), *channelsname="Channels";
 int32 pointsdim, channelsdim;

 int ret;
 int32 rank, sdsid=local_arg->sdsid, dimid, starts[MAXRANK_FOR_MYDATA], ends[MAXRANK_FOR_MYDATA], dims[MAXRANK_FOR_MYDATA];
 comp_info cinfo;

 if (args[ARGS_CONTINUOUS].is_set) multiplexed(tinfo);

 /*{{{  Prepare the variables for SDcreate and SDwritedata*/
 if (tinfo->itemsize==1) {
  rank=2;
 } else {
  rank=3;
  dims[2]=tinfo->itemsize;
 }
 if (tinfo->multiplexed) {
  pointsdim=0;
  channelsdim=1;
 } else {
  channelsdim=0;
  pointsdim=1;
 }
 starts[0]=starts[1]=starts[2]=0;
 dims[pointsdim]=(args[ARGS_CONTINUOUS].is_set ? SD_UNLIMITED : tinfo->nr_of_points);
 ends[pointsdim]=tinfo->nr_of_points;
 ends[channelsdim]=dims[channelsdim]=tinfo->nr_of_channels;
 ends[2]=dims[2];
 /*}}}  */
 
 if (!args[ARGS_CONTINUOUS].is_set || sdsid==FAIL) {
  /*{{{  Create a new dataset*/
  sdsid = SDcreate(local_arg->fileid, tinfo->comment, DF_DATATYPE, rank, dims);
  if (sdsid == FAIL) {
   ERREXIT(tinfo->emethods, "write_hdf: SDcreate failed\n");
  }
  if (args[ARGS_COMPRESS].is_set) {
   cinfo.skphuff.skp_size=sizeof(DATATYPE);
   cinfo.deflate.level=9;
   ret=SDsetcompress(sdsid, compression_types[args[ARGS_COMPRESS].arg.i], &cinfo);
   if (ret == FAIL) {
    ERREXIT(tinfo->emethods, "write_hdf: SDsetcompress failed\n");
   }
  }
  dimid=SDgetdimid(sdsid, pointsdim);
  SDsetdimname(dimid, pointsname);
  dimid=SDgetdimid(sdsid, channelsdim);
  SDsetdimname(dimid, channelsname);
  /* For now, we store channel names and probe positions as name=position
   * attributes of the channel dimension. There doesn't appear to be a string
   * array type (only a char array), otherwise that would be better... */
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   SDsetattr(dimid, tinfo->channelnames[channel], DFNT_FLOAT64, 3, &tinfo->probepos[3*channel]);
  }
  if (tinfo->itemsize>1) {
   dimid=SDgetdimid(sdsid, 2);
   SDsetdimname(dimid, "Item");
  }
  SDsetattr(sdsid, "Sfreq", DF_DATATYPE, 1, &tinfo->sfreq);
  SDsetattr(sdsid, "Beforetrig", DFNT_INT32, 1, &tinfo->beforetrig);
  SDsetattr(sdsid, "Leaveright", DFNT_INT32, 1, &tinfo->leaveright);
  SDsetattr(sdsid, "Condition", DFNT_INT32, 1, &tinfo->condition);
  SDsetattr(sdsid, "Nr_of_averages", DFNT_INT32, 1, &tinfo->nrofaverages);
  if (args[ARGS_CONTINUOUS].is_set) {
   record_triggers(tinfo);
  } else {
   write_triggers(tinfo,&tinfo->triggers,sdsid);
  }
  /*}}}  */
 } else {
  /*{{{  Append to an existing dataset*/
  if (dims[1]!=tinfo->nr_of_channels) {
   ERREXIT(tinfo->emethods, "write_hdf: Cannot write epochs with varying nr_of_channels!\n");
  }
  starts[pointsdim]=local_arg->pointno;
  record_triggers(tinfo);
  /*}}}  */
 }
 
 ret=SDwritedata(sdsid, starts, NULL, ends, (VOIDP)tinfo->tsdata);
 if (ret == FAIL) {
  ERREXIT(tinfo->emethods, "write_hdf: SDwritedata failed\n");
 }
 
 local_arg->pointno+=tinfo->nr_of_points;
 
 if (args[ARGS_CONTINUOUS].is_set) {
  local_arg->sdsid=sdsid;
 } else {
  SDendaccess(sdsid);
 }

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_hdf_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_hdf_exit(transform_info_ptr tinfo) {
 struct write_hdf_storage *local_arg=(struct write_hdf_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_CONTINUOUS].is_set) {
  if (local_arg->triggers.current_length>0) {
   push_trigger(&local_arg->triggers,0L,0,NULL); /* End of list */
   write_triggers(tinfo,&local_arg->triggers,local_arg->sdsid);
  }
  if (local_arg->triggers.buffer_start!=NULL) {
   clear_triggers(&local_arg->triggers);
   growing_buf_free(&local_arg->triggers);
  }
  SDendaccess(local_arg->sdsid);
 }
 SDend(local_arg->fileid);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_hdf(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_hdf(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_hdf_init;
 tinfo->methods->transform= &write_hdf;
 tinfo->methods->transform_exit= &write_hdf_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_hdf";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to an HDF file.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_hdf_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
