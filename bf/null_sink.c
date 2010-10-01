/*
 * Copyright (C) 1996,1998,1999 Bernd Feige
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
 * null_sink -- A tiny dummy 'collect' method that serves to close an
 * iterated queue, freeing the memory of the last epoch.
 * 						-- Bernd Feige 22.10.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  null_sink_init(transform_info_ptr tinfo) {*/
METHODDEF void
null_sink_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  null_sink(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
null_sink(transform_info_ptr tinfo) {
 free_tinfo(tinfo);
 /* Reject all epochs */
 return (DATATYPE *)NULL;
}
/*}}}  */

/*{{{  null_sink_exit(transform_info_ptr tinfo) {*/
METHODDEF void
null_sink_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_null_sink(transform_info_ptr tinfo) {*/
GLOBAL void
select_null_sink(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &null_sink_init;
 tinfo->methods->transform= &null_sink;
 tinfo->methods->transform_exit= &null_sink_exit;
 tinfo->methods->method_type=COLLECT_METHOD;
 tinfo->methods->method_name="null_sink";
 tinfo->methods->method_description=
  "Collect method to just throw away each incoming epoch and close the\n"
  " epoch loop, eg if no final output of the queue is needed\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
