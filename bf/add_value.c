/*
 * Copyright (C) 1997-2001,2003,2004,2006,2010,2011,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * add_value.c method to add a value to each point in the incoming epochs
 * - either a constant value
 * - or various calculated values
 *	-- Bernd Feige 15.06.1997
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include "noise.h"
#include "transform.h"
#include "bf.h"

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif
/*}}}  */

/*{{{  Local definitions*/
enum add_value_type {
 ADD_VALUE_NEGMEAN, 
 ADD_VALUE_NEGPOINTMEAN, 
 ADD_VALUE_NEGMIN, 
 ADD_VALUE_NEGPOINTMIN, 
 ADD_VALUE_NEGMAX, 
 ADD_VALUE_NEGPOINTMAX, 
 ADD_VALUE_NEGQUANTILE, 
 ADD_VALUE_NEGPOINTQUANTILE, 
 ADD_VALUE_NOISE, 
 ADD_VALUE_GAUSSNOISE, 
 ADD_VALUE_TRIGGERS, 

 ADD_VALUE
};
LOCAL const char *const add_value_typenames[]={
 "negmean",
 "negpointmean",
 "negmin",
 "negpointmin",
 "negmax",
 "negpointmax",
 "negquantile",
 "negpointquantile",
 "noise",
 "gaussnoise",
 "triggers",
 NULL
};
/*}}}  */

enum ARGS_ENUM {
 ARGS_BYNAME=0,
 ARGS_ITEMPART,
 ARGS_TYPE,
 ARGS_VALUE, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channels", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_SELECTION, "Special values", " ", 0, add_value_typenames},
 {T_ARGS_TAKES_DOUBLE, "value", " ", 1, NULL}
};
/*}}}  */

struct add_value_storage {
 int *channel_list;
 Bool have_channel_list;
 enum add_value_type type;
 double value;
 int fromitem;
 int toitem;
};

/*{{{  add_value_init(transform_info_ptr tinfo)*/
METHODDEF void
add_value_init(transform_info_ptr tinfo) {
 struct add_value_storage *local_arg=(struct add_value_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_TYPE].is_set) {
  local_arg->type=(enum add_value_type)args[ARGS_TYPE].arg.i;
 } else {
  local_arg->type=ADD_VALUE;
 }
 if (local_arg->type==ADD_VALUE
  || local_arg->type==ADD_VALUE_NOISE || local_arg->type==ADD_VALUE_GAUSSNOISE
  || local_arg->type==ADD_VALUE_NEGQUANTILE || local_arg->type==ADD_VALUE_NEGPOINTQUANTILE
  ) {
  if (!args[ARGS_VALUE].is_set) {
   ERREXIT(tinfo->emethods, "add_value_init: A value must be given!\n");
  }
  local_arg->value=args[ARGS_VALUE].arg.d;
  if ((local_arg->type==ADD_VALUE_NEGQUANTILE || local_arg->type==ADD_VALUE_NEGPOINTQUANTILE)
   && (local_arg->value<0 || local_arg->value>1)) {
    ERREXIT(tinfo->emethods, "add_value_init: value must be between 0 and 1 for quantile!\n");
  }
 }

 if (args[ARGS_BYNAME].is_set) {
  if (local_arg->type==ADD_VALUE_NEGQUANTILE) {
   ERREXIT(tinfo->emethods, "add_value_init: Channel name list does not work with negquantile!\n");
  }
  /* Note that this is NULL if no channel matched, which is why we need have_channel_list as well... */
  local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_BYNAME].arg.s);
  local_arg->have_channel_list=TRUE;
 } else {
  local_arg->channel_list=NULL;
  local_arg->have_channel_list=FALSE;
 }

 local_arg->fromitem=0;
 local_arg->toitem=tinfo->itemsize-tinfo->leaveright-1;
 if (args[ARGS_ITEMPART].is_set) {
  local_arg->fromitem=local_arg->toitem=args[ARGS_ITEMPART].arg.i;
  if (local_arg->fromitem<0 || local_arg->fromitem>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "add_value_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  add_value(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
add_value(transform_info_ptr tinfo) {
 struct add_value_storage *local_arg=(struct add_value_storage *)tinfo->methods->local_storage;
 DATATYPE value;
 int itempart;
 array indata;
 Bool work_on_maps=FALSE;

 tinfo_array(tinfo, &indata);
 switch (local_arg->type) {
  /* Operations which are done on maps */
  case ADD_VALUE_NEGMEAN:
  case ADD_VALUE_NEGMIN:
  case ADD_VALUE_NEGMAX:
  case ADD_VALUE_NEGQUANTILE:
  case ADD_VALUE_TRIGGERS:
   array_transpose(&indata);	/* Vectors are maps */
   work_on_maps=TRUE;
   break;
  /* Operations which are done on channels */
  case ADD_VALUE_NEGPOINTMEAN:
  case ADD_VALUE_NEGPOINTMIN:
  case ADD_VALUE_NEGPOINTMAX:
  case ADD_VALUE_NEGPOINTQUANTILE:
  case ADD_VALUE_NOISE:
  case ADD_VALUE_GAUSSNOISE:
  case ADD_VALUE:
   break;
 }

 for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
  array_use_item(&indata, itempart);
  do {
   if (!work_on_maps && local_arg->have_channel_list && !is_in_channellist(indata.current_vector+1, local_arg->channel_list)) {
    array_nextvector(&indata);
    continue;
   }
   switch (local_arg->type) {
    case ADD_VALUE_NEGMEAN: {
     long n_elements=0;
     value=0.0;
     do {
      if (local_arg->have_channel_list && !is_in_channellist(indata.current_element+1, local_arg->channel_list)) {
       array_advance(&indata);
      } else {
       value+=array_scan(&indata);
       n_elements++;
      }
     } while (indata.message==ARRAY_CONTINUE);
     value= -value/n_elements;
     }
     array_previousvector(&indata);
     break;
    case ADD_VALUE_NEGPOINTMEAN:
     value= -array_mean(&indata);
     array_previousvector(&indata);
     break;
    case ADD_VALUE_NEGMIN:
     value= FLT_MAX;
     do {
      if (local_arg->have_channel_list && !is_in_channellist(indata.current_element+1, local_arg->channel_list)) {
       array_advance(&indata);
      } else {
       DATATYPE const hold=array_scan(&indata);
       if (hold<value) value=hold;
      }
     } while (indata.message==ARRAY_CONTINUE);
     value= -value;
     array_previousvector(&indata);
     break;
    case ADD_VALUE_NEGPOINTMIN:
     value= -array_min(&indata);
     array_previousvector(&indata);
     break;
    case ADD_VALUE_NEGMAX:
     value= -FLT_MAX;
     do {
      if (local_arg->have_channel_list && !is_in_channellist(indata.current_element+1, local_arg->channel_list)) {
       array_advance(&indata);
      } else {
       DATATYPE const hold=array_scan(&indata);
       if (hold>value) value=hold;
      }
     } while (indata.message==ARRAY_CONTINUE);
     value= -value;
     array_previousvector(&indata);
     break;
    case ADD_VALUE_NEGPOINTMAX:
     value= -array_max(&indata);
     array_previousvector(&indata);
     break;
    case ADD_VALUE_NEGQUANTILE:
    case ADD_VALUE_NEGPOINTQUANTILE:
     value= -array_quantile(&indata,local_arg->value);
     break;
    case ADD_VALUE_NOISE:
     /* This needs a special treatment, since values vary across channels AND points */
     value=local_arg->value;
     do {
      array_write(&indata, READ_ELEMENT(&indata)+(((DATATYPE)rand())/RAND_MAX-0.5)*2*value);
     } while (indata.message==ARRAY_CONTINUE);
     continue;
    case ADD_VALUE_GAUSSNOISE:
     value=local_arg->value;
     do {
      array_write(&indata, READ_ELEMENT(&indata)+snorm()*value);
     } while (indata.message==ARRAY_CONTINUE);
     continue;
    case ADD_VALUE_TRIGGERS:
     value=0;
     if (tinfo->triggers.buffer_start!=NULL) {
      struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
      long const pointno=indata.current_vector;
      while (intrig->code!=0) {
       if (intrig->position==pointno) {
	value=intrig->code;
       }
       intrig++;
      }
     }
     break;
    case ADD_VALUE:
     value=local_arg->value;
     break;
    default:
     continue;
   }
   do {
    if (work_on_maps && local_arg->have_channel_list && !is_in_channellist(indata.current_element+1, local_arg->channel_list)) {
     array_advance(&indata);
    } else {
     array_write(&indata, READ_ELEMENT(&indata)+value);
    }
   } while (indata.message==ARRAY_CONTINUE);
  } while (indata.message!=ARRAY_ENDOFSCAN);
 }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  add_value_exit(transform_info_ptr tinfo)*/
METHODDEF void
add_value_exit(transform_info_ptr tinfo) {
 struct add_value_storage *local_arg=(struct add_value_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_add(transform_info_ptr tinfo)*/
/* Oops, note that our scheme of dumping and compiling dumped
 * scripts depends upon the condition that the select call for a method
 * is called select_METHODNAME where METHODNAME is the name as seen by
 * the user. This could only be overridden by having the name of the
 * select call explicitly set as a method property in each and every 
 * select code piece... */
GLOBAL void
select_add(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &add_value_init;
 tinfo->methods->transform= &add_value;
 tinfo->methods->transform_exit= &add_value_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="add";
 tinfo->methods->method_description=
  "Method to add to each value in the incoming epochs either\n"
  " - the negative of the mean of all values in each map (value==negmean)\n"
  " - ~ of the mean of all points within each channel (value==negpointmean)\n"
  " - ~ of the maximum of all values in each map (value==negmax)\n"
  " - ~ of the maximum of all points within each channel (value==negpointmax)\n"
  " - random values with an amplitude given by the next argument (value==noise)\n"
  " - a constant value\n";
 tinfo->methods->local_storage_size=sizeof(struct add_value_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
