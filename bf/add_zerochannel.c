/*
 * Copyright (C) 2008,2016,2024 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * add_zerochannel.c method to add a single zeroed-out channel to the data,
 * particularly before rereferencing
 *	-- Bernd Feige 25.07.2008
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  Arguments and local storage*/
enum ARGS_ENUM {
 ARGS_CHANNELNAME=0, 
 ARGS_X, 
 ARGS_Y,
 ARGS_Z, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channelname", "", ARGDESC_UNUSED, (char const *const *)"Ref"},
 {T_ARGS_TAKES_DOUBLE, "x", "", 0.0, NULL},
 {T_ARGS_TAKES_DOUBLE, "y", "", 0.0, NULL},
 {T_ARGS_TAKES_DOUBLE, "z", "", 0.0, NULL},
};

struct add_zerochannel_storage {
 char const *channelname;
 double probepos[3];
};
/*}}}  */

/*{{{  add_zerochannel_init(transform_info_ptr tinfo)*/
METHODDEF void
add_zerochannel_init(transform_info_ptr tinfo) {
 struct add_zerochannel_storage *local_arg=(struct add_zerochannel_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 local_arg->channelname=args[ARGS_CHANNELNAME].arg.s;
 local_arg->probepos[0]=args[ARGS_X].arg.d;
 local_arg->probepos[1]=args[ARGS_Y].arg.d;
 local_arg->probepos[2]=args[ARGS_Z].arg.d;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  add_zerochannel(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
add_zerochannel(transform_info_ptr tinfo) {
 struct add_zerochannel_storage *local_arg=(struct add_zerochannel_storage *)tinfo->methods->local_storage;
 struct transform_info_struct tmptinfo= *tinfo;
 tmptinfo.nr_of_channels=1;
 tmptinfo.channelnames= (char **)&local_arg->channelname;
 tmptinfo.probepos=&local_arg->probepos[0];
 return add_channels_or_points(tinfo, &tmptinfo, /* channel_list= */NULL, ADD_CHANNELS, /* zero_newdata= */TRUE);
}
/*}}}  */

/*{{{  add_zerochannel_exit(transform_info_ptr tinfo)*/
METHODDEF void
add_zerochannel_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_add_zerochannel(transform_info_ptr tinfo)*/
GLOBAL void
select_add_zerochannel(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &add_zerochannel_init;
 tinfo->methods->transform= &add_zerochannel;
 tinfo->methods->transform_exit= &add_zerochannel_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="add_zerochannel";
 tinfo->methods->method_description=
  "Transform method to add a channel with all-zero data and the given position.\n"
  " This is useful to capture the value of the reference channel after referencing, for example.\n";
 tinfo->methods->local_storage_size=sizeof(struct add_zerochannel_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
