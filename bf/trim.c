/*
 * Copyright (C) 1996-2006,2010,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * trim method to trim the data to the specified number of points
 * 						-- Bernd Feige 28.03.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum collapse_choices {
 COLLAPSE_BY_AVERAGING,
 COLLAPSE_BY_SUMMATION,
 COLLAPSE_BY_MEDIAN,
 COLLAPSE_BY_HIGHEST,
 COLLAPSE_BY_LOWEST,
};
LOCAL const char *const collapse_choice[]={
 "-a", "-s", "-M", "-h", "-l", NULL
};
enum ARGS_ENUM {
 ARGS_COLLAPSE=0, 
 ARGS_USE_CHANNEL,
 ARGS_USE_XVALUES,
 ARGS_RANGES, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SELECTION, "Collapse ranges by averaging, summation, median, highest or lowest value", " ", 0, collapse_choice},
 {T_ARGS_TAKES_STRING_WORD, "channelname: Use value regions of this channel instead of point or x value regions", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_NOTHING, "xstart and xend are used to specify ranges instead of offset and length", "x", FALSE, NULL},
 {T_ARGS_TAKES_SENTENCE, "Ranges: offset1 length1 [offset2 length2 ...]", "", ARGDESC_UNUSED, NULL}
};

/*{{{  Define the local storage struct*/
struct range {
 long offset;
 long length;
};
struct trim_storage {
 growing_buf rangearg;
 long current_epoch;
};
/*}}}  */

/*{{{  trim_init(transform_info_ptr tinfo) {*/
METHODDEF void
trim_init(transform_info_ptr tinfo) {
 struct trim_storage *local_arg=(struct trim_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_USE_XVALUES].is_set && args[ARGS_USE_CHANNEL].is_set) {
  ERREXIT(tinfo->emethods, "trim: -n and -x flags are exclusive.\n");
 }

 growing_buf_init(&local_arg->rangearg);
 growing_buf_takethis(&local_arg->rangearg, args[ARGS_RANGES].arg.s);

 local_arg->current_epoch=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  trim(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
trim(transform_info_ptr tinfo) {
 struct trim_storage *local_arg=(struct trim_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int item, shift, nrofshifts=1;
 long nr_of_input_ranges,nr_of_ranges;
 long output_points;
 long rangeno, current_output_point=0;
 array myarray, newarray;
 DATATYPE *oldtsdata;
 DATATYPE *new_xdata=NULL;
 growing_buf ranges, tokenbuf;
 growing_buf triggers;

 growing_buf_init(&ranges);
 growing_buf_init(&tokenbuf);
 growing_buf_init(&triggers);

 if (tinfo->data_type==FREQ_DATA) {
  tinfo->nr_of_points=tinfo->nroffreq;
  nrofshifts=tinfo->nrofshifts;
 }

 /*{{{  Parse the ranges argument */
 nr_of_input_ranges=growing_buf_count_tokens(&local_arg->rangearg);
 if (nr_of_input_ranges<=0 || nr_of_input_ranges%2!=0) {
  ERREXIT(tinfo->emethods, "trim: Need an even number of arguments >=2\n");
 }
 nr_of_input_ranges/=2;
 output_points=0;
 growing_buf_allocate(&ranges, 0);
 growing_buf_allocate(&tokenbuf, 0);
 growing_buf_get_firsttoken(&local_arg->rangearg,&tokenbuf);
 while (tokenbuf.current_length>0) {
  struct range thisrange;
  if (args[ARGS_USE_CHANNEL].is_set) {
   int const channel=find_channel_number(tinfo,args[ARGS_USE_CHANNEL].arg.s);
   if (channel>=0) {
    DATATYPE const minval=get_value(tokenbuf.buffer_start, NULL);
    growing_buf_get_nexttoken(&local_arg->rangearg,&tokenbuf);
    DATATYPE const maxval=get_value(tokenbuf.buffer_start, NULL);
    array indata;
    tinfo_array(tinfo, &indata);
    indata.current_vector=channel;
    thisrange.offset= -1; /* Marks no start recorded yet */
    do {
     DATATYPE const currval=indata.read_element(&indata);
     if (currval>=minval && currval<=maxval) {
      if (thisrange.offset== -1) {
       thisrange.offset=indata.current_element;
       thisrange.length= 1;
      } else {
       thisrange.length++;
      }
     } else {
      if (thisrange.offset!= -1) {
       growing_buf_append(&ranges, (char *)&thisrange, sizeof(struct range));
       output_points+=(args[ARGS_COLLAPSE].is_set ? 1 : thisrange.length);
       thisrange.offset= -1;
      }
     }
     array_advance(&indata);
    } while (indata.message==ARRAY_CONTINUE);
    if (thisrange.offset!= -1) {
     growing_buf_append(&ranges, (char *)&thisrange, sizeof(struct range));
     output_points+=(args[ARGS_COLLAPSE].is_set ? 1 : thisrange.length);
    }
   } else {
    ERREXIT1(tinfo->emethods, "set xdata_from_channel: Unknown channel name >%s<\n", MSGPARM(args[ARGS_USE_CHANNEL].arg.s));
   }
  } else {
   if (args[ARGS_USE_XVALUES].is_set) {
    if (tinfo->xdata==NULL) create_xaxis(tinfo, NULL);
    thisrange.offset=decode_xpoint(tinfo, tokenbuf.buffer_start);
    growing_buf_get_nexttoken(&local_arg->rangearg,&tokenbuf);
    thisrange.length=decode_xpoint(tinfo, tokenbuf.buffer_start)-thisrange.offset+1;
   } else {
    thisrange.offset=gettimeslice(tinfo, tokenbuf.buffer_start);
    growing_buf_get_nexttoken(&local_arg->rangearg,&tokenbuf);
    thisrange.length=gettimeslice(tinfo, tokenbuf.buffer_start);
    if (thisrange.length==0) {
     thisrange.length=tinfo->nr_of_points-thisrange.offset;
    }
   }
   if (thisrange.length<=0) {
    ERREXIT1(tinfo->emethods, "trim: length is %d but must be >0.\n", MSGPARM(thisrange.length));
   }
   growing_buf_append(&ranges, (char *)&thisrange, sizeof(struct range));
   output_points+=(args[ARGS_COLLAPSE].is_set ? 1 : thisrange.length);
  }
  growing_buf_get_nexttoken(&local_arg->rangearg,&tokenbuf);
 }
 /*}}}  */

 nr_of_ranges=ranges.current_length/sizeof(struct range);
 if (nr_of_ranges==0) {
  TRACEMS(tinfo->emethods, 0, "trim: No valid ranges selected, rejecting epoch.\n");
  return NULL;
 }
 TRACEMS2(tinfo->emethods, 1, "trim: nr_of_ranges=%ld, output_points=%ld\n", MSGPARM(nr_of_ranges), MSGPARM(output_points));

 newarray.nr_of_vectors=tinfo->nr_of_channels*nrofshifts;
 newarray.nr_of_elements=output_points;
 newarray.element_skip=tinfo->itemsize;
 if (array_allocate(&newarray)==NULL) {
  ERREXIT(tinfo->emethods, "trim: Error allocating memory.\n");
 }
 oldtsdata=tinfo->tsdata;

 if (tinfo->xdata!=NULL) {
  /* Store xchannelname at the end of xdata, as in create_xaxis(); otherwise we'll have 
   * a problem after free()'ing xdata... */
  new_xdata=(DATATYPE *)malloc(output_points*sizeof(DATATYPE)+strlen(tinfo->xchannelname)+1);
  if (new_xdata==NULL) {
   ERREXIT(tinfo->emethods, "trim: Error allocating xdata memory.\n");
  }
 }

 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger trig= *(struct trigger *)tinfo->triggers.buffer_start;
  growing_buf_allocate(&triggers, 0);
  /* Record the file position... */
  if (args[ARGS_COLLAPSE].is_set) {
   /* Make triggers in subsequent append() work as expected at least with
    * continuous reading */
   trig.position=local_arg->current_epoch*nr_of_ranges;
   trig.code= -1;
  }
  growing_buf_append(&triggers, (char *)&trig, sizeof(struct trigger));
 }
 
 for (rangeno=0; rangeno<nr_of_ranges; rangeno++) {
  struct range *rangep=((struct range *)(ranges.buffer_start))+rangeno;
  long const old_startpoint=(rangep->offset>0 ?  rangep->offset : 0);
  long const new_startpoint=(rangep->offset<0 ? -rangep->offset : 0);

 /*{{{  Transfer the data*/
 if (old_startpoint<tinfo->nr_of_points 
  /* Skip this explicitly if no "real" point needs copying (length confined within negative offset)
   * since otherwise an endless loop can result as the stop conditions below look at the array 
   * states and no array is touched... */
  && new_startpoint<rangep->length)
 for (item=0; item<tinfo->itemsize; item++) {
  array_use_item(&newarray, item);
  for (shift=0; shift<nrofshifts; shift++) {
   tinfo_array(tinfo, &myarray);
   array_use_item(&myarray, item);
  do {
   DATATYPE sum=0.0;
   long reached_length=new_startpoint;
   myarray.current_element= old_startpoint;
   if (args[ARGS_COLLAPSE].is_set) {
    newarray.current_element=current_output_point;
    newarray.message=ARRAY_CONTINUE;	/* Otherwise the loop below may finish promptly... */
    switch (args[ARGS_COLLAPSE].arg.i) {
     case COLLAPSE_BY_HIGHEST:
      sum= -FLT_MAX;
      break;
     case COLLAPSE_BY_LOWEST:
      sum=  FLT_MAX;
      break;
     default:
      break;
    }
   } else {
    newarray.current_element=current_output_point+new_startpoint;
   }
   if (args[ARGS_COLLAPSE].is_set && args[ARGS_COLLAPSE].arg.i==COLLAPSE_BY_MEDIAN) {
    array helparray=myarray;
    helparray.ringstart=ARRAY_ELEMENT(&myarray);
    helparray.current_element=helparray.current_vector=0;
    helparray.nr_of_vectors=1;
    helparray.nr_of_elements=rangep->length;
    sum=array_median(&helparray);
    myarray.message=ARRAY_CONTINUE;
   } else
   do {
    if (reached_length>=rangep->length) break;
    if (args[ARGS_COLLAPSE].is_set) {
     DATATYPE const hold=array_scan(&myarray);
     switch (args[ARGS_COLLAPSE].arg.i) {
      case COLLAPSE_BY_SUMMATION:
      case COLLAPSE_BY_AVERAGING:
       sum+=hold;
       break;
      case COLLAPSE_BY_HIGHEST:
       if (hold>sum) sum=hold;
       break;
      case COLLAPSE_BY_LOWEST:
       if (hold<sum) sum=hold;
       break;
      default:
       break;
     }
    } else {
     array_write(&newarray, array_scan(&myarray));
    }
    reached_length++;
   } while (newarray.message==ARRAY_CONTINUE && myarray.message==ARRAY_CONTINUE);
   if (args[ARGS_COLLAPSE].is_set) {
    if (args[ARGS_COLLAPSE].arg.i==COLLAPSE_BY_AVERAGING && item<tinfo->itemsize-tinfo->leaveright) sum/=reached_length;
    array_write(&newarray, sum);
   }
   if (newarray.message==ARRAY_CONTINUE) {
    array_nextvector(&newarray);
   }
   if (myarray.message==ARRAY_CONTINUE) {
    array_nextvector(&myarray);
   }
  } while (myarray.message==ARRAY_ENDOFVECTOR);
   tinfo->tsdata+=myarray.nr_of_vectors*myarray.nr_of_elements*myarray.element_skip;
  }
  tinfo->tsdata=oldtsdata;
 }
 /*}}}  */

 if (new_xdata!=NULL) {
  /*{{{  Transfer xdata*/
  DATATYPE sum=0.0;
  long reached_length=new_startpoint;
  long point, newpoint;
  if (args[ARGS_COLLAPSE].is_set) {
   newpoint=current_output_point;
  } else {
   newpoint=current_output_point+new_startpoint;
  }
  for (point=old_startpoint;
    point<tinfo->nr_of_points && reached_length<rangep->length;
    point++) {
   if (args[ARGS_COLLAPSE].is_set) {
    sum+=tinfo->xdata[point];
   } else {
    new_xdata[newpoint]=tinfo->xdata[point];
    newpoint++;
   }
   reached_length++;
  }
  if (args[ARGS_COLLAPSE].is_set) {
   /* Summation of x axis values does not appear to make sense, so we always average: */
   new_xdata[newpoint]=sum/reached_length;
  }
  /*}}}  */
  /* Save xchannelname to the end of new_xdata */
  strcpy((char *)(new_xdata+output_points),tinfo->xchannelname);
 }

  if (tinfo->triggers.buffer_start!=NULL) {
   /* Collect triggers... */
   struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
   while (intrig->code!=0) {
    if (intrig->position>=old_startpoint && intrig->position<old_startpoint+rangep->length) {
     struct trigger trig= *intrig;
     if (args[ARGS_COLLAPSE].is_set) {
      trig.position=current_output_point;
     } else {
      trig.position=intrig->position-old_startpoint+current_output_point;
     }
     growing_buf_append(&triggers, (char *)&trig, sizeof(struct trigger));
    }
    intrig++;
   }
  }
  current_output_point+=(args[ARGS_COLLAPSE].is_set ? 1 : rangep->length);
 }

 if (new_xdata!=NULL) {
  free(tinfo->xdata);
  tinfo->xdata=new_xdata;
  tinfo->xchannelname=(char *)(new_xdata+output_points);
 }
 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger trig;
  /* Write end marker */
  trig.position=0;
  trig.code=0;
  growing_buf_append(&triggers, (char *)&trig, sizeof(struct trigger));
  growing_buf_free(&tinfo->triggers);
  tinfo->triggers=triggers;
 }

 tinfo->multiplexed=FALSE;
 tinfo->nr_of_points=output_points;
 if (tinfo->data_type==FREQ_DATA) {
  tinfo->nroffreq=tinfo->nr_of_points;
 } else {
  tinfo->beforetrig-=((struct range *)(ranges.buffer_start))->offset;
  tinfo->aftertrig=output_points-tinfo->beforetrig;
 }
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tinfo->itemsize*nrofshifts;
 local_arg->current_epoch++;

 growing_buf_free(&tokenbuf);
 growing_buf_free(&ranges);

 return newarray.start;
}
/*}}}  */

/*{{{  trim_exit(transform_info_ptr tinfo) {*/
METHODDEF void
trim_exit(transform_info_ptr tinfo) {
 struct trim_storage *local_arg=(struct trim_storage *)tinfo->methods->local_storage;

 growing_buf_free(&local_arg->rangearg);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_trim(transform_info_ptr tinfo) {*/
GLOBAL void
select_trim(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &trim_init;
 tinfo->methods->transform= &trim;
 tinfo->methods->transform_exit= &trim_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="trim";
 tinfo->methods->method_description=
  "Transform method to trim all channels to the specified length.\n"
  " Copying starts at the indicated offset (prepending zeros if offset is\n"
  " negative) and continues through length points, padding with zeros if\n"
  " necessary.\n";
 tinfo->methods->local_storage_size=sizeof(struct trim_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
