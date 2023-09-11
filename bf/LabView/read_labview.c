/*
 * Copyright (C) 2008,2012,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_labview.c module to read data from an hdf file
 *	-- Bernd Feige 26.03.2008
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Intel_compat.h>
#include "LabView.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

#define MAX_COMMENTLEN 1024
/* Compatible with synamps_sm.c */
#define trigcode_STARTSTOP 256

/*{{{  #defines*/
enum ARGS_ENUM {
 ARGS_FROMEPOCH=0, 
 ARGS_EPOCHS,
 ARGS_IFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.Dat"},
};
/*}}}  */

/*{{{  Definition of read_labview_storage*/
struct descriptor {
 LabView_TD1 TD1;
 LabView_TD2 TD2;
 LabView_Offset_Datatype datastart;
};
struct read_labview_storage {
 FILE *infile;
 int fromepoch;
 int epochs;
 int last_time_offset; /* Difference between absolute and file time, in seconds */
 int current_epoch;
 growing_buf descriptors;
 struct descriptor *current_descriptor;
};
/*}}}  */

/*{{{  read_labview_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_labview_init(transform_info_ptr tinfo) {
 struct read_labview_storage *local_arg=(struct read_labview_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int i;
 LabView_Offset_Datatype nexttable=LabView_First_Offset_Table;
 int current_table=0;
 growing_buf offset_tables;
 growing_buf offsets;

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 if (strcmp(args[ARGS_IFILE].arg.s, "stdin")==0) local_arg->infile=stdin;
 else if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_labview_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }

 growing_buf_init(&local_arg->descriptors);
 growing_buf_allocate(&local_arg->descriptors,0);

 growing_buf_init(&offset_tables);
 growing_buf_allocate(&offset_tables,0);
 growing_buf_append(&offset_tables,(char *)&nexttable,sizeof(nexttable));

 growing_buf_init(&offsets);
 growing_buf_allocate(&offsets,0);
 while (current_table<offset_tables.current_length/sizeof(nexttable)) {
  nexttable=((LabView_Offset_Datatype *)offset_tables.buffer_start)[current_table];
  fseek(local_arg->infile,nexttable,SEEK_SET);
  growing_buf_clear(&offsets);
  for (i=0; i<LabView_Entries_per_Offset_Table; i++) {
   LabView_Offset_Datatype offset;
   if (1!=fread(&offset, sizeof(LabView_Offset_Datatype), 1, local_arg->infile)) {
    ERREXIT(tinfo->emethods, "read_labview: Error reading offset\n");
   }
#ifdef LITTLE_ENDIAN
   if (sizeof(offset)==4) {
    Intel_int32((uint32_t *)&offset);
   } else {
    Intel_int16((uint16_t *)&offset);
   }
#endif
   if (offset==0) {
    nexttable=0;
    break;
   }
   growing_buf_append(&offsets,(char *)&offset,sizeof(offset));
  }
  for (i=0; i<offsets.current_length/sizeof(LabView_Offset_Datatype); i++) {
   LabView_Offset_Datatype const offset=((LabView_Offset_Datatype *)offsets.buffer_start)[i];
   struct descriptor d;
   fseek(local_arg->infile,offset,SEEK_SET);
   if (1==read_struct((char *)&d.TD1, sm_LabView_TD1, local_arg->infile)) {
#   ifdef LITTLE_ENDIAN
    change_byteorder((char *)&d.TD1, sm_LabView_TD1);
#   endif
    if (1==read_struct((char *)&d.TD2, sm_LabView_TD2, local_arg->infile)) {
#    ifdef LITTLE_ENDIAN
     change_byteorder((char *)&d.TD2, sm_LabView_TD2);
#    endif
     //print_structcontents((char *)&d.TD1, sm_LabView_TD1, smd_LabView_TD1, stdout);
     //print_structcontents((char *)&d.TD2, sm_LabView_TD2, smd_LabView_TD2, stdout);
     d.datastart=ftell(local_arg->infile);
     growing_buf_append(&local_arg->descriptors,(char *)&d,sizeof(d));
    }
   }
  }
  if (nexttable==LabView_First_Offset_Table) {
   struct descriptor *last_descriptor=(struct descriptor *)((struct descriptor *)(local_arg->descriptors.buffer_start+local_arg->descriptors.current_length))-1;
   LabView_TD2 * const TD2p=&last_descriptor->TD2;
   //print_structcontents((char *)TD2p, sm_LabView_TD2, smd_LabView_TD2, stdout);
   /* Skip the data and the pointer to the previous table */
   fseek(local_arg->infile,TD2p->nr_of_channels*TD2p->nr_of_points*sizeof(LabView_Datatype)+sizeof(LabView_Offset_Datatype),SEEK_CUR);
   while (TRUE) {
    if (1!=fread(&nexttable,sizeof(LabView_Offset_Datatype),1,local_arg->infile)) {
     ERREXIT(tinfo->emethods, "read_labview_init: Error reading nexttable\n");
    }
#ifdef LITTLE_ENDIAN
    if (sizeof(nexttable)==4) {
     Intel_int32((uint32_t *)&nexttable);
    } else {
     Intel_int16((uint16_t *)&nexttable);
    }
#endif
    if (nexttable==0) break;
    growing_buf_append(&offset_tables,(char *)&nexttable,sizeof(nexttable));
   }
  }
  current_table++;
 }
 TRACEMS1(tinfo->emethods, 1, "read_labview_init: %d epochs\n", MSGPARM(local_arg->descriptors.current_length/sizeof(struct descriptor)));

 local_arg->current_descriptor=((struct descriptor *)local_arg->descriptors.buffer_start)+local_arg->fromepoch-1;
 /* This of course is only correct if all blocks have the same length, but it is informational only: */
 tinfo->points_in_file=local_arg->descriptors.current_length/sizeof(struct descriptor)*local_arg->current_descriptor->TD2.nr_of_points;
 /* Impossible value since time since program start is always larger than recorded seconds */
 local_arg->last_time_offset= -1;
 /* Used to determine the point number counting from the start of the file, for writing start/stop triggers */
 local_arg->current_epoch= local_arg->fromepoch-1;

 growing_buf_free(&offsets);
 growing_buf_free(&offset_tables);
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_labview(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_labview(transform_info_ptr tinfo) {
 struct read_labview_storage *local_arg=(struct read_labview_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 int const this_time_offset=local_arg->current_descriptor->TD1.SecTot-local_arg->current_descriptor->TD1.SecDR;

 if (local_arg->epochs-- ==0 ||
     (char *)local_arg->current_descriptor-local_arg->descriptors.buffer_start>=local_arg->descriptors.current_length) {
  return NULL;
 }

 tinfo->nr_of_channels=local_arg->current_descriptor->TD2.nr_of_channels;
 tinfo->nr_of_points=local_arg->current_descriptor->TD2.nr_of_points;
 tinfo->sfreq=local_arg->current_descriptor->TD2.nr_of_points;
 tinfo->beforetrig=0;

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 tinfo->multiplexed=TRUE;
 myarray.nr_of_elements=tinfo->nr_of_channels;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 tinfo->xdata=NULL;
 if (array_allocate(&myarray)==NULL
  || (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL) {
  ERREXIT(tinfo->emethods, "read_labview: Error allocating data\n");
 }
 /*}}}  */

 snprintf(tinfo->comment, MAX_COMMENTLEN, "read_labview %s", args[ARGS_IFILE].arg.s);

 /* Read data */
 fseek(local_arg->infile,local_arg->current_descriptor->datastart,SEEK_SET);
 TRACEMS1(tinfo->emethods, 1, "read_labview: Reading at %d\n", MSGPARM(local_arg->current_descriptor->datastart));
 do {
  LabView_Datatype d;
  if (1!=fread(&d,sizeof(LabView_Datatype),1,local_arg->infile)) {
   ERREXIT(tinfo->emethods, "read_labview: Error reading data\n");
  }
#ifdef LITTLE_ENDIAN
  if (sizeof(d)==4) {
   Intel_int32((uint32_t *)&d);
  } else {
   Intel_int16((uint16_t *)&d);
  }
#endif
  array_write(&myarray,(DATATYPE)d);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 /*{{{  Handle triggers within the epoch (time skips = STARTSTOP) */
 if (local_arg->last_time_offset>=0 && this_time_offset!=local_arg->last_time_offset) {
  push_trigger(&tinfo->triggers, local_arg->current_epoch*tinfo->sfreq, -1, NULL);
  push_trigger(&tinfo->triggers, 0L, trigcode_STARTSTOP, "PAUSE");
  push_trigger(&tinfo->triggers, 0L, 0, NULL); /* End of list */
 }
 local_arg->last_time_offset=this_time_offset;
 /*}}}  */

 /* This subroutine prepares any channel names/positions that might not be set */
 create_channelgrid(tinfo);

 /*{{{  Setup tinfo values*/
 tinfo->file_start_point=local_arg->current_epoch*tinfo->nr_of_points;
 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->data_type=TIME_DATA;
 /*}}}  */

 local_arg->current_descriptor++;
 local_arg->current_epoch++;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_labview_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_labview_exit(transform_info_ptr tinfo) {
 struct read_labview_storage *local_arg=(struct read_labview_storage *)tinfo->methods->local_storage;

 if (local_arg->infile!=stdin) fclose(local_arg->infile);
 local_arg->infile=NULL;
 growing_buf_free(&local_arg->descriptors);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_labview(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_labview(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_labview_init;
 tinfo->methods->transform= &read_labview;
 tinfo->methods->transform_exit= &read_labview_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_labview";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from a LabView(tm) file.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_labview_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
