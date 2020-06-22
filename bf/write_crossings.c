/*
 * Copyright (C) 1996-1999,2001-2007,2010,2012-2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_crossings is a method that looks at the contents of a given channel
 *  and prints (into a file, one per line) the point numbers at which the value 
 *  of this channel crossed a threshold value and an integer that is either
 *  1 or -1 to tell whether the direction was to the positive (1) or to the 
 *  negative (-1) side. This output can be directly used as a trigger file
 *  with codes 1 and -1.
 *  If extrema are detected, a third column is output that gives the actual
 *  value attained at the extremum. The trigger value in this case has the
 *  meaning 'maximum' (1) and 'minimum' (-1).
 *  The point numbers output correspond to the number within all points seen
 *  by this method (most useful, of course, if a continuous file is read
 *  with the -c option).
 * 						-- Bernd Feige 2.02.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_EXTREMA=0, 
 ARGS_CLOSE,
 ARGS_EPOCHMODE, 
 ARGS_REPORT_XVALUE, 
 ARGS_ITEMPART, 
 ARGS_REFRACTORY_PERIOD, 
 ARGS_POINTOFFSET, 
 ARGS_CHANNELNAMES, 
 ARGS_THRESHOLD, 
 ARGS_OFILE, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Write extrema rather than threshold crossings", "E", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close the output file after each epoch (and open it again next time)", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Epoch mode - Restart detector for each new epoch", "e", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Report x axis values rather than point numbers", "x", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_STRING_WORD, "Refractory period", "R", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "Offset to add to each point number", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_DOUBLE, "threshold", "", 0, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.crs"}
};

/*{{{  Definition of write_crossings_storage*/
struct write_crossings_storage {
 FILE *outfile;
 array last_three_points;
 int channels_in_list;
 int *channel_list;
 int itempart;
 long refractory_period;
 long remaining_refractory_points;
 long pointoffset;
 /* Note how many points have already been seen: */
 long past_points;
 long epochs_seen;
 /* sign(val-threshold). This can be -1, +1 or 0 if undetermined: */
 int *signs_of_last_diff;
 growing_buf triggers;
};
/*}}}  */

LOCAL void open_file(transform_info_ptr tinfo) {
 struct write_crossings_storage *local_arg=(struct write_crossings_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) {
  local_arg->outfile=stdout;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) {
  local_arg->outfile=stderr;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "triggers")==0) {
  /* If the `file name' argument is "triggers", we collect the markers in 
   * memory. outfile==NULL is taken as indication for this condition. */
  local_arg->outfile=NULL;
 } else
 if ((local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, "w"))==NULL) {
  ERREXIT1(tinfo->emethods, "write_crossings_init: Can't open file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 if (local_arg->outfile!=NULL) {
  /* For info, output the sampling frequency as comment */
  fprintf(local_arg->outfile, "# %s\n# Sfreq=%f\n# Epochlength=%d\n# Threshold=%g\n", (args[ARGS_EXTREMA].is_set ? "Extrema" : "Crossings"), (float)tinfo->sfreq, tinfo->nr_of_points, args[ARGS_THRESHOLD].arg.d);
  if (args[ARGS_ITEMPART].is_set) {
   fprintf(local_arg->outfile, "# Itempart=%ld\n", args[ARGS_ITEMPART].arg.i);
  }
  if (args[ARGS_REFRACTORY_PERIOD].is_set) {
   fprintf(local_arg->outfile, "# Refractory period=%s\n", args[ARGS_REFRACTORY_PERIOD].arg.s);
  }
  if (args[ARGS_POINTOFFSET].is_set) {
   fprintf(local_arg->outfile, "# Point offset=%ld\n", local_arg->pointoffset);
  }
 }
}

LOCAL void close_file(transform_info_ptr tinfo) {
 struct write_crossings_storage *local_arg=(struct write_crossings_storage *)tinfo->methods->local_storage;

 if (local_arg->outfile==NULL) {
  if (local_arg->triggers.buffer_start!=NULL) {
   clear_triggers(&local_arg->triggers);
   growing_buf_free(&local_arg->triggers);
  }
 } else {
  if (local_arg->outfile!=stdout && local_arg->outfile!=stderr) {
   fclose(local_arg->outfile);
  } else {
   fflush(local_arg->outfile);
  }
 }
}

/*{{{  write_crossings_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_crossings_init(transform_info_ptr tinfo) {
 struct write_crossings_storage *local_arg=(struct write_crossings_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 growing_buf_init(&local_arg->triggers);

 if (args[ARGS_ITEMPART].is_set) {
  local_arg->itempart=args[ARGS_ITEMPART].arg.i;
  if (local_arg->itempart<0 || local_arg->itempart>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "write_crossings_init: No item number %d in file\n", MSGPARM(local_arg->itempart));
  }
 } else {
  local_arg->itempart=0;
 }
 if (args[ARGS_REFRACTORY_PERIOD].is_set) {
  local_arg->refractory_period=gettimeslice(tinfo, args[ARGS_REFRACTORY_PERIOD].arg.s);
  if (local_arg->refractory_period<0) {
   ERREXIT1(tinfo->emethods, "write_crossings_init: Invalid refractory period %d.\n", MSGPARM(local_arg->refractory_period));
  }
 } else {
  local_arg->refractory_period=0;
 }
 if (args[ARGS_POINTOFFSET].is_set) {
  if (args[ARGS_REPORT_XVALUE].is_set) {
   ERREXIT(tinfo->emethods, "write_crossings_init: Combining options -x and -o makes no sense.\n");
  }
  local_arg->pointoffset=gettimeslice(tinfo, args[ARGS_POINTOFFSET].arg.s);
 } else {
  local_arg->pointoffset=0;
 }

 local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_CHANNELNAMES].arg.s);
 if (local_arg->channel_list==NULL) {
  ERREXIT(tinfo->emethods, "write_crossings_init: No channels were selected.\n");
 }
 local_arg->channels_in_list=0;
 while (local_arg->channel_list[local_arg->channels_in_list]!=0) local_arg->channels_in_list++;

 if (!args[ARGS_CLOSE].is_set) open_file(tinfo);

 local_arg->past_points=local_arg->pointoffset;
 local_arg->remaining_refractory_points=0;
 local_arg->epochs_seen=0;
 if (args[ARGS_EXTREMA].is_set) {
  local_arg->last_three_points.nr_of_vectors=3;
  local_arg->last_three_points.nr_of_elements=local_arg->channels_in_list;
  local_arg->last_three_points.element_skip=1;
  if (array_allocate(&local_arg->last_three_points)==NULL) {
   ERREXIT(tinfo->emethods, "write_crossings_init: Error allocating ring buffer memory\n");
  }
 } else {
  if ((local_arg->signs_of_last_diff=(int *)calloc(local_arg->channels_in_list, sizeof(int)))==NULL) {
   ERREXIT(tinfo->emethods, "write_crossings_init: Error allocating signs_of_last_diff array\n");
  }
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_crossings(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_crossings(transform_info_ptr tinfo) {
 struct write_crossings_storage *local_arg=(struct write_crossings_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel, channel_list_index=0;
 long point=0;
 array tsdata;

 if (args[ARGS_CLOSE].is_set) open_file(tinfo);
 if (args[ARGS_EPOCHMODE].is_set) {
  /* Reset detector */
  local_arg->past_points=local_arg->pointoffset;
  local_arg->remaining_refractory_points=0;
  if (!args[ARGS_EXTREMA].is_set) {
   memset(local_arg->signs_of_last_diff, 0, local_arg->channels_in_list*sizeof(int));
  }
 }

 if (local_arg->outfile==NULL) {
  growing_buf_clear(&local_arg->triggers);
 } else {
 if (args[ARGS_EPOCHMODE].is_set || args[ARGS_REPORT_XVALUE].is_set) {
  /* In epoch mode or if we report x values, the epoch number cannot be
   * calculated because point numbers and x values repeat across epochs. Thus,
   * output the epoch number as well. */
  fprintf(local_arg->outfile, "# Epoch=%ld", local_arg->epochs_seen+1);
  if (tinfo->z_label!=NULL) {
   fprintf(local_arg->outfile, ": %s=%g\n", tinfo->z_label, tinfo->z_value);
  } else {
   fprintf(local_arg->outfile, "\n");
  }
 }
 /* Report the channel name only once if only one channel is selected: */
 if (local_arg->channels_in_list==1 && local_arg->epochs_seen==0) {
  fprintf(local_arg->outfile, "# Channel=%s\n", tinfo->channelnames[local_arg->channel_list[0] - 1]);
 }
 }

 tinfo_array(tinfo, &tsdata);
 array_use_item(&tsdata, local_arg->itempart);

 while ((channel= local_arg->channel_list[channel_list_index] - 1)>=0) {
  long local_point=0;
  int sign_of_last_diff=0;
  Bool channelname_reported=(local_arg->channels_in_list==1);

  if (!args[ARGS_EXTREMA].is_set) {
   sign_of_last_diff=local_arg->signs_of_last_diff[channel_list_index];
  }

  point=local_arg->past_points;

  tsdata.current_element=0;
  tsdata.current_vector=channel;
 do {
  if (args[ARGS_EXTREMA].is_set) {
   array_ringadvance(&local_arg->last_three_points);
   /* array_ringadvance has set the current vector to the 'new' vector */
   local_arg->last_three_points.current_element=channel_list_index;
   WRITE_ELEMENT(&local_arg->last_three_points, array_scan(&tsdata));
   if (local_arg->remaining_refractory_points>0) {
    local_arg->remaining_refractory_points--;
   } else {
   if (point-local_arg->pointoffset>=2) {
    /* Have written at least 3 points into last_three_points... */
    DATATYPE p1, p2, p3;
    int condition=0;
    local_arg->last_three_points.current_vector=0;
    p1=READ_ELEMENT(&local_arg->last_three_points);
    local_arg->last_three_points.current_vector=1;
    p2=READ_ELEMENT(&local_arg->last_three_points);
    local_arg->last_three_points.current_vector=2;
    p3=READ_ELEMENT(&local_arg->last_three_points);
    /* This used to be `p3<p2', but what if two points jointly forming an
     * extremum are exactly equal? In this case, we choose now to detect the
     * first of the points as the extremum. Otherwise, none would be seen... */
    if (p2>=args[ARGS_THRESHOLD].arg.d && p1<p2 && p3<=p2) {
     condition=  1;
    } else if (p2<= -args[ARGS_THRESHOLD].arg.d && p1>p2 && p3>=p2) {
     condition= -1;
    }
    if (condition!=0) {
     /*{{{ Output a trigger */
     if (local_arg->outfile==NULL) {
      int const code=condition*(channel_list_index+1);
      push_trigger(&local_arg->triggers, (local_point>0 ? local_point-1 : 0L), code, NULL);
     } else {
#define WHERE_SIZE 1024
     char where[WHERE_SIZE];

     if (!channelname_reported) {
      fprintf(local_arg->outfile, "# Channel=%s\n", tinfo->channelnames[channel]);
      channelname_reported=TRUE;
     }

     if (args[ARGS_REPORT_XVALUE].is_set) {
      if (tinfo->xdata==NULL) {
       create_xaxis(tinfo, NULL);
      }
      if (local_point>0) {
       snprintf(where, WHERE_SIZE, "%s=%g", tinfo->xchannelname, tinfo->xdata[local_point-1]);
      } else {
       snprintf(where, WHERE_SIZE, "%s=Previous", tinfo->xchannelname);
      }
     } else {
      snprintf(where, WHERE_SIZE, "%ld", point-1);
     }
     fprintf(local_arg->outfile, "%s\t%d\t%g\n", where, condition, p2);
     }
     local_arg->remaining_refractory_points=local_arg->refractory_period;
     /*}}}  */
    }
   }
   }
  } else {
  DATATYPE const diff=array_scan(&tsdata)-args[ARGS_THRESHOLD].arg.d;
  int const sign_of_this_diff=(diff>=0 ? 1 : -1);
   if (sign_of_last_diff==0 || local_arg->remaining_refractory_points>0) {
    if (local_arg->remaining_refractory_points>0) local_arg->remaining_refractory_points--;
   } else {
   if (sign_of_this_diff!=sign_of_last_diff) {
   /*{{{ Output a trigger */
   if (local_arg->outfile==NULL) {
    push_trigger(&local_arg->triggers, local_point, sign_of_this_diff*(channel_list_index+1), NULL);
   } else {
   char where[WHERE_SIZE];

   if (!channelname_reported) {
    fprintf(local_arg->outfile, "# Channel=%s\n", tinfo->channelnames[channel]);
    channelname_reported=TRUE;
   }

   if (args[ARGS_REPORT_XVALUE].is_set) {
    if (tinfo->xdata==NULL) {
     create_xaxis(tinfo, NULL);
    }
    snprintf(where, WHERE_SIZE, "%s=%g", tinfo->xchannelname, tinfo->xdata[local_point]);
   } else {
    snprintf(where, WHERE_SIZE, "%ld", point);
   }
   fprintf(local_arg->outfile, "%s\t%d\n", where, sign_of_this_diff);
   }
   local_arg->remaining_refractory_points=local_arg->refractory_period;
   /*}}}  */
  }
  }
  sign_of_last_diff=sign_of_this_diff;
  }
  point++;
  local_point++;
 } while (tsdata.message==ARRAY_CONTINUE);

  if (!args[ARGS_EXTREMA].is_set) {
   local_arg->signs_of_last_diff[channel_list_index]=sign_of_last_diff;
  }
  channel_list_index++;
 }

 /* Transfer triggers to tinfo->triggers if requested */
 if (local_arg->outfile==NULL) {
  int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);
  /* If we haven't seen any event, we can bypass all this... */
  if (nevents!=0) {
   int oldevent, newevent;
   struct trigger * const oldtrig=(struct trigger *)tinfo->triggers.buffer_start;
   struct trigger * const newtrig=(struct trigger *)local_arg->triggers.buffer_start;
   growing_buf outtriggers;
   growing_buf_init(&outtriggers);

   /* Count oldevents (including the file position entry): */
   for (oldevent=0; oldtrig!=NULL && oldtrig[oldevent].code!=0; oldevent++);
   /* Handle the file position entry: This is taken from oldtrig if available,
    * otherwise we just note how many points were seen here before */
   if (oldevent==0) {
    push_trigger(&outtriggers, local_arg->past_points, -1, NULL);
   } else {
    push_trigger(&outtriggers, oldtrig->position, oldtrig->code, oldtrig->description);
    oldevent=1;
   }
   /* Merge old and new events in ascending order of `position' */
   newevent=0;
   /* As long as we have newevents: */
   while (newevent<nevents) {
    if (oldtrig==NULL || oldtrig[oldevent].code==0
     || oldtrig[oldevent].position>newtrig[newevent].position) {
     push_trigger(&outtriggers, newtrig[newevent].position, newtrig[newevent].code, newtrig[newevent].description);
     newevent++;
    } else {
     push_trigger(&outtriggers, oldtrig[oldevent].position, oldtrig[oldevent].code, oldtrig[oldevent].description);
     oldevent++;
    }
   }
   /* Now copy any oldtrig entries with larger positions than the largest
    * newtrig entry: */
   while (oldtrig!=NULL && oldtrig[oldevent].code!=0) {
    push_trigger(&outtriggers, oldtrig[oldevent].position, oldtrig[oldevent].code, oldtrig[oldevent].description);
    oldevent++;
   }
   push_trigger(&outtriggers, 0L, 0, NULL); /* End of list */
   growing_buf_free(&tinfo->triggers);
   tinfo->triggers=outtriggers;
  }
 }

 local_arg->epochs_seen++;
 local_arg->past_points=point;

 if (args[ARGS_CLOSE].is_set) close_file(tinfo);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  write_crossings_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_crossings_exit(transform_info_ptr tinfo) {
 struct write_crossings_storage *local_arg=(struct write_crossings_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (!args[ARGS_CLOSE].is_set) close_file(tinfo);

 if (args[ARGS_EXTREMA].is_set) {
  array_free(&local_arg->last_three_points);
 } else {
  free_pointer((void **)&local_arg->signs_of_last_diff);
 }
 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_crossings(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_crossings(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_crossings_init;
 tinfo->methods->transform= &write_crossings;
 tinfo->methods->transform_exit= &write_crossings_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="write_crossings";
 tinfo->methods->method_description=
  "`Transform' method finding the positions at which a channel crosses a\n"
  " threshold value, or at which a channel has a local maximum>=threshold\n"
  " or a local minimum<=(-threshold) (option -E)\n"
  " If `Output file' is `triggers', the positions are written to the epoch\n"
  " trigger list with code (+-)markerchannelnumber; In 1-channel Extrema\n"
  " mode, code is set to the (rounded integer) extremal value.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_crossings_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
