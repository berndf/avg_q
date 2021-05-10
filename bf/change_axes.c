/*
 * Copyright (C) 1996-1999,2002,2003,2010,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * change_axes: transform method to shift, scale and rename the x and z axes
 * 						-- Bernd Feige 21.03.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_XOFFSET=0, 
 ARGS_XFACTOR, 
 ARGS_XNEWLABEL,
 ARGS_ZOFFSET, 
 ARGS_ZFACTOR, 
 ARGS_ZNEWLABEL,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "x_offset", "", 0, NULL},
 {T_ARGS_TAKES_DOUBLE, "x_factor", "", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "x_newlabel", "", ARGDESC_UNUSED, (const char *const *)"*"},
 {T_ARGS_TAKES_DOUBLE, "z_offset", "", 0, NULL},
 {T_ARGS_TAKES_DOUBLE, "z_factor", "", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "z_newlabel", "", ARGDESC_UNUSED, (const char *const *)"*"}
};

/*{{{  Local definitions*/
LOCAL char *
new_string(char const *oldstr) {
 char *newstr;
 if (oldstr==(char *)NULL) {
  return (char *)NULL;
 } else {
  newstr=(char *)malloc(strlen(oldstr)+1);
  if (newstr!=(char *)NULL) strcpy(newstr, oldstr);
 }
 return newstr;
}
/*}}}  */

/*{{{  change_axes_init(transform_info_ptr tinfo) {*/
METHODDEF void
change_axes_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  change_axes(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
change_axes(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 int i;
 long const xoffset=gettimeslice(tinfo, args[ARGS_XOFFSET].arg.s);

 /*{{{  Scale xdata*/
 if (tinfo->xdata==(DATATYPE *)NULL) {
  create_xaxis(tinfo, NULL);
 }
 for (i=0; i<tinfo->nr_of_points; i++) {
  tinfo->xdata[i]=(tinfo->xdata[i]+xoffset)*args[ARGS_XFACTOR].arg.d;
 }
 /*}}}  */
 tinfo->z_value=(tinfo->z_value+args[ARGS_ZOFFSET].arg.d)*args[ARGS_ZFACTOR].arg.d;

 /* The xchannelname must be set after a possible create_xaxis because create_xaxis sets it too... */
 if (strcmp(args[ARGS_XNEWLABEL].arg.s, "*")!=0) tinfo->xchannelname=new_string(args[ARGS_XNEWLABEL].arg.s);
 if (strcmp(args[ARGS_ZNEWLABEL].arg.s, "*")!=0) tinfo->z_label=new_string(args[ARGS_ZNEWLABEL].arg.s);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  change_axes_exit(transform_info_ptr tinfo) {*/
METHODDEF void
change_axes_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_change_axes(transform_info_ptr tinfo) {*/
GLOBAL void
select_change_axes(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &change_axes_init;
 tinfo->methods->transform= &change_axes;
 tinfo->methods->transform_exit= &change_axes_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="change_axes";
 tinfo->methods->method_description=
  "change_axes changes (shifts and scales) the x and z axes and sets the\n"
  "axis labels if told so. A `*' as label will leave the label unchanged.\n"
  "If no x axis exists, a new one is built from the point numbers (0..N-1).\n"
  "The offset is added FIRST (which makes it easy to control the final zero point).\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
