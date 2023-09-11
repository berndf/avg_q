/*
 * Copyright (C) 1996-2002,2004,2006-2010,2012-2014,2016,2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * query is a method to write global or dataset values to a stream
 * 						-- Bernd Feige 27.09.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const variables_choice[]={
 "sfreq",
 "nr_of_points",
 "nr_of_channels",
 "itemsize",
 "leaveright",
 "length_of_output_region",
 "beforetrig",
 "aftertrig",
 "file_start_point",
 "points_in_file",
 "nroffreq",
 "basefreq",
 "nrofaverages",
 "accepted_epochs",
 "rejected_epochs",
 "failed_assertions",
 "condition",
 "z_label",
 "z_value",
 "comment",
 "datetime",
 "xchannelname",
 "xdata",
 "channelnames",
 "channelpositions",
 "triggers",
 "triggers_for_trigfile",
 "triggers_for_trigfile_s",
 "triggers_for_trigfile_ms",
 "filetriggers_for_trigfile",
 "filetriggers_for_trigfile_s",
 "filetriggers_for_trigfile_ms",
 "CWD",
 NULL
};
enum variables_choice {
 C_SFREQ,
 C_NR_OF_POINTS,
 C_NR_OF_CHANNELS,
 C_ITEMSIZE,
 C_LEAVERIGHT,
 C_LENGTH_OF_OUTPUT_REGION,
 C_BEFORETRIG,
 C_AFTERTRIG,
 C_FILESTARTPOINT,
 C_POINTSINFILE,
 C_NROFFREQ,
 C_BASEFREQ,
 C_NROFAVERAGES,
 C_ACCEPTED_EPOCHS,
 C_REJECTED_EPOCHS,
 C_FAILED_ASSERTIONS,
 C_CONDITION,
 C_Z_LABEL,
 C_Z_VALUE,
 C_COMMENT,
 C_DATETIME,
 C_XCHANNELNAME,
 C_XDATA,
 C_CHANNELNAMES,
 C_CHANNELPOSITIONS,
 C_TRIGGERS,
 C_TRIGGERS_FOR_TRIGFILE,
 C_TRIGGERS_FOR_TRIGFILE_S,
 C_TRIGGERS_FOR_TRIGFILE_MS,
 C_FILETRIGGERS_FOR_TRIGFILE,
 C_FILETRIGGERS_FOR_TRIGFILE_S,
 C_FILETRIGGERS_FOR_TRIGFILE_MS,
 C_CWD,
};

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_PRINTNAME, 
 ARGS_TABDELIMITED, 
 ARGS_VARNAME, 
 ARGS_OFILE, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append output if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Output as `name=value' pair", "N", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Write a TAB after the entry instead of a NEWLINE", "t", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "var_name", "", 0, variables_choice},
 {T_ARGS_TAKES_FILENAME, "Output file", " ", ARGDESC_UNUSED, (const char *const *)"stdout"}
};

/*{{{  Definition of query_storage*/
struct query_storage {
 FILE *outfile;
 char const *delimiter;
 long total_points;
};
/*}}}  */

LOCAL void
myflush(transform_info_ptr tinfo, growing_buf *bufferp) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;
 if (local_arg->outfile==NULL) {
  TRACEMS(tinfo->emethods, -1, bufferp->buffer_start);
 } else {
  fputs(bufferp->buffer_start, local_arg->outfile);
 }
 growing_buf_clear(bufferp);
}

/*{{{  query_init(transform_info_ptr tinfo) {*/
METHODDEF void
query_init(transform_info_ptr tinfo) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 growing_buf buffer;
 growing_buf_init(&buffer);
 growing_buf_allocate(&buffer, 0);

 if (!args[ARGS_OFILE].is_set) {
  local_arg->outfile=NULL;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) {
  local_arg->outfile=stdout;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) {
  local_arg->outfile=stderr;
 } else
 if ((local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, args[ARGS_APPEND].is_set ? "a" : "w"))==NULL) {
  ERREXIT1(tinfo->emethods, "query_init: Can't open file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 if (args[ARGS_TABDELIMITED].is_set) {
  local_arg->delimiter="\t";
 } else {
  local_arg->delimiter="\n";
 }
 switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
  case C_TRIGGERS_FOR_TRIGFILE:
  case C_TRIGGERS_FOR_TRIGFILE_S:
  case C_TRIGGERS_FOR_TRIGFILE_MS:
  case C_FILETRIGGERS_FOR_TRIGFILE:
  case C_FILETRIGGERS_FOR_TRIGFILE_S:
  case C_FILETRIGGERS_FOR_TRIGFILE_MS:
   growing_buf_appendf(&buffer, "# Sfreq=%f%s", tinfo->sfreq, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  default:
   break;
 }

 /* This is needed for C_TRIGGERS_FOR_TRIGFILE: */
 local_arg->total_points=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  query(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
query(transform_info_ptr tinfo) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel;
 int point;
 transform_info_ptr const tinfoptr=tinfo;
 growing_buf buffer;
 growing_buf_init(&buffer);
 growing_buf_allocate(&buffer, 0);

 if (args[ARGS_PRINTNAME].is_set) {
  growing_buf_appendf(&buffer, "%s=", variables_choice[args[ARGS_VARNAME].arg.i]);
 }
 switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
  case C_SFREQ:
   growing_buf_appendf(&buffer, "%g%s", tinfoptr->sfreq, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_NR_OF_POINTS:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->nr_of_points, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_NR_OF_CHANNELS:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->nr_of_channels, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_ITEMSIZE:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->itemsize, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_LEAVERIGHT:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->leaveright, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_LENGTH_OF_OUTPUT_REGION:
   growing_buf_appendf(&buffer, "%ld%s", tinfoptr->length_of_output_region, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_BEFORETRIG:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->beforetrig, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_AFTERTRIG:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->aftertrig, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_FILESTARTPOINT:
   growing_buf_appendf(&buffer, "%ld%s", tinfoptr->file_start_point, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_POINTSINFILE:
   growing_buf_appendf(&buffer, "%ld%s", tinfoptr->points_in_file, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_NROFFREQ:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->nroffreq, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_BASEFREQ:
   growing_buf_appendf(&buffer, "%g%s", tinfoptr->basefreq, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_NROFAVERAGES:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->nrofaverages, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_ACCEPTED_EPOCHS:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->accepted_epochs, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_REJECTED_EPOCHS:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->rejected_epochs, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_FAILED_ASSERTIONS:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->failed_assertions, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_CONDITION:
   growing_buf_appendf(&buffer, "%d%s", tinfoptr->condition, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_Z_LABEL:
   growing_buf_appendf(&buffer, "%s%s", tinfoptr->z_label, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_Z_VALUE:
   growing_buf_appendf(&buffer, "%g%s", tinfoptr->z_value, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_COMMENT:
   growing_buf_appendf(&buffer, "%s%s", tinfoptr->comment, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_DATETIME: {
   short dd, mm, yy, yyyy, hh, mi, ss;
   if (comment2time(tinfoptr->comment, &dd, &mm, &yy, &yyyy, &hh, &mi, &ss)) {
    growing_buf_appendf(&buffer, "%02d/%02d/%04d,%02d:%02d:%02d%s", mm, dd, yyyy, hh, mi, ss, local_arg->delimiter);
   } else {
    growing_buf_appendf(&buffer, "UNDEFINED%s", local_arg->delimiter);
   }
   myflush(tinfo, &buffer);
   }
   break;
  case C_XCHANNELNAME:
   growing_buf_appendf(&buffer, "%s%s", tinfoptr->xchannelname, local_arg->delimiter); myflush(tinfo, &buffer);
   break;
  case C_XDATA:
   if (tinfoptr->xdata==NULL) {
    create_xaxis(tinfoptr, NULL);
   }
   for (point=0; point<tinfoptr->nr_of_points; point++) {
    growing_buf_appendf(&buffer, "%g%s", tinfoptr->xdata[point], local_arg->delimiter); myflush(tinfo, &buffer);
   }
   break;
  case C_CHANNELNAMES:
   for (channel=0; channel<tinfoptr->nr_of_channels; channel++) {
    growing_buf_appendf(&buffer, "%s%s", tinfoptr->channelnames[channel], local_arg->delimiter); myflush(tinfo, &buffer);
   }
   break;
  case C_CHANNELPOSITIONS:
   for (channel=0; channel<tinfoptr->nr_of_channels; channel++) {
    growing_buf_appendf(&buffer, "%s\t%g\t%g\t%g%s", tinfoptr->channelnames[channel], tinfoptr->probepos[channel*3], tinfoptr->probepos[channel*3+1], tinfoptr->probepos[channel*3+2], local_arg->delimiter); myflush(tinfo, &buffer);
   }
   break;
  case C_TRIGGERS:
   if (tinfoptr->triggers.buffer_start!=NULL) {
    struct trigger *intrig=(struct trigger *)tinfoptr->triggers.buffer_start;
    growing_buf_appendf(&buffer, "# File position: %ld%s", intrig->position, local_arg->delimiter); myflush(tinfo, &buffer);
    intrig++;
    while (intrig->code!=0) {
     if (intrig->description==NULL) {
      if (tinfoptr->xdata==NULL) {
       growing_buf_appendf(&buffer, "%ld\t%d%s", intrig->position, intrig->code, local_arg->delimiter); myflush(tinfo, &buffer);
      } else {
       growing_buf_appendf(&buffer, "%s=%g\t%d%s", tinfoptr->xchannelname, tinfoptr->xdata[intrig->position], intrig->code, local_arg->delimiter); myflush(tinfo, &buffer);
      }
     } else {
      if (tinfoptr->xdata==NULL) {
       growing_buf_appendf(&buffer, "%ld\t%d\t%s%s", intrig->position, intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, &buffer);
      } else {
       growing_buf_appendf(&buffer, "%s=%g\t%d\t%s%s", tinfoptr->xchannelname, tinfoptr->xdata[intrig->position], intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, &buffer);
      }
     }
     intrig++;
    }
   }
   break;
  case C_TRIGGERS_FOR_TRIGFILE:
  case C_TRIGGERS_FOR_TRIGFILE_S:
  case C_TRIGGERS_FOR_TRIGFILE_MS:
   /* This option aims at writing the epoch triggers as absolute file triggers
    * suitable as a trigger file.
    * Note that the positions correspond to the data seen by the query method. */
   if (tinfoptr->triggers.buffer_start!=NULL) {
    struct trigger *intrig=(struct trigger *)tinfoptr->triggers.buffer_start+1;
    while (intrig->code!=0) {
     long const pointno=local_arg->total_points+intrig->position;
     switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
      case C_TRIGGERS_FOR_TRIGFILE:
       growing_buf_appendf(&buffer, "%ld", pointno);
       break;
      case C_TRIGGERS_FOR_TRIGFILE_S:
       growing_buf_appendf(&buffer, "%gs", ((double)pointno)/tinfo->sfreq);
       break;
      case C_TRIGGERS_FOR_TRIGFILE_MS:
       growing_buf_appendf(&buffer, "%gms", ((double)pointno*1000.0)/tinfo->sfreq);
       break;
      default:
       /* Can't happen */
       break;
     }
     if (intrig->description==NULL) {
      growing_buf_appendf(&buffer, "\t%d%s", intrig->code, local_arg->delimiter); myflush(tinfo, &buffer);
     } else {
      growing_buf_appendf(&buffer, "\t%d\t%s%s", intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, &buffer);
     }
     intrig++;
    }
   }
   local_arg->total_points+=tinfoptr->nr_of_points;
   break;
  case C_FILETRIGGERS_FOR_TRIGFILE:
  case C_FILETRIGGERS_FOR_TRIGFILE_S:
  case C_FILETRIGGERS_FOR_TRIGFILE_MS:
   /* Dump the file triggers made available by the GET_EPOCH_METHOD */
   if (tinfoptr->filetriggersp!=NULL && tinfoptr->filetriggersp->buffer_start!=NULL) {
    int const nevents=tinfoptr->filetriggersp->current_length/sizeof(struct trigger);
    int current_trigger;
    for (current_trigger=0; current_trigger<nevents; current_trigger++) {
     struct trigger * const intrig=((struct trigger *)tinfoptr->filetriggersp->buffer_start)+current_trigger;
     switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
      case C_FILETRIGGERS_FOR_TRIGFILE:
       growing_buf_appendf(&buffer, "%ld", intrig->position);
       break;
      case C_FILETRIGGERS_FOR_TRIGFILE_S:
       growing_buf_appendf(&buffer, "%gs", ((double)intrig->position)/tinfo->sfreq);
       break;
      case C_FILETRIGGERS_FOR_TRIGFILE_MS:
       growing_buf_appendf(&buffer, "%gms", ((double)intrig->position*1000.0)/tinfo->sfreq);
       break;
      default:
       /* Can't happen */
       break;
     }
     growing_buf_appendf(&buffer, "\t%d", intrig->code);
     if (intrig->description!=NULL) {
      /* With strings, current_length includes the zero at the end; we want to
       * overwrite this zero and copy that of str */
      if (buffer.current_length>0) buffer.current_length--;
      growing_buf_appendchar(&buffer, '\t');
      for (char *indesc=intrig->description; *indesc!='\0'; indesc++) {
       char thischar= *indesc;
       if (strchr("\n\r\t", thischar)) thischar=' ';
       growing_buf_appendchar(&buffer, thischar);
      }
      growing_buf_appendchar(&buffer, '\0');
     }
     growing_buf_appendstring(&buffer, local_arg->delimiter); myflush(tinfo, &buffer);
    }
   }
   break;
  case C_CWD: {
   char *const cwd=getcwd(NULL,0);
   if (cwd==NULL) {
    growing_buf_appendstring(&buffer, "ERROR");
   } else {
    growing_buf_appendstring(&buffer, cwd);
   }
   growing_buf_appendstring(&buffer, local_arg->delimiter); myflush(tinfo, &buffer);
   }
   break;
 }
 if (local_arg->outfile!=NULL) fflush(local_arg->outfile);

 growing_buf_free(&buffer);
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  query_exit(transform_info_ptr tinfo) {*/
METHODDEF void
query_exit(transform_info_ptr tinfo) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;

 if (local_arg->outfile!=NULL && 
     local_arg->outfile!=stdout && 
     local_arg->outfile!=stderr) {
  fclose(local_arg->outfile);
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_query(transform_info_ptr tinfo) {*/
GLOBAL void
select_query(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &query_init;
 tinfo->methods->transform= &query;
 tinfo->methods->transform_exit= &query_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="query";
 tinfo->methods->method_description=
  "`Transform' method to query some values used by avg_q\n";
 tinfo->methods->local_storage_size=sizeof(struct query_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
