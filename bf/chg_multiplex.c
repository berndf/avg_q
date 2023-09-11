/*
 * Copyright (C) 1993,1994,2002,2003,2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * chg_multiplex.c provides the global functions to change the data organisation
 * in memory between multiplexed [column, row] and non-multiplexed [row, column]
 * form. rows=nr_of_channels; columns=nr_of_points   -- Bernd Feige 15.01.1993
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*
 * If IN_PLACE is defined, no additional memory is used and the ordering
 * is done in place by SWAPs. Else, a buffer of the same size as the data
 * buffer is user for reordering. In both cases, though, the resulting 
 * data is returned in the same memory the original data was in.
 */

/* #undef IN_PLACE Can be set from outside */

/*
 * We determine that, in the target order for chg_multiplex, rows be in one 
 * piece in memory;
 * To extract one specific column, you have to skip (columns) bytes 
 * to get the next of the (rows) values for that column.
 *
 * chg_multiplex returns 1 on success, 0 on error
 */

#ifdef IN_PLACE
/*{{{  */
/*
 * The principle of in-place reordering by swapping is crude here:
 * The elements are processed by memory position; For each
 * storage position, the question is where to find the element which
 * should be in this position. For the first (columns) values, the
 * answer is easy: simply convert the 'new' index (row*columns+column)
 * into the 'old' index (column*rows+row). But for higher values, the
 * corresponding 'old' values may already have been swapped, perhaps
 * multiply. The procedure now is just to keep calculating 'old' indices,
 * always taking the last 'old' as 'new' index, until the resulting
 * position is above the region which has already been ordered.
 * In fact, this is nothing more than to follow the 'swap path' of a value.
 */

#define SWAP(a,b) tmp=(a); (a)=(b); (b)=tmp

LOCAL int
chg_multiplex(DATATYPE *data, int rows, int columns, int itemsize) {
 int newpos, where, itempart;
 DATATYPE tmp;
 div_t quotient;

 /* The first element is at the right place, and the last one too,
  * so the last two never have to be swapped */
 for (newpos=1; newpos<rows*columns-2; newpos++) {
  where=newpos;
  do {
   quotient=div(where, columns);
   where=quotient.rem*rows+quotient.quot;
  } while (where<newpos);
  if (where!=newpos) {
   for (itempart=0; itempart<itemsize; itempart++) {
    SWAP(data[where*itemsize+itempart], data[newpos*itemsize+itempart]);
   }
  }
 }
 return 1;
}
/*}}}  */
#else
/*{{{  */
static int
chg_multiplex(DATATYPE *data, int rows, int columns, int itemsize) {
 int row, col, itempart;
 const size_t datasize=rows*columns*itemsize*sizeof(DATATYPE);
 DATATYPE *ndata;

 if ((ndata=(DATATYPE *)malloc(datasize))==NULL) return 0;
 for (col=0; col<columns; col++) {
  for (row=0; row<rows; row++) {
   for (itempart=0; itempart<itemsize; itempart++) {
    ndata[(row*columns+col)*itemsize+itempart]=data[(col*rows+row)*itemsize+itempart];
   }
  }
 }
 memcpy(data, ndata, datasize);
 free(ndata);
 return 1;
}
/*}}}  */
#endif

/*{{{  multiplexed(transform_info_ptr tinfo)*/
GLOBAL void
multiplexed(transform_info_ptr tinfo) {
 if (tinfo->data_type==FREQ_DATA) {
  ERREXIT(tinfo->emethods, "multiplexed: FREQ_DATA is not supported!\n");
 }
 if (!tinfo->multiplexed) {
  if (tinfo->nr_of_points>1 && tinfo->nr_of_channels>1) {
   if (!chg_multiplex(tinfo->tsdata, tinfo->nr_of_points, tinfo->nr_of_channels, tinfo->itemsize)) {
    ERREXIT(tinfo->emethods, "multiplexed: chg_multiplex failed !\n");
   }
  }
  tinfo->multiplexed=TRUE;
 }
}
/*}}}  */

/*{{{  nonmultiplexed(transform_info_ptr tinfo)*/
GLOBAL void
nonmultiplexed(transform_info_ptr tinfo) {
 if (tinfo->data_type==FREQ_DATA) {
  ERREXIT(tinfo->emethods, "nonmultiplexed: FREQ_DATA is not supported!\n");
 }
 if (tinfo->multiplexed) {
  if (tinfo->nr_of_points>1 && tinfo->nr_of_channels>1) {
   if (!chg_multiplex(tinfo->tsdata, tinfo->nr_of_channels, tinfo->nr_of_points, tinfo->itemsize)) {
    ERREXIT(tinfo->emethods, "nonmultiplexed: chg_multiplex failed !\n");
   }
  }
  tinfo->multiplexed=FALSE;
 }
}
/*}}}  */
