/*
 * Copyright (C) 2001,2002,2010,2013-2015 Bernd Feige
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

/*{{{  push_init(transform_info_ptr tinfo) {*/
METHODDEF void
push_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  push(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
push(transform_info_ptr tinfo) {
 transform_info_ptr const newtinfo=(transform_info_ptr)calloc(1,sizeof(struct transform_info_struct));
 DATATYPE * const newtsdata=(DATATYPE *)malloc(tinfo->length_of_output_region*sizeof(DATATYPE));

 if (newtinfo==NULL) {
  ERREXIT(tinfo->emethods, "push: Error malloc'ing tinfo struct\n");
 }
 if (newtsdata==NULL) {
  ERREXIT(tinfo->emethods, "push: Error malloc'ing data\n");
 }
 memcpy(newtsdata, tinfo->tsdata, tinfo->length_of_output_region*sizeof(DATATYPE));
 deepcopy_tinfo(newtinfo, tinfo);
 newtinfo->tsdata=newtsdata;

 /* Insert newtinfo just after tinfo */
 newtinfo->next=tinfo->next;
 newtinfo->previous=tinfo;
 tinfo->next=newtinfo;
 if (newtinfo->next!=NULL) newtinfo->next->previous=newtinfo;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  push_exit(transform_info_ptr tinfo) {*/
METHODDEF void
push_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_push(transform_info_ptr tinfo) {*/
GLOBAL void
select_push(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &push_init;
 tinfo->methods->transform= &push;
 tinfo->methods->transform_exit= &push_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="push";
 tinfo->methods->method_description=
  "Transform method to duplicate the first data set and store\n"
  "it as the second in the linked list of data sets.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
 tinfo->methods->argument_descriptors=NULL;
}
/*}}}  */
