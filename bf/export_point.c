/*
 * Copyright (C) 1996-1999,2001,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * export_point 'transform method' outputs the values of all the channels
 * to an ARRAY_DUMP ASCII compatible file.
 * 						-- Bernd Feige 29.06.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const channel_coordinate_choice[]={
 "-0", "-1", "-2", "-3", NULL
};
enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_CLOSE, 
 ARGS_MATLAB,
 ARGS_AT_XVALUE,
 ARGS_COORDINATES,
 ARGS_POINTNO,
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Open the output file for append instead of overwriting it", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close and reopen the output file for each epoch", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "MatLab format instead of ARRAY_DUMP ASCII", "m", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Select x value of point to export instead of point number", "x", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "Select number of channel coordinates", " ", 3, channel_coordinate_choice},
 {T_ARGS_TAKES_STRING_WORD, "point_number", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL}
};

/*{{{  #defines*/
struct export_point_info {
 FILE *file;
 int  channelcoords_to_display;
 long pointno;
};
/*}}}  */

/*{{{  export_file_open and export_file_close*/
LOCAL void
export_file_open(transform_info_ptr tinfo) {
 struct export_point_info *exp_info=(struct export_point_info *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) {
  exp_info->file=stdout;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) {
  exp_info->file=stderr;
 } else {
  if ((exp_info->file=fopen(args[ARGS_OFILE].arg.s, args[ARGS_APPEND].is_set ? "a" : "w"))==NULL) {
   ERREXIT1(tinfo->emethods, "export_file_open: Can't open file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
 }
}

LOCAL void                                            
export_file_close(transform_info_ptr tinfo) {        
 struct export_point_info *exp_info=(struct export_point_info *)tinfo->methods->local_storage;

 if (exp_info->file!=stdout && exp_info->file!=stderr) {
  fclose(exp_info->file);
 } else {
  fflush(exp_info->file);
 }
}
/*}}}  */

/*{{{  export_point_init(transform_info_ptr tinfo) {*/
METHODDEF void
export_point_init(transform_info_ptr tinfo) {
 struct export_point_info *exp_info=(struct export_point_info *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 if (!args[ARGS_CLOSE].is_set) export_file_open(tinfo);

 /*{{{  Process options*/
 if (args[ARGS_AT_XVALUE].is_set) {
  exp_info->pointno=find_pointnearx(tinfo, (DATATYPE)atof(args[ARGS_POINTNO].arg.s));
 } else {
 exp_info->pointno=gettimeslice(tinfo, args[ARGS_POINTNO].arg.s);
 }
 exp_info->channelcoords_to_display=(args[ARGS_COORDINATES].is_set ? args[ARGS_COORDINATES].arg.i : 3);
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  export_point(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
export_point(transform_info_ptr tinfo) {
 struct export_point_info *exp_info=(struct export_point_info *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array_dump_format format=(args[ARGS_MATLAB].is_set ? ARRAY_MATLAB : ARRAY_ASCII);
 int channel, itempart, i;
 double *channelpos;
 array myarray, newarray;

 if (exp_info->pointno<0 || exp_info->pointno>=tinfo->nr_of_points) {
  ERREXIT1(tinfo->emethods, "export_point: Point number %d is out of range.\n", MSGPARM(exp_info->pointno));
 }
 TRACEMS2(tinfo->emethods, 1, "export_point: Exporting point number %d to file %s\n", MSGPARM(exp_info->pointno), MSGPARM(args[ARGS_OFILE].arg.s));
 if (args[ARGS_CLOSE].is_set) export_file_open(tinfo);
 tinfo_array(tinfo, &myarray);
 myarray.current_element=exp_info->pointno;
 if (exp_info->channelcoords_to_display==0 && tinfo->itemsize==1) {
  /*{{{  Don't need to build new array, just access tinfo*/
  newarray=myarray;
  array_transpose(&newarray);
  array_setto_vector(&newarray);	/* Select only one element */
  array_transpose(&newarray);
  /*}}}  */
 } else {
  /*{{{  Build new array containing the data*/
  newarray.nr_of_vectors=myarray.nr_of_vectors;
  newarray.nr_of_elements=exp_info->channelcoords_to_display+tinfo->itemsize;
  newarray.element_skip=1;
  if (array_allocate(&newarray)==NULL) {
   ERREXIT(tinfo->emethods, "export_point: Can't allocate output array\n");
  }
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   channelpos=tinfo->probepos+3*channel;
   for (i=0; i<exp_info->channelcoords_to_display; i++) {
    array_write(&newarray, channelpos[i]);
   }
   myarray.current_vector=channel;
   for (itempart=0; itempart<tinfo->itemsize; itempart++) {
    array_use_item(&myarray, itempart);
    array_write(&newarray, READ_ELEMENT(&myarray));
   }
  }
  /*}}}  */
 }
 array_dump(exp_info->file, &newarray, format);
 if (newarray.nr_of_elements>1) array_free(&newarray);

 if (args[ARGS_CLOSE].is_set) export_file_close(tinfo);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  export_point_exit(transform_info_ptr tinfo) {*/
METHODDEF void
export_point_exit(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 if (!args[ARGS_CLOSE].is_set) export_file_close(tinfo);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_export_point(transform_info_ptr tinfo) {*/
GLOBAL void
select_export_point(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &export_point_init;
 tinfo->methods->transform= &export_point;
 tinfo->methods->transform_exit= &export_point_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="export_point";
 tinfo->methods->method_description=
  "Transform method printing a 'point' - the channel positions and values\n"
  "at the specified point - to an ARRRAY_DUMP file\n";
 tinfo->methods->local_storage_size=sizeof(struct export_point_info);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
