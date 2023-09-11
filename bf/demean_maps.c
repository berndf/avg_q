/*
 * Copyright (C) 1996,1998,1999,2001 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * demean_maps.c method to subtract the map mean from each incoming map
 *	-- Bernd Feige 09.11.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  demean_maps_init(transform_info_ptr tinfo)*/
METHODDEF void
demean_maps_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  demean_maps(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
demean_maps(transform_info_ptr tinfo) {
 int itempart;
 transform_info_ptr const tinfoptr=tinfo;
 array indata;

  tinfo_array(tinfoptr, &indata);
  array_transpose(&indata);	/* Vector=map */
  for (itempart=0; itempart<tinfoptr->itemsize-tinfoptr->leaveright; itempart++) {
   array_use_item(&indata, itempart);
   do {
    DATATYPE mean=array_mean(&indata);
    array_previousvector(&indata);
    do {
     array_write(&indata, READ_ELEMENT(&indata)-mean);
    } while (indata.message==ARRAY_CONTINUE);
   } while (indata.message!=ARRAY_ENDOFSCAN);
  }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  demean_maps_exit(transform_info_ptr tinfo)*/
METHODDEF void
demean_maps_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_demean_maps(transform_info_ptr tinfo)*/
GLOBAL void
select_demean_maps(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &demean_maps_init;
 tinfo->methods->transform= &demean_maps;
 tinfo->methods->transform_exit= &demean_maps_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="demean_maps";
 tinfo->methods->method_description=
  "Transform method to subtract the map mean from each incoming map\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
