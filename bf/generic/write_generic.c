/*
 * Copyright (C) 1997-2004,2008,2010,2012-2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_generic.c generic binary file output
 *	-- Bernd Feige 18.02.1997
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <setjmp.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const datatype_choice[]={
 "uint8",
 "int8",
 "int16",
 "int32",
 "float32",
 "float64",
 "string",
 NULL
};
enum DATATYPE_ENUM {
 DT_UINT8=0,
 DT_INT8,
 DT_INT16,
 DT_INT32,
 DT_FLOAT32,
 DT_FLOAT64,
 DT_STRING,
};

enum ARGS_ENUM {
 ARGS_APPEND=0,
 ARGS_CLOSE,
 ARGS_WRITE_XDATA,
 ARGS_WRITE_Z_VALUE,
 ARGS_WRITE_COMMENT,
 ARGS_SWAPBYTEORDER,
 ARGS_POINTSFASTEST,
 ARGS_WRITE_CHANNELNAMES,
 ARGS_EPOCHSEP,
 ARGS_OFILE,
 ARGS_DATATYPE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close the file after writing each epoch (and open it again next time)", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Prepend the x axis data as additional `channel'", "x", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Prepend the z value as additional column", "z", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Prepend the comment string or value as additional column", "C", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Swap byte order relative to the current machine", "S", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Points vary fastest in the output file (`nonmultiplexed')", "P", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Prepend the channel name or value as additional `point'", "N", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "epochsep: Output this string between epochs", "s", ARGDESC_UNUSED, (const char *const *)"\\n"},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_SELECTION, "data_type", "", DT_INT16, datatype_choice}
};

/*{{{  struct write_generic_storage {*/
struct write_generic_storage {
 FILE *outfile;
 enum DATATYPE_ENUM datatype;
 Bool last_was_newline;
 Bool beginning_of_file;
 growing_buf epochsep;

 jmp_buf error_jmp;
};
/*}}}  */

/*{{{  Local open_file and close_file routines*/
/*{{{  write_generic_open_file(transform_info_ptr tinfo) {*/
LOCAL void
write_generic_open_file(transform_info_ptr tinfo) {
 struct write_generic_storage *local_arg=(struct write_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const * const filemode_rw=(local_arg->datatype==DT_STRING ? "r+" : "r+b");
 char const * const filemode_w=(local_arg->datatype==DT_STRING ? "w" : "wb");

 /* We make FALSE the default so that epochsep will be output when the 
  * user chooses to write to stdout or stderr in multiple runs */
 local_arg->beginning_of_file=FALSE;
 if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) {
  local_arg->outfile=stdout;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) {
  local_arg->outfile=stderr;
 } else {
  local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, filemode_rw);
  if (!args[ARGS_APPEND].is_set || local_arg->outfile==NULL) {   /* target does not exist*/
   /*{{{  Create file*/
   if (local_arg->outfile!=NULL) fclose(local_arg->outfile);

   if ((local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, filemode_w))==NULL) {
    ERREXIT1(tinfo->emethods, "write_generic_open_file: Can't open %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
   }
   local_arg->beginning_of_file=TRUE;
   TRACEMS1(tinfo->emethods, 1, "write_generic_open_file: Creating file %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
   /*}}}  */
  } else {
   /*{{{  Append to file*/
   fseek(local_arg->outfile, 0, SEEK_END);
   if (ftell(local_arg->outfile)==0) {
    local_arg->beginning_of_file=TRUE;
   }
   TRACEMS1(tinfo->emethods, 1, "write_generic_open_file: Appending to file %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
   /*}}}  */
  }
 }
}
/*}}}  */

/*{{{  write_generic_close_file(transform_info_ptr tinfo) {*/
LOCAL void
write_generic_close_file(transform_info_ptr tinfo) {
 struct write_generic_storage *local_arg=(struct write_generic_storage *)tinfo->methods->local_storage;
 if (local_arg->outfile!=stdout && local_arg->outfile!=stderr) {
  fclose(local_arg->outfile);
 } else {
  fflush(local_arg->outfile);
 }
 local_arg->outfile=NULL;
}
/*}}}  */
/*}}}  */

/*{{{  write_generic_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_generic_init(transform_info_ptr tinfo) {
 struct write_generic_storage *local_arg=(struct write_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /*{{{  Process options*/
 local_arg->datatype=(enum DATATYPE_ENUM)args[ARGS_DATATYPE].arg.i;

 growing_buf_init(&local_arg->epochsep);
 if (args[ARGS_EPOCHSEP].is_set) {
  char const *inbuf=args[ARGS_EPOCHSEP].arg.s;
  growing_buf_allocate(&local_arg->epochsep, 0);
  while (*inbuf !='\0') {
   char c=*inbuf++;
   if (c=='\\') {
    switch (*inbuf++) {
     case 'n':
      c='\n';
      break;
     case '\0':
      inbuf--;
      break;
     default:
      break;
    }
    if (c=='\0') break;
   }
   growing_buf_appendchar(&local_arg->epochsep, c);
  }
  growing_buf_appendchar(&local_arg->epochsep, '\0');
 }
 /*}}}  */

 if (!args[ARGS_CLOSE].is_set) write_generic_open_file(tinfo);

 local_arg->last_was_newline=TRUE;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

LOCAL void
write_value(DATATYPE dat, FILE *outfile, 
 struct write_generic_storage *local_arg, 
 Bool swap_byteorder, Bool string_output_newline) {
 Bool err=FALSE;
 switch (local_arg->datatype) {
  case DT_UINT8: {
   unsigned char c= (unsigned char)rint(dat);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_INT8: {
   signed char c= (signed char)rint(dat);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_INT16: {
   int16_t c= (int16_t)rint(dat);
   if (swap_byteorder) Intel_int16((uint16_t *)&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_INT32: {
   int32_t c= (int32_t)rint(dat);
   if (swap_byteorder) Intel_int32((uint32_t *)&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_FLOAT32: {
   float c=dat;
   if (swap_byteorder) Intel_float(&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_FLOAT64: {
   double c=dat;
   if (swap_byteorder) Intel_double(&c);
   err=(fwrite(&c, sizeof(c), 1, outfile)!=1);
   }
   break;
  case DT_STRING: {
   double c=dat;
   if (!local_arg->last_was_newline) {
    fputs("\t", outfile);
   }
   fprintf(outfile, "%g", c);
   if (string_output_newline) {
    fputs("\n", outfile);
    local_arg->last_was_newline=TRUE;
   } else {
    local_arg->last_was_newline=FALSE;
   }
   err=FALSE;
   }
   break;
 }
 if (err) {
  longjmp(local_arg->error_jmp, err);
 }
 local_arg->beginning_of_file=FALSE;
}

/*{{{  write_generic(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_generic(transform_info_ptr tinfo) {
 struct write_generic_storage *local_arg=(struct write_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *outfile;
 array myarray;

 if (args[ARGS_CLOSE].is_set) {
  write_generic_open_file(tinfo);
 }
 outfile=local_arg->outfile;

 if (args[ARGS_WRITE_XDATA].is_set && tinfo->xdata==NULL) {
  create_xaxis(tinfo,NULL);
 }

 if (local_arg->epochsep.buffer_start!=NULL && !local_arg->beginning_of_file) {
  fputs(local_arg->epochsep.buffer_start, outfile);
 }

 tinfo_array(tinfo, &myarray);
 switch (setjmp(local_arg->error_jmp)) {
  case FALSE:
   if (args[ARGS_POINTSFASTEST].is_set) {
    if (args[ARGS_WRITE_XDATA].is_set) {
     int i;
     if (args[ARGS_WRITE_COMMENT].is_set) {
      if (local_arg->datatype==DT_STRING) {
       if (!local_arg->last_was_newline) {
	fputs("\t", outfile);
       }
       fputs(tinfo->comment, outfile);
       local_arg->last_was_newline=FALSE;
      } else {
       write_value(atof(tinfo->comment), outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
      }
     }
     if (args[ARGS_WRITE_Z_VALUE].is_set) {
      write_value(tinfo->z_value, outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
     }
     if (args[ARGS_WRITE_CHANNELNAMES].is_set) {
      if (local_arg->datatype==DT_STRING) {
       if (!local_arg->last_was_newline) {
	fputs("\t", outfile);
       }
       fputs(tinfo->xchannelname, outfile);
       local_arg->last_was_newline=FALSE;
      } else {
       write_value(atof(tinfo->xchannelname), outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
      }
     }
     for (i=0; i<tinfo->nr_of_points; i++) {
      write_value(tinfo->xdata[i], outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, i==tinfo->nr_of_points-1);
     }
    }
   } else {
    if (args[ARGS_WRITE_CHANNELNAMES].is_set) {
     int i;
     if (args[ARGS_WRITE_COMMENT].is_set) {
      if (local_arg->datatype==DT_STRING) {
       if (!local_arg->last_was_newline) {
	fputs("\t", outfile);
       }
       fputs("Comment", outfile);
       local_arg->last_was_newline=FALSE;
      } else {
       write_value(0, outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
      }
     }
     if (args[ARGS_WRITE_Z_VALUE].is_set) {
      if (local_arg->datatype==DT_STRING) {
       if (!local_arg->last_was_newline) {
	fputs("\t", outfile);
       }
       fputs((tinfo->z_label==NULL ? "z_label" : tinfo->z_label), outfile);
       local_arg->last_was_newline=FALSE;
      } else {
       write_value(0, outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
      }
     }
     if (args[ARGS_WRITE_XDATA].is_set) {
      if (local_arg->datatype==DT_STRING) {
       if (!local_arg->last_was_newline) {
	fputs("\t", outfile);
       }
       fputs(tinfo->xchannelname, outfile);
       local_arg->last_was_newline=FALSE;
      } else {
       write_value(atof(tinfo->xchannelname), outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
      }
     }
     for (i=0; i<tinfo->nr_of_channels; i++) {
      if (local_arg->datatype==DT_STRING) {
       if (!local_arg->last_was_newline) {
	fputs("\t", outfile);
       }
       fputs(tinfo->channelnames[i], outfile);
       if (i==tinfo->nr_of_channels-1) {
	fputs("\n", outfile);
	local_arg->last_was_newline=TRUE;
       } else {
	local_arg->last_was_newline=FALSE;
       }
      } else {
       write_value(atof(tinfo->channelnames[i]), outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, i==tinfo->nr_of_channels-1);
      }
     }
    }
    array_transpose(&myarray);	/* channels are the elements */
   }

   do {
    DATATYPE * const item0_addr=ARRAY_ELEMENT(&myarray);
    int itempart;
    if (args[ARGS_WRITE_COMMENT].is_set && myarray.current_element==0) {
     if (local_arg->datatype==DT_STRING) {
      if (!local_arg->last_was_newline) {
       fputs("\t", outfile);
      }
      fputs(tinfo->comment, outfile);
      local_arg->last_was_newline=FALSE;
     } else {
      write_value(atof(tinfo->comment), outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
     }
    }
    if (args[ARGS_WRITE_Z_VALUE].is_set && myarray.current_element==0) {
     write_value(tinfo->z_value, outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
    }
    if (args[ARGS_WRITE_XDATA].is_set && !args[ARGS_POINTSFASTEST].is_set && myarray.current_element==0) {
     write_value(tinfo->xdata[myarray.current_vector], outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
    }
    if (args[ARGS_WRITE_CHANNELNAMES].is_set && args[ARGS_POINTSFASTEST].is_set && myarray.current_element==0) {
     if (local_arg->datatype==DT_STRING) {
      if (!local_arg->last_was_newline) {
       fputs("\t", outfile);
      }
      fputs(tinfo->channelnames[myarray.current_vector], outfile);
      local_arg->last_was_newline=FALSE;
     } else {
      write_value(atof(tinfo->channelnames[myarray.current_vector]), outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, FALSE);
     }
    }
    /* We do that here so that it is easy to know whether we are writing the last element: */
    array_advance(&myarray);
    /* If multiple items are present, output them side by side: */
    for (itempart=0; itempart<tinfo->itemsize; itempart++) {
     write_value(item0_addr[itempart], outfile, local_arg, args[ARGS_SWAPBYTEORDER].is_set, myarray.message!=ARRAY_CONTINUE && itempart+1==tinfo->itemsize);
    }
   } while (myarray.message!=ARRAY_ENDOFSCAN);
   break;
  case TRUE:
   ERREXIT(tinfo->emethods, "write_generic: Error writing data\n");
   break;
 }

 if (args[ARGS_CLOSE].is_set) write_generic_close_file(tinfo);
 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_generic_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_generic_exit(transform_info_ptr tinfo) {
 struct write_generic_storage *local_arg=(struct write_generic_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (!args[ARGS_CLOSE].is_set) write_generic_close_file(tinfo);
 growing_buf_free(&local_arg->epochsep);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_generic(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_generic(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_generic_init;
 tinfo->methods->transform= &write_generic;
 tinfo->methods->transform_exit= &write_generic_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_generic";
 tinfo->methods->method_description=
  "Generic put-epoch method. This is for writing headerless files with\n"
  " data stored in successive elements of one of various types.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_generic_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
