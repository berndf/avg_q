/*
 * Copyright (C) 1999,2001,2003 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * Transform method to calculate the (pseudo)inverse of the input matrix.
 * This is done so that `project -n -C -p 0 orig.asc 0' with
 * orig.asc being the original data results in the identity matrix.
 * This is needed to transform an ICA weights matrix into the corresponding
 * set of maps.
 *	-- Bernd Feige 5.07.1999
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "array.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_ITEMPART=0, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
};
struct invert_storage {
 int fromitem;
 int toitem;
};

/*{{{  invert_init(transform_info_ptr tinfo)*/
METHODDEF void
invert_init(transform_info_ptr tinfo) {
 struct invert_storage *local_arg=(struct invert_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 local_arg->fromitem=0;
 local_arg->toitem=tinfo->itemsize-tinfo->leaveright-1;
 if (args[ARGS_ITEMPART].is_set) {
  local_arg->fromitem=local_arg->toitem=args[ARGS_ITEMPART].arg.i;
  if (local_arg->fromitem<0 || local_arg->fromitem>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "invert_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  invert(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
invert(transform_info_ptr tinfo) {
 struct invert_storage *local_arg=(struct invert_storage *)tinfo->methods->local_storage;
 int itempart;
 array indata;

 tinfo_array(tinfo, &indata); /* The channels are the vectors */
 for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
  array_use_item(&indata, itempart);
  array_pseudoinverse(&indata);
 }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  invert_exit(transform_info_ptr tinfo)*/
METHODDEF void
invert_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_invert(transform_info_ptr tinfo)*/
GLOBAL void
select_invert(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &invert_init;
 tinfo->methods->transform= &invert;
 tinfo->methods->transform_exit= &invert_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="invert";
 tinfo->methods->method_description=
  "Method to calculate the (pseudo)inverse of the input matrix.\n"
  " This is done so that `project -n -C -p 0 orig.asc 0' with\n"
  " orig.asc being the original data results in the identity matrix.\n"
  " This is needed to transform an ICA weights matrix into maps.\n";
 tinfo->methods->local_storage_size=sizeof(struct invert_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
