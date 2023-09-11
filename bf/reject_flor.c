/*
 * Copyright (C) 1996-1999,2001 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * reject_flor simple method to include into an average() process chain
 * (usually just after the get_epoch method) to reject epochs that contain
 * the error marker used by the Tuebingen group (Herta Flor), ie +-9999.0.
 * 						-- Bernd Feige 18.05.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  reject_flor_init(transform_info_ptr tinfo) {*/
METHODDEF void
reject_flor_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  reject_flor(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
reject_flor(transform_info_ptr tinfo) {
 int point, channel;
 DATATYPE item;

 multiplexed(tinfo);
 for (point=0; point<tinfo->nr_of_points; point++) {
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   item=tinfo->tsdata[point*tinfo->nr_of_channels+channel];
   if (item> 9998.0 || item< -9998.0) {
    TRACEMS2(tinfo->emethods, 1, "reject_flor: rejected at %d on channel %d\n", MSGPARM(tinfo->accepted_epochs+tinfo->rejected_epochs+1), MSGPARM(channel+1));
    return NULL;
   }
  }
 }
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  reject_flor_exit(transform_info_ptr tinfo) {*/
METHODDEF void
reject_flor_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_reject_flor(transform_info_ptr tinfo) {*/
GLOBAL void
select_reject_flor(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &reject_flor_init;
 tinfo->methods->transform= &reject_flor;
 tinfo->methods->transform_exit= &reject_flor_exit;
 tinfo->methods->method_type=REJECT_METHOD;
 tinfo->methods->method_name="reject_flor";
 tinfo->methods->method_description=
  "Rejection method to reject any epoch containing the Tuebingen error"
  "marker 9999\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
