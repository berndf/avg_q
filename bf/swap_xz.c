/*
 * Copyright (C) 1993,1994,1996-1999,2003,2004,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */

/*
 * swap_xz global function to transform a linked tinfo chain into another
 * in which the former x coordinate is the z coordinate and vice versa.
 * The original data is NOT freed; in fact, those buffers of the old chain 
 * that can be used by the new are addressed by the new tinfo's (eg the
 * sensor names and positions).
 *	-- Bernd Feige 26.04.1993
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

GLOBAL transform_info_ptr
swap_xz(transform_info_ptr tinfo) {
 transform_info_ptr newtinfo, tinfoptr;
 DATATYPE *xdata, *ydata;
 int i, xsize, channel, point, itempart;
 int nr_of_channels=0, nr_of_points=0, itemsize=0;
 char *const newxchannelname=(tinfo->z_label==NULL ? (char *)"Dataset" : tinfo->z_label);

 /* Count how many tinfo structs there are in the input (our x values),
  * ensuring that the geometry of all data sets is identical: */
 for (xsize=0, tinfoptr=tinfo; tinfoptr!=NULL; xsize++, tinfoptr=tinfoptr->next) {
  if (nr_of_channels==0) {
   nr_of_channels=tinfoptr->nr_of_channels;
  } else if (tinfoptr->nr_of_channels!=nr_of_channels) {
   ERREXIT(tinfo->emethods, "swap_xz: varying nr_of_channels!\n");
  }
  if (nr_of_points==0) {
   nr_of_points=tinfoptr->nr_of_points;
  } else if (tinfoptr->nr_of_points!=nr_of_points) {
   ERREXIT(tinfo->emethods, "swap_xz: varying nr_of_points!\n");
  }
  if (itemsize==0) {
   itemsize=tinfoptr->itemsize;
  } else if (tinfoptr->itemsize!=itemsize) {
   ERREXIT(tinfo->emethods, "swap_xz: varying itemsize!\n");
  }
 }
 /*{{{  Allocate the memory areas*/
 if ((newtinfo=(struct transform_info_struct *)malloc(tinfo->nr_of_points*sizeof(struct transform_info_struct)))==NULL) {
  ERREXIT(tinfo->emethods, "swap_xz: Error malloc'ing tinfo structs\n");
 }
 if ((xdata=(DATATYPE *)malloc(xsize*sizeof(DATATYPE)+strlen(newxchannelname)+1))==NULL) {
  ERREXIT(tinfo->emethods, "swap_xz: Error malloc'ing xdata array\n");
 }
 /* Allocate the y data arrays for all tinfo->nr_of_points new tinfo's */
 if ((ydata=(DATATYPE *)malloc(xsize*tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "swap_xz: Error malloc'ing ydata array\n");
 }
 /*}}}  */

 /*{{{  Copy the xdata and ydata values*/
 for (i=0, tinfoptr=tinfo; i<xsize; i++) {
  nonmultiplexed(tinfoptr);
  xdata[i]=(tinfo->z_label==NULL ? i : tinfoptr->z_value);
  for (channel=0; channel<tinfoptr->nr_of_channels; channel++) {
   for (point=0; point<tinfoptr->nr_of_points; point++) {
    for (itempart=0; itempart<tinfoptr->itemsize; itempart++) {
     ydata[(point*tinfo->nr_of_channels*xsize+channel*xsize+i)*tinfoptr->itemsize+itempart]=tinfoptr->tsdata[(channel*tinfoptr->nr_of_points+point)*tinfo->itemsize+itempart];
    }
   }
  }
  tinfoptr=tinfoptr->next;
 }
 strcpy((char *)(xdata+xsize),newxchannelname);
 /*}}}  */

 /*{{{  Set the contents of the new tinfo chain*/
 for (i=0; i<tinfo->nr_of_points; i++) {
  memcpy(newtinfo+i, tinfo, sizeof(struct transform_info_struct));
  newtinfo[i].multiplexed=FALSE;
  newtinfo[i].xchannelname=(char *)(xdata+xsize);
  newtinfo[i].xdata=xdata;
  newtinfo[i].nr_of_points=xsize;
  newtinfo[i].z_label=tinfo->xchannelname;
  newtinfo[i].z_value=tinfo->xdata[i];
  newtinfo[i].tsdata=ydata+i*tinfo->nr_of_channels*tinfo->itemsize*xsize;
  newtinfo[i].previous=newtinfo+i-1;
  newtinfo[i].next=newtinfo+i+1;
 }
 newtinfo[0].previous=NULL;
 newtinfo[tinfo->nr_of_points-1].next=NULL;
 /*}}}  */

 return newtinfo;
}

GLOBAL void
swap_xz_free(transform_info_ptr newtinfo) {
 if (newtinfo!=NULL) {
  free_pointer((void **)&newtinfo->tsdata);
  free_pointer((void **)&newtinfo->xdata);
  free((void *)newtinfo);
 }
}
