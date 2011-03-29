/*
 * Copyright (C) 1996-1999,2001,2003-2008,2010-2011 Bernd Feige
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
 * add_channels.c method to add channels - or points - from a readasc file
 *	-- Bernd Feige 9.09.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

/*{{{  Arguments and local storage*/
LOCAL const char *const type_choice[]={
 "-c",
 "-p",
 "-i",
 "-l",
 NULL
};
enum ARGS_ENUM {
 ARGS_CLOSE=0, 
 ARGS_EVERY, 
 ARGS_BYNAME,
 ARGS_FROMEPOCH, 
 ARGS_TYPE, 
 ARGS_ADD_CHANNELSFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Close and reopen the file for each epoch", "C", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Read every epoch in add_channels_file in turn", "e", TRUE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Only add the named channels", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Start with epoch number fromepoch (>=1)", "f", 1, NULL},
 {T_ARGS_TAKES_SELECTION, "Add channels, points, items or link epoch", " ", 0, type_choice},
 {T_ARGS_TAKES_FILENAME, "add_channels_file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"}
};

struct add_channels_storage {
 enum add_channels_types type;
 struct transform_info_struct side_tinfo;
 struct transform_methods_struct side_method;
 growing_buf side_argbuf;
};
/*}}}  */

/*{{{  add_channels_init(transform_info_ptr tinfo)*/
METHODDEF void
add_channels_init(transform_info_ptr tinfo) {
 struct add_channels_storage *local_arg=(struct add_channels_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;

 side_tinfo->emethods=tinfo->emethods;
 side_tinfo->methods= &local_arg->side_method;
 select_readasc(side_tinfo);
 growing_buf_init(&local_arg->side_argbuf);
 growing_buf_allocate(&local_arg->side_argbuf, 0);
 if (args[ARGS_CLOSE].is_set) {
  growing_buf_appendstring(&local_arg->side_argbuf, "-c ");
 }
 if (args[ARGS_FROMEPOCH].is_set) {
#define BUFFER_SIZE 80
  char buffer[BUFFER_SIZE];
  snprintf(buffer, BUFFER_SIZE, "-f %ld ", args[ARGS_FROMEPOCH].arg.i);
  growing_buf_appendstring(&local_arg->side_argbuf, buffer);
 }
 growing_buf_appendstring(&local_arg->side_argbuf, args[ARGS_ADD_CHANNELSFILE].arg.s);
 if (args[ARGS_TYPE].is_set) {
  local_arg->type=(enum add_channels_types)args[ARGS_TYPE].arg.i;
 } else {
  local_arg->type=ADD_CHANNELS;
 }

 if (!local_arg->side_argbuf.can_be_freed || !setup_method(side_tinfo, &local_arg->side_argbuf)) {
  ERREXIT(tinfo->emethods, "add_channels_init: Error setting readasc arguments.\n");
 }

 (*side_tinfo->methods->transform_init)(side_tinfo);

 side_tinfo->tsdata=NULL;	/* We still have to fetch the data... */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  GLOBAL DATATYPE add_channels_or_points(transform_info_ptr const to_tinfo, transform_info_ptr const from_tinfo, int * const channellist, enum add_channels_types const type, Bool const zero_newdata) {*/
/* This global function copies the contents of to_tinfo, appending the selected
 * channels or points from from_tinfo. 
 * If the flag zero_newdata is TRUE, the data itself is not copied but zeroed out, 
 * while the channel information is copied.
 * As required by our conventions for transform methods, to_tinfo->tsdata is not 
 * replaced by the new version but the new tsdata is returned instead; but all 
 * other arrays are filled out appropriately. */
GLOBAL DATATYPE *
add_channels_or_points(transform_info_ptr const to_tinfo, transform_info_ptr const from_tinfo, int * const channellist, enum add_channels_types const type, Bool const zero_newdata) {
 int shift, channel, itempart;
 int channels_in_fromtinfo=from_tinfo->nr_of_channels;
 int const from_nrofshifts=(from_tinfo->data_type==TIME_DATA ? 1 : from_tinfo->nrofshifts);
 int const to_nrofshifts=(to_tinfo->data_type==TIME_DATA ? 1 : to_tinfo->nrofshifts);
 int new_nr_of_channels, new_nr_of_points, new_itemsize;
 array myarray, sidearray, newarray;
 struct transform_info_struct newtinfo;

 /* Below, nr_of_points is used for calculations, but tinfo_array knows about FREQ_DATA
  * and will use nroffreq instead, therefore we always keep the two identical below! */
 if (to_tinfo->data_type==FREQ_DATA) {
  from_tinfo->nr_of_points=from_tinfo->nroffreq;
  to_tinfo->nr_of_points=to_tinfo->nroffreq;
 }

 if (channellist!=NULL) {
  for (channels_in_fromtinfo=0; channellist[channels_in_fromtinfo]!=0; channels_in_fromtinfo++);
 }
 new_nr_of_channels=to_tinfo->nr_of_channels+(type==ADD_CHANNELS ? channels_in_fromtinfo : 0);
 new_nr_of_points=to_tinfo->nr_of_points+(type==ADD_POINTS ? from_tinfo->nr_of_points : 0);
 new_itemsize=to_tinfo->itemsize+(type==ADD_ITEMS ? from_tinfo->itemsize : 0);

 if ((type!=ADD_CHANNELS && channels_in_fromtinfo!=to_tinfo->nr_of_channels) ||
     (type!=ADD_POINTS && from_tinfo->nr_of_points!=to_tinfo->nr_of_points) || 
     (!zero_newdata && type!=ADD_ITEMS && to_tinfo->itemsize!=from_tinfo->itemsize) ||
     from_tinfo->data_type!=to_tinfo->data_type ||
     from_nrofshifts!=to_nrofshifts) {
  ERREXIT(to_tinfo->emethods, "add_channels: File sizes do not match.\n");
 }

 /* Actually to_tinfo must contain the resulting tinfo at the end. 
  * newtinfo is only used to prepare the combined data epoch. */
 memcpy(&newtinfo, to_tinfo, sizeof(struct transform_info_struct));

 newtinfo.nr_of_channels=new_nr_of_channels;
 newtinfo.nr_of_points=newtinfo.nroffreq=new_nr_of_points;
 newtinfo.itemsize=new_itemsize;
 newtinfo.length_of_output_region=new_itemsize*to_nrofshifts*new_nr_of_channels*new_nr_of_points;
 if ((newtinfo.tsdata=(DATATYPE *)calloc(newtinfo.length_of_output_region, sizeof(DATATYPE)))==NULL) {
  ERREXIT(to_tinfo->emethods, "add_channels: Error allocating data.\n");
 }

 for (shift=0; shift<from_nrofshifts; shift++) {
 /*{{{  Setup arrays*/
 tinfo_array_setshift(to_tinfo, &myarray, shift);
 tinfo_array_setshift(from_tinfo, &sidearray, shift);
 tinfo_array_setshift(&newtinfo, &newarray, shift);
 if (type==ADD_CHANNELS) {
  array_transpose(&myarray);
  array_transpose(&sidearray);
  array_transpose(&newarray);
 }
 /*}}}  */
 
 for (itempart=0; itempart<new_itemsize; itempart++) {
  /*{{{  Copy elements together*/
  array_use_item(&newarray, itempart);
  if (type==ADD_ITEMS) {
   if (itempart<to_tinfo->itemsize) {
    array_use_item(&myarray, itempart);
   } else {
    array_use_item(&sidearray, itempart-to_tinfo->itemsize);
   }
  } else {
   array_use_item(&myarray, itempart);
   array_use_item(&sidearray, itempart);
  }
  channel=0;
  do {
   /* First copy one vector of the old data: */
   if (type!=ADD_ITEMS || itempart<to_tinfo->itemsize) {
    do {
     array_write(&newarray, array_scan(&myarray));
    } while (myarray.message==ARRAY_CONTINUE);
   }

   /* And now add the new data: */
   if (type==ADD_ITEMS && itempart<to_tinfo->itemsize) {
    /* Nothing needs to be done since the vector is filled already */
   } else if (zero_newdata) {
    array_nextvector(&newarray);
   } else {
    if (type!=ADD_CHANNELS) {
     sidearray.current_vector=(channellist==NULL ? channel : channellist[channel]-1);
     channel++;
    } else {
     channel=0;
    }
    do {
     if (type==ADD_CHANNELS) {
      sidearray.current_element=(channellist==NULL ? channel : channellist[channel]-1);
      channel++;
      if (sidearray.current_element== -1) {
       array_nextvector(&sidearray);
       break;
      }
     }
     array_write(&newarray, array_scan(&sidearray));
    } while (sidearray.message==ARRAY_CONTINUE);
   }
  } while (newarray.message==ARRAY_ENDOFVECTOR);
  /*}}}  */
 }

 switch (type) {
  case ADD_CHANNELS: {
   /*{{{  Copy channel names and positions*/
   char **new_channelnames=(char **)malloc(new_nr_of_channels*sizeof(char *));
   double *new_probepos=(double *)malloc(3*new_nr_of_channels*sizeof(double));

   if (to_tinfo->channelnames!=NULL && from_tinfo->channelnames!=NULL) {
    int new_channel, stringsize=0;
    char *new_channelstrings, *in_strings;
    for (channel=0; channel<to_tinfo->nr_of_channels; channel++) {
     stringsize+=strlen(to_tinfo->channelnames[channel])+1;
    }
    for (channel=0; channel<channels_in_fromtinfo; channel++) {
     stringsize+=strlen(from_tinfo->channelnames[channellist==NULL ? channel : channellist[channel]-1])+1;
    }
    if (new_channelnames==NULL || (in_strings=new_channelstrings=(char *)malloc(stringsize))==NULL) {
     ERREXIT(to_tinfo->emethods, "add_channels: Error allocating channel name memory.\n");
    }
    for (channel=new_channel=0; channel<to_tinfo->nr_of_channels; channel++, new_channel++) {
     new_channelnames[new_channel]=in_strings;
     strcpy(in_strings, to_tinfo->channelnames[channel]);
     in_strings+=strlen(in_strings)+1;
    }
    for (channel=0; channel<channels_in_fromtinfo; channel++, new_channel++) {
     new_channelnames[new_channel]=in_strings;
     strcpy(in_strings, from_tinfo->channelnames[channellist==NULL ? channel : channellist[channel]-1]);
     in_strings+=strlen(in_strings)+1;
    }
    free_pointer((void **)&to_tinfo->channelnames[0]);
    free_pointer((void **)&to_tinfo->channelnames);
    to_tinfo->channelnames=new_channelnames;
   }
   if (to_tinfo->probepos!=NULL && from_tinfo->probepos!=NULL) {
    int new_channel;
    if (new_probepos==NULL) {
     ERREXIT(to_tinfo->emethods, "add_channels: Error allocating probe position memory.\n");
    }
    for (channel=new_channel=0; channel<to_tinfo->nr_of_channels; channel++, new_channel++) {
     new_probepos[3*new_channel  ]=to_tinfo->probepos[3*channel  ];
     new_probepos[3*new_channel+1]=to_tinfo->probepos[3*channel+1];
     new_probepos[3*new_channel+2]=to_tinfo->probepos[3*channel+2];
    }
    for (channel=0; channel<channels_in_fromtinfo; channel++, new_channel++) {
     int const fromchannel=(channellist==NULL ? channel : channellist[channel]-1);
     new_probepos[3*new_channel  ]=from_tinfo->probepos[3*fromchannel  ];
     new_probepos[3*new_channel+1]=from_tinfo->probepos[3*fromchannel+1];
     new_probepos[3*new_channel+2]=from_tinfo->probepos[3*fromchannel+2];
    }
    free_pointer((void **)&to_tinfo->probepos);
    to_tinfo->probepos=new_probepos;
   }
   /*}}}  */
   }
   break;
  case ADD_POINTS:
   /*{{{  Join xdata if available*/
   /* The output data set will have explicit xdata if both xdata's are present */
   if (to_tinfo->xdata!=NULL && from_tinfo->xdata!=NULL) {
    DATATYPE *xdata=(DATATYPE *)malloc(new_nr_of_points*sizeof(DATATYPE));
    if (xdata==NULL) {
     ERREXIT(to_tinfo->emethods, "add_channels: Error allocating xdata memory.\n");
    }
    memcpy(xdata, to_tinfo->xdata, to_tinfo->nr_of_points*sizeof(DATATYPE));
    memcpy(xdata+to_tinfo->nr_of_points, from_tinfo->xdata, from_tinfo->nr_of_points*sizeof(DATATYPE));
    free(to_tinfo->xdata);
    to_tinfo->xdata=xdata;
   } else {
    free_pointer((void **)&to_tinfo->xdata);
   }
   /*}}}  */
   /*{{{  Join triggers*/
   if (from_tinfo->triggers.buffer_start!=NULL) {
    struct trigger *intrig;

    if (to_tinfo->triggers.buffer_start==NULL) {
     growing_buf_allocate(&to_tinfo->triggers, 0);
     /* Record the file position... */
     growing_buf_append(&to_tinfo->triggers, from_tinfo->triggers.buffer_start, sizeof(struct trigger));
    } else {
     /* Delete the end marker */
     to_tinfo->triggers.current_length-=sizeof(struct trigger);
    }
    for (intrig=(struct trigger *)from_tinfo->triggers.buffer_start+1; intrig->code!=0; intrig++) {
     char *description=intrig->description;
     if (description!=NULL) {
      description=(char *)malloc(strlen(intrig->description)+1);
      strcpy(description,intrig->description);
     }
     push_trigger(&to_tinfo->triggers, intrig->position+to_tinfo->nr_of_points, intrig->code, description);
    }
    /* Write end marker */
    push_trigger(&to_tinfo->triggers, 0, 0, NULL);
   }
   /*}}}  */
   break;
  case ADD_ITEMS:
   break;
  default:
   /* Can't happen, but keeps the compiler happy and adds security; note
    * that the append method supports additional add_channels types... */
   ERREXIT(to_tinfo->emethods, "add_channels: Unknown add_channels type!\n");
   break;
 }
 }
 to_tinfo->nr_of_points=to_tinfo->nroffreq=new_nr_of_points;
 to_tinfo->nr_of_channels=new_nr_of_channels;
 to_tinfo->itemsize=new_itemsize;
 to_tinfo->length_of_output_region=newtinfo.length_of_output_region;
 return newtinfo.tsdata;
}
/*}}}  */

/*{{{  add_channels(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
add_channels(transform_info_ptr tinfo) {
 struct add_channels_storage *local_arg=(struct add_channels_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;
 int *channel_list=NULL;
 DATATYPE *newtsdata;

 if (side_tinfo->tsdata==NULL || args[ARGS_EVERY].is_set) {
  /*{{{  (Try to) Read the next add_channels epoch */
  if (side_tinfo->tsdata!=NULL) free_tinfo(side_tinfo);
  side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo);
  /*}}}  */
 }

 /* Rejecting epochs is our choice if more epochs come in than present in
  * the add_channels file */
 if (side_tinfo->tsdata==NULL) {
  TRACEMS(tinfo->emethods, 0, "add_channels: Rejecting epoch since corresponding add_channels_file epoch could not be read!\n");
  return NULL;
 }

 /*{{{  Handle linking the epoch*/
 if (local_arg->type==ADD_LINKEPOCHS) {
  transform_info_ptr tinfo_next=(transform_info_ptr)malloc(sizeof(struct transform_info_struct));
  transform_info_ptr tinfop=tinfo;
  if (tinfo_next==NULL) {
   ERREXIT(tinfo->emethods, "add_channels: Error malloc'ing tinfo struct\n");
  }
  if (args[ARGS_EVERY].is_set) {
   memcpy(tinfo_next, side_tinfo, sizeof(struct transform_info_struct));
   /* Clear the side_tinfo memory, so that nothing is freed that we still need. */
   memset((void *)side_tinfo, 0, sizeof(struct transform_info_struct));
   side_tinfo->emethods=tinfo->emethods;
   side_tinfo->methods= &local_arg->side_method;
  } else {
   newtsdata=(DATATYPE *)malloc(side_tinfo->length_of_output_region*sizeof(DATATYPE));
   if (newtsdata==NULL) {
    ERREXIT(tinfo->emethods, "add_channels: Error malloc'ing data\n");
   }
   deepcopy_tinfo(tinfo_next, side_tinfo);
   memcpy(newtsdata, side_tinfo->tsdata, side_tinfo->length_of_output_region*sizeof(DATATYPE));
   tinfo_next->tsdata=newtsdata;
  }

  while (tinfop->next!=NULL) tinfop=tinfop->next;	/* Go to the end */

  tinfo_next->previous=tinfop;
  tinfo_next->next=NULL;
  tinfop->next=tinfo_next;

  return tinfo->tsdata;
 }
 /*}}}  */

 if (args[ARGS_BYNAME].is_set) {
  channel_list=expand_channel_list(side_tinfo, args[ARGS_BYNAME].arg.s);
  if (channel_list==NULL) {
   ERREXIT(tinfo->emethods, "add_channels_init: Zero channels were selected by -n!\n");
  }
 }

 newtsdata=add_channels_or_points(tinfo, side_tinfo, channel_list, local_arg->type, FALSE);
 
 if (args[ARGS_BYNAME].is_set) {
  free(channel_list);
 }

 return newtsdata;
}
/*}}}  */

/*{{{  add_channels_exit(transform_info_ptr tinfo)*/
METHODDEF void
add_channels_exit(transform_info_ptr tinfo) {
 struct add_channels_storage *local_arg=(struct add_channels_storage *)tinfo->methods->local_storage;
 transform_info_ptr side_tinfo= &local_arg->side_tinfo;
 if (side_tinfo->methods->init_done) 
  (*side_tinfo->methods->transform_exit)(side_tinfo);
 free_tinfo(side_tinfo);
 free_methodmem(side_tinfo);
 growing_buf_free(&local_arg->side_argbuf);
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_add_channels(transform_info_ptr tinfo)*/
GLOBAL void
select_add_channels(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &add_channels_init;
 tinfo->methods->transform= &add_channels;
 tinfo->methods->transform_exit= &add_channels_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="add_channels";
 tinfo->methods->method_description=
  "Transform method to add channels, points or items from a readasc file.\n"
  " The number of and points and items (resp.: channels and items or\n"
  " channels and points) must be identical.\n"
  " Linking (option -l) has no such restriction.\n";
 tinfo->methods->local_storage_size=sizeof(struct add_channels_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
