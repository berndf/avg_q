/*
 * Copyright (C) 1996-1999,2001,2003,2011,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * integrate is a simple transform method generating the integral --
 * defined by the accumulating sum between the successive data points.
 * The integration is done in place.
 * 						-- Bernd Feige 21.07.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_EPOCHMODE=0,
 ARGS_BYNAME,
 ARGS_ITEMPART, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Epoch mode - Restart for each epoch", "e", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
};
struct integrate_storage {
 array last_point;
 int *channel_list;
 Bool have_channel_list;
 int fromitem;
 int toitem;
};

/*{{{  integrate_init(transform_info_ptr tinfo) {*/
METHODDEF void
integrate_init(transform_info_ptr tinfo) {
 struct integrate_storage *local_arg=(struct integrate_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_BYNAME].is_set) {
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
   ERREXIT1(tinfo->emethods, "integrate_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 local_arg->last_point.start=NULL; /* Mark as uninitialized */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  integrate(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
integrate(transform_info_ptr tinfo) {
 struct integrate_storage *local_arg=(struct integrate_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE accu;
 transform_info_ptr const tinfoptr=tinfo;
 array myarray;

  int itempart;
  int shift, nrofshifts=1;
  DATATYPE *save_tsdata;

  tinfo_array(tinfoptr, &myarray);	/* The channels are the vectors */
  save_tsdata=myarray.start;

  if (tinfoptr->data_type==FREQ_DATA) {
   nrofshifts=tinfoptr->nrofshifts;
  }
  for (shift=0; shift<nrofshifts; shift++) {
   myarray.start=save_tsdata+shift*tinfo->nroffreq*tinfo->nr_of_channels*tinfo->itemsize;
   array_setreadwrite(&myarray);
   for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
    array_use_item(&myarray, itempart);
    do {
     if (local_arg->have_channel_list && !is_in_channellist(myarray.current_vector+1, local_arg->channel_list)) {
      array_nextvector(&myarray);
      continue;
     }
     local_arg->last_point.current_vector=myarray.current_vector; /* Note the vector number before it can get modified */
     if (args[ARGS_EPOCHMODE].is_set || local_arg->last_point.start==NULL) {
      accu=0.0;
     } else {
      accu=READ_ELEMENT(&local_arg->last_point);
     }
     do {
      array_write(&myarray, accu+=READ_ELEMENT(&myarray));
     } while (myarray.message==ARRAY_CONTINUE);
     if (!args[ARGS_EPOCHMODE].is_set) {
      /* Store the current value */
      if (local_arg->last_point.start==NULL) {
       int const savevector=local_arg->last_point.current_vector;
       local_arg->last_point.nr_of_vectors=myarray.nr_of_vectors;
       local_arg->last_point.nr_of_elements=1;
       local_arg->last_point.element_skip=1;
       if (array_allocate(&local_arg->last_point)==NULL) {
	ERREXIT(tinfo->emethods, "integrate: Error allocating last_point memory\n");
       }
       local_arg->last_point.current_vector=savevector;
      }
      array_write(&local_arg->last_point, accu);
     }
    } while (myarray.message==ARRAY_ENDOFVECTOR);
   }
  }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  integrate_exit(transform_info_ptr tinfo) {*/
METHODDEF void
integrate_exit(transform_info_ptr tinfo) {
 struct integrate_storage *local_arg=(struct integrate_storage *)tinfo->methods->local_storage;

 array_free(&local_arg->last_point);
 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_integrate(transform_info_ptr tinfo) {*/
GLOBAL void
select_integrate(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &integrate_init;
 tinfo->methods->transform= &integrate;
 tinfo->methods->transform_exit= &integrate_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="integrate";
 tinfo->methods->method_description=
  "Transform method generating the integral --\n"
  "defined by the cumulative sum of the data points\n";
 tinfo->methods->local_storage_size=sizeof(struct integrate_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
