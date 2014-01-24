/*
 * Copyright (C) 2007-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_brainvision.c module to read data from BrainVision vhdr+vmrk+eeg files
 *	-- Bernd Feige 11.06.2007
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <setjmp.h>
#include <string.h>
#include <locale.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
/*}}}  */

#define MAX_COMMENTLEN 1024

/*{{{  Local defs and args*/
LOCAL struct triggercode {
 char *triggerstring;
 int triggercode;
} triggercodes[]={
 {"New Segment,",256}, /* similar to NAV_STARTSTOP for synamps */
 {"DC Correction,",257}, /* similar to NAV_DCRESET for synamps */
 {"SyncStatus,Sync On",0}, /* Dropped. */
 {"SyncStatus,Sync Off",0}, /* Dropped. */
 {NULL,0}
};
/* Presently, I've seen only files with INT_16 and IEEE_FLOAT_32 */
LOCAL char *datatype_choice[]={
 "INT_8",
 "INT_16",
 "INT_32",
 "IEEE_FLOAT_32",
 "IEEE_FLOAT_64",
 NULL
};
LOCAL int datatype_size[]={
 sizeof(int8_t),
 sizeof(int16_t),
 sizeof(int32_t),
 sizeof(float),
 sizeof(double),
};
enum DATATYPE_ENUM {
 DT_INT8=0,
 DT_INT16,
 DT_INT32,
 DT_FLOAT32,
 DT_FLOAT64,
};
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
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (char const *const *)"*.trg"},
 {T_ARGS_TAKES_FILENAME, "Input file (.vhdr)", "", ARGDESC_UNUSED, (char const *const *)"*.vhdr"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (char const *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (char const *const *)"1s"}
};
/*}}}  */

/*{{{  Definition of read_brainvision_storage*/
struct read_brainvision_storage {
 FILE *infile;	/* Input file */
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 int nr_of_channels;
 int itemsize;
 long points_in_file;
 long bytes_per_point;
 long current_point;

 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 float sfreq;
 enum DATATYPE_ENUM datatype;
 Bool multiplexed;
 char *markerfilename;

 growing_buf filepath_buf;
 growing_buf channelnames_buf;
 growing_buf resolutions_buf;
 growing_buf coordinates_buf;

 Bool first_in_epoch;
 jmp_buf error_jmp;
};
/*}}}  */

/*{{{  Single point interface - Not very useful with this format */

/*{{{  read_brainvision_get_filestrings: Allocate and set strings and probepos array*/
LOCAL void
read_brainvision_get_filestrings(transform_info_ptr tinfo) {
 struct read_brainvision_storage *local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;
 char *innamebuf;
 int channel, ntokens;

 tinfo->xdata=NULL;
 if ((tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
     (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL ||
     (innamebuf=(char *)malloc(local_arg->channelnames_buf.current_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_brainvision: Error allocating channelnames\n");
 }
 memcpy(innamebuf, local_arg->channelnames_buf.buffer_start, local_arg->channelnames_buf.current_length);
 ntokens=growing_buf_count_tokens(&local_arg->channelnames_buf);
 //printf("Have found %d channel names!\n", ntokens);
 if (ntokens!=tinfo->nr_of_channels) {
  ERREXIT2(tinfo->emethods, "read_brainvision: Channel count mismatch, %d!=%d\n", MSGPARM(ntokens), MSGPARM(tinfo->nr_of_channels));
 }
 /* Reset token pointer, just as growing_buf_get_firsttoken does */
 local_arg->channelnames_buf.current_token=local_arg->channelnames_buf.buffer_start;
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  tinfo->channelnames[channel]=innamebuf+(local_arg->channelnames_buf.current_token-local_arg->channelnames_buf.buffer_start);
  growing_buf_get_nexttoken(&local_arg->channelnames_buf,NULL);
 }
 if (local_arg->coordinates_buf.buffer_start!=NULL) {
  if (local_arg->coordinates_buf.current_length!=3*tinfo->nr_of_channels*sizeof(double)) {
   ERREXIT2(tinfo->emethods, "read_brainvision: Coordinates count mismatch, %d!=%d\n", MSGPARM(local_arg->coordinates_buf.current_length/sizeof(double)), MSGPARM(3*tinfo->nr_of_channels));
  }
  if ((tinfo->probepos=(double *)malloc(3*tinfo->nr_of_channels*sizeof(double)))==NULL) {
   ERREXIT(tinfo->emethods, "read_brainvision: Error allocating probe positions\n");
  }
  memcpy(tinfo->probepos, local_arg->coordinates_buf.buffer_start, local_arg->coordinates_buf.current_length);
 } else {
  tinfo->probepos=NULL;
  create_channelgrid(tinfo);
 }
}
/*}}}  */

/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_brainvision_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_brainvision_storage *local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_brainvision_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_brainvision_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_brainvision_storage *local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_brainvision_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_brainvision_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else if (local_arg->markerfilename!=NULL) {
  FILE * const markerfile=fopen(local_arg->markerfilename, "r");
  growing_buf readbuf;
  if (markerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_brainvision_build_trigbuffer: Can't open marker file %s\n", MSGPARM(local_arg->markerfilename));
  }
  growing_buf_init(&readbuf);
  growing_buf_allocate(&readbuf, 0);
  while (1) {
   growing_buf_read_line(markerfile, &readbuf);
reevaluate:
   if (feof(markerfile)) break;
   if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
    /* Empty line or comment */
   } else if (strcmp(readbuf.buffer_start,"Brain Vision Data Exchange Marker File, Version 1.0")==0) {
    /* Header line */
   } else if (strcmp(readbuf.buffer_start,"[Common Infos]")==0) {
    while (1) {
     growing_buf_read_line(markerfile, &readbuf);
     if (*readbuf.buffer_start=='[' || feof(markerfile)) goto reevaluate;
    }
   } else if (strcmp(readbuf.buffer_start,"[Marker Infos]")==0) {
    while (1) {
     growing_buf_read_line(markerfile, &readbuf);
     if (*readbuf.buffer_start=='[' || feof(markerfile)) goto reevaluate;
     if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
      /* Empty line or comment */
     } else if (strncmp(readbuf.buffer_start,"Mk",2)==0) {
      char * const equalsign=strchr(readbuf.buffer_start+2, '=');
      if (equalsign!=NULL) {
       char * const firstcomma=strchr(equalsign+1, ',');
       if (firstcomma!=NULL) {
	char * const secondcomma=strchr(firstcomma+1, ',');
	if (secondcomma!=NULL) {
	 /* Positions in BrainVision apparently start at 1 */
	 long const trigpoint=atol(secondcomma+1)-1;
	 int code= -1;
	 char *description;
	 struct triggercode *in_triggercodes;
	 if ((description=(char *)malloc(secondcomma-equalsign))==NULL) {
	  ERREXIT(tinfo->emethods, "read_brainvision_build_trigbuffer: Error allocating description\n");
	 }
	 strncpy(description,equalsign+1,secondcomma-equalsign-1);
	 description[secondcomma-equalsign-1]='\0';
	 for (in_triggercodes=triggercodes; in_triggercodes->triggerstring!=NULL; in_triggercodes++) {
	  if (strncmp(in_triggercodes->triggerstring, equalsign+1, secondcomma-equalsign-1)==0) {
	   code=in_triggercodes->triggercode;
	   break;
	  }
	 }
	 if (code==0) {
	  /* Allow 0 codes in triggercodes to cause dropping of these triggers */
	  free(description);
	 } else {
	  if (code==-1) {
	   if (strncmp("Stimulus,S",description,10)==0) {
	    code=atoi(description+10);
	   } else if (strncmp("Response,R",description,10)==0) {
	    code= -atoi(description+10);
	   } else if (strncmp("Comment,",description,8)==0) {
	    code= -256;
	   }
	  }
	  push_trigger(&local_arg->triggers, trigpoint, code, description);
	 }
	}
       }
      }
     }
    }
   }
  }
  growing_buf_free(&readbuf);
  fclose(markerfile);
 } else {
  TRACEMS(tinfo->emethods, 0, "read_brainvision_build_trigbuffer: No trigger source known.\n");
 }
}
/*}}}  */
/*{{{  read_brainvision_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_brainvision_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_brainvision_storage *local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_brainvision_build_trigbuffer(tinfo);
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

/*{{{  read_brainvision_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_brainvision_init(transform_info_ptr tinfo) {
 struct read_brainvision_storage * const local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 struct stat statbuff;
 FILE * const vhdr=fopen(args[ARGS_IFILE].arg.s,"r");
 char const *last_sep=strrchr(args[ARGS_IFILE].arg.s, PATHSEP);
 growing_buf readbuf;

 if(vhdr==NULL) {
  ERREXIT1(tinfo->emethods, "read_brainvision_init: Can't open vhdr file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }

 /* Record the path component so that we can open the other files in the same directory */
 growing_buf_init(&local_arg->filepath_buf);
 growing_buf_allocate(&local_arg->filepath_buf, 0);
 if (last_sep!=NULL) {
  growing_buf_append(&local_arg->filepath_buf, args[ARGS_IFILE].arg.s, last_sep-args[ARGS_IFILE].arg.s+1);
 }
 growing_buf_appendchar(&local_arg->filepath_buf, '\0');

 local_arg->infile=NULL;
 local_arg->datatype= -1;
 local_arg->multiplexed= TRUE;
 local_arg->markerfilename=NULL;
 local_arg->itemsize=1;
 growing_buf_init(&local_arg->triggers);
 growing_buf_init(&local_arg->channelnames_buf);
 local_arg->channelnames_buf.delimiters="";
 growing_buf_allocate(&local_arg->channelnames_buf, 0);
 growing_buf_init(&local_arg->resolutions_buf);
 growing_buf_allocate(&local_arg->resolutions_buf, 0);
 growing_buf_init(&local_arg->coordinates_buf);

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 growing_buf_init(&readbuf);
 growing_buf_allocate(&readbuf, 0);
 /* Read fractional sensitivities correctly: */
 setlocale(LC_NUMERIC, "C");
 while (1) {
  growing_buf_read_line(vhdr, &readbuf);
reevaluate:
  if (feof(vhdr)) break;
  //printf(">%s<\n", readbuf.buffer_start);
  if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
   /* Empty line or comment */
  } else if (strcmp(readbuf.buffer_start,"Brain Vision Data Exchange Header File Version 1.0")==0) {
   /* Header line */
  } else if (strcmp(readbuf.buffer_start,"[Common Infos]")==0) {
   while (1) {
    growing_buf_read_line(vhdr, &readbuf);
    //printf("%s\n", readbuf.buffer_start);
    if (*readbuf.buffer_start=='[' || feof(vhdr)) goto reevaluate;
    if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
     /* Empty line or comment */
    } else if (strncmp(readbuf.buffer_start,"DataFile=",9)==0) {
     int const savelength=local_arg->filepath_buf.current_length;
     growing_buf_appendstring(&local_arg->filepath_buf, readbuf.buffer_start+9);
     local_arg->infile=fopen(local_arg->filepath_buf.buffer_start, "rb");
     if(local_arg->infile==NULL) {
      ERREXIT1(tinfo->emethods, "read_brainvision_init: Can't open eeg file %s\n", MSGPARM(local_arg->filepath_buf.buffer_start));
     }
     local_arg->filepath_buf.current_length=savelength;
     //printf("Opened %s\n", filename);
    } else if (strncmp(readbuf.buffer_start,"MarkerFile=",11)==0) {
     int const savelength=local_arg->filepath_buf.current_length;
     growing_buf_appendstring(&local_arg->filepath_buf, readbuf.buffer_start+11);
     if ((local_arg->markerfilename=(char *)malloc(local_arg->filepath_buf.current_length))==NULL) {
      ERREXIT(tinfo->emethods, "read_brainvision_init: Error allocating marker name\n");
     }
     strcpy(local_arg->markerfilename, local_arg->filepath_buf.buffer_start);
     local_arg->filepath_buf.current_length=savelength;
    } else if (strncmp(readbuf.buffer_start,"DataOrientation=",16)==0) {
     /* Can be MULTIPLEXED or VECTORIZED */
     local_arg->multiplexed=(strcmp(readbuf.buffer_start+16, "MULTIPLEXED")==0);
     //printf("local_arg->multiplexed=%d, >%s<\n", local_arg->multiplexed, readbuf.buffer_start+16);
    } else if (strncmp(readbuf.buffer_start,"NumberOfChannels=",17)==0) {
     local_arg->nr_of_channels=atoi(readbuf.buffer_start+17);
     //printf("nr_of_channels=%d\n", local_arg->nr_of_channels);
    } else if (strncmp(readbuf.buffer_start,"SamplingInterval=",17)==0) {
     local_arg->sfreq=1.0e6/atoi(readbuf.buffer_start+17);
     //printf("sfreq=%g\n", local_arg->sfreq);
    }
   }
  } else if (strcmp(readbuf.buffer_start,"[Binary Infos]")==0) {
   //printf("Binary Infos\n");
   while (1) {
    growing_buf_read_line(vhdr, &readbuf);
    if (*readbuf.buffer_start=='[' || feof(vhdr)) goto reevaluate;
    if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
     /* Empty line or comment */
    } else if (strncmp(readbuf.buffer_start,"BinaryFormat=",13)==0) {
     char * const format=readbuf.buffer_start+13;
     char ** in_datatype;
     for (in_datatype=datatype_choice; *in_datatype!=NULL; in_datatype++) {
      if (strcmp(format, *in_datatype)==0) {
       local_arg->datatype=(enum DATATYPE_ENUM)(in_datatype-datatype_choice);
       //printf("Datatype %d, %d bytes\n", local_arg->datatype, datatype_size[local_arg->datatype]);
      }
     }
     if (local_arg->datatype== -1) {
      ERREXIT1(tinfo->emethods, "read_brainvision_init: Unknown data format %s\n", MSGPARM(format));
     }
    }
   }
  } else if (strcmp(readbuf.buffer_start,"[Channel Infos]")==0) {
   //printf("Channel Infos\n");
   while (1) {
    growing_buf_read_line(vhdr, &readbuf);
    if (*readbuf.buffer_start=='[' || feof(vhdr)) goto reevaluate;
    if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
     /* Empty line or comment */
    } else if (strncmp(readbuf.buffer_start,"Ch",2)==0) {
     char * const equalsign=strchr(readbuf.buffer_start+2, '=');
     if (equalsign!=NULL) {
      char * const firstcomma=strchr(equalsign+1, ',');
      if (firstcomma!=NULL) {
       char * const secondcomma=strchr(firstcomma+1, ',');
       growing_buf_append(&local_arg->channelnames_buf, equalsign+1, firstcomma-equalsign-1);
       growing_buf_appendchar(&local_arg->channelnames_buf, '\0');
       if (secondcomma!=NULL) {
	double resolution=strtod(secondcomma+1,NULL);
	if (resolution==0.0) resolution=1.0;
	growing_buf_append(&local_arg->resolutions_buf, (char *)&resolution, sizeof(double));
       }
      }
     }
    }
   }
  } else if (strcmp(readbuf.buffer_start,"[Coordinates]")==0) {
   //printf("Coordinates\n");
   growing_buf_allocate(&local_arg->coordinates_buf, 0);
   while (1) {
    growing_buf_read_line(vhdr, &readbuf);
    if (*readbuf.buffer_start=='[' || feof(vhdr)) goto reevaluate;
    if (readbuf.current_length==1 || strncmp(readbuf.buffer_start,";",1)==0) {
     /* Empty line or comment */
    } else if (strncmp(readbuf.buffer_start,"Ch",2)==0) {
     char * const equalsign=strchr(readbuf.buffer_start+2, '=');
     if (equalsign!=NULL) {
      char * const firstcomma=strchr(equalsign+1, ',');
      if (firstcomma!=NULL) {
       char * const secondcomma=strchr(firstcomma+1, ',');
       if (secondcomma!=NULL) {
	double const r=strtod(equalsign+1,NULL);
	double const theta=strtod(firstcomma+1,NULL)/180.0*M_PI;
	double const phi=strtod(secondcomma+1,NULL)/180.0*M_PI;
	double const x=r*sin(theta)*cos(phi);
	double const y=r*sin(theta)*sin(phi);
	double const z=r*cos(theta);
	growing_buf_append(&local_arg->coordinates_buf, (char *)&x, sizeof(double));
	growing_buf_append(&local_arg->coordinates_buf, (char *)&y, sizeof(double));
	growing_buf_append(&local_arg->coordinates_buf, (char *)&z, sizeof(double));
       }
      }
     }
    }
   }
  } else if (strcmp(readbuf.buffer_start,"[Comment]")==0) {
   while (1) {
    growing_buf_read_line(vhdr, &readbuf);
    if (*readbuf.buffer_start=='[' || feof(vhdr)) goto reevaluate;
   }
  }
 }
 setlocale(LC_NUMERIC, "");
 fclose(vhdr);
 growing_buf_free(&readbuf);

 /*{{{  Parse arguments that can be in seconds*/
 tinfo->sfreq=local_arg->sfreq;
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 fstat(fileno(local_arg->infile),&statbuff);
 if (datatype_size[local_arg->datatype]==0) {
  /* We don't know bytes_per_point and how many samples may fit in the input file... */
  local_arg->bytes_per_point = 0;
  local_arg->points_in_file = 0;
 } else {
  local_arg->bytes_per_point=(local_arg->nr_of_channels*local_arg->itemsize)*datatype_size[local_arg->datatype];
  if (statbuff.st_size==0) {
   local_arg->points_in_file = 0;
  } else {
   local_arg->points_in_file = statbuff.st_size/local_arg->bytes_per_point;
  }
 }
 tinfo->points_in_file=local_arg->points_in_file;
 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_brainvision_init: Error allocating triglist memory\n");
   }
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   if (local_arg->points_in_file==0) {
    ERREXIT(tinfo->emethods, "read_brainvision: Unable to determine the number of samples in the input file!\n");
   }
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }

 read_brainvision_reset_triggerbuffer(tinfo);
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_value: Central function for reading a number*/
enum ERROR_ENUM {
 ERR_NONE=0,
 ERR_READ,
 ERR_NUMBER
};
LOCAL DATATYPE
read_value(FILE *infile, 
 struct read_brainvision_storage *local_arg) {
 DATATYPE dat=0;
 enum ERROR_ENUM err=ERR_NONE;
 switch (local_arg->datatype) {
  case DT_INT8: {
   signed char c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
   dat=c;
   }
   break;
  case DT_INT16: {
   int16_t c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
#ifndef LITTLE_ENDIAN
   Intel_int16(&c);
#endif
   dat=c;
   }
   break;
  case DT_INT32: {
   int32_t c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
#ifndef LITTLE_ENDIAN
   Intel_int32(&c);
#endif
   dat=c;
   }
   break;
  case DT_FLOAT32: {
   float c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
#ifndef LITTLE_ENDIAN
   Intel_float(&c);
#endif
   dat=c;
   }
   break;
  case DT_FLOAT64: {
   double c;
   if (fread(&c, sizeof(c), 1, infile)!=1) err=ERR_READ;
#ifndef LITTLE_ENDIAN
   Intel_double(&c);
#endif
   dat=c;
   }
   break;
 }
 if (err!=ERR_NONE) {
  longjmp(local_arg->error_jmp, err);
 }
 return dat;
}
/*}}}  */

/*{{{  read_brainvision(transform_info_ptr tinfo) {*/
/*
 * The method read_brainvision() returns a pointer to the allocated tsdata memory
 * location or NULL if End Of File encountered. EOF within the data set
 * results in an ERREXIT; NULL is returned ONLY if the first read resulted
 * in an EOF condition. This allows programs to look for multiple data sets
 * in a single file.
 */

METHODDEF DATATYPE *
read_brainvision(transform_info_ptr tinfo) {
 struct read_brainvision_storage * const local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 FILE *infile=local_arg->infile;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->nr_of_channels;
 tinfo->multiplexed=local_arg->multiplexed;
 tinfo->itemsize=local_arg->itemsize;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_brainvision: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Find the next window that fits into the actual data*/
 do {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Simulate a trigger at current_point+beforetrig */
   file_start_point=local_arg->current_point;
   trigger_point=file_start_point+tinfo->beforetrig;
   file_end_point=trigger_point+tinfo->aftertrig-1;
   if (local_arg->points_in_file>0 && file_end_point>=local_arg->points_in_file) return NULL;
   local_arg->current_trigger++;
   local_arg->current_point+=tinfo->nr_of_points;
   tinfo->condition=0;
  } else 
  do {
   tinfo->condition=read_brainvision_read_trigger(tinfo, &trigger_point, &description);
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
  } while (not_correct_trigger || file_start_point<0 || (local_arg->points_in_file>0 && file_end_point>=local_arg->points_in_file));
 } while (--local_arg->fromepoch>0);
 if (description==NULL) {
  TRACEMS3(tinfo->emethods, 1, "read_brainvision: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_brainvision: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
 }
 /*}}}  */

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_brainvision_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_brainvision_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0L, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /* If bytes_per_point is unknown (DT_STRING...) we just continuously scan the data.
  * fromepoch won't work! */
 if (local_arg->multiplexed && local_arg->bytes_per_point>0) {
  /* Multiplexed: read all data of one data point at once, only one seek necessary */
  fseek(infile, file_start_point*local_arg->bytes_per_point, SEEK_SET);
 }

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize;
 if (tinfo->multiplexed==FALSE) {
  myarray.nr_of_elements=tinfo->nr_of_points;
  myarray.nr_of_vectors=tinfo->nr_of_channels;
 } else {
  myarray.nr_of_elements=tinfo->nr_of_channels;
  myarray.nr_of_vectors=tinfo->nr_of_points;
 }
 tinfo->xdata=NULL;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_brainvision: Error allocating data\n");
 }
 /*}}}  */

 read_brainvision_get_filestrings(tinfo);
 if (description==NULL) {
  char const *last_sep=strrchr(args[ARGS_IFILE].arg.s, PATHSEP);
  char const *last_component=(last_sep==NULL ? args[ARGS_IFILE].arg.s : last_sep+1);
  snprintf(tinfo->comment, MAX_COMMENTLEN, "read_brainvision %s", last_component);
 } else {
  snprintf(tinfo->comment, MAX_COMMENTLEN, "%s", description);
 }

 switch ((enum ERROR_ENUM)setjmp(local_arg->error_jmp)) {
  case ERR_NONE:
   local_arg->first_in_epoch=TRUE;
   do {
    double resolution=1.0;
    if (!local_arg->multiplexed) {
     /* Nonmultiplexed: read all data of one channel at once, but need to seek for every channel */
     int const channel=myarray.current_vector;
     resolution=((double *)local_arg->resolutions_buf.buffer_start)[channel];
     fseek(infile, (local_arg->points_in_file*channel+file_start_point)*local_arg->itemsize*datatype_size[local_arg->datatype], SEEK_SET);
    }
    do {
     DATATYPE * const item0_addr=ARRAY_ELEMENT(&myarray);
     int itempart;
     if (local_arg->multiplexed) {
      int const channel=myarray.current_element;
      resolution=((double *)local_arg->resolutions_buf.buffer_start)[channel];
     }
     /* Multiple items are always stored side by side: */
     for (itempart=0; itempart<tinfo->itemsize; itempart++) {
      item0_addr[itempart]=read_value(infile, local_arg)*resolution;
     }
     array_advance(&myarray);
     local_arg->first_in_epoch=FALSE;
    } while (myarray.message==ARRAY_CONTINUE);
   } while (myarray.message!=ARRAY_ENDOFSCAN);
   break;
  case ERR_READ:
   if (local_arg->points_in_file==0) {
    /* While we can avoid this error where we know points_in_file beforehand, the
     * following is needed for a graceful exit on EOF where we don't: */
    if (local_arg->first_in_epoch) {
     TRACEMS(tinfo->emethods, 1, "read_brainvision: *End of File*\n");
     array_free(&myarray);
     free_pointer((void **)&tinfo->comment);
     return NULL;
    }
   }
  case ERR_NUMBER:
   /* Note that a number_err is always a hard error... */
   ERREXIT(tinfo->emethods, "read_brainvision: Error reading data\n");
   break;
 }

 tinfo->sfreq=local_arg->sfreq;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_brainvision_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_brainvision_exit(transform_info_ptr tinfo) {
 struct read_brainvision_storage * const local_arg=(struct read_brainvision_storage *)tinfo->methods->local_storage;

 growing_buf_free(&local_arg->filepath_buf);
 growing_buf_free(&local_arg->triggers);
 growing_buf_free(&local_arg->channelnames_buf);
 growing_buf_free(&local_arg->resolutions_buf);
 growing_buf_free(&local_arg->coordinates_buf);
 if (local_arg->infile!=NULL) fclose(local_arg->infile);
 free_pointer((void **)&local_arg->markerfilename);
 free_pointer((void **)&local_arg->trigcodes);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_brainvision(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_brainvision(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_brainvision_init;
 tinfo->methods->transform= &read_brainvision;
 tinfo->methods->transform_exit= &read_brainvision_exit;
 tinfo->methods->get_filestrings= &read_brainvision_get_filestrings;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_brainvision";
 tinfo->methods->method_description=
  "Get-epoch method to read data from the BrainVision format\n";
 tinfo->methods->local_storage_size=sizeof(struct read_brainvision_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
