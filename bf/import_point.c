/*
 * Copyright (C) 1996-1999,2001,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * import_point 'transform method' reads an ARRAY_DUMP file
 * and places the data at one point of the input epoch.
 * It will read as many channel positions as requested for each channel
 * and place the rest of the data in successive items.
 * 						-- Bernd Feige 29.09.1994
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
 ARGS_CLOSE=0, 
 ARGS_AT_XVALUE,
 ARGS_COORDINATES,
 ARGS_POINTNO,
 ARGS_IFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Close and reopen the input file for each epoch", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Select x value of point to import instead of point number", "x", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "Select number of channel coordinates", " ", 3, channel_coordinate_choice},
 {T_ARGS_TAKES_STRING_WORD, "point_number", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL}
};

/*{{{  #defines*/
struct import_point_info {
 FILE *file;
 int  channelcoords_to_import;
 long pointno;

 int  c_opt;
};
/*}}}  */

/*{{{  import_file_open and import_file_close*/
LOCAL void
import_file_open(transform_info_ptr tinfo) {
 struct import_point_info *imp_info=(struct import_point_info *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (strcmp(args[ARGS_IFILE].arg.s, "stdin")==0) {
  imp_info->file=stdin;
 } else {
  if ((imp_info->file=fopen(args[ARGS_IFILE].arg.s, "r"))==NULL) {
   ERREXIT1(tinfo->emethods, "import_file_open: Can't open %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
 }
}

LOCAL void                                            
import_file_close(transform_info_ptr tinfo) {        
 struct import_point_info *imp_info=(struct import_point_info *)tinfo->methods->local_storage;

 if (imp_info->file!=stdin) fclose(imp_info->file);
}
/*}}}  */

/*{{{  import_point_init(transform_info_ptr tinfo) {*/
METHODDEF void
import_point_init(transform_info_ptr tinfo) {
 struct import_point_info *imp_info=(struct import_point_info *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 if (!args[ARGS_CLOSE].is_set) import_file_open(tinfo);

 /*{{{  Process options*/
 if (args[ARGS_AT_XVALUE].is_set) {
  imp_info->pointno=find_pointnearx(tinfo, (DATATYPE)atof(args[ARGS_POINTNO].arg.s));
 } else {
 imp_info->pointno=gettimeslice(tinfo, args[ARGS_POINTNO].arg.s);
 }
 imp_info->channelcoords_to_import=(args[ARGS_COORDINATES].is_set ? args[ARGS_COORDINATES].arg.i : 3);
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  import_point(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
import_point(transform_info_ptr tinfo) {
 struct import_point_info *imp_info=(struct import_point_info *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel, itempart, i;
 double *channelpos;
 array myarray, newarray;

 if (imp_info->pointno<0 || imp_info->pointno>=tinfo->nr_of_points) {
  ERREXIT1(tinfo->emethods, "import_point: Point number %d is out of range.\n", MSGPARM(imp_info->pointno));
 }
 TRACEMS2(tinfo->emethods, 1, "Import_point: importing point number %d from file %s\n", MSGPARM(imp_info->pointno), MSGPARM(args[ARGS_IFILE].arg.s));
 if (args[ARGS_CLOSE].is_set) import_file_open(tinfo);
 array_undump(imp_info->file, &newarray);
 if (newarray.message==ARRAY_ERROR) {
  ERREXIT(tinfo->emethods, "import_point: array_undump failed\n");
 }
 if (newarray.nr_of_vectors!=tinfo->nr_of_channels || newarray.nr_of_elements>imp_info->channelcoords_to_import+tinfo->itemsize) {
  ERREXIT(tinfo->emethods, "import_point: The import array won't fit in the current epoch\n");
 }
 if (newarray.nr_of_elements<imp_info->channelcoords_to_import) {
  ERREXIT(tinfo->emethods, "import_point: Less data items per channel than coords requested\n");
 }

 tinfo_array(tinfo, &myarray);
 myarray.current_element=imp_info->pointno;
 /*{{{  Transfer the data*/
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  channelpos=tinfo->probepos+3*channel;
  newarray.message=ARRAY_CONTINUE;
  /* Otherwise, the END_OF_VECTOR will be there from the last time through;
   * which causes the itempart loop to quit if -0 */
  for (i=0; i<imp_info->channelcoords_to_import; i++) {
   channelpos[i]=array_scan(&newarray);
  }
  myarray.current_vector=channel;
  for (itempart=0; newarray.message==ARRAY_CONTINUE; itempart++) {
   array_use_item(&myarray, itempart);
   WRITE_ELEMENT(&myarray, array_scan(&newarray));
  }
 }
 /*}}}  */
 array_free(&newarray);

 if (args[ARGS_CLOSE].is_set) import_file_close(tinfo);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  import_point_exit(transform_info_ptr tinfo) {*/
METHODDEF void
import_point_exit(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 if (!args[ARGS_CLOSE].is_set) import_file_close(tinfo);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_import_point(transform_info_ptr tinfo) {*/
GLOBAL void
select_import_point(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &import_point_init;
 tinfo->methods->transform= &import_point;
 tinfo->methods->transform_exit= &import_point_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="import_point";
 tinfo->methods->method_description=
  "Transform method loading a `point' - the channel positions and values\n"
  " at the specified point - from an ARRAY_DUMP file\n";
 tinfo->methods->local_storage_size=sizeof(struct import_point_info);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
