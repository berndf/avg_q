/*
 * Copyright (C) 2014,2016,2024 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_brainvision
 *	-- Bernd Feige 28.11.2014
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <setjmp.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const datatype_choice[]={
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
 ARGS_APPEND=0,
 ARGS_CLOSE,
 ARGS_SWAPBYTEORDER,
 ARGS_POINTSFASTEST,
 ARGS_OFILE,
 ARGS_DATATYPE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close the file after writing each epoch (and open it again next time)", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Swap byte order relative to the current machine", "S", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Points vary fastest in the output file (`nonmultiplexed')", "P", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_SELECTION, "data_type", "", DT_INT16, datatype_choice}
};

/*{{{  struct write_brainvision_storage {*/
struct write_brainvision_storage {
 FILE *outfile;
 FILE *vmrkfile;
 enum DATATYPE_ENUM datatype;
 long total_points;
 long current_trigger;

 jmp_buf error_jmp;
};
/*}}}  */

/*{{{  Local open_file and close_file routines*/
/*{{{  write_brainvision_open_file(transform_info_ptr tinfo) {*/
LOCAL void
write_brainvision_open_file(transform_info_ptr tinfo) {
 struct write_brainvision_storage *local_arg=(struct write_brainvision_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 growing_buf eegfilename,vmrkfilename;
 FILE *vhdrfile;
 char *scratch;
 int channel, skippath;
 growing_buf_init(&eegfilename);
 growing_buf_init(&vmrkfilename);
 growing_buf_takethis(&eegfilename,args[ARGS_OFILE].arg.s);
 scratch=strrchr(eegfilename.buffer_start,'.');
 if (scratch!=NULL) {
  *scratch='\0';
  eegfilename.current_length=scratch-eegfilename.buffer_start+1;
 }
 growing_buf_takethis(&vmrkfilename,eegfilename.buffer_start);
 growing_buf_appendstring(&eegfilename,".eeg");
 growing_buf_appendstring(&vmrkfilename,".vmrk");

 /* Note how many characters should be skipped to skip the path component */
 scratch=strrchr(eegfilename.buffer_start,PATHSEP);
 skippath= scratch==NULL ? 0 : scratch-eegfilename.buffer_start+1;

 vhdrfile=fopen(args[ARGS_OFILE].arg.s, "r+");
 if (!args[ARGS_APPEND].is_set || vhdrfile==NULL) {   /* target does not exist*/
  /*{{{  Create files*/
  if (vhdrfile!=NULL) fclose(vhdrfile);

  if ((local_arg->outfile=fopen(eegfilename.buffer_start, "wb"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_brainvision_open_file: Can't open %s\n", MSGPARM(eegfilename.buffer_start));
  }
  if ((vhdrfile=fopen(args[ARGS_OFILE].arg.s, "w"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_brainvision_open_file: Can't open %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
  fprintf(vhdrfile,"\
Brain Vision Data Exchange Header File Version 1.0\n\
; Data created by avg_q (https://github.com/berndf/avg_q)\n\
\n\
[Common Infos]\n\
Codepage=UTF-8\n\
DataFile=%s\n\
MarkerFile=%s\n\
DataFormat=BINARY\n\
; Data orientation: MULTIPLEXED=ch1,pt1, ch2,pt1 ...\n\
DataOrientation=%s\n\
NumberOfChannels=%d\n\
; Sampling interval in microseconds\n\
SamplingInterval=%g\n\
\n\
[Binary Infos]\n\
BinaryFormat=%s\n\
\n\
[Channel Infos]\n\
; Each entry: Ch<Channel number>=<Name>,<Reference channel name>,\n\
; <Resolution in \"Unit\">,<Unit>, Future extensions..\n\
; Fields are delimited by commas, some fields might be omitted (empty).\n\
; Commas in channel names are coded as \"\\1\".\n\
",
   eegfilename.buffer_start+skippath, 
   vmrkfilename.buffer_start+skippath, 
   args[ARGS_POINTSFASTEST].is_set ? "VECTORIZED" : "MULTIPLEXED",
   tinfo->nr_of_channels,
   1e6/tinfo->sfreq,
   datatype_choice[args[ARGS_DATATYPE].arg.i]
  );
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   fprintf(vhdrfile,"Ch%d=%s,,1,µV\n",channel+1,tinfo->channelnames[channel]);
  }
  fclose(vhdrfile);
  if ((local_arg->vmrkfile=fopen(vmrkfilename.buffer_start, "w"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_brainvision_open_file: Can't open %s\n", MSGPARM(vmrkfilename.buffer_start));
  }
  fprintf(local_arg->vmrkfile,"\
Brain Vision Data Exchange Marker File, Version 1.0\n\
\n\
[Common Infos]\n\
Codepage=UTF-8\n\
DataFile=%s\n\
\n\
[Marker Infos]\n\
; Each entry: Mk<Marker number>=<Type>,<Description>,<Position in data points>,\n\
; <Size in data points>, <Channel number (0 = marker is related to all channels)>\n\
; Fields are delimited by commas, some fields might be omitted (empty).\n\
; Commas in type or description text are coded as \"\\1\".\n\
",
   eegfilename.buffer_start+skippath
  );
  local_arg->total_points=0;	/* For computing absolute trigger points */
  TRACEMS1(tinfo->emethods, 1, "write_brainvision_open_file: Creating file %s\n", MSGPARM(eegfilename.buffer_start));
  /*}}}  */
 } else {
  /*{{{  Append to file*/
  fclose(vhdrfile);
  if ((local_arg->outfile=fopen(eegfilename.buffer_start, "ab"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_brainvision_open_file: Can't open %s\n", MSGPARM(eegfilename.buffer_start));
  }
  local_arg->total_points=ftell(local_arg->outfile)/tinfo->nr_of_channels/datatype_size[args[ARGS_DATATYPE].arg.i];	/* For computing absolute trigger points */
  if ((local_arg->vmrkfile=fopen(vmrkfilename.buffer_start, "a"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_brainvision_open_file: Can't open %s\n", MSGPARM(vmrkfilename.buffer_start));
  }
  TRACEMS1(tinfo->emethods, 1, "write_brainvision_open_file: Appending to file %s\n", MSGPARM(eegfilename.buffer_start));
  /*}}}  */
 }
 growing_buf_free(&eegfilename);
 growing_buf_free(&vmrkfilename);
}
/*}}}  */

/*{{{  write_brainvision_close_file(transform_info_ptr tinfo) {*/
LOCAL void
write_brainvision_close_file(transform_info_ptr tinfo) {
 struct write_brainvision_storage *local_arg=(struct write_brainvision_storage *)tinfo->methods->local_storage;
 if (local_arg->outfile!=NULL) {
  fclose(local_arg->outfile);
  local_arg->outfile=NULL;
 }
 if (local_arg->vmrkfile!=NULL) {
  fclose(local_arg->vmrkfile);
  local_arg->vmrkfile=NULL;
 }
}
/*}}}  */
/*}}}  */

/*{{{  write_brainvision_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_brainvision_init(transform_info_ptr tinfo) {
 struct write_brainvision_storage *local_arg=(struct write_brainvision_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /*{{{  Process options*/
 local_arg->datatype=(enum DATATYPE_ENUM)args[ARGS_DATATYPE].arg.i;
 /*}}}  */

 if (!args[ARGS_CLOSE].is_set) write_brainvision_open_file(tinfo);

 local_arg->current_trigger=1;	/* This is for counting the marker numbers in Mk1,Mk2,... */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

LOCAL void
write_value(DATATYPE dat, FILE *outfile, 
 struct write_brainvision_storage *local_arg, 
 Bool swap_byteorder, Bool string_output_newline) {
 Bool err=FALSE;
 switch (local_arg->datatype) {
  case DT_INT8: {
   signed char c= (signed char)rint(dat);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_INT16: {
   int16_t c= (int16_t)rint(dat);
   if (swap_byteorder) Intel_int16((uint16_t *)&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_INT32: {
   int32_t c= (int32_t)rint(dat);
   if (swap_byteorder) Intel_int32((uint32_t *)&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_FLOAT32: {
   float c=dat;
   if (swap_byteorder) Intel_float(&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_FLOAT64: {
   double c=dat;
   if (swap_byteorder) Intel_double(&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
 }
 if (err) {
  longjmp(local_arg->error_jmp, err);
 }
}

/*{{{  write_brainvision(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_brainvision(transform_info_ptr tinfo) {
 struct write_brainvision_storage *local_arg=(struct write_brainvision_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;

 if (args[ARGS_CLOSE].is_set) {
  write_brainvision_open_file(tinfo);
 }

 tinfo_array(tinfo, &myarray);
 switch (setjmp(local_arg->error_jmp)) {
  case FALSE:
   if (!args[ARGS_POINTSFASTEST].is_set) {
    array_transpose(&myarray);	/* channels are the elements */
   }

   do {
    DATATYPE * const item0_addr=ARRAY_ELEMENT(&myarray);
    int itempart;
    /* We do that here so that it is easy to know whether we are writing the last element: */
    array_advance(&myarray);
    /* If multiple items are present, output them side by side: */
    for (itempart=0; itempart<tinfo->itemsize; itempart++) {
     write_value(item0_addr[itempart], local_arg->outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, myarray.message!=ARRAY_CONTINUE && itempart+1==tinfo->itemsize);
    }
   } while (myarray.message!=ARRAY_ENDOFSCAN);
   break;
  case TRUE:
   ERREXIT(tinfo->emethods, "write_brainvision: Error writing data\n");
   break;
 }

 /* Output triggers */
 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
  while (intrig->code!=0) {
   /* Positions in BrainVision apparently start at 1 */
   long const pointno=local_arg->total_points+intrig->position+1;
   char *fulldesc=NULL, *usedesc=NULL;
   if (intrig->description!=NULL) {
    /* Decide what to do with the textual description */
    if (strncmp(intrig->description,"Stimulus,S",10)==0 || strncmp(intrig->description,"Response,R",10)==0) {
     /* Use as "stimulus type,description" as is */
     fulldesc=intrig->description;
    } else {
     /* Replace commas by blanks to maintain a syntactically correct marker line */
     for (char *indesc=intrig->description; *indesc!='\0'; indesc++) {
      if (*indesc==',') *indesc=' ';
     }
     usedesc=intrig->description;
    }
   }
   if (fulldesc!=NULL) {
    /* Use fulldesc as "stimulus type,description" as is */
    fprintf(local_arg->vmrkfile, "Mk%ld=%s,%ld,1,0\n", 
     local_arg->current_trigger, 
     fulldesc, 
     pointno);
   } else if (intrig->code==256 || intrig->code==257) {
    fprintf(local_arg->vmrkfile, "Mk%ld=%s,%ld,1,0\n",
     local_arg->current_trigger,
     intrig->code==256 ? "New Segment," : "DC Correction,",
     pointno);
   } else {
    if (usedesc!=NULL) {
     fprintf(local_arg->vmrkfile, "Mk%ld=%s%3d %s,%ld,1,0\n",
      local_arg->current_trigger,
      intrig->code>0 ? "Stimulus,S" : "Response,R",
      abs(intrig->code),
      usedesc,
      pointno);
    } else {
     fprintf(local_arg->vmrkfile, "Mk%ld=%s%3d,%ld,1,0\n",
      local_arg->current_trigger,
      intrig->code>0 ? "Stimulus,S" : "Response,R",
      abs(intrig->code),
      pointno);
    }
   }
   local_arg->current_trigger++;
   intrig++;
  }
 }
 local_arg->total_points+=tinfo->nr_of_points;

 if (args[ARGS_CLOSE].is_set) write_brainvision_close_file(tinfo);
 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_brainvision_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_brainvision_exit(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;

 if (!args[ARGS_CLOSE].is_set) write_brainvision_close_file(tinfo);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_brainvision(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_brainvision(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_brainvision_init;
 tinfo->methods->transform= &write_brainvision;
 tinfo->methods->transform_exit= &write_brainvision_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_brainvision";
 tinfo->methods->method_description=
  "Write Brain Products (TM) BrainVision data format.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_brainvision_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
