/*
 * Copyright (C) 1997-1999,2002-2004,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * append.c collect method to append the incoming epochs to a single epoch
 *	-- Bernd Feige 05.03.1997
 *
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const type_choice[]={
 "-c",
 "-p",
 "-i",
 "-l",
 NULL
};
enum ARGS_ENUM {
 ARGS_TYPE=0,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SELECTION, "Add channels, points, items or link epochs", " ", 1, type_choice},
};
/*{{{  Global defines*/
struct append_local_struct {
 struct transform_info_struct tinfo;
 transform_info_ptr tinfop;
 int nroffiles;
 enum add_channels_types type;
};
/*}}}  */

/*{{{  append_init(transform_info_ptr tinfo)*/
METHODDEF void
append_init(transform_info_ptr tinfo) {
 struct append_local_struct *localp=(struct append_local_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 localp->tinfo.tsdata=NULL;
 localp->tinfop=NULL;
 localp->nroffiles=0;
 if (args[ARGS_TYPE].is_set) {
  localp->type=(enum add_channels_types)args[ARGS_TYPE].arg.i;
 } else {
  localp->type=ADD_POINTS;
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  append(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
append(transform_info_ptr tinfo) {
 struct append_local_struct *localp=(struct append_local_struct *)tinfo->methods->local_storage;

 /* For the very first incoming epoch set, in either case the epochs must just be stored: */
 if (localp->type==ADD_LINKEPOCHS || localp->tinfop==NULL) {
  transform_info_ptr tinfoptr;
  /* Go to the start: */
  for (tinfoptr=tinfo; tinfoptr->previous!=NULL; tinfoptr=tinfoptr->previous);
  for (; tinfoptr!=NULL; tinfoptr=tinfoptr->next) {
   if (localp->tinfop==NULL) {
    deepcopy_tinfo(&localp->tinfo, tinfoptr);
    localp->tinfop= &localp->tinfo;
    localp->tinfop->previous=localp->tinfop->next=NULL;
   } else {
    transform_info_ptr tinfo_next=(transform_info_ptr)malloc(sizeof(struct transform_info_struct));
    if (tinfo_next==NULL) {
     ERREXIT(tinfo->emethods, "append: Error malloc'ing tinfo struct\n");
    }
    /* This copies everything *except* tsdata: */
    deepcopy_tinfo(tinfo_next, tinfoptr);
    tinfo_next->previous=localp->tinfop;
    tinfo_next->next=NULL;
    localp->tinfop->next=tinfo_next;
    localp->tinfop=tinfo_next;
   }
   tinfoptr->tsdata=NULL;
  }
  free_tinfo(tinfo); /* Free everything from the old epoch(s) *except* tsdata... */
  return tinfo->tsdata;
 } else {	/* !ARGS_LINKEPOCHS */
  transform_info_ptr tinfoptr, tinfop;
  /* Go to the start: */
  for (tinfoptr=tinfo; tinfoptr->previous!=NULL; tinfoptr=tinfoptr->previous);
  for (tinfop= &localp->tinfo; tinfoptr!=NULL; tinfoptr=tinfoptr->next, tinfop=tinfop->next) {
   if (tinfop==NULL) {
    ERREXIT(tinfo->emethods, "append: Varying number of linked epochs in input!\n");
   } else {
   DATATYPE * const newtsdata=add_channels_or_points(tinfop, tinfoptr, /* channel_list= */NULL, localp->type, /* zero_newdata= */FALSE);
   free_pointer((void **)&tinfop->tsdata);
   tinfop->tsdata=newtsdata;
   }
  }
 }	/* !ARGS_LINKEPOCHS */

 free_tinfo(tinfo); /* Free everything from the old epoch */

 localp->nroffiles++;
 return localp->tinfo.tsdata;
}
/*}}}  */

/*{{{  append_exit(transform_info_ptr tinfo)*/
METHODDEF void
append_exit(transform_info_ptr tinfo) {
 struct append_local_struct *localp=(struct append_local_struct *)tinfo->methods->local_storage;

 if (localp->tinfop!=NULL) {
  memcpy(tinfo, &localp->tinfo, sizeof(struct transform_info_struct));
  /* Correct the backwards pointer of the next tinfo to point to us: */
  if (tinfo->next!=NULL) tinfo->next->previous=tinfo;
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_append(transform_info_ptr tinfo)*/
GLOBAL void
select_append(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &append_init;
 tinfo->methods->transform= &append;
 tinfo->methods->transform_exit= &append_exit;
 tinfo->methods->method_type=COLLECT_METHOD;
 tinfo->methods->method_name="append";
 tinfo->methods->method_description=
  "Collect method to append the time points of corresponding channels of\n"
  " all incoming epochs (which should have equal number of channels and items)\n"
  " or add channels (equal points and items) or items (equal channels and points)\n"
  " to form a single epoch in memory.\n"
  "Alternatively (option -l), the epochs can be collected into a linked list\n"
  " of epochs to be able to display them together via the posplot method.\n";
 tinfo->methods->local_storage_size=sizeof(struct append_local_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
