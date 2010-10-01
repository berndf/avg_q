/*
 * Copyright (C) 1996-1999,2001,2003,2010 Bernd Feige
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
 * extract_item extracts items from tuple data
 * 						-- Bernd Feige 16.03.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "growing_buf.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_ITEMNO=0, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SENTENCE, "item_number1 item_number2 ...", "", ARGDESC_UNUSED, (const char *const *)"0"}
};

struct extract_item_storage {
 int nr_of_item_numbers;
 int *item_numbers;
};

/*{{{  extract_item_init(transform_info_ptr tinfo) {*/
METHODDEF void
extract_item_init(transform_info_ptr tinfo) {
 struct extract_item_storage *local_arg=(struct extract_item_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int *inbuf;
 growing_buf buf;
 Bool havearg;

 growing_buf_init(&buf);
 growing_buf_takethis(&buf, args[ARGS_ITEMNO].arg.s);
 havearg=growing_buf_firsttoken(&buf);
 local_arg->nr_of_item_numbers=buf.nr_of_tokens;
 if ((local_arg->item_numbers=(int *)malloc(local_arg->nr_of_item_numbers*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "extract_item_init: Error allocating memory\n");
 }
 inbuf=local_arg->item_numbers;
 while (havearg) {
  *inbuf=atoi(buf.current_token);
  if (*inbuf<0 || *inbuf>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "extract_item_init: Invalid item number %d\n", MSGPARM(*inbuf));
  }
  inbuf++;
  havearg=growing_buf_nexttoken(&buf);
 }
 growing_buf_free(&buf);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  extract_item(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
extract_item(transform_info_ptr tinfo) {
 struct extract_item_storage *local_arg=(struct extract_item_storage *)tinfo->methods->local_storage;
 int i;
 DATATYPE *newdata;
 array inarray, newarray;

 tinfo_array(tinfo, &inarray);
 newarray.nr_of_elements=inarray.nr_of_elements;
 newarray.nr_of_vectors=inarray.nr_of_vectors;
 newarray.element_skip=local_arg->nr_of_item_numbers;
 if ((newdata=array_allocate(&newarray))==NULL) {
  ERREXIT(tinfo->emethods, "extract_item: Can't allocate new channel memory !\n");
 }

 for (i=0; i<local_arg->nr_of_item_numbers; i++) {
  int item_number=local_arg->item_numbers[i];
  if (item_number<0 || item_number>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "extract_item: No item_number %d in dataset.\n", MSGPARM(item_number));
  }
  array_use_item(&inarray, item_number);
  array_use_item(&newarray, i);
  array_copy(&inarray, &newarray);
 }

 tinfo->itemsize=newarray.element_skip;
 tinfo->leaveright=0;
 tinfo->length_of_output_region=newarray.nr_of_elements*newarray.element_skip*newarray.nr_of_vectors;
 tinfo->multiplexed=FALSE;

 return newdata;
}
/*}}}  */

/*{{{  extract_item_exit(transform_info_ptr tinfo) {*/
METHODDEF void
extract_item_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_extract_item(transform_info_ptr tinfo) {*/
GLOBAL void
select_extract_item(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &extract_item_init;
 tinfo->methods->transform= &extract_item;
 tinfo->methods->transform_exit= &extract_item_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="extract_item";
 tinfo->methods->method_description=
  "Transform method to extract items from tuple data.\n"
  "item_number values start at 0 (eg: Re=0, Im=1).\n";
 tinfo->methods->local_storage_size=sizeof(struct extract_item_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
