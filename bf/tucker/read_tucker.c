/*
 * Copyright (C) 1996-1999,2001-2004,2006-2008,2010-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_tucker.c module to read data from the Tucker format that is
 * used with the `Geodesic Net' 128-electrode system.
 *	-- Bernd Feige 16.12.1996
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
#include <ctype.h>
#include <read_struct.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
#include "tucker.h"
/*}}}  */

#define MAX_COMMENTLEN 1024

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
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.raw"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
};

/*{{{  Definition of read_tucker_storage*/
struct read_tucker_storage {
 struct tucker_header header;
 FILE *infile;
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 long current_point;
 char (*EventCodes)[5]; /* Pointer to fixed-length strings */
 long SizeofHeader;
 long points_in_file;
 int  bytes_per_sample;
 long bytes_per_point;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 void *buffer;

 DATATYPE Factor;
};
/*}}}  */

/*{{{  Single point interface*/

/*{{{  read_tucker_get_filestrings: Allocate and set strings and probepos array*/
LOCAL void
read_tucker_get_filestrings(transform_info_ptr tinfo) {
 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, free_tinfo will free someone else's data ! */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */
}
/*}}}  */

/*{{{  read_tucker_seek_point(transform_info_ptr tinfo, long point) {*/
/* This function not only seeks to the specified point, but also reads
 * the point into the buffer if necessary. The get_singlepoint function
 * then only transfers the data. */
LOCAL void
read_tucker_seek_point(transform_info_ptr tinfo, long point) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;

 fseek(local_arg->infile,point*local_arg->bytes_per_point+local_arg->SizeofHeader,SEEK_SET);
 if ((int)fread(local_arg->buffer,1,local_arg->bytes_per_point,local_arg->infile)!=local_arg->bytes_per_point) {
  ERREXIT(tinfo->emethods, "read_tucker_seek_point: Error reading data\n");
 }
 /*{{{  Swap byte order if necessary*/
# ifdef LITTLE_ENDIAN
 switch (local_arg->header.Version) {
  case 2:
   /* Data are signed shorts */
   {uint16_t *pdata=(uint16_t *)local_arg->buffer, *p_end=(uint16_t *)((char *)local_arg->buffer+local_arg->bytes_per_point);
    while (pdata<p_end) Intel_int16(pdata++);
   }
   break;
  case 4:
   /* Data are floats */
   {float *pdata=(float *)local_arg->buffer, *p_end=(float *)((char *)local_arg->buffer+local_arg->bytes_per_point);
    while (pdata<p_end) Intel_float(pdata++);
   }
   break;
  case 6:
   /* Data are doubles */
   {double *pdata=(double *)local_arg->buffer, *p_end=(double *)((char *)local_arg->buffer+local_arg->bytes_per_point);
    while (pdata<p_end) Intel_double(pdata++);
   }
   break;
  default:
   break;
 }
# endif
 /*}}}  */
 local_arg->current_point=point;
}
/*}}}  */

/*{{{  read_tucker_get_singlepoint(transform_info_ptr tinfo, array *toarray) {*/
LOCAL int
read_tucker_get_singlepoint(transform_info_ptr tinfo, array *toarray) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;

 if (local_arg->current_point>=local_arg->points_in_file) return -1;
 read_tucker_seek_point(tinfo, local_arg->current_point);

 switch (local_arg->header.Version) {
  case 2:
   /* Data are signed shorts */
   {uint16_t *pdata=(uint16_t *)local_arg->buffer;
    do {
     array_write(toarray, local_arg->Factor*(*pdata++));
    } while (toarray->message==ARRAY_CONTINUE);
   }
   break;
  case 4:
   /* Data are floats */
   {float *pdata=(float *)local_arg->buffer;
    do {
     array_write(toarray, *pdata++);
    } while (toarray->message==ARRAY_CONTINUE);
   }
   break;
  case 6:
   /* Data are doubles */
   {double *pdata=(double *)local_arg->buffer;
    do {
     array_write(toarray, *pdata++);
    } while (toarray->message==ARRAY_CONTINUE);
   }
   break;
  default:
   break;
 }

 local_arg->current_point++;
 return 0;
}
/*}}}  */

/*{{{  Maintaining the buffered triggers list*/
LOCAL void 
read_tucker_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */

/*{{{  read_tucker_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events */
LOCAL void 
read_tucker_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_tucker_storage * const local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_tucker_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_tucker_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  int eventno;
  long current_triggerpoint=0;
  int last_trigvalues[local_arg->header.NEvents];

  TRACEMS(tinfo->emethods, 1, "read_tucker_build_trigbuffer: Scanning trigger traces\n");
  while (current_triggerpoint<local_arg->points_in_file) {
   read_tucker_seek_point(tinfo, current_triggerpoint);
   switch (local_arg->header.Version) {
    case 2:
     /* Data are signed shorts */
     {uint16_t *pdata=((uint16_t *)local_arg->buffer)+local_arg->header.NChan;
      for (eventno=0; eventno<local_arg->header.NEvents; eventno++) {
       /* Trigger on the first non-zero point */
       if (last_trigvalues[eventno]==0 && pdata[eventno]!=0) {
	push_trigger(&local_arg->triggers, current_triggerpoint, eventno+1, strdup(local_arg->EventCodes[eventno]));
       }
       last_trigvalues[eventno]=pdata[eventno];
      }
     }
     break;
    case 4:
     /* Data are floats */
     {float *pdata=((float *)local_arg->buffer)+local_arg->header.NChan;
      for (eventno=0; eventno<local_arg->header.NEvents; eventno++) {
       /* Trigger on the first non-zero point */
       if (last_trigvalues[eventno]==0 && pdata[eventno]!=0) {
	push_trigger(&local_arg->triggers, current_triggerpoint, eventno+1, strdup(local_arg->EventCodes[eventno]));
       }
       last_trigvalues[eventno]=pdata[eventno];
      }
     }
     break;
    case 6:
     /* Data are doubles */
     {double *pdata=((double *)local_arg->buffer)+local_arg->header.NChan;
      for (eventno=0; eventno<local_arg->header.NEvents; eventno++) {
       /* Trigger on the first non-zero point */
       if (last_trigvalues[eventno]==0 && pdata[eventno]!=0) {
	push_trigger(&local_arg->triggers, current_triggerpoint, eventno+1, strdup(local_arg->EventCodes[eventno]));
       }
       last_trigvalues[eventno]=pdata[eventno];
      }
     }
     break;
    default:
     break;
   }
   current_triggerpoint++;
  }
 }
}
/*}}}  */

/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_tucker_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_tucker_build_trigbuffer(tinfo);
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

/*{{{  read_tucker_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_tucker_init(transform_info_ptr tinfo) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 struct stat statbuff;

 growing_buf_init(&local_arg->triggers);

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s,"rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_tucker_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 if (read_struct((char *)&local_arg->header, sm_tucker, local_arg->infile)==0) {
  ERREXIT1(tinfo->emethods, "read_tucker_init: Can't read header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifdef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->header, sm_tucker);
#endif
 if (local_arg->header.NEvents>0) {
  int eventno;
  if ((local_arg->EventCodes=(char (*)[sizeof(local_arg->EventCodes[0])])malloc(local_arg->header.NEvents*sizeof(local_arg->EventCodes[0])))==NULL) {
   ERREXIT(tinfo->emethods, "read_tucker_init: Error allocating event code table\n");
  }
  for (eventno=0; eventno<local_arg->header.NEvents; eventno++) {
   if ((int)fread((void *)&local_arg->EventCodes[eventno], 4, 1, local_arg->infile)!=1) {
    ERREXIT(tinfo->emethods, "read_tucker_init: Error reading event code table\n");
   }
   local_arg->EventCodes[eventno][4]=(char)0;
  }
 } else {
  local_arg->EventCodes=NULL;
  if (!args[ARGS_TRIGFILE].is_set && !args[ARGS_CONTINUOUS].is_set) {
   ERREXIT(tinfo->emethods, "read_tucker_init: No event trace present!\n");
  }
 }
 local_arg->SizeofHeader = ftell(local_arg->infile);

 switch (local_arg->header.Version) {
  case 2:
   /* Data are signed shorts */
   local_arg->bytes_per_sample=2;
   local_arg->Factor=((DATATYPE)local_arg->header.Range)/(1<<local_arg->header.Bits);
   break;
  case 4:
   /* Data are floats */
   local_arg->bytes_per_sample=4;
   break;
  case 6:
   /* Data are doubles */
   local_arg->bytes_per_sample=8;
   break;
  default:
   ERREXIT1(tinfo->emethods, "read_tucker_init: Unknown Version %d\n", MSGPARM(local_arg->header.Version));
   break;
 }
 local_arg->bytes_per_point=(local_arg->header.NChan+local_arg->header.NEvents)*local_arg->bytes_per_sample;
 fstat(fileno(local_arg->infile),&statbuff);
 local_arg->points_in_file = (statbuff.st_size-local_arg->SizeofHeader)/local_arg->bytes_per_point;
 tinfo->points_in_file=local_arg->points_in_file;
 if ((local_arg->buffer=malloc(local_arg->bytes_per_point))==NULL) {
  ERREXIT(tinfo->emethods, "read_tucker_init: Error allocating buffer memory\n");
 }
 
 tinfo->sfreq=local_arg->header.SampRate;
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 if (local_arg->beforetrig==0 && local_arg->aftertrig==0) {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Read the whole file as one epoch: */
   local_arg->aftertrig=local_arg->points_in_file;
   TRACEMS1(tinfo->emethods, 1, "read_tucker_init: Reading ALL data (%d points)\n", MSGPARM(local_arg->aftertrig));
  } else {
   ERREXIT(tinfo->emethods, "read_tucker_init: Zero epoch length.\n");
  }
 }
 if (args[ARGS_TRIGLIST].is_set) {
  local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
  if (local_arg->trigcodes==NULL) {
   ERREXIT(tinfo->emethods, "read_tucker_init: Error allocating triglist memory\n");
  }
 } else {
  local_arg->trigcodes=NULL;
 }

 read_tucker_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_tucker(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_tucker(transform_info_ptr tinfo) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->header.NChan;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_tucker: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Find the next window that fits into the actual data*/
 /* This is just for the Continuous option (no trigger file): */
 file_end_point=local_arg->current_point-1;
 do {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Simulate a trigger at current_point+beforetrig */
   file_start_point=file_end_point+1;
   trigger_point=file_start_point+tinfo->beforetrig;
   file_end_point=trigger_point+tinfo->aftertrig-1;
   if (file_end_point>=local_arg->points_in_file) return NULL;
   local_arg->current_trigger++;
   local_arg->current_point+=tinfo->nr_of_points;
   tinfo->condition=0;
  } else 
  do {
   tinfo->condition=read_tucker_read_trigger(tinfo, &trigger_point, &description);
   if (tinfo->condition==0) return NULL;	/* No more triggers in file */
   file_start_point=trigger_point-tinfo->beforetrig+local_arg->offset;
   file_end_point=trigger_point+tinfo->aftertrig-1-local_arg->offset;
   
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
  } while (not_correct_trigger || file_start_point<0 || file_end_point>=local_arg->points_in_file);
 } while (--local_arg->fromepoch>0);
 TRACEMS3(tinfo->emethods, 1, "read_tucker: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_tucker_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_tucker_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_elements=tinfo->nr_of_channels;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_tucker: Error allocating data\n");
 }
 /* Byte order is multiplexed, ie channels fastest */
 tinfo->multiplexed=TRUE;
 /*}}}  */

 read_tucker_get_filestrings(tinfo);
 /* Construct comment */
 {
  char const *last_sep=strrchr(args[ARGS_IFILE].arg.s, PATHSEP);
  char const *last_component=(last_sep==NULL ? args[ARGS_IFILE].arg.s : last_sep+1);
  int const commentlen=strlen(last_component)+40+(description==NULL ? 0 : strlen(description)+1);

  if ((tinfo->comment=(char *)malloc(commentlen))==NULL) {
   ERREXIT(tinfo->emethods, "read_tucker: Error allocating comment\n");
  }
  snprintf(tinfo->comment, commentlen, "read_tucker %s %02d/%02d/%04d,%02d:%02d:%02d", last_component, local_arg->header.Month, local_arg->header.Day, local_arg->header.Year, local_arg->header.Hour, local_arg->header.Minute, local_arg->header.Sec);
  if (description!=0) {
   strcat(tinfo->comment, " ");
   strcat(tinfo->comment, description);
  }
 }
 local_arg->current_point=file_start_point;
 do {
  read_tucker_get_singlepoint(tinfo, &myarray);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 tinfo->z_label=NULL;
 tinfo->sfreq=local_arg->header.SampRate;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_tucker_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_tucker_exit(transform_info_ptr tinfo) {
 struct read_tucker_storage *local_arg=(struct read_tucker_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->trigcodes);
 free_pointer((void **)&local_arg->EventCodes);
 growing_buf_free(&local_arg->triggers);
 fclose(local_arg->infile);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_tucker(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_tucker(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_tucker_init;
 tinfo->methods->transform= &read_tucker;
 tinfo->methods->transform_exit= &read_tucker_exit;
 tinfo->methods->get_singlepoint= &read_tucker_get_singlepoint;
 tinfo->methods->seek_point= &read_tucker_seek_point;
 tinfo->methods->get_filestrings= &read_tucker_get_filestrings;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_tucker";
 tinfo->methods->method_description=
  "Get-epoch method to read continuous files in the `simple-binary' export format\n"
  "used with EGI Net Station software.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_tucker_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
