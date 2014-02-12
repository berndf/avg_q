/*
 * Copyright (C) 2002,2004 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * swap_ix is a simple transform method swapping items and x data.
 * Note that xdata is, of course, lost.
 * 						-- Bernd Feige 2.07.2002
 */
/*}}}  */

/*{{{  #includes*/
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  swap_ix_init(transform_info_ptr tinfo) {*/
METHODDEF void
swap_ix_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  swap_ix(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
swap_ix(transform_info_ptr tinfo) {
 int const olditemsize=tinfo->itemsize;

 if (tinfo->data_type==FREQ_DATA) {
  if (tinfo->nrofshifts>1) {
   ERREXIT(tinfo->emethods, "swap_ix: FREQ_DATA with nrofshifts>1 not (yet?) implemented.\n");
  }
  tinfo->nr_of_points=tinfo->nroffreq;
  tinfo->data_type=TIME_DATA;
 }
 free_pointer((void **)&tinfo->xdata);
 tinfo->xchannelname=NULL;
 nonmultiplexed(tinfo);
 
 tinfo->itemsize=tinfo->nr_of_points;
 tinfo->nr_of_points=olditemsize;
 tinfo->beforetrig=0; tinfo->aftertrig=tinfo->nr_of_points;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  swap_ix_exit(transform_info_ptr tinfo) {*/
METHODDEF void
swap_ix_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_swap_ix(transform_info_ptr tinfo) {*/
GLOBAL void
select_swap_ix(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &swap_ix_init;
 tinfo->methods->transform= &swap_ix;
 tinfo->methods->transform_exit= &swap_ix_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="swap_ix";
 tinfo->methods->method_description=
  "Transform method to swap item and x axes.\n"
  " Note that x data will be lost.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
