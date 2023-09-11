/*
 * Copyright (C) 2001,2002 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * push is a simple transform method duplicating the current data set and storing it in the linked list
 * 						-- Bernd Feige 17.08.2001
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  pop_init(transform_info_ptr tinfo) {*/
METHODDEF void
pop_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  pop(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
pop(transform_info_ptr tinfo) {
 transform_info_ptr const tinfop=tinfo->next;

 if (tinfop==NULL) {
  ERREXIT(tinfo->emethods, "pop: At least two data sets must be available in the linked list.\n");
 }
 tinfop->previous=NULL;	/* tinfop will be first */
 if (tinfop->next!=NULL) {
  /* Note that we set this to tinfo, not tinfop, because we will copy tinfop to tinfo below! */
  tinfop->next->previous=tinfo;
 }
 deepfree_tinfo(tinfo);
 free_pointer((void **)&tinfo->tsdata);
 /* This trick now avoids tinfo itself being freed, since that is often static */
 memcpy(tinfo, tinfop, sizeof(struct transform_info_struct));
 free((void *)tinfop);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  pop_exit(transform_info_ptr tinfo) {*/
METHODDEF void
pop_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_pop(transform_info_ptr tinfo) {*/
GLOBAL void
select_pop(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &pop_init;
 tinfo->methods->transform= &pop;
 tinfo->methods->transform_exit= &pop_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="pop";
 tinfo->methods->method_description=
  "Transform method to free the first data set and replace it\n"
  "by the second in the linked list of data sets.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
 tinfo->methods->argument_descriptors=NULL;
}
/*}}}  */
