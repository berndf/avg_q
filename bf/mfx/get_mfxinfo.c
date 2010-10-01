/*
 * Copyright (C) 1996-1998,2002,2004,2008 Bernd Feige
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

/*
 * get_mfxinfo.c module to transfer header data from an open
 * mfx file to a transform_info_struct structure
 *	-- Bernd Feige 19.01.1993
 */

#include <stdlib.h>
#include <string.h>
#include "mfx_file.h"
#include "transform.h"
#include "bf.h"

/*
 * get_mfxinfo returns a negative value on error.
 */

GLOBAL int 
get_mfxinfo(MFX_FILE *mfxfile, transform_info_ptr tinfo) {
 int i, j;
 char *namebuf, *inbuf;

 tinfo->nr_of_channels=mfxfile->nr_of_channels_selected;
 tinfo->sfreq=1.0/mfxfile->fileheader.sample_period;

 /*{{{}}}*/
 /*{{{  Allocate channelnames memory*/
 if ((tinfo->channelnames=(char **)calloc(tinfo->nr_of_channels, sizeof(char *)))==NULL) {
  return -1;
 }
 /*}}}  */

 /*{{{  Allocate namebuf*/
 /* First, count how many bytes are necessary to hold the names strings */
 for (i=j=0; i<tinfo->nr_of_channels; i++) {
  j+=strlen(mfx_getchannelname(mfxfile, i))+1;
 }
 if ((namebuf=(char *)malloc(j))==NULL) {
  free(tinfo->channelnames);
  tinfo->channelnames=NULL;
  return -2;
 }
 /*}}}  */

 /*{{{  Transfer channel names*/
 for (i=0, inbuf=namebuf; i<tinfo->nr_of_channels; i++) {
  strcpy(tinfo->channelnames[i]=inbuf, mfx_getchannelname(mfxfile, i));
  inbuf+=strlen(inbuf)+1;
 }
 /*}}}  */

 /*{{{  Allocate probepos memory*/
 if ((tinfo->probepos=(double *)malloc(3*sizeof(double)*tinfo->nr_of_channels))==NULL) {
  free(tinfo->channelnames);
  return -3;
 }
 /*}}}  */
 /*{{{  Copy probe positions*/
 for (i=0; i<tinfo->nr_of_channels; i++) {
  for (j=0; j<3; j++) {
   tinfo->probepos[3*i+j]=mfxfile->channelheaders[mfxfile->selected_channels[i]-1].position[j];
  }
 }
 /*}}}  */

 /*{{{  Build tinfo->comment*/
 namebuf=mfxfile->fileheader.file_descriptor;
 if (namebuf==NULL) {
  tinfo->comment=NULL;
 } else {
  if ((tinfo->comment=(char *)malloc(strlen(namebuf)+1))==NULL) {
   free(tinfo->channelnames);
   return -4;
  }
  strcpy(tinfo->comment, namebuf);
 }
 /*}}}  */

 /* Since some xdata might have been built by another method, we must invalidate it explicitly */
 free_pointer((void **)&tinfo->xdata);
 tinfo->xchannelname=NULL;

 return 0;
}

GLOBAL void 
get_mfxinfo_free(transform_info_ptr tinfo) {
 /*{{{  Free allocated memory*/
 if (tinfo->channelnames!=NULL) {
  free_pointer((void **)&tinfo->channelnames[0]);
  free_pointer((void **)&tinfo->channelnames);
 }
 free_pointer((void **)&tinfo->probepos);
 free_pointer((void **)&tinfo->comment);
 /*}}}  */
}
