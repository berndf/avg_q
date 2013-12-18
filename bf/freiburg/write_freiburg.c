/*
 * Copyright (C) 1996-1999,2001,2003-2005,2007,2012 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_freiburg.c method to write data to a Freiburg 
 *  (Stefan Krieger/Stephanie Lis/Bernd Tritschler) file
 *	-- Bernd Feige 21.01.1996
 *
 * If (local_arg->outfname eq "stdout") local_arg->outfptr=stdout;
 * If (local_arg->outfname eq "stderr") local_arg->outfptr=stderr;
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <string.h>
#include <Intel_compat.h>
#include <read_struct.h>
#include "transform.h"
#include "bf.h"
#include "freiburg.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_CLOSE,
 ARGS_SENSITIVITY,
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Close the file after writing each epoch (and open it again next time)", "c", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "sensitivity: Use this sensitivity in nV/Bit (assumes data is in uV)", "s", 1, NULL},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL},
};

/*{{{  struct write_freiburg_storage {*/
struct write_freiburg_storage {
 BT_file btfile;
 FILE *outfptr;
 FILE *coafptr;
 short *intermediate_buf;
 unsigned short *outbuf;
 short *lastval;

 float sensitivity;
 Bool appendmode;
};
/*}}}  */

/* This is only for the searching within .coa files on append mode open */
#define COA_BUFSIZE 64
#define SEGMENT_TABLE_STRING "Segment_Table"

#ifndef __GNUC__
/* This is because not all compilers accept automatic, variable-size arrays...
 * Have to use constant lengths for those path strings... I hate it... */
#define MAX_PATHLEN 1024
#endif

/*{{{  Local open_file and close_file routines*/
/*{{{  write_freiburg_open_file(transform_info_ptr tinfo) {*/
LOCAL void
write_freiburg_open_file(transform_info_ptr tinfo) {
 struct write_freiburg_storage *local_arg=(struct write_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *outfptr=NULL;
#ifdef __GNUC__
 char co_name[strlen(args[ARGS_OFILE].arg.s)+4];
 char coa_name[strlen(args[ARGS_OFILE].arg.s)+5];
#else
 char co_name[MAX_PATHLEN];
 char coa_name[MAX_PATHLEN];
#endif
 strcpy(co_name, args[ARGS_OFILE].arg.s); strcat(co_name, ".co");
 strcpy(coa_name, args[ARGS_OFILE].arg.s); strcat(coa_name, ".coa");
 local_arg->coafptr=NULL;
 
 if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) outfptr=stdout;
 else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) outfptr=stderr;
 local_arg->appendmode=args[ARGS_APPEND].is_set;
 if (local_arg->appendmode) {
  /*{{{  Append mode open if cofile exists and is of non-zero length*/
  if (outfptr==NULL) {
   if ((outfptr=fopen(co_name, "a+b"))==NULL) {
    ERREXIT1(tinfo->emethods, "write_freiburg_init: Can't append to %s\n", MSGPARM(co_name));
   }
  }
  fseek(outfptr, 0L, SEEK_END);
  if (ftell(outfptr)==0L) {
   local_arg->appendmode=FALSE;	/* If at start: write header */
  } else {
   char coa_buffer[COA_BUFSIZE];
   if ((local_arg->coafptr=fopen(coa_name, "r+b"))==NULL) {
    ERREXIT1(tinfo->emethods, "write_freiburg_init: Can't append to %s\n", MSGPARM(coa_name));
   }
   /*{{{  Seek to the end of the current segment table*/
   do {
    fgets(coa_buffer, COA_BUFSIZE, local_arg->coafptr);
   } while (!feof(local_arg->coafptr) && strncmp(coa_buffer, SEGMENT_TABLE_STRING, strlen(SEGMENT_TABLE_STRING))!=0);
   while (!feof(local_arg->coafptr) && 
          (fgets(coa_buffer, COA_BUFSIZE, local_arg->coafptr), 
          strchr(coa_buffer, '\n')==NULL));
   fseek(local_arg->coafptr, -2L, SEEK_CUR); /* Step back to before the \r */
   /*}}}  */
  }
  /*}}}  */
 }
 if (outfptr==NULL) {
  /*{{{  Open the cofile in truncate mode*/
  if ((outfptr=fopen(co_name, "wb"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_freiburg_init: Can't open %s\n", MSGPARM(co_name));
  }
  /*}}}  */
 }
 if (local_arg->coafptr==NULL) {
  /*{{{  Open the coafile in write/truncate mode*/
  if ((local_arg->coafptr=fopen(coa_name, "wb"))==NULL) {
   ERREXIT1(tinfo->emethods, "write_freiburg_init: Can't open %s\n", MSGPARM(coa_name));
  }
  /*}}}  */
 }
 local_arg->outfptr=outfptr;
 if (local_arg->appendmode==FALSE) {	/* Don't write header in append mode */
  /*{{{  Write header*/
  int channel;
  short dd, mm, yy, yyyy, hh, mi, ss;

  local_arg->btfile.nr_of_channels=tinfo->nr_of_channels;
  local_arg->btfile.sampling_interval_us=(short int)rint(1e6/tinfo->sfreq);
  local_arg->btfile.blockluecke=0;
  if (tinfo->comment!=NULL) {
   strncpy(local_arg->btfile.text, tinfo->comment, 32);
   if (comment2time(tinfo->comment, &dd, &mm, &yy, &yyyy, &hh, &mi, &ss)) {
     local_arg->btfile.start_month=mm;
     local_arg->btfile.start_day=dd;
     local_arg->btfile.start_year=yyyy;
     local_arg->btfile.start_hour=hh;
     local_arg->btfile.start_minute=mi;
     local_arg->btfile.start_second=ss;
   }
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->btfile, sm_BT_file);
#endif
  write_struct((char *)&local_arg->btfile, sm_BT_file, outfptr);
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->btfile, sm_BT_file);
#endif

  fprintf(local_arg->coafptr, "Channels %d\r\nSampling_Interval %d\r\nChannel_Table %d\r\n", tinfo->nr_of_channels, local_arg->btfile.sampling_interval_us, tinfo->nr_of_channels);
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   fprintf(local_arg->coafptr, "%d %d %s\r\n", channel+1, (int)(local_arg->sensitivity*1000), tinfo->channelnames[channel]);
  }
  fprintf(local_arg->coafptr, "Segment_Table 0\r\n64 ");
  /*}}}  */
 }
}
/*}}}  */

/*{{{  write_freiburg_close_file(transform_info_ptr tinfo) {*/
LOCAL void
write_freiburg_close_file(transform_info_ptr tinfo) {
 struct write_freiburg_storage *local_arg=(struct write_freiburg_storage *)tinfo->methods->local_storage;

 fprintf(local_arg->coafptr, "\r\nEVENT 0\r\n0\r\nEND\r\n");
 fclose(local_arg->coafptr);
 if (local_arg->outfptr!=stdout && local_arg->outfptr!=stderr) {
  fclose(local_arg->outfptr);
 } else {
  fflush(local_arg->outfptr);
 }
}
/*}}}  */

/*{{{  Data compression function*/
/* freiburg_compress returns the size of the compressed data on success and 
 * -1 on error */
LOCAL long
freiburg_compress(transform_info_ptr tinfo, unsigned short *from, unsigned short *to, int nr_of_channels, int nr_of_points) {
 unsigned short *f=from, *oldf=from, *t=to;
 int channel, point=1;

 if (nr_of_points<=0) return -1;

 /* Write the first nr_of_channels whole shorts */ 
 for (channel=0; channel<nr_of_channels; channel++) {
  unsigned short word;
  word= *f++;
  if ((word&0x8000)!=0) {
   word|=0x4000;	/* Set bit 14 if negative */
  } else {
   word&=0x3fff;	/* Mask bit 14 to 0 */
   word|=0x8000;	/* Set bit 15 */
  }
#ifdef LITTLE_ENDIAN
  Intel_int16(&word);
#endif
  *t++=word;
 }

 while (point<nr_of_points) {
  for (channel=0; channel<nr_of_channels; channel++) {
   unsigned short oldword, word;
   short diff;
   oldword= *oldf++;
   word= *f++;
   diff=(short)word-(short)oldword;

   if (abs(diff)>63) {
    /* write a whole short */
    if ((word&0x8000)!=0) {
     word|=0x4000;	/* Set bit 14 if negative */
    } else {
     word&=0x3fff;	/* Mask bit 14 to 0 */
     word|=0x8000;	/* Set bit 15 */
    }
#ifdef LITTLE_ENDIAN
    Intel_int16(&word);
#endif
   *t++=word;
   } else {
    /* Write byte relative to oldword */
    unsigned char byte=(unsigned char)diff;
    if (diff<0) {
     /* Negative difference: Reset bit 7 */
     byte&=0x7f;
    } else {
     /* Positive difference: Reset bits 6 and 7 */
     byte&=0x3f;
    }
    *(unsigned char *)t =byte; t++;
   }
  }
  point++;
 }
 return (char *)t-(char *)to;
}
/*}}}  */
/*}}}  */

/*{{{  write_freiburg_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_freiburg_init(transform_info_ptr tinfo) {
 struct write_freiburg_storage *local_arg=(struct write_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /*{{{  Process options*/
 local_arg->sensitivity=(args[ARGS_SENSITIVITY].is_set ? args[ARGS_SENSITIVITY].arg.i/1000.0 : 0.0);
 /*}}}  */

 if ((local_arg->intermediate_buf =(short *)malloc(tinfo->nr_of_channels*tinfo->nr_of_points*sizeof(short)))==NULL
   ||(local_arg->outbuf=(unsigned short *)malloc(tinfo->nr_of_channels*tinfo->nr_of_points*sizeof(short)))==NULL) {
  ERREXIT(tinfo->emethods, "write_freiburg_init: Error allocating buffer memory.\n");
 }
 if (args[ARGS_CLOSE].is_set==FALSE) write_freiburg_open_file(tinfo);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_freiburg(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_freiburg(transform_info_ptr tinfo) {
 struct write_freiburg_storage *local_arg=(struct write_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *outfptr=local_arg->outfptr;
 short *inbuf=local_arg->intermediate_buf;
 long outlen;
 array myarray;

 if (tinfo->itemsize!=1) {
  ERREXIT(tinfo->emethods, "write_freiburg: Only itemsize=1 is supported.\n");
 }
 if (args[ARGS_CLOSE].is_set) {
  write_freiburg_open_file(tinfo);
  outfptr=local_arg->outfptr;
 }
 if (tinfo->data_type==FREQ_DATA) tinfo->nr_of_points=tinfo->nroffreq;
 /*{{{  Write epoch*/
 tinfo_array(tinfo, &myarray);
 array_transpose(&myarray);	/* channels are the elements */
 do {
  do {
   *inbuf++ = (short int)rint(array_scan(&myarray)/local_arg->sensitivity);
  } while (myarray.message==ARRAY_CONTINUE);
 } while (myarray.message!=ARRAY_ENDOFSCAN);
 outlen=freiburg_compress(tinfo, (unsigned short *)local_arg->intermediate_buf, local_arg->outbuf, tinfo->nr_of_channels, tinfo->nr_of_points);
 if ((int)fwrite(local_arg->outbuf, 1, outlen, outfptr)!=outlen) {
  ERREXIT(tinfo->emethods, "write_freiburg: Write error on data file.\n");
 }
 fprintf(local_arg->coafptr, "%ld ", outlen);
 /*}}}  */
 if (args[ARGS_CLOSE].is_set) write_freiburg_close_file(tinfo);
 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_freiburg_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_freiburg_exit(transform_info_ptr tinfo) {
 struct write_freiburg_storage *local_arg=(struct write_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_CLOSE].is_set==FALSE) write_freiburg_close_file(tinfo);
 free_pointer((void **)&local_arg->outbuf);
 free_pointer((void **)&local_arg->lastval);

 local_arg->outfptr=NULL;
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_freiburg(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_freiburg(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_freiburg_init;
 tinfo->methods->transform= &write_freiburg;
 tinfo->methods->transform_exit= &write_freiburg_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_freiburg";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to a sleep (Bernd Tritschler) file.\n"
  " Each epoch is compressed by itself. To the name given in Outfile, `.co'\n"
  " is appended for the data file and `.coa' for the info file.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_freiburg_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
