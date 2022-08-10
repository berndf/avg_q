/*
 * Copyright (C) 2003,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * swap_ic is a simple transform method swapping items and channels.
 * Note that channel names are, of course, lost.
 * 						-- Bernd Feige 9.05.2003
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  swap_ic_init(transform_info_ptr tinfo) {*/
METHODDEF void
swap_ic_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  swap_ic(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
swap_ic(transform_info_ptr tinfo) {
 int const olditemsize=tinfo->itemsize;

 if (tinfo->data_type==FREQ_DATA) {
  if (tinfo->nrofshifts>1) {
   ERREXIT(tinfo->emethods, "swap_ic: FREQ_DATA with nrofshifts>1 not (yet?) implemented.\n");
  }
  tinfo->nr_of_points=tinfo->nroffreq;
  tinfo->data_type=TIME_DATA;
 }
 if (tinfo->itemsize>1) {
  if (tinfo->nr_of_channels>1) {
   /* itemsize>1 and nr_of_channels>1: This needs reordering in memory since items must be adjacent. */
   int const itemsize=tinfo->itemsize;
   size_t const datasize=tinfo->length_of_output_region*sizeof(DATATYPE);
   int row, col, itempart;
   DATATYPE *const data=tinfo->tsdata;
   DATATYPE *ndata;

   if ((ndata=(DATATYPE *)malloc(datasize))==NULL) {
    ERREXIT(tinfo->emethods, "swap_ic: Error allocating temporary memory.\n");
    return 0;
   }

   /* As in chg_multiplex.c, rows are defined as adjacent (running fastest).
    * We replicate the loops so that we don't have to do the test for each value:
    * The target address is different each time to ensure that what previously
    * were channels will be adjacent afterwards. */
   if (tinfo->multiplexed) {
    int const rows=tinfo->nr_of_channels;
    int const cols=tinfo->nr_of_points;
    for (col=0; col<cols; col++) {
     for (row=0; row<rows; row++) {
      for (itempart=0; itempart<itemsize; itempart++) {
       ndata[(col*itemsize+itempart)*rows+row]=data[(col*rows+row)*itemsize+itempart];
      }
     }
    }
   } else {
    int const rows=tinfo->nr_of_points;
    int const cols=tinfo->nr_of_channels;
    for (col=0; col<cols; col++) {
     for (row=0; row<rows; row++) {
      for (itempart=0; itempart<itemsize; itempart++) {
       ndata[(row*itemsize+itempart)*cols+col]=data[(col*rows+row)*itemsize+itempart];
      }
     }
    }
   }
   memcpy(data, ndata, datasize);
   free(ndata);
   /* In each of the above cases, the old 'item' and to-be 'channel' was fastest-varying,
    * after the old 'channel' and to-be 'item' axis of course... */
   tinfo->multiplexed=TRUE;
  } else {
   /* Output is nr_of_channels>1 but itemsize==1 */
   tinfo->multiplexed=TRUE;	/* Items were of course adjacent... */
  }
 } else {
  /* itemsize==1 */
  if (tinfo->nr_of_channels>1) {
   /* Output is itemsize>1 but nr_of_channels==1 */
   /* Make channels adjacent in memory so that they can simply become items: */
   multiplexed(tinfo);
  }
 }
 
 tinfo->itemsize=tinfo->nr_of_channels;
 tinfo->nr_of_channels=olditemsize;

 free_pointer((void **)&tinfo->channelnames);
 free_pointer((void **)&tinfo->probepos);
 create_channelgrid(tinfo);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  swap_ic_exit(transform_info_ptr tinfo) {*/
METHODDEF void
swap_ic_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_swap_ic(transform_info_ptr tinfo) {*/
GLOBAL void
select_swap_ic(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &swap_ic_init;
 tinfo->methods->transform= &swap_ic;
 tinfo->methods->transform_exit= &swap_ic_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="swap_ic";
 tinfo->methods->method_description=
  "Transform method to swap items and channels.\n"
  " Note that channel name data will be lost.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
