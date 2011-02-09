/*
 * Copyright (C) 1996-2002,2004,2006-2011 Bernd Feige
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
 "points_in_file",
 "nroffreq",
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
 C_POINTSINFILE,
 C_NROFFREQ,
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

#define OUTPUT_LINE_LENGTH 1024

/*{{{  Definition of query_storage*/
struct query_storage {
 FILE *outfile;
 char const *delimiter;
 long total_points;
};
/*}}}  */

/*{{{  query_init(transform_info_ptr tinfo) {*/
METHODDEF void
query_init(transform_info_ptr tinfo) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

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

 /* This is needed for C_TRIGGERS_FOR_TRIGFILE: */
 local_arg->total_points=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  query(transform_info_ptr tinfo) {*/
LOCAL void
myflush(transform_info_ptr tinfo, char *buffer) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;
 if (local_arg->outfile==NULL) {
  TRACEMS(tinfo->emethods, -1, buffer);
 } else {
  fputs(buffer, local_arg->outfile);
 }
}
METHODDEF DATATYPE *
query(transform_info_ptr tinfo) {
 struct query_storage *local_arg=(struct query_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel;
 int point;
 char buffer[OUTPUT_LINE_LENGTH], *inbuf=buffer;
 transform_info_ptr const tinfoptr=tinfo;

 if (args[ARGS_PRINTNAME].is_set) {
  snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s=", variables_choice[args[ARGS_VARNAME].arg.i]);
  inbuf+=strlen(inbuf);
 }
 switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
  case C_SFREQ:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%g%s", tinfoptr->sfreq, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_NR_OF_POINTS:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->nr_of_points, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_NR_OF_CHANNELS:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->nr_of_channels, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_ITEMSIZE:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->itemsize, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_LEAVERIGHT:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->leaveright, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_LENGTH_OF_OUTPUT_REGION:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%ld%s", tinfoptr->length_of_output_region, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_BEFORETRIG:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->beforetrig, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_AFTERTRIG:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->aftertrig, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_POINTSINFILE:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%ld%s", tinfoptr->points_in_file, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_NROFFREQ:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->nroffreq, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_NROFAVERAGES:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->nrofaverages, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_ACCEPTED_EPOCHS:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->accepted_epochs, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_REJECTED_EPOCHS:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->rejected_epochs, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_FAILED_ASSERTIONS:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->failed_assertions, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_CONDITION:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%d%s", tinfoptr->condition, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_Z_LABEL:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s%s", tinfoptr->z_label, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_Z_VALUE:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%g%s", tinfoptr->z_value, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_COMMENT:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s%s", tinfoptr->comment, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_DATETIME: {
   short dd, mm, yy, yyyy, hh, mi, ss;
   if (comment2time(tinfoptr->comment, &dd, &mm, &yy, &yyyy, &hh, &mi, &ss)) {
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%02d/%02d/%04d,%02d:%02d:%02d%s", mm, dd, yyyy, hh, mi, ss, local_arg->delimiter);
   } else {
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "UNDEFINED%s", local_arg->delimiter);
   }
   myflush(tinfo, buffer);
   }
   break;
  case C_XCHANNELNAME:
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s%s", tinfoptr->xchannelname, local_arg->delimiter); myflush(tinfo, buffer);
   break;
  case C_XDATA:
   if (tinfoptr->xdata==NULL) {
    create_xaxis(tinfoptr);
   }
   for (point=0; point<tinfoptr->nr_of_points; point++) {
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%g%s", tinfoptr->xdata[point], local_arg->delimiter); myflush(tinfo, buffer);
    inbuf=buffer;
   }
   break;
  case C_CHANNELNAMES:
   for (channel=0; channel<tinfoptr->nr_of_channels; channel++) {
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s%s", tinfoptr->channelnames[channel], local_arg->delimiter); myflush(tinfo, buffer);
    inbuf=buffer;
   }
   break;
  case C_CHANNELPOSITIONS:
   for (channel=0; channel<tinfoptr->nr_of_channels; channel++) {
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s %g %g %g%s", tinfoptr->channelnames[channel], tinfoptr->probepos[channel*3], tinfoptr->probepos[channel*3+1], tinfoptr->probepos[channel*3+2], local_arg->delimiter); myflush(tinfo, buffer);
    inbuf=buffer;
   }
   break;
  case C_TRIGGERS:
   if (tinfoptr->triggers.buffer_start!=NULL) {
    struct trigger *intrig=(struct trigger *)tinfoptr->triggers.buffer_start;
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "File position: %ld%s", intrig->position, local_arg->delimiter); myflush(tinfo, buffer);
    intrig++;
    while (intrig->code!=0) {
     if (intrig->description==NULL) {
      if (tinfoptr->xdata==NULL) {
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%ld %d%s", intrig->position, intrig->code, local_arg->delimiter); myflush(tinfo, buffer);
      } else {
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s=%g %d%s", tinfoptr->xchannelname, tinfoptr->xdata[intrig->position], intrig->code, local_arg->delimiter); myflush(tinfo, buffer);
      }
     } else {
      if (tinfoptr->xdata==NULL) {
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%ld %d %s%s", intrig->position, intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, buffer);
      } else {
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s=%g %d %s%s", tinfoptr->xchannelname, tinfoptr->xdata[intrig->position], intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, buffer);
      }
     }
     inbuf=buffer;
     intrig++;
    }
   }
   break;
  case C_TRIGGERS_FOR_TRIGFILE:
  case C_TRIGGERS_FOR_TRIGFILE_S:
  case C_TRIGGERS_FOR_TRIGFILE_MS:
   /* This option aims at writing the epoch triggers as absolute file 
    * triggers suitable as a trigger file. Note that for the correct 
    * positions to be calculated, the query method must be at a point in
    * the queue where it sees all epochs in the shape as they are written
    * to an output file. */
   if (tinfoptr->triggers.buffer_start!=NULL) {
    struct trigger *intrig=(struct trigger *)tinfoptr->triggers.buffer_start+1;
    while (intrig->code!=0) {
     long const pointno=local_arg->total_points+intrig->position;
     switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
      case C_TRIGGERS_FOR_TRIGFILE:
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%ld", pointno);
       break;
      case C_TRIGGERS_FOR_TRIGFILE_S:
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%gs", ((double)pointno)/tinfo->sfreq);
       break;
      case C_TRIGGERS_FOR_TRIGFILE_MS:
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%gms", ((double)pointno*1000.0)/tinfo->sfreq);
       break;
      default:
       /* Can't happen */
       break;
     }
     inbuf+=strlen(inbuf);
     if (intrig->description==NULL) {
      snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "\t%d%s", intrig->code, local_arg->delimiter); myflush(tinfo, buffer);
     } else {
      snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "\t%d\t%s%s", intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, buffer);
     }
     inbuf=buffer;
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
    snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "# Sfreq=%f%s", tinfo->sfreq, local_arg->delimiter); myflush(tinfo, buffer);
    for (current_trigger=0; current_trigger<nevents; current_trigger++) {
     struct trigger * const intrig=((struct trigger *)tinfoptr->filetriggersp->buffer_start)+current_trigger;
     switch((enum variables_choice)args[ARGS_VARNAME].arg.i) {
      case C_FILETRIGGERS_FOR_TRIGFILE:
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%ld", intrig->position);
       break;
      case C_FILETRIGGERS_FOR_TRIGFILE_S:
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%gs", ((double)intrig->position)/tinfo->sfreq);
       break;
      case C_FILETRIGGERS_FOR_TRIGFILE_MS:
       snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%gms", ((double)intrig->position*1000.0)/tinfo->sfreq);
       break;
      default:
       /* Can't happen */
       break;
     }
     inbuf+=strlen(inbuf);
     if (intrig->description==NULL) {
      snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "\t%d%s", intrig->code, local_arg->delimiter); myflush(tinfo, buffer);
     } else {
      snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "\t%d\t%s%s", intrig->code, intrig->description, local_arg->delimiter); myflush(tinfo, buffer);
     }
     inbuf=buffer;
    }
   }
   break;
  case C_CWD:
   if (getcwd(inbuf,OUTPUT_LINE_LENGTH-(inbuf-buffer)-3)==NULL) {
    strcpy(inbuf,"ERROR");
   }
   inbuf+=strlen(inbuf);
   snprintf(inbuf, OUTPUT_LINE_LENGTH-(inbuf-buffer), "%s", local_arg->delimiter); myflush(tinfo, buffer);
   break;
 }
 if (local_arg->outfile!=NULL) fflush(local_arg->outfile);

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
