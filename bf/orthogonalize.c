/*
 * Copyright (C) 2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * orthogonalize transform method to successively orthogonalize the input
 * vectors. The first vector remains unmodified.
 * 						-- Bernd Feige 6.08.2010
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_ORTHOGONALIZE_MAPS=0,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Orthogonalize maps instead of time courses", "m", FALSE, NULL},
};

/*{{{  orthogonalize_init(transform_info_ptr tinfo) {*/
METHODDEF void
orthogonalize_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  orthogonalize(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
orthogonalize(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 array myarray;

 tinfo_array(tinfo, &myarray);
 if (args[ARGS_ORTHOGONALIZE_MAPS].is_set) {
  array_transpose(&myarray);
 }
 array_make_orthogonal(&myarray);
 if (myarray.message==ARRAY_ERROR) {
  ERREXIT(tinfo->emethods, "orthogonalize: Error orthogonalizing vectors\n");
 }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  orthogonalize_exit(transform_info_ptr tinfo) {*/
METHODDEF void
orthogonalize_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_orthogonalize(transform_info_ptr tinfo) {*/
GLOBAL void
select_orthogonalize(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &orthogonalize_init;
 tinfo->methods->transform= &orthogonalize;
 tinfo->methods->transform_exit= &orthogonalize_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="orthogonalize";
 tinfo->methods->method_description=
  "Transform method orthogonalizing time courses (default)\n"
  "or maps\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
