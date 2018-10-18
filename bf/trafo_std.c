/*
 * Copyright (C) 1993-2004,2006,2007,2009,2011-2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * trafo_std.c provides the default error and trace handling functions
 * for the transform modules	-- Bernd Feige 15.01.1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
/* Needed with glibc 2.1 to have NAN defined (from math.h) */
#define __USE_ISOC9X 1
/* Needed with glibc 2.2 to have NAN defined (sigh...) */
#define __USE_ISOC99 1
#endif
#include <math.h>
#include <errno.h>
#include "transform.h"
#include "bf.h"

/*
 * The following two functions are the standard error and trace functions
 * selected by fftspect_defaults
 */

/*{{{}}}*/
/*{{{  error_exit(const external_methods_ptr emeth, const char *msgtext)*/
LOCAL void
error_exit(const external_methods_ptr emeth, const char *msgtext) __attribute__ ((__noreturn__));
LOCAL void
error_exit(const external_methods_ptr emeth, const char *msgtext) {
 fprintf(stderr, "ERROR - ");
 fprintf(stderr, msgtext, emeth->message_parm[0], emeth->message_parm[1],
	emeth->message_parm[2], emeth->message_parm[3],
	emeth->message_parm[4], emeth->message_parm[5],
	emeth->message_parm[6], emeth->message_parm[7]);
 exit(1);
}
/*}}}  */

/*{{{  trace_message(const external_methods_ptr emeth, const char *msgtext)*/
LOCAL void
trace_message(const external_methods_ptr emeth, const int lvl, const char *msgtext) {
 int const llvl=(lvl>=0 ? lvl : -lvl-1);
 if (emeth->trace_level>=llvl) {
  /* lvl==0 represents an unmaskable warning that goes to STDERR,
   * lvl<0  will output only the message without the level info to STDOUT
   * to allow informative output to be printed as is */
  if (lvl==0) {
   fprintf(stderr, "WARNING - ");
  } else if (lvl>0) {
   fprintf(stderr, " T%d - ", lvl);
  }
  fprintf((lvl<0 ? stdout : stderr), msgtext, emeth->message_parm[0], emeth->message_parm[1],
	emeth->message_parm[2], emeth->message_parm[3],
	emeth->message_parm[4], emeth->message_parm[5],
	emeth->message_parm[6], emeth->message_parm[7]);
 }
}
/*}}}  */

/*{{{  trace_message(const external_methods_ptr emeth, const char *msgtext)*/
LOCAL char *execution_messages[]={
 "starting init",
 "finished init",
 "starting exec",
 "finished exec",
 "starting exit",
 "finished exit"
};
LOCAL void
execution_callback(const transform_info_ptr tinfo, const execution_callback_place where) {
 fprintf(stderr, "QUEUE line %d script %d: %s - %s\n", tinfo->methods->line_of_script, tinfo->methods->script_number, tinfo->methods->method_name, execution_messages[where]);
}
/*}}}  */

/*{{{  trafo_std_defaults(transform_info_ptr tinfo)*/
GLOBAL void
trafo_std_defaults(transform_info_ptr tinfo) {
 if (!tinfo->emethods->has_been_set) {
  set_external_methods(tinfo->emethods, &error_exit, &trace_message, (tinfo->emethods->trace_level>=6 ? &execution_callback : NULL));
 }
}
/*}}}  */

/* The following two functions provide an official way to clear and set the
 * external method functions. A program using the libbf methods should either
 * call clear_external_methods() if it wants to use the defaults above or
 * set_external_methods to set own versions (higher-level programs, GUIs).
 * clear_external_methods is not really necessary IF the emethod memory is 
 * initialized to zero, but it is safer and cleaner to use it.
 * The whole issue arises only because I want the library to provide a 
 * reasonable default for these methods, and because C does not have object
 * constructors... */

/*{{{  clear_external_methods(const external_methods_ptr emeth)*/
GLOBAL void
clear_external_methods(const external_methods_ptr emeth) {
 emeth->error_exit= NULL;
 emeth->trace_message= NULL;
 emeth->execution_callback= NULL;
 emeth->has_been_set=FALSE;
}
/*}}}  */

/*{{{  set_external_methods(transform_info_ptr tinfo)*/
GLOBAL void
set_external_methods(const external_methods_ptr emeth, 
  void (*error_exitp)(const external_methods_ptr emeth, const char *msgtext) __attribute__ ((__noreturn__)),
  void (*trace_messagep)(const external_methods_ptr emeth, const int lvl, const char *msgtext),
  void (*execution_callbackp)(const transform_info_ptr tinfo, const execution_callback_place where)) {

 emeth->error_exit= error_exitp;
 emeth->trace_message= trace_messagep;
 emeth->execution_callback= execution_callbackp;
 emeth->has_been_set=TRUE;
}
/*}}}  */

/*
 * The following are global functions implementing standards used by
 * the methods
 */

/*{{{  find_channel_number(transform_info_ptr tinfo, char *channel_name) {*/
/* find_channel_number finds the number (index>=0) of the channel with name
 * channel_name. Returns -1 if no channel with that name was found. */
GLOBAL int
find_channel_number(transform_info_ptr tinfo, char const *channel_name) {
 char ** const channelnames=tinfo->channelnames;
 int const nr_of_channels=tinfo->nr_of_channels;
 int channel;

 if (channelnames==NULL) return -1;
 for (channel=0; channel<nr_of_channels && strcmp(channelnames[channel], channel_name)!=0; channel++);
 if (channel==nr_of_channels) return -1;	/* Not found */
 return channel;
}
/*}}}  */

/*{{{  get_value(char const *number, char const **EndPointer)*/
/* This global function extends strtod by the ability to recognize Inf
 * and NAN */
GLOBAL double
get_value(char const *number, char **EndPointer) {
 double value;
 double mysign=1.0;
 while (strchr(" \t+-", *number)!=NULL) {
  if (*number=='-') mysign*= -1.0;
  number++;
 }
 if (strncmp(number, "Inf", 3)==0) {
  value=INFINITY;
  if (EndPointer!=NULL) *EndPointer=(char *)number+3;
 } else if (strncmp(number, "NaN", 3)==0) {
  value=NAN;
  if (EndPointer!=NULL) *EndPointer=(char *)number+3;
 } else {
  value=strtod(number, EndPointer);
 }
 return value*mysign;
}
/*}}}  */

/* This is used both in gettimefloat and in create_xaxis */
#define NR_OF_TIME_UNITS 5
LOCAL struct {
 float unit;	/* In seconds */
 char *name;
} time_units_and_names[NR_OF_TIME_UNITS]={
 {1e-6, "us"},
 {1e-3, "ms"},
 {1.0,  "s"},
 {60.0, "min"},
 {3600.0, "h"}
};
/*{{{  gettimeslice(transform_info_ptr tinfo, char *number)*/
/* gettimeslice returns the number of data points corresponding
 * to the number string; if number ends with "s" or "ms", the sampling
 * rate given in tinfo is used to calculate it, otherwise the number
 * itself is returned (as double precision value).
 */
GLOBAL long
gettimeslice(transform_info_ptr tinfo, char const *number) {
 return (long)rint(gettimefloat(tinfo, number));
}
GLOBAL double
gettimefloat(transform_info_ptr tinfo, char const *number) {
 char *EndPointer;
 double value;

 value=get_value(number, &EndPointer);
 if (EndPointer[0]!='\0') {
  if (strcmp(EndPointer,"Hz")==0) {
   /* A value given in Hertz is converted to a fraction of half the sfreq: */
   if (tinfo->xchannelname!=NULL && strcmp(tinfo->xchannelname,"Freq[Hz]")==0 &&
       tinfo->xdata!=NULL && tinfo->nr_of_points>=2) {
    double const freq_step=(tinfo->xdata[tinfo->nr_of_points-1]-tinfo->xdata[0])/(tinfo->nr_of_points-1);
    value/=freq_step;
   }
  } else {
   int i;
   for (i=0; i<NR_OF_TIME_UNITS; i++) {
    if (strcmp(EndPointer,time_units_and_names[i].name)==0) {
     value*=tinfo->sfreq*time_units_and_names[i].unit;
     break;
    }
   }
   if (i==NR_OF_TIME_UNITS) {
    ERREXIT1(tinfo->emethods, "gettimefloat: Invalid float value: %s\n", MSGPARM(number));
   }
  }
 }
 return value;
}
/*}}}  */

/*{{{  getfreqfloat(transform_info_ptr tinfo, char *number)*/
/* getfreqfloat supports arguments in the frequency domain given as fraction of
 * half the sfreq. Numbers with 'Hz' appended are converted correspondingly.
 * This is used by fftfilter.
 */
GLOBAL double
getfreqfloat(transform_info_ptr tinfo, char const *number) {
 char *EndPointer;
 double value;

 value=get_value(number, &EndPointer);
 if (EndPointer[0]!='\0') {
  if (strcmp(EndPointer,"Hz")==0) {
   /* A value given in Hertz is converted to a fraction of half the sfreq: */
   value/=tinfo->sfreq/2;
  } else {
   ERREXIT1(tinfo->emethods, "getfreqfloat: Invalid float value: %s\n", MSGPARM(number));
  }
 }
 return value;
}
/*}}}  */

/*{{{  find_pointnearx(transform_info_ptr tinfo, DATATYPE x) {*/
/* This function returns the data point with x value closest to the
 * given one. Assumes that the x values are monotonically increasing.
 * Guaranteed to return a valid point number. */
GLOBAL long
find_pointnearx(transform_info_ptr tinfo, DATATYPE x) {
 long pointno;
 if (tinfo->nr_of_points<=1) return 0;
 if (tinfo->xdata==NULL) create_xaxis(tinfo, NULL);
 for (pointno=1; pointno<tinfo->nr_of_points-1 && tinfo->xdata[pointno]<x; pointno++);
 if (fabs(x-tinfo->xdata[pointno-1])<fabs(x-tinfo->xdata[pointno])) pointno--;
 return pointno;
}
/*}}}  */

/*{{{  decode_xpoint(transform_info_ptr tinfo, char *token) {*/
/* This function adds a convenience property to find_pointnearx:
 * Point addresses may have the form xvalue{+|-}npoints. */
GLOBAL long
decode_xpoint(transform_info_ptr tinfo, char *token) {
 char *endptr;
 long point=find_pointnearx(tinfo, (DATATYPE)strtod(token, &endptr));
 if (*endptr=='+' || *endptr=='-') {
  point+=strtol(endptr, &endptr, 10);
 }
 if (*endptr!='\0') {
  ERREXIT1(tinfo->emethods, "decode_xpoint: Can't decode >%s<\n", MSGPARM(token));
 }
 return point;
}
/*}}}  */

/*{{{  create_xaxis(transform_info_ptr tinfo, char const *unitname)*/
/* create_xaxis allocates space for and fills values into the xdata array
 * and sets default strings for xchannelname, depending on the data_type.
 */
#define LENGTH_OF_XCHANNELNAME 15
#define TARGET_POINTS_PER_UNIT 5.0
GLOBAL void
create_xaxis(transform_info_ptr tinfo, char const *unitname) {
 /* If unitname==NULL, a heuristic is applied.
  * Another valid unitname is "points".
  * When "abs_" is prepended, we try to reconstruct the absolute position in the input. */
 int i;
 char const *_unitname=unitname;
 Bool absolute=FALSE;
 DATATYPE factor=tinfo->sfreq;
 DATATYPE unit=1.0;
 long int offset= -tinfo->beforetrig;
 if (_unitname!=NULL) {
  if (strncmp(_unitname,"abs_",4)==0) {
   absolute=TRUE;
   _unitname+=4;
   offset=tinfo->file_start_point;
  }
  if (strcmp(_unitname,"points")==0) {
   factor=1.0;
  } else {
   for (i=0; i<NR_OF_TIME_UNITS; i++) {
    if (strcmp(_unitname,time_units_and_names[i].name)==0) {
     unit=time_units_and_names[i].unit;
     break;
    }
   }
   /* If not found, fall back to heuristic */
   if (i==NR_OF_TIME_UNITS) _unitname=NULL;
  }
 }

 if (tinfo->data_type==FREQ_DATA) tinfo->nr_of_points=tinfo->nroffreq;
 if (tinfo->xdata!=NULL) free(tinfo->xdata);
 if ((tinfo->xdata=(DATATYPE *)malloc(tinfo->nr_of_points*sizeof(DATATYPE)+LENGTH_OF_XCHANNELNAME))==NULL) {
  ERREXIT(tinfo->emethods, "create_xaxis: Error allocating xdata memory\n");
 }
 /* Note that xchannelname has a special treatment; Within avg_q, this
  * string is not allocated separately. */
 tinfo->xchannelname=(char *)(tinfo->xdata+tinfo->nr_of_points);
 if (tinfo->data_type==FREQ_DATA) {
  for (i=0; i<tinfo->nroffreq; i++) tinfo->xdata[i]=i*tinfo->basefreq;
  strcpy(tinfo->xchannelname, "Freq[Hz]");
 } else {
  if (_unitname==NULL) {
   /* The heuristic to choose a unit accepts a unit if the defined
    * TARGET_POINTS_PER_UNIT value falls in the interval of 
    * (sampling-)points_per_unit values resulting from this and the next unit */
   for (i=0; i<NR_OF_TIME_UNITS-1 && 
	(tinfo->sfreq*time_units_and_names[i].unit>TARGET_POINTS_PER_UNIT || 
	 tinfo->sfreq*time_units_and_names[i+1].unit<=TARGET_POINTS_PER_UNIT); 
	i++);
   unit=time_units_and_names[i].unit;
   _unitname=time_units_and_names[i].name;
  }
  snprintf(tinfo->xchannelname, LENGTH_OF_XCHANNELNAME, "%s[%s]", absolute ? "Abs" : "Lat", _unitname);
  for (i=0; i<tinfo->nr_of_points; i++)
   tinfo->xdata[i]=(DATATYPE)(i+offset)/factor/unit;
 }
}
/*}}}  */

/*{{{  copy_channelinfo(transform_info_ptr tinfo, char **channelnames, double *probepos)*/
/* copy_channelinfo will copy the channel names and probe positions to newly-
 * allocated arrays that correspond to the general memory conventions. This is
 * useful for get_epoch methods and such that have a copy of these descriptors
 * in their local storage but cannot put pointers to them into tinfo because
 * a `free' would eventually be called on them. */
GLOBAL void
copy_channelinfo(transform_info_ptr tinfo, char **channelnames, double *probepos) {
 int channel, stringlength=0;
 char *in_buffer;

 if (channelnames!=NULL) {
  if (tinfo->channelnames!=NULL) {
   free_pointer((void **)&tinfo->channelnames[0]);
   free_pointer((void **)&tinfo->channelnames);
  }
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   stringlength+=strlen(channelnames[channel])+1;
  }
  in_buffer=(char *)malloc(stringlength);
  tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *));
  if (in_buffer==NULL || tinfo->channelnames==NULL) {
   ERREXIT(tinfo->emethods, "copy_channelinfo: Error allocating channelnames array\n");
  }
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   tinfo->channelnames[channel]=in_buffer;
   strcpy(in_buffer, channelnames[channel]);
   in_buffer=in_buffer+strlen(in_buffer)+1;
  }
 }
 if (probepos!=NULL) {
  free_pointer((void **)&tinfo->probepos);
  tinfo->probepos=(double *)malloc(3*tinfo->nr_of_channels*sizeof(double));
  if (tinfo->probepos==NULL) {
   ERREXIT(tinfo->emethods, "copy_channelinfo: Error allocating probepos array\n");
  }
  memcpy(tinfo->probepos, probepos, 3*tinfo->nr_of_channels*sizeof(double));
 }
}
/*}}}  */

/*{{{  deepcopy_tinfo(transform_info_ptr to_tinfo, transform_info_ptr from_tinfo) {*/
/* deepcopy_tinfo copies the whole tinfo structure including all strings and
 * arrays, EXCEPT tsdata. This makes sense if the target number of channels AND 
 * points is the same (itemsize could be modified afterwards). The requirement
 * for equal number of points could be lifted by setting from_tinfo->xdata=NULL. */
GLOBAL void
deepcopy_tinfo(transform_info_ptr to_tinfo, transform_info_ptr from_tinfo) {
 int const comment_len=(from_tinfo->comment!=NULL ? strlen(from_tinfo->comment)+1 : 0)+(from_tinfo->xchannelname!=NULL ? strlen(from_tinfo->xchannelname)+1 : 0)+(from_tinfo->z_label!=NULL ? strlen(from_tinfo->z_label)+1 : 0);
 char *new_comment=(char *)malloc(comment_len);
 if (new_comment==NULL) {
  ERREXIT(from_tinfo->emethods, "deepcopy_tinfo: Error allocating new comment\n");
 }

 memcpy(to_tinfo, from_tinfo, sizeof(struct transform_info_struct));

 to_tinfo->channelnames=NULL; to_tinfo->probepos=NULL;
 copy_channelinfo(to_tinfo, from_tinfo->channelnames, from_tinfo->probepos);
 *new_comment='\0';
 if (from_tinfo->comment!=NULL) {
  strcpy(new_comment, from_tinfo->comment);
  to_tinfo->comment=new_comment;
  new_comment+=strlen(new_comment)+1;
 }
 if (from_tinfo->xchannelname!=NULL) {
  strcpy(new_comment, from_tinfo->xchannelname);
  to_tinfo->xchannelname=new_comment;
  new_comment+=strlen(new_comment)+1;
 } else {
  to_tinfo->xchannelname=NULL;
 }
 if (from_tinfo->z_label!=NULL) {
  strcpy(new_comment, from_tinfo->z_label);
  to_tinfo->z_label=new_comment;
  new_comment+=strlen(new_comment)+1;
 }
 if (from_tinfo->xdata!=NULL) {
  if ((to_tinfo->xdata=(DATATYPE *)malloc(from_tinfo->nr_of_points*sizeof(DATATYPE)))==NULL) {
   ERREXIT(from_tinfo->emethods, "deepcopy_tinfo: Error allocating xdata\n");
  }
  memcpy(to_tinfo->xdata, from_tinfo->xdata, from_tinfo->nr_of_points*sizeof(DATATYPE));
 }
 growing_buf_init(&to_tinfo->triggers);
 if (from_tinfo->triggers.buffer_start!=NULL) {
  growing_buf_takewithlength(&to_tinfo->triggers, from_tinfo->triggers.buffer_start, from_tinfo->triggers.current_length);
  {struct trigger *intrig=(struct trigger *)to_tinfo->triggers.buffer_start;
   for (; intrig->code!=0; intrig++) {
    if (intrig->description!=NULL) {
     char * const descriptionp=(char *)malloc(strlen(intrig->description)+1);
     strcpy(descriptionp,intrig->description);
     intrig->description=descriptionp;
    }
   }
  }
 }
}
/*}}}  */

/*{{{  deepfree_tinfo(transform_info_ptr tinfo) {*/
/* This utility function tries to free all memory allocated by deepcopy_tinfo.
 * Since the number of malloc's used by deepcopy is made to be standard, this
 * should be safe to free any data set. */
GLOBAL void
deepfree_tinfo(transform_info_ptr tinfo) {
 free_pointer((void **)&tinfo->comment);
 if (tinfo->channelnames!=NULL) {
  free_pointer((void **)&tinfo->channelnames[0]);
  free_pointer((void **)&tinfo->channelnames);
 }
 free_pointer((void **)&tinfo->probepos);
 free_pointer((void **)&tinfo->xdata);
 tinfo->xchannelname=NULL;
 tinfo->z_label=NULL;
 clear_triggers(&tinfo->triggers);
 growing_buf_free(&tinfo->triggers);
}
GLOBAL void
free_tinfo(transform_info_ptr tinfo) {
 /* We have to go through the linked structures, free and unlink them */
 transform_info_ptr tinfop=tinfo;
 while (tinfop->next!=NULL) tinfop=tinfop->next;	/* Go to the end */
 /* Now proceed backwards: */
 do {
  transform_info_ptr const storeprevious=tinfop->previous;
  deepfree_tinfo(tinfop);
  free_pointer((void **)&tinfop->tsdata);
  tinfop->next=tinfop->previous=NULL;	/* Unlink them... */
  /* Don't try to free the tinfo structure to which tinfo points 
   * (that's often static) */
  if (tinfop!=tinfo) {
   free((void *)tinfop);
  }
  tinfop=storeprevious;
 } while (tinfop!=NULL);
}
/*}}}  */

/*{{{  create_channelgrid(transform_info_ptr tinfo)*/
/* create_channelgrid creates channel names and positions on a square grid
 * as a last resort if no channel positions are available.
 */
GLOBAL void
create_channelgrid(transform_info_ptr tinfo) {
 create_channelgrid_ncols(tinfo, (int)rint(sqrt(tinfo->nr_of_channels)));
}
GLOBAL void
create_channelgrid_ncols(transform_info_ptr tinfo, int const ncols) {
 int channel;
 if (tinfo->channelnames==NULL) {
  char *in_buffer;
  long channelmem_required=0;
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   channelmem_required+=((int)log10(channel+1))+2;
  }
  in_buffer=(char *)malloc(channelmem_required);
  tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *));
  if (in_buffer==NULL || tinfo->channelnames==NULL) {
   ERREXIT(tinfo->emethods, "create_channelgrid: Error allocating channelnames array\n");
  }
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   tinfo->channelnames[channel]=in_buffer;
   sprintf(in_buffer, "%d", channel+1);
   in_buffer=in_buffer+strlen(in_buffer)+1;
  }
 }
 if (tinfo->probepos==NULL) {
  tinfo->probepos=(double *)malloc(3*tinfo->nr_of_channels*sizeof(double));
  if (tinfo->probepos==NULL) {
   ERREXIT(tinfo->emethods, "create_channelgrid: Error allocating probepos array\n");
  }
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   tinfo->probepos[3*channel  ]=  channel%ncols;
   tinfo->probepos[3*channel+1]= -channel/ncols;
   tinfo->probepos[3*channel+2]=  0.0;
  }
 }
}
/*}}}  */

/*{{{  free_pointer(void **ptr)*/
/* free_pointer frees the memory pointed to by *ptr if *ptr!=NULL,
 * and sets *ptr to NULL afterwards.
 */
GLOBAL void
free_pointer(void **ptr) {
 if (*ptr!=NULL) {
  free(*ptr);
  *ptr=NULL;
 }
}
/*}}}  */

/*{{{  comment2time(char *comment)*/
GLOBAL int
comment2time(char *comment, short *dd, short *mm, short *yy, short *yyyy, short *hh, short *mi, short *ss) {
 char *slash;

 *dd= *mm= *yy= *yyyy= *hh= *mi= *ss= 0;

 if (comment==NULL) return FALSE;

 /* A slash before comment+2 would be dangerous, since we will try to read the 
  * day from (slash-2)... */
 slash=comment+1;
 do {
  slash=strchr(slash+1, '/');
 } while (slash!=NULL && strlen(slash)>8 && (slash[3]!='/' || (slash[6]!=',' && slash[8]!=',')));
 if (slash!=NULL && strlen(slash)>8) {
  /* For Neuroscan files, the start time is included as mm/dd/yy,hh:mm:ss.
   * We also support, and actually favor, the mm/dd/yyyy version. */
  char * const colon=strchr(slash, ':');
  if (colon!=NULL && strlen(colon)>=3) {
   *hh=atoi(colon-2);
   *mi=atoi(colon+1);
   if (strlen(colon)>=5) *ss=atoi(colon+4);
  }
  *mm=atoi(slash-2);
  *dd=atoi(slash+1);
  *yy=atoi(slash+4);
  if (slash[6]==',') {
   /* yy is actually in the yy form; construct yyyy */
   if (*yy<50) {
    *yyyy= *yy+2000;
   } else {
    *yyyy= *yy+1900;
   }
  } else {
   /* yy is in yyyy form; construct yy */
   *yyyy= *yy;
   *yy%=100;
  }
  return TRUE;
 }
 return FALSE;
}
/*}}}  */

/*{{{  get_trigcode_list()*/
/* Utility function parsing a string of integer codes like "5,3,10" into a 0-terminated list */
GLOBAL int *
get_trigcode_list(char const *const csv) {
 growing_buf buf, tokenbuf;
 int *trigcodes;
 int trigno=0;

 growing_buf_init(&buf);
 growing_buf_takethis(&buf, csv);
 buf.delimiters=",";
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);

 if ((trigcodes=(int *)malloc((growing_buf_count_tokens(&buf)+1)*sizeof(int)))==NULL) {
  return NULL;
 }
 growing_buf_get_firsttoken(&buf,&tokenbuf);
 while (tokenbuf.current_length>0) {
  trigcodes[trigno]=atoi(tokenbuf.buffer_start);
  growing_buf_get_nexttoken(&buf,&tokenbuf);
  trigno++;
 }
 trigcodes[trigno]=0;   /* End mark */
 growing_buf_free(&tokenbuf);
 growing_buf_free(&buf);
 return trigcodes;
}
/*}}}  */
/*{{{  read_trigger_from_trigfile(FILE *triggerfile, DATATYPE sfreq, long *trigpoint)*/
GLOBAL int
read_trigger_from_trigfile(FILE *triggerfile, DATATYPE sfreq, long *trigpoint, char **descriptionp) {
 growing_buf buffer, tokenbuf;
 int code=0;

 /* Initialize *descriptionp to NULL to avoid having an uninitialized value here which might be free'd */
 if (descriptionp!=NULL) *descriptionp=NULL;
 growing_buf_init(&buffer);
 growing_buf_allocate(&buffer,0);
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);
 buffer.delimiters="\t\r\n";
 /* The do while (1) is here to be able to skip comment lines */
 do {
  int c;
  /* The trigger file format is  point code  on a line by itself. Lines starting
   * with '#' are ignored. The `point' string may be a floating point number
   * and may have 's' or 'ms' appended to denote seconds or milliseconds. */
  growing_buf_clear(&buffer);
  do {
   c=fgetc(triggerfile);
   if (c=='\n' || c==EOF) break;
   growing_buf_appendchar(&buffer,c);
  } while (1);
 
  if (buffer.current_length==0) {
   if (c==EOF) break;
   else continue;
  }
  if (*buffer.buffer_start=='#') continue;

  growing_buf_appendchar(&buffer,'\0');
  if (growing_buf_get_firsttoken(&buffer,&tokenbuf)) {
   double trigpos;
   char *inbuffer=tokenbuf.buffer_start;
   errno=0;
   trigpos=strtod(inbuffer, &inbuffer);
   if (errno!=0 || inbuffer==tokenbuf.buffer_start) {
    /* Avoid invalid lines. We'd need to generate an error here but don't have the tinfo struct. */
    continue;
   }

   if (*inbuffer=='m') {
    trigpos/=1000.0;
    inbuffer++;
   }
   if (*inbuffer=='s') {
    trigpos*=sfreq;
   }
   *trigpoint=(long)rint(trigpos);

   if (growing_buf_get_nexttoken(&buffer,&tokenbuf)) {
    inbuffer=tokenbuf.buffer_start;
    errno=0;
    code=strtol(inbuffer, &inbuffer, 10);
    if (errno!=0 || inbuffer==tokenbuf.buffer_start) {
     /* Avoid invalid lines. We'd need to generate an error here but don't have the tinfo struct. */
     continue;
    }
    if (descriptionp!=NULL && growing_buf_get_nexttoken(&buffer,&tokenbuf)) {
     *descriptionp=strdup(tokenbuf.buffer_start);
    }
   } else {
    code=1;
   }
   break;
  }
 } while (1);

 growing_buf_free(&tokenbuf);
 growing_buf_free(&buffer);

 return code;
}
/*}}}  */

/*{{{  push_trigger(growing_buf *triggersp, long position, int code, char *description)*/
GLOBAL void
push_trigger(growing_buf *triggersp, long position, int code, char *description) {
 struct trigger trig;
 if (triggersp->buffer_start==NULL) {
  growing_buf_allocate(triggersp, 0);
 }
 trig.position=position;
 trig.code=code;
 trig.description=(description==NULL ? NULL : strdup(description));
 growing_buf_append(triggersp, (char *)&trig, sizeof(struct trigger));
}
/*}}}  */

/*{{{  clear_triggers(growing_buf *triggersp)*/
GLOBAL void
clear_triggers(growing_buf *triggersp) {
 if (triggersp!=NULL && triggersp->buffer_start!=NULL) {
  struct trigger *intrig=(struct trigger *)triggersp->buffer_start;
  struct trigger *const afterlast=(struct trigger *)(triggersp->buffer_start+triggersp->current_length);
  /* Traverse the trigger list to free all descriptions.
   * Respect both end-of-list conventions because clear_triggers may be called in situations where a
   * correct end marker has not yet been written, such as after an ERREXIT while opening a trigger file */
  for (; intrig<afterlast && intrig->code!=0; intrig++) {
   free_pointer((void *)&intrig->description);
  }
  growing_buf_clear(triggersp);
  push_trigger(triggersp, 0L, -1, NULL); /* File position entry */
  push_trigger(triggersp, 0L, 0, NULL); /* End of list */
 }
}
/*}}}  */

/*{{{  fprint_cstring(FILE *outfile, char *string)*/
GLOBAL void
fprint_cstring(FILE *outfile, char const *string) {
 /* This function serves to output the string with all provisions so that
  * it survives hopefully unchanged the process of being read as a C string
  * (ie, this is to output C source code). String delimiters are provided as well. */
 char const *instring=string;
 fputc('"', outfile);
 while (*instring!='\0') {
  if (strchr("\\\"", *instring)!=NULL) {
   fputc('\\', outfile); /* Escape backslashes and quotes... */
  }
  fputc(*instring, outfile);
  instring++;
 }
 fputc('"', outfile);
}
/*}}}  */
