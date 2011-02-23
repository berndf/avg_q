/*
 * Copyright (C) 1995-1998,2001,2003 Bernd Feige
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
/* expand_channel_list.c was derived from the channel selection part of
 * mfx_file. It takes a channel selection string and returns a malloc'ed
 * list of channel_index+1 values terminated by 0. If no channel was selected,
 * a NULL pointer is returned.		-- Bernd Feige 29.12.1995 */
 
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"

enum {
 EXPAND_NOERR=0,
 EXPAND_NONUM,
 EXPAND_INTERVAL,
 EXPAND_UNTERMINATED
} expand_lasterr;
LOCAL char *expand_errors[]={
 "No error.",
 "Expansion list member doesn't end with number",
 "Empty expansion interval",
 "Expansion interval unterminated"
};

/*{{{}}}*/
/*{{{  Name expansion*/
/*
 * Internal startexpand(inlist) initializes the expansion of a 'list'
 * string like "FG12-FG15, tt, TRIGGER, t2-t5" into "FG12", "FG13", ...
 * The expanded strings are returned with each call to nextexpand.
 * nextexpand returns NULL if no further elements are available.
 */
static struct {
 char const *currpos;	/* The interpreter position in the input sring */
 int  endcount;
 int  count;
 char *numpos;	/* The position at which numbers are printed in outbuf */
 char outbuf[80];
} expandinfo;

static void
startexpand(char const * inlist) {
 expandinfo.currpos=inlist;
 expandinfo.endcount=0;	/* This signals that no expansion occurs now */
}
static char*
nextexpand(void) {
 char const *current;
 char *tobuf;

 if (expandinfo.endcount) {	/* Expansion in progress */
  sprintf(expandinfo.numpos, "%d", expandinfo.count);
  if (expandinfo.count++==expandinfo.endcount) expandinfo.endcount=0;
  expand_lasterr=EXPAND_NOERR;
  return expandinfo.outbuf;
 }
 /* Normal reading: */
 current=expandinfo.currpos;
 expandinfo.count=0;
 while (1) {
  /* Skip white space, commas and hyphens we were possibly left on */
  while (*current && strchr(" \t,-", *current)) current++;
  if (*current=='\0') {
   /* expandinfo.count is set if the starting value of a range was read */
   expand_lasterr=(expandinfo.count==0 ? EXPAND_NOERR : EXPAND_UNTERMINATED);
   return (char*)NULL;
  }
  /* Now copy characters to outbuf up to next delimiter */
  {int escaped=FALSE;
  for (tobuf=expandinfo.outbuf, expandinfo.numpos=NULL; 
       (escaped==TRUE || strchr("-,", *current)==NULL)
	&& *current!='\0'; current++) {
   if (escaped==FALSE && *current=='\\') {
    escaped=TRUE;
    continue;
   }
   if (*current>='0' && *current<='9') {
    if (expandinfo.numpos==NULL) {
     expandinfo.numpos=tobuf;
    }
   } else {
    expandinfo.numpos=NULL;
   }
   *tobuf++ = *current;
   escaped=FALSE;
  }
  }
  /* Terminate tobuf string and decide what to do */
  *tobuf='\0';
  expandinfo.currpos=current;
  switch (*current) {
   case '\0':
   case ',':
    /* Was there a range started ? If not, just return outbuf */
    if (expandinfo.count==0) {
     expand_lasterr=EXPAND_NOERR;
     return expandinfo.outbuf;
    }
    /* Else: End the specified interval */
    if (expandinfo.numpos==NULL) {
     expand_lasterr=EXPAND_NONUM;
     return (char*)NULL;
    }
    expandinfo.endcount=atoi(expandinfo.numpos);
    if (expandinfo.endcount<expandinfo.count) {
     expand_lasterr=EXPAND_INTERVAL;
     return (char*)NULL;
    }
    return nextexpand();
    break;
   case '-':	/* Start of an interval */
    if (expandinfo.numpos==NULL) {
     expand_lasterr=EXPAND_NONUM;
     return (char*)NULL;
    }
    expandinfo.count=atoi(expandinfo.numpos);
    break;
  }
 }
}
/*}}}  */

/*
 * int *expand_channel_list(transform_info_ptr tinfo, char *channelnames)
 *  is used to select the channels to be read by channel names
 *  channelnames is a string of the form "A2-A5,A9, t3"
 *  As a special request, "ALL" means that all channels in file get selected.
 */

GLOBAL int *
expand_channel_list(transform_info_ptr tinfo, char const *channelnames) {
 int const nr_of_channels=tinfo->nr_of_channels;
 char* channelname;
 int *selected_channels;
 int argcnt, channelnr;
 Bool ignoreNA=FALSE, invertselection=FALSE;

 if (nr_of_channels==0 || tinfo->channelnames==NULL) return NULL; /* No element was selected */

 /*{{{  Handle "ALL" request*/
 if (strcmp(channelnames, "ALL")==0) {
  if ((selected_channels=(int *)malloc((nr_of_channels+1)*sizeof(int)))==NULL) {
   ERREXIT(tinfo->emethods, "expand_channel_list: Error allocating channel list\n");
  }
  for (argcnt=0; argcnt<nr_of_channels; argcnt++) {
   selected_channels[argcnt]=argcnt+1;
  }
  selected_channels[argcnt]=0;
  return selected_channels;
 }
 /*}}}  */

 while (TRUE) {
  switch(*channelnames) {
   case '?':
    /* If names string begins with this char, a missing channel name
     * is not a fatal condition (ignore if not available) */
    ignoreNA=TRUE;
    channelnames++;
    continue;
   case '!':
    /* If names string begins with this char, all channels EXCEPT those 
     * mentioned are selected */
    invertselection=TRUE;
    channelnames++;
    continue;
   default:
    break;
  }
  break;
 }

 if (invertselection) {
  Bool *channelmap=(Bool *)calloc(nr_of_channels, sizeof(Bool));
  if (channelmap==NULL) {
   ERREXIT(tinfo->emethods, "expand_channel_list: Error allocating channel map\n");
  }
  /* We create a channel map where we mark channels for deletion and
   * then return a list of the remaining */
  startexpand(channelnames);
  while ((channelname=nextexpand())!=(char *)NULL) {
   if ((channelnr=find_channel_number(tinfo, channelname))<0) {
    if (!ignoreNA) {
     ERREXIT1(tinfo->emethods, "expand_channel_list: Unknown channel name >%s<\n", MSGPARM(channelname));
    }
   } else {
    channelmap[channelnr]=TRUE;
   }
  }
  if (expand_lasterr!=EXPAND_NOERR) {
   ERREXIT1(tinfo->emethods, "expand_channel_list: %s\n", MSGPARM(expand_errors[expand_lasterr]));
  }
  argcnt=0;
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (!channelmap[channelnr]) argcnt++;
  }
  if (argcnt==0) return NULL;	/* NULL pointer if no element was selected */
  if ((selected_channels=(int *)malloc((argcnt+1)*sizeof(int)))==NULL) {
   ERREXIT(tinfo->emethods, "expand_channel_list: Error allocating channel list\n");
  }
  argcnt=0;
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (!channelmap[channelnr]) selected_channels[argcnt++]=channelnr+1;
  }
  free(channelmap);
 } else {

 startexpand(channelnames);	/* First traversal to count selected channels */
 for (argcnt=0; (channelname=nextexpand())!=(char *)NULL; ) {
  Bool found_one=FALSE;
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (strcmp(tinfo->channelnames[channelnr], channelname)==0) {
    argcnt++;
    found_one=TRUE;
   }
  }
  if (!found_one) {
   if (!ignoreNA) {
    ERREXIT1(tinfo->emethods, "expand_channel_list: Unknown channel name >%s<\n", MSGPARM(channelname));
   }
  }
 }
 if (expand_lasterr!=EXPAND_NOERR) {
  ERREXIT1(tinfo->emethods, "expand_channel_list: %s\n", MSGPARM(expand_errors[expand_lasterr]));
 }
 if (argcnt==0) return NULL;	/* NULL pointer if no element was selected */
 if ((selected_channels=(int *)malloc((argcnt+1)*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "expand_channel_list: Error allocating channel list\n");
 }
 startexpand(channelnames);	/* Now read the channel numbers */
 for (argcnt=0; (channelname=nextexpand())!=(char *)NULL; ) {
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (strcmp(tinfo->channelnames[channelnr], channelname)==0) {
    selected_channels[argcnt++]=channelnr+1;
   }
  }
 }

 }
 selected_channels[argcnt]=0;
 return selected_channels;
}

/* Utility function to query whether a channel is mentioned in the list */
GLOBAL Bool
is_in_channellist(int val, int *list) {
 if (list==NULL) return FALSE;
 while (*list!=0) {
  if (val== *list) return TRUE;
  list++;
 }
 return FALSE;
}
