/*
 * Copyright (C) 2002,2003 Bernd Feige
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
 * link_order is a simple transform method reordering the linked list
 * 						-- Bernd Feige 14.06.2002
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_NUMBERS=0,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SENTENCE, "dataset_no_1 [dataset_no_2 ...]", "", ARGDESC_UNUSED, NULL}
};

typedef struct {
 transform_info_ptr tinfop;
 Bool was_processed;
} dataset_entry;

struct link_order_storage {
 int *numbers;
 growing_buf datasets;
};

/*{{{  link_order_init(transform_info_ptr tinfo) {*/
METHODDEF void
link_order_init(transform_info_ptr tinfo) {
 struct link_order_storage *local_arg=(struct link_order_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int number=0, number1;
 growing_buf buf,tokenbuf;
 Bool havearg;

 growing_buf_init(&buf);
 growing_buf_takethis(&buf, args[ARGS_NUMBERS].arg.s);
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);

 havearg=growing_buf_get_firsttoken(&buf,&tokenbuf);
 while (havearg) {
  number++;
  havearg=growing_buf_get_nexttoken(&buf,&tokenbuf);
 }

 if ((local_arg->numbers=(int *)malloc((number+1)*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "link_order_init: Error allocating numbers memory\n");
 }

 number=0;
 havearg=growing_buf_get_firsttoken(&buf,&tokenbuf);
 while (havearg) {
  local_arg->numbers[number]=atoi(tokenbuf.buffer_start);
  if (local_arg->numbers[number]<=0) {
   ERREXIT1(tinfo->emethods, "link_order_init: Number '%s' not >0\n", MSGPARM(tokenbuf.buffer_start));
  }
  for (number1=0; number1<number; number1++) {
   if (local_arg->numbers[number1]==local_arg->numbers[number]) {
    ERREXIT(tinfo->emethods, "link_order_init: All numbers must be different.\n");
   }
  }
  havearg=growing_buf_get_nexttoken(&buf,&tokenbuf);
  number++;
 }
 growing_buf_free(&tokenbuf);
 growing_buf_free(&buf);
 local_arg->numbers[number]=0;

 growing_buf_init(&local_arg->datasets);
 growing_buf_allocate(&local_arg->datasets, 0);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  link_order(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
link_order(transform_info_ptr tinfo) {
 struct link_order_storage *local_arg=(struct link_order_storage *)tinfo->methods->local_storage;
 int number=0, ndatasets, indataset;
 transform_info_ptr tinfoptr;
 dataset_entry ds, *previousds=NULL;

 growing_buf_clear(&local_arg->datasets);

 if (tinfo->previous!=NULL) {
  ERREXIT(tinfo->emethods, "link_order: Assumption that tinfo is the first dataset doesn't hold!!\n");
 }
 /* Fill the dataset entry scratchpad */
 for (tinfoptr=tinfo; tinfoptr!=NULL; tinfoptr=tinfoptr->next) {
  ds.tinfop=tinfoptr;
  ds.was_processed=FALSE;	/* This dataset was not yet mentioned in the list */
  growing_buf_append(&local_arg->datasets, (char *)&ds, sizeof(dataset_entry));
 }
 ndatasets=local_arg->datasets.current_length/sizeof(dataset_entry);

 /* First add the named datasets in the order of their mentioning: */
 while (local_arg->numbers[number]!=0) {
  dataset_entry * const startds=(dataset_entry *)local_arg->datasets.buffer_start;
  dataset_entry * const thisds=startds+local_arg->numbers[number]-1;

  if (local_arg->numbers[number]>ndatasets) {
   ERREXIT2(tinfo->emethods, "link_order: Dataset %d requested but only %d data sets available\n", MSGPARM(local_arg->numbers[number]), MSGPARM(ndatasets));
  }
  /*{{{  Assure that the to-be first dataset occupies the *tinfo memory: */
  /* Here we need the assumption that tinfo is the first data set, whose
   * structure memory is probably not dynamically allocated. */
  if (number==0 && local_arg->numbers[number]!=1) {
   struct transform_info_struct const save_tinfo= *(thisds->tinfop);
   *(thisds->tinfop) = *tinfo;
   *tinfo = save_tinfo;
   /* Update the adresses in the dataset list: */
   startds->tinfop= thisds->tinfop;
   thisds->tinfop= tinfo;
  }
  /*}}}  */
  if (previousds!=NULL) {
   thisds->tinfop->previous=previousds->tinfop;
   previousds->tinfop->next=thisds->tinfop;
  } else {
   thisds->tinfop->previous=NULL;
  }
  thisds->tinfop->next=NULL;
  thisds->was_processed=TRUE;
  previousds=thisds;
  number++;
 }
 /* Then, add the unnamed datasets in their original order */
 for (indataset=0; indataset<ndatasets; indataset++) {
  dataset_entry * const thisds=((dataset_entry *)local_arg->datasets.buffer_start)+indataset;
  if (!thisds->was_processed) {
   if (previousds!=NULL) {
    thisds->tinfop->previous=previousds->tinfop;
    previousds->tinfop->next=thisds->tinfop;
   } else {
    thisds->tinfop->previous=NULL;
   }
   thisds->tinfop->next=NULL;
   previousds=thisds;
  }
 }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  link_order_exit(transform_info_ptr tinfo) {*/
METHODDEF void
link_order_exit(transform_info_ptr tinfo) {
 struct link_order_storage *local_arg=(struct link_order_storage *)tinfo->methods->local_storage;
 free_pointer((void **)&local_arg->numbers);
 growing_buf_free(&local_arg->datasets);
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_link_order(transform_info_ptr tinfo) {*/
GLOBAL void
select_link_order(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &link_order_init;
 tinfo->methods->transform= &link_order;
 tinfo->methods->transform_exit= &link_order_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="link_order";
 tinfo->methods->method_description=
  "Transform method to reorder the linked list (stack) of datasets in memory.\n"
  "'link_order 10 11' for example makes dataset 10 the first, 11 the second,\n"
  "followed by what was first before. Dataset numbers start with 1.\n";
 tinfo->methods->local_storage_size=sizeof(struct link_order_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
