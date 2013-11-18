/*
 * Copyright (C) 1996-1999,2001,2003,2006-2010 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_hdf.c module to read data from an hdf file
 *	-- Bernd Feige 21.10.1996
 *
 * HDF is special in that it supports both epoched and continuous data. 
 * We specify that beforetrig and aftertrig are optional. 
 * Another interesting difference is that multiple data sets may be present, also
 * with mixed dimensionality. Epochs are stored as separate data sets by default.
 *
 * Continuous file: is read as a single epoch if beforetrig and aftertrig are
 * missing or zero. Otherwise, the specified section around triggers given in
 * the file or via a trigger file is read.
 * Epoched file: Epochs are read.
 *
 * Problem with triggers: The dimension ID returned is the same for all data sets
 * in the file, correspondingly the attributes of the points dimension (triggers)
 * are always the same.
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"

#include "mfhdf.h"
/*}}}  */

/*{{{  #defines*/
enum ARGS_ENUM {
 ARGS_CONTINUOUS=0,
 ARGS_TRIGTRANSFER, 
 ARGS_TRIGLIST, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_TRIGFILE,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these `StimTypes'. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.hdf"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", " ", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", " ", ARGDESC_UNUSED, (const char *const *)"1s"}
};

#ifdef FLOAT_DATATYPE
#define DF_DATATYPE DFNT_FLOAT32
#define C_OTHERDATATYPE double
#endif
#ifdef DOUBLE_DATATYPE
#define DF_DATATYPE DFNT_FLOAT64
#define C_OTHERDATATYPE float
#endif

#ifndef MAX_NC_NAME
#define MAX_NC_NAME H4_MAX_NC_NAME
#endif

/* MAXRANK_FOR_MYDATA defines what is the maximum dimensionality acceptable to us */
#define MAXRANK_FOR_MYDATA 3

#define DESCBUF_LEN 2048
/*}}}  */

/*{{{  Definition of read_hdf_storage*/
struct read_hdf_storage {
 long current_point;
 int fromepoch;
 int epochs;
 int32 sdsid;
 int32 current_dataset;
 int32 ndatasets;
 int32 fileid;
 int32 rank;
 int32 dims[MAXRANK_FOR_MYDATA];
 int32 df_datatype;
 int32 sd_nattr;
 int32 points_dimid;
 int32 pointsdim;
 int32 channels_dimid;
 int32 channelsdim;
 char descbuf[DESCBUF_LEN];

 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
};
/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_hdf_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*{{{  read_hdf_build_trigbuffer(transform_info_ptr tinfo) {*/
LOCAL void 
read_hdf_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_hdf_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_hdf_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else if (local_arg->points_dimid!=FAIL) {
  char attrname[MAX_NC_NAME];
  int32 attrnum, attrnt, attrcount;
  if ((attrnum=SDfindattr(local_arg->sdsid, "Triggers"))!=FAIL) {
   int ntrigs;
   int this_entry=0;
   int32 *trigentries;
   SDattrinfo(local_arg->sdsid, attrnum, attrname, &attrnt, &attrcount);
   /* Triggers are stored as an attribute of the data set */
   //printf("attrnum=%ld, attrname=%s, attrcount=%ld, sdsid=%ld\n", attrnum, attrname, attrcount,local_arg->sdsid);
   ntrigs=attrcount/2-1;
   if (attrnt!=DFNT_INT32 || ntrigs<0 || attrcount%2!=0) {
    TRACEMS(tinfo->emethods, 0, "read_hdf warning: Trigger information unusable\n");
    return;
   }
   if ((trigentries=(int32 *)malloc(attrcount*sizeof(int32)))==NULL) {
    ERREXIT(tinfo->emethods, "read_hdf: Error allocating trigger memory.\n");
   }
   SDreadattr(local_arg->sdsid, attrnum, trigentries);
   while (this_entry<=ntrigs) {
    //printf("Trigger %ld %d\n", trigentries[2*this_entry], trigentries[2*this_entry+1]);
    push_trigger(&local_arg->triggers, trigentries[2*this_entry], trigentries[2*this_entry+1], NULL);
    this_entry++;
   }
   free(trigentries);
  }
 } else {
  TRACEMS(tinfo->emethods, 0, "read_hdf_build_trigbuffer: No trigger source known.\n");
 }
}
/*}}}  */
/*{{{  read_hdf_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_hdf_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.current_length==0) {
  /* Load the event information */
  read_hdf_build_trigbuffer(tinfo);
 }
 {
 int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);

 if (local_arg->current_trigger<nevents) {
  struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+local_arg->current_trigger;
  *position=intrig->position;
   code    =intrig->code;
  if (descriptionp!=NULL) *descriptionp=intrig->description;
 }
 }
 local_arg->current_trigger++;
 return code;
}
/*}}}  */
/*}}}  */

/*{{{  read_hdf_locate_dimensions(transform_info_ptr tinfo) {*/
LOCAL void 
read_hdf_locate_dimensions(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 const char *channelsname="Channels";
 int32 dimid, dimsize, dimnt, dimnattr;
 char dimname[MAX_NC_NAME];
 int dim;
 local_arg->points_dimid=local_arg->channels_dimid=FAIL;
 local_arg->channelsdim=FAIL;
 local_arg->pointsdim=FAIL;
 //printf("Getting dimensions for sdsid=%ld\n", local_arg->sdsid);
 for (dim=0; dim<=1; dim++) {
  dimid=SDgetdimid(local_arg->sdsid, dim);
  SDdiminfo(dimid, dimname, &dimsize, &dimnt, &dimnattr);
  if (strcmp(dimname, channelsname)==0) {
   //printf("Channels dimid %ld, dim %d:Dimname=%s, nattr=%ld\n", dimid, dim, dimname, dimnattr);
   tinfo->multiplexed=(dim==1); /* Multiplexed means pointsdim=0, channelsdim=1 */
   if (dimnattr==local_arg->dims[dim]) {
    local_arg->channelsdim=dim;
    local_arg->channels_dimid=dimid;
   }
  } else {
   //printf("Points dimid %ld, dim %d:Dimname=%s, nattr=%ld\n", dimid, dim, dimname, dimnattr);
   local_arg->pointsdim=dim;
   local_arg->points_dimid=dimid;
  }
 }
}
/*}}}  */
  
/*{{{ read_hdf_setup_channelinfo(transform_info_ptr tinfo) {*/
LOCAL void
read_hdf_setup_channelinfo(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 if (local_arg->channels_dimid!=FAIL) {
  int channel, nr_of_channels=local_arg->dims[local_arg->channelsdim];
  int namelen=0;
  char *innames, attrname[MAX_NC_NAME];
  int32 attrnt, attrcount;
 
  for (channel=0; channel<nr_of_channels; channel++) {
   SDattrinfo(local_arg->channels_dimid, channel, attrname, &attrnt, &attrcount);
   if (attrnt!=DFNT_FLOAT64 || attrcount!=3) {
    TRACEMS(tinfo->emethods, 0, "read_hdf warning: Channel name/position information unusable\n");
    return;
   }
   namelen+=strlen(attrname)+1;
  }
  if ((tinfo->channelnames=(char **)malloc(nr_of_channels*sizeof(char *)))==NULL ||
      (innames=(char *)malloc(namelen))==NULL ||
      (tinfo->probepos=(double *)malloc(3*nr_of_channels*sizeof(double)))==NULL) {
   ERREXIT(tinfo->emethods, "read_hdf: Error allocating names/probepos memory\n");
  }
  for (channel=0; channel<local_arg->dims[local_arg->channelsdim]; channel++) {
   SDattrinfo(local_arg->channels_dimid, channel, attrname, &attrnt, &attrcount);
   tinfo->channelnames[channel]=innames;
   strcpy(innames, attrname);
   innames+=strlen(innames)+1;
   SDreadattr(local_arg->channels_dimid, channel, &tinfo->probepos[3*channel]);
  }
 }
}/*}}}*/

/*{{{  read_hdf_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_hdf_init(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int32 nattr;

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 local_arg->fileid=SDstart(args[ARGS_IFILE].arg.s, DFACC_RDONLY);
 if (local_arg->fileid==FAIL) {
  ERREXIT1(tinfo->emethods, "read_hdf_init: Error opening %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 SDfileinfo(local_arg->fileid, &local_arg->ndatasets, &nattr);
 if (local_arg->ndatasets<=0) {
  ERREXIT1(tinfo->emethods, "read_hdf_init: Can't find a Scientific Data Set in %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_hdf_init: Error allocating triglist memory\n");
   }
  }
 }
 read_hdf_reset_triggerbuffer(tinfo);
 local_arg->current_dataset=0;
 local_arg->current_point=0;
 local_arg->sdsid=local_arg->rank=FAIL;
 local_arg->df_datatype=DFNT_NONE;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_hdf(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_hdf(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;

 int ret;
 long offset;
 long trigger_point=0, file_start_point, file_end_point;
 int32 dimsize0=0, attrnum;
 int32 starts[MAXRANK_FOR_MYDATA], ends[MAXRANK_FOR_MYDATA];

 if (local_arg->epochs-- ==0) return NULL;

 if (local_arg->sdsid==FAIL) {
  /* A new dataset must be found; it carries its own triggers. */
  growing_buf_clear(&local_arg->triggers);
  read_hdf_reset_triggerbuffer(tinfo);
  local_arg->current_point=0;
 
  /*{{{  Select the next suitable dataset*/
  while (local_arg->sdsid==FAIL) {
   int32 dimid, dimnt, dimnattr;
   char dimname[MAX_NC_NAME];

   if (local_arg->current_dataset>=local_arg->ndatasets) {
    if (local_arg->rank==FAIL) {
     ERREXIT(tinfo->emethods, "read_hdf: Cannot find a suitable first epoch\n");
    } else {
     return NULL;
    }
   }

   local_arg->sdsid=SDselect(local_arg->fileid, local_arg->current_dataset);
   ret=SDgetinfo(local_arg->sdsid, local_arg->descbuf, &local_arg->rank, local_arg->dims, &local_arg->df_datatype, &local_arg->sd_nattr);
   //printf("Seeking dataset... %s\n", local_arg->descbuf);
   if (local_arg->sdsid == FAIL || ret==FAIL) {
    ERREXIT2(tinfo->emethods, "read_hdf: Error accessing data set %d of %d\n", MSGPARM(local_arg->current_dataset), MSGPARM(local_arg->ndatasets));
   }
   local_arg->current_dataset++;
   if (SDiscoordvar(local_arg->sdsid) || (local_arg->df_datatype!=DFNT_FLOAT32 && local_arg->df_datatype!=DFNT_FLOAT64) || local_arg->rank>MAXRANK_FOR_MYDATA) {
    local_arg->sdsid=FAIL;
    continue;
   }
   dimid=SDgetdimid(local_arg->sdsid, 0);
   SDdiminfo(dimid, dimname, &dimsize0, &dimnt, &dimnattr);
   if (dimsize0>0) {
    /* Epoched data.
     * Reading epochs from data sets with unlimited first dimension (dimsize0==0) 
     * is handled by accepting this SD and reading epochs within it below. */
    if (args[ARGS_CONTINUOUS].is_set) {
     /* Can't read epoch data in continuous mode */
     local_arg->sdsid=FAIL;
     continue;
    } else {
     if (local_arg->trigcodes!=NULL) {
      /* User requested to filter by condition and we have epoched data: Look at Condition attribute */
      if ((attrnum=SDfindattr(local_arg->sdsid, "Condition"))!=FAIL) {
       int trigno=0;
       Bool not_correct_trigger=TRUE;
       SDreadattr(local_arg->sdsid, attrnum, &tinfo->condition);
       while (local_arg->trigcodes[trigno]!=0) {
	if (local_arg->trigcodes[trigno]==tinfo->condition) {
	 not_correct_trigger=FALSE;
	 break;
	}
	trigno++;
       }
       if (not_correct_trigger) {
	local_arg->sdsid=FAIL;
	continue;
       }
      } else {
       local_arg->sdsid=FAIL;
       continue;
      }
     }
    }
    if (--local_arg->fromepoch>0) {
     local_arg->sdsid=FAIL;
    }
   }
  }
  /*}}}  */
  TRACEMS1(tinfo->emethods, 1, "read_hdf: Reading dataset >%s<\n", MSGPARM(local_arg->descbuf));
 }

 /*{{{  Examine the dataset and dimension attributes*/
 tinfo->sfreq=0.0;
 tinfo->multiplexed= -1;
 tinfo->beforetrig=0;
 tinfo->aftertrig=0;
 tinfo->leaveright=0;
 tinfo->condition=0;
 tinfo->xdata=NULL;
 tinfo->probepos=NULL;
 tinfo->channelnames=NULL;
 tinfo->nrofaverages=1;
  
 read_hdf_locate_dimensions(tinfo);
 read_hdf_setup_channelinfo(tinfo);
 if ((attrnum=SDfindattr(local_arg->sdsid, "Sfreq"))!=FAIL) {
  union {DATATYPE d; C_OTHERDATATYPE o;} readit;
  SDreadattr(local_arg->sdsid, attrnum, &readit);
  if (local_arg->df_datatype!=DF_DATATYPE) {
   tinfo->sfreq=readit.o;
  } else {
   tinfo->sfreq=readit.d;
  }
 } 
 if ((attrnum=SDfindattr(local_arg->sdsid, "Beforetrig"))!=FAIL) {
  SDreadattr(local_arg->sdsid, attrnum, &tinfo->beforetrig);
 } 
 if ((attrnum=SDfindattr(local_arg->sdsid, "Leaveright"))!=FAIL) {
  SDreadattr(local_arg->sdsid, attrnum, &tinfo->leaveright);
 }
 if ((attrnum=SDfindattr(local_arg->sdsid, "Condition"))!=FAIL) {
  SDreadattr(local_arg->sdsid, attrnum, &tinfo->condition);
 }
 if ((attrnum=SDfindattr(local_arg->sdsid, "Nr_of_averages"))!=FAIL) {
  SDreadattr(local_arg->sdsid, attrnum, &tinfo->nrofaverages);
 }
 /*}}}  */
 
 /*{{{  Check attributes and the array geometry*/
 if (tinfo->sfreq== 0.0) {
  tinfo->sfreq=100.0;
  TRACEMS(tinfo->emethods, 0, "read_hdf: sfreq attribute not specified, setting to 100.\n");
 }
 /* An unlimited first dimension must be the points dimension */
 if (dimsize0==0) tinfo->multiplexed=TRUE;
 if (tinfo->multiplexed== -1) {
  if (local_arg->rank==1) {
   local_arg->dims[1]=1;
   tinfo->multiplexed=TRUE;
  } else {
   if (local_arg->dims[0]>local_arg->dims[1]) {
    tinfo->multiplexed=TRUE;
   } else {
    tinfo->multiplexed=FALSE;
   }
  }
  TRACEMS1(tinfo->emethods, 0, "read_hdf: Assuming %s format.\n", MSGPARM(tinfo->multiplexed ? "MULTIPLEXED" : "NONMULTIPLEXED"));
 }
 if (tinfo->multiplexed) {
  local_arg->pointsdim=0;
  local_arg->channelsdim=1;
 } else {
  local_arg->channelsdim=0;
  local_arg->pointsdim=1;
 }
 tinfo->nr_of_channels=local_arg->dims[local_arg->channelsdim];
 tinfo->points_in_file=local_arg->dims[local_arg->pointsdim];
 /*}}}  */
 
 /*{{{  Allocate the comment*/
 if ((tinfo->comment=(char *)malloc(strlen(local_arg->descbuf)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_hdf: Error allocating comment\n");
 }
 strcpy(tinfo->comment, local_arg->descbuf);
 /*}}}  */

 offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 if (args[ARGS_BEFORETRIG].is_set) tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 if (args[ARGS_AFTERTRIG].is_set) tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 if (tinfo->aftertrig==0) {
  /* Automatically set the epoch length*/
  tinfo->aftertrig=local_arg->dims[local_arg->pointsdim]-tinfo->beforetrig;
 }
 tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_hdf: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }
 /*{{{  Find a suitable epoch*/
 while (TRUE) {
  /* Skip local_arg->fromepoch-1 epochs */
  file_start_point=local_arg->current_point;
  if (dimsize0==0) {
   if (args[ARGS_CONTINUOUS].is_set) {
    /* Simulate a trigger at current_point+beforetrig */
    trigger_point=file_start_point+tinfo->beforetrig;
    file_end_point=trigger_point+tinfo->aftertrig-1;
    if (file_end_point>=local_arg->dims[local_arg->pointsdim]) return NULL;
    local_arg->current_trigger++;
    local_arg->current_point+=tinfo->nr_of_points;
    tinfo->condition=0;
   } else {
    Bool not_correct_trigger=FALSE;
    char *description=NULL;
    do {
     tinfo->condition=read_hdf_read_trigger(tinfo, &trigger_point, &description);
     if (tinfo->condition==0) return NULL;	/* No more triggers in file */
     file_start_point=trigger_point-tinfo->beforetrig+offset;
     file_end_point=trigger_point+tinfo->aftertrig-1-offset;
     
     if (local_arg->trigcodes==NULL) {
      not_correct_trigger=FALSE;
     } else {
      int trigno=0;
      not_correct_trigger=TRUE;
      while (local_arg->trigcodes[trigno]!=0) {
       if (local_arg->trigcodes[trigno]==tinfo->condition) {
	not_correct_trigger=FALSE;
	break;
       }
       trigno++;
      }
     }
    } while (not_correct_trigger || file_start_point<0 || file_end_point>=local_arg->dims[local_arg->pointsdim]);
   }
  }
  if (local_arg->fromepoch<=1) break;
  local_arg->fromepoch--;
 }
 TRACEMS3(tinfo->emethods, 1, "read_hdf: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 /*}}}  */
 //printf("nr_of_points=%ld, beforetrig=%ld, aftertrig=%ld, file_start_point=%ld\n", tinfo->nr_of_points, tinfo->beforetrig, tinfo->aftertrig, file_start_point);
 file_end_point=file_start_point+tinfo->nr_of_points-1;

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  read_hdf_reset_triggerbuffer(tinfo);
  /* First trigger entry holds file_start_point */
  if (dimsize0==0) {
   push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  } else {
   code=read_hdf_read_trigger(tinfo, &trigpoint, &thisdescription);
   if (code== -1) {
    /* File start already coded in epoch read - transfer it */
    push_trigger(&tinfo->triggers, trigpoint, code, thisdescription);
   } else {
    read_hdf_reset_triggerbuffer(tinfo); /* Back to the first trigger */
    push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
   }
  }
  for (trigs_in_epoch=1; code=read_hdf_read_trigger(tinfo, &trigpoint, &thisdescription), 
	 (code!=0 && trigpoint<=file_end_point); ) {
   if (trigpoint>=file_start_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=(local_arg->rank==3 ? local_arg->dims[2] : 1);
 if (tinfo->multiplexed) {
  myarray.nr_of_vectors=tinfo->nr_of_points;
  myarray.nr_of_elements=tinfo->nr_of_channels;
 } else {
  myarray.nr_of_vectors=tinfo->nr_of_channels;
  myarray.nr_of_elements=tinfo->nr_of_points;
 }
 if (dimsize0==0) {
  if (file_start_point+tinfo->nr_of_points>local_arg->dims[0]) {
   free_pointer((void **)&tinfo->comment);
   if (tinfo->channelnames!=NULL) {
    free_pointer((void **)&tinfo->channelnames[0]);
    free_pointer((void **)&tinfo->channelnames);
   }
   free_pointer((void **)&tinfo->probepos);
   return NULL;
  }
  starts[local_arg->pointsdim]=file_start_point;
 } else {
  starts[local_arg->pointsdim]=0;
 }
 starts[local_arg->channelsdim]=0;
 starts[2]=0;
 ends[local_arg->pointsdim]=tinfo->nr_of_points;
 ends[local_arg->channelsdim]=tinfo->nr_of_channels;
 ends[2]=tinfo->itemsize;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tinfo->itemsize;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_hdf: Error allocating data\n");
 }
 /*}}}  */
 
 /*{{{  Read the data*/
 {
 VOIDP usestart=myarray.start;
 if (local_arg->df_datatype!=DF_DATATYPE) usestart=calloc(tinfo->length_of_output_region,sizeof(C_OTHERDATATYPE));
 ret=SDreaddata(local_arg->sdsid, starts, NULL, ends, usestart);
 if (ret == FAIL) {
  array_free(&myarray);
  if (local_arg->df_datatype!=DF_DATATYPE) free(usestart);
  ERREXIT(tinfo->emethods, "read_hdf: SDreaddata failed\n");
 }
 if (local_arg->df_datatype!=DF_DATATYPE) {
  C_OTHERDATATYPE *from=(C_OTHERDATATYPE *)usestart;
  DATATYPE *to=myarray.start;
  long i;
  for (i=0; i<tinfo->length_of_output_region; i++) {
   *to++ = *from++;
  }
  free(usestart);
 }
 }
 /*}}}  */

 /* This subroutine prepares any channel names/positions that might not be set */
 create_channelgrid(tinfo);

 /*{{{  Setup tinfo values*/
 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->data_type=TIME_DATA;
 /*}}}  */

 if (dimsize0>0) {
  /* Epoch mode: next epoch is next dataset */
  local_arg->sdsid=FAIL;
 }

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_hdf_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_hdf_exit(transform_info_ptr tinfo) {
 struct read_hdf_storage *local_arg=(struct read_hdf_storage *)tinfo->methods->local_storage;

 SDend(local_arg->fileid);
 growing_buf_free(&local_arg->triggers);
 free_pointer((void **)&local_arg->trigcodes);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_hdf(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_hdf(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_hdf_init;
 tinfo->methods->transform= &read_hdf;
 tinfo->methods->transform_exit= &read_hdf_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_hdf";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from an HDF file.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_hdf_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
