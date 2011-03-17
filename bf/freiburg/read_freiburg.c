/*
 * Copyright (C) 1996-1999,2001,2003,2004,2007-2009,2011 Bernd Feige
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
 * read_freiburg.c module to read data from Freiburg format files
 * (by Stefan Krieger und Stephanie Lis).
 * Because of the quite ill-defined nature of the file header information,
 * all this is more a heuristical than a deterministic business...
 * Trials are stored in separate files and compressed; averages are 
 * uncompressed.
 *	-- Bernd Feige 6.10.1995
 *
 * Addition: Reading Bernd Tritschler's continuous sleep files is less special.
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <ctype.h>
#include <read_struct.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"

#include "freiburg.h"
/*}}}  */

#define MAX_COMMENTLEN 1024
#define SEGMENT_LENGTH 512

#ifndef __GNUC__
/* This is because not all compilers accept automatic, variable-size arrays...
 * Have to use constant lengths for those path strings... I hate it... */
#define MAX_PATHLEN 1024
#define MAX_NR_OF_CHANNELS 1024
#endif
#define MAX_COALINE 1024
#define SEGMENT_TABLE_STRING "Segment_Table"

enum continuous_types {
 SLEEP_BT_TYPE, SLEEP_KL_TYPE
};

#define MAX_NUMBER_OF_CHANNELS_IN_EP 128
/* This is for plausibility checking: */
#define MAX_POSSIBLE_SFREQ 2000.0

enum ARGS_ENUM {
 ARGS_CONTINUOUS=0, 
 ARGS_ZEROCHECK, 
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_SFREQ,
 ARGS_REPAIR_CHANNELS,
 ARGS_REPAIR_OFFSET,
 ARGS_IFILE,
 ARGS_NCHANNELS,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Continuous mode (sleep files, either KL or BT)", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Zero check. Eliminate data points with all channels zero :-<", "z", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_DOUBLE, "sampling_freq: Explicitly specify the sampling frequency in Hz", "s", 200, NULL},
 {T_ARGS_TAKES_LONG, "Channels: Explicitly specify the number of channels (continuous only)", "C", 30, NULL},
 {T_ARGS_TAKES_LONG, "Repair_offset: Don't use the header, start at this file offset (continuous only)", "R", FREIBURG_HEADER_LENGTH, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "{nr_of_channels|epochlength}", "", ARGDESC_UNUSED, (const char *const *)"1s"}
};

/*{{{  Definition of read_freiburg_storage*/
struct read_freiburg_storage {
 BT_file btfile;
 FILE *infile;
 /* This holds the last point read (all channels), for the difference compression. */
 short *last_values_buf;
 Bool no_last_values;
 Bool average_mode;
 Bool zero_idiotism_encountered;
 Bool zero_idiotism_warned;
 enum continuous_types continuous_type;
 int current_trigger;
 long current_point;
 int nr_of_channels;
 long offset;
 long epochlength;
 long fromepoch;
 long epochs;
 float sfreq;

 struct freiburg_channels_struct *in_channels;
 /* The following hold information that can be read from BT's .coa files: */
 char **channelnames;
 float *uV_per_bit;
 growing_buf segment_table;
};
/*}}}  */

/*{{{  freiburg_get_segment(transform_info_ptr tinfo, array *toarray)*/
LOCAL void
freiburg_get_segment_init(transform_info_ptr tinfo) {
 struct read_freiburg_storage *local_arg=(struct read_freiburg_storage *)tinfo->methods->local_storage;

 local_arg->zero_idiotism_encountered=local_arg->zero_idiotism_warned=FALSE;

 if ((local_arg->last_values_buf=(short *)malloc(tinfo->nr_of_channels*sizeof(short)))==NULL) {
  ERREXIT(tinfo->emethods, "freiburg_get_segment_init: Error allocating buffer memory\n");
 }
 local_arg->no_last_values=TRUE;
}

/* freiburg_get_segment returns 0 on success and -1 on out-of-input-bounds */
LOCAL int
freiburg_get_segment(transform_info_ptr tinfo, array *toarray) {
 struct read_freiburg_storage *local_arg=(struct read_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /* Well, again... It appears that at some time, there was a bug in KL's
  * programs resulting in ZEROES inserted before every new block, ie every
  * 513 data points... No indication of that in the header (`blockluecke' or
  * such, still don't know what that value means)... End of broadcast... */
 do {
  /* If this is FALSE at the start, it will never become TRUE... */
  int everything_was_zero=args[ARGS_ZEROCHECK].is_set;

  /* NOTE: The first time through, the values in last_values_buf are not
   * initialized. It should be assumed that the values in the very first point 
   * read are NOT marked as differences, but with BT's Nxxx files, it's 
   * different at the start of a `Block' if there was saturation as well... */
  short *in_last_values_buf=local_arg->last_values_buf;
  const Bool no_last_values=local_arg->no_last_values;

  do {
   short oldword= *in_last_values_buf;
   int nextchar=fgetc(local_arg->infile);
   unsigned short word;
   DATATYPE floatval;

   if (nextchar==EOF) return -1;
   if (nextchar&0x0080 || no_last_values) {
    /* data is a whole short */
    word=(((unsigned int)nextchar)<<8)+(((unsigned int)fgetc(local_arg->infile))&0x00ff);
    if ((word&0x4000)==0) {
     word&=0x7fff;	/* Make positive if bit 14 clear */
    }
   } else {
    /* data is relative to oldword */
    unsigned short byte=(unsigned short)nextchar;

    if (byte&0x0040) {
     /* Negative difference */
     byte|=0xffc0;
    } else {
     /* Positive difference */
     byte&=0x003f;
    }
    word=(unsigned short)((short)oldword+(short)byte); /* Signed addition */
   }
   if (word!=0) everything_was_zero=FALSE;
   *(unsigned short *)in_last_values_buf=word;

   floatval= *in_last_values_buf++;
   if (local_arg->uV_per_bit!=NULL) floatval*=local_arg->uV_per_bit[toarray->current_element];
   array_write(toarray, floatval);
  } while (toarray->message==ARRAY_CONTINUE);
  if (everything_was_zero) {
   array_previousvector(toarray);
   local_arg->zero_idiotism_encountered=TRUE;
  } else {
   break;
  }
 } while (TRUE);
 local_arg->current_point++;
 local_arg->no_last_values=FALSE;
 return 0;
}

LOCAL void
freiburg_get_segment_free(transform_info_ptr tinfo) {
 struct read_freiburg_storage *local_arg=(struct read_freiburg_storage *)tinfo->methods->local_storage;
 free_pointer((void **)&local_arg->last_values_buf);
}
/*}}}  */

/*{{{  read_freiburg_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_freiburg_init(transform_info_ptr tinfo) {
 struct read_freiburg_storage *local_arg=(struct read_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 struct stat statbuf;

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */
 local_arg->channelnames=NULL;
 local_arg->uV_per_bit=NULL;
 growing_buf_init(&local_arg->segment_table); /* This sets buffer_start=NULL */
 local_arg->nr_of_channels=0;

 if (args[ARGS_CONTINUOUS].is_set) {
  /*{{{  Open a continuous (sleep, Bernd Tritschler) file*/
  FILE *infile;
  local_arg->in_channels= &sleep_channels[0];

  if (stat(args[ARGS_IFILE].arg.s, &statbuf)!= 0 || !S_ISREG(statbuf.st_mode)) {
   /* File does not exist or isn't a regular file: Try BT format */
#ifdef __GNUC__
   char co_name[strlen(args[ARGS_IFILE].arg.s)+4];
   char coa_name[strlen(args[ARGS_IFILE].arg.s)+5];
#else
   char co_name[MAX_PATHLEN];
   char coa_name[MAX_PATHLEN];
#endif
   char coa_buf[MAX_COALINE];
   FILE *coafile;

   strcpy(co_name, args[ARGS_IFILE].arg.s); strcat(co_name, ".co");
   if((infile=fopen(co_name,"rb"))==NULL) {
    ERREXIT1(tinfo->emethods, "read_freiburg_init: Can't open file %s\n", MSGPARM(co_name));
   }
   local_arg->infile=infile;
   if (args[ARGS_REPAIR_OFFSET].is_set) {
    fseek(infile, args[ARGS_REPAIR_OFFSET].arg.i, SEEK_SET);
   } else {
   if (read_struct((char *)&local_arg->btfile, sm_BT_file, infile)==0) {
    ERREXIT1(tinfo->emethods, "read_freiburg_init: Header read error in file %s\n", MSGPARM(co_name));
   }
#  ifndef LITTLE_ENDIAN
   change_byteorder((char *)&local_arg->btfile, sm_BT_file);
#  endif
   }
   strcpy(coa_name, args[ARGS_IFILE].arg.s); strcat(coa_name, ".coa");
   if((coafile=fopen(coa_name,"rb"))==NULL) {
    TRACEMS1(tinfo->emethods, 0, "read_freiburg_init: No file %s found\n", MSGPARM(coa_name));
   } else {
    while (!feof(coafile)) {
     Bool repeat;
     fgets(coa_buf, MAX_COALINE-1, coafile);
     do {
      repeat=FALSE;
      if (strncmp(coa_buf, "Channel_Table ", 14)==0) {
       int havechannels=0, stringlength=0;
       long tablepos=ftell(coafile);
       char *innames;
       while (!feof(coafile)) {
	char *eol;
	fgets(coa_buf, MAX_COALINE-1, coafile);
	if (!isdigit(*coa_buf)) break;
	if (strncmp(coa_buf, "0 0 ", 4)==0) {
	 /* Empty channel descriptors: Don't generate channelnames */
	} else {
	 for (eol=coa_buf+strlen(coa_buf)-1; eol>=coa_buf && (*eol=='\n' || *eol=='\r'); eol--) *eol='\0';
	 /* This includes 1 for the zero at the end: */
	 stringlength+=strlen(strrchr(coa_buf, ' '));
	}
	havechannels++;
       }
       if (havechannels==0) continue;	/* Channel table is unuseable */
       local_arg->nr_of_channels=havechannels;
       innames=NULL;
       if ((stringlength!=0 && 
	   ((local_arg->channelnames=(char **)malloc(havechannels*sizeof(char *)))==NULL || 
	    (innames=(char *)malloc(stringlength))==NULL)) ||
	   (local_arg->uV_per_bit=(float *)malloc(havechannels*sizeof(float)))==NULL) {
	ERREXIT(tinfo->emethods, "read_freiburg_init: Error allocating .coa memory\n");
       }
       fseek(coafile, tablepos, SEEK_SET);
       havechannels=0;
       while (!feof(coafile)) {
	char *eol;
	fgets(coa_buf, MAX_COALINE-1, coafile);
	if (!isdigit(*coa_buf)) break;
	for (eol=coa_buf+strlen(coa_buf)-1; eol>=coa_buf && (*eol=='\n' || *eol=='\r'); eol--) *eol='\0';
	if (innames!=NULL) {
	 strcpy(innames, strrchr(coa_buf, ' ')+1);
	 local_arg->channelnames[havechannels]=innames;
	 innames+=strlen(innames)+1;
	}
	/* The sensitivity in .coa files is given as nV/Bit */
	local_arg->uV_per_bit[havechannels]=atoi(strchr(coa_buf, ' ')+1)/1000.0;
	if (local_arg->uV_per_bit[havechannels]==0.0) {
	 local_arg->uV_per_bit[havechannels]=0.086;
	 TRACEMS1(tinfo->emethods, 1, "read_freiburg_init: Sensitivity for channel %d set to 86 nV/Bit\n", MSGPARM(havechannels));
	}
	havechannels++;
       }
       repeat=TRUE;
      } else if (strncmp(coa_buf, SEGMENT_TABLE_STRING, strlen(SEGMENT_TABLE_STRING))==0) {
       long current_offset=0;
       char *inbuf=coa_buf;
       Bool havesomething=FALSE;

       growing_buf_allocate(&local_arg->segment_table, 0);
       while (!feof(coafile)) {
	int const nextchar=fgetc(coafile);
	if (nextchar=='\r' || nextchar=='\n' || nextchar==' ') {
	 if (havesomething) {
	  *inbuf = '\0';
	  current_offset+=atoi(coa_buf);
	  growing_buf_append(&local_arg->segment_table, (char *)&current_offset, sizeof(long));
	  inbuf=coa_buf;
	  havesomething=FALSE;
	 }
	 if (nextchar=='\n') break;
	} else {
	 *inbuf++ = nextchar;
	 havesomething=TRUE;
	}
       }
       repeat=FALSE;
      }
     } while (repeat);
    }
    fclose(coafile);
    if (local_arg->uV_per_bit==NULL) {
     TRACEMS1(tinfo->emethods, 0, "read_freiburg_init: No channel table found in file %s\n", MSGPARM(coa_name));
    }
    if (local_arg->segment_table.buffer_start==NULL) {
     TRACEMS1(tinfo->emethods, 0, "read_freiburg_init: No segment table found in file %s\n", MSGPARM(coa_name));
    }
    /* Determine the exact number of points in file: Start with the previous
     * segment and add the last segment. */
    fstat(fileno(infile),&statbuf);
    //printf("File size is %ld\n", statbuf.st_size);
    //printf("Last segment (EOF) at %ld\n", ((long *)local_arg->segment_table.buffer_start)[local_arg->segment_table.current_length/sizeof(long)-1]);
    while (local_arg->segment_table.current_length>=1 && ((long *)local_arg->segment_table.buffer_start)[local_arg->segment_table.current_length/sizeof(long)-1]>=statbuf.st_size) {
     //printf("%ld - Removing last segment!\n", ((long *)local_arg->segment_table.buffer_start)[local_arg->segment_table.current_length/sizeof(long)-1]);
     local_arg->segment_table.current_length--;
    }
    /* Size without the last segment */
    tinfo->points_in_file=(local_arg->segment_table.current_length/sizeof(long)-1)*SEGMENT_LENGTH;
    /* Count the points in the last segment */
    {
     array myarray;
     int length_of_last_segment=0;
     myarray.element_skip=tinfo->itemsize=1;
     myarray.nr_of_vectors=1;
     myarray.nr_of_elements=local_arg->nr_of_channels;
     if (array_allocate(&myarray)==NULL) {
      ERREXIT(tinfo->emethods, "read_freiburg_init: Error allocating myarray\n");
     }
     fseek(infile, ((long *)local_arg->segment_table.buffer_start)[local_arg->segment_table.current_length/sizeof(long)-1], SEEK_SET);
     //printf("Seeking to %ld\n", ((long *)local_arg->segment_table.buffer_start)[local_arg->segment_table.current_length/sizeof(long)-1]);
     tinfo->nr_of_channels=local_arg->nr_of_channels; /* This is used by freiburg_get_segment_init! */
     freiburg_get_segment_init(tinfo);
     while (freiburg_get_segment(tinfo, &myarray)==0) length_of_last_segment++;
     freiburg_get_segment_free(tinfo);
     tinfo->points_in_file+=length_of_last_segment;
     TRACEMS1(tinfo->emethods, 1, "read_freiburg_init: Last segment has %ld points\n",length_of_last_segment);
     array_free(&myarray);
    }
   }
   if (local_arg->channelnames==NULL && local_arg->btfile.text[0]=='\0') {
    local_arg->in_channels= &goeppi_channels[0];
    TRACEMS(tinfo->emethods, 0, "read_freiburg_init: Assuming Goeppingen style setup!\n");
   }
   local_arg->continuous_type=SLEEP_BT_TYPE;
   TRACEMS(tinfo->emethods, 1, "read_freiburg_init: Opened file in BT format\n");
  } else {
   if((infile=fopen(args[ARGS_IFILE].arg.s,"rb"))==NULL) {
    ERREXIT1(tinfo->emethods, "read_freiburg_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
   }
   if (args[ARGS_REPAIR_OFFSET].is_set) {
    fseek(infile, args[ARGS_REPAIR_OFFSET].arg.i, SEEK_SET);
   } else {
   if (read_struct((char *)&local_arg->btfile, sm_BT_file, infile)==0) {
    ERREXIT1(tinfo->emethods, "read_freiburg_init: Header read error in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
   }
#  ifdef LITTLE_ENDIAN
   change_byteorder((char *)&local_arg->btfile, sm_BT_file);
#  endif
   }
   local_arg->continuous_type=SLEEP_KL_TYPE;
   /* Year and day are swapped in KL format with respect to BT format... */
   {short buf=local_arg->btfile.start_year; local_arg->btfile.start_year=local_arg->btfile.start_day; local_arg->btfile.start_day=buf;}
   /* Well, sometimes or most of the time, in KL files the sampling interval 
    * was not set correctly... */
   if (!args[ARGS_SFREQ].is_set && local_arg->btfile.sampling_interval_us!=9765 && local_arg->btfile.sampling_interval_us!=9766) {
    TRACEMS1(tinfo->emethods, 0, "read_freiburg_init: sampling_interval_us was %d, corrected!\n", MSGPARM(local_arg->btfile.sampling_interval_us));
    local_arg->btfile.sampling_interval_us=9766;
   }
   TRACEMS(tinfo->emethods, 1, "read_freiburg_init: Opened file in KL format\n");
  }
  if (args[ARGS_REPAIR_CHANNELS].is_set) local_arg->btfile.nr_of_channels=args[ARGS_REPAIR_CHANNELS].arg.i;
  tinfo->nr_of_channels=local_arg->btfile.nr_of_channels;
  if (tinfo->nr_of_channels<=0 || tinfo->nr_of_channels>MAX_NUMBER_OF_CHANNELS_IN_EP) {
   ERREXIT1(tinfo->emethods, "read_freiburg_init: Impossible: %d channels?\n", MSGPARM(tinfo->nr_of_channels));
  }
  if (local_arg->nr_of_channels==0) {
   local_arg->nr_of_channels=tinfo->nr_of_channels;
  } else {
   if (local_arg->nr_of_channels!=tinfo->nr_of_channels) {
    ERREXIT2(tinfo->emethods, "read_freiburg_init: Setup has %d channels, but the file has %d!\n", MSGPARM(local_arg->nr_of_channels), MSGPARM(tinfo->nr_of_channels));
   }
  }
  local_arg->sfreq=(args[ARGS_SFREQ].is_set ? args[ARGS_SFREQ].arg.d : 1.0e6/local_arg->btfile.sampling_interval_us);
  tinfo->sfreq=local_arg->sfreq;
  if (tinfo->sfreq<=0.0 || tinfo->sfreq>MAX_POSSIBLE_SFREQ) {
   ERREXIT1(tinfo->emethods, "read_freiburg_init: Impossible: sfreq=%gHz?\n", MSGPARM(tinfo->sfreq));
  }

  /*{{{  Parse arguments that can be in seconds*/
  tinfo->nr_of_points=gettimeslice(tinfo, args[ARGS_NCHANNELS].arg.s);
  if (tinfo->nr_of_points<=0) {
   /* Read the whole file as one epoch */
   TRACEMS1(tinfo->emethods, 1, "read_freiburg_init: Reading %ld points\n",tinfo->points_in_file);
   tinfo->nr_of_points=tinfo->points_in_file;
  }
  local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
  local_arg->epochlength=tinfo->nr_of_points;
  /*}}}  */

  tinfo->beforetrig= -local_arg->offset;
  tinfo->aftertrig=tinfo->nr_of_points+local_arg->offset;
  /*}}}  */
 } else {
  local_arg->in_channels= &freiburg_channels[0];
  local_arg->current_trigger=0;
  if (stat(args[ARGS_IFILE].arg.s, &statbuf)!=0) {
   ERREXIT1(tinfo->emethods, "read_freiburg_init: Can't stat file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
  local_arg->nr_of_channels=atoi(args[ARGS_NCHANNELS].arg.s);
  /* average mode if the name does not belong to a directory */
  local_arg->average_mode= !S_ISDIR(statbuf.st_mode);
  tinfo->nr_of_channels=MAX_NUMBER_OF_CHANNELS_IN_EP;
 }
 freiburg_get_segment_init(tinfo);
 local_arg->current_point=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_freiburg(transform_info_ptr tinfo) {*/
/*
 * The method read_freiburg() returns a pointer to the allocated tsdata memory
 * location or NULL if End Of File encountered. EOF within the data set
 * results in an ERREXIT; NULL is returned ONLY if the first read resulted
 * in an EOF condition. This allows programs to look for multiple data sets
 * in a single file.
 */

METHODDEF DATATYPE *
read_freiburg(transform_info_ptr tinfo) {
 struct read_freiburg_storage *local_arg=(struct read_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 struct freiburg_channels_struct *in_channels=local_arg->in_channels;
 array myarray;
 FILE *infile;
 int i, point, channel;
 int trial_not_accepted;
 long length_of_data;
 short header[FREIBURG_HEADER_LENGTH/sizeof(short)];
 char const *last_sep=strrchr(args[ARGS_IFILE].arg.s, PATHSEP);
 char const *last_component=(last_sep==NULL ? args[ARGS_IFILE].arg.s : last_sep+1);
 char comment[MAX_COMMENTLEN];

 tinfo->nrofaverages=1;
 if (args[ARGS_CONTINUOUS].is_set) {
  /*{{{  Read an epoch from a continuous file*/
  infile=local_arg->infile;

  if (local_arg->epochs--==0) return NULL;

  /*{{{  Configure myarray*/
  myarray.element_skip=tinfo->itemsize=1;
  myarray.nr_of_vectors=tinfo->nr_of_points=local_arg->epochlength;
  myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->nr_of_channels;
  if (array_allocate(&myarray)==NULL) {
   ERREXIT(tinfo->emethods, "read_freiburg: Error allocating data\n");
  }
  tinfo->multiplexed=TRUE;
  /*}}}  */

  do {
   do {
    if (local_arg->segment_table.buffer_start!=NULL && local_arg->current_point%SEGMENT_LENGTH==0) {
     int const segment=local_arg->current_point/SEGMENT_LENGTH;
     long const nr_of_segments=local_arg->segment_table.current_length/sizeof(long);
     if (segment>=nr_of_segments) {
      array_free(&myarray);
      return NULL;
     }
     fseek(infile, ((long *)local_arg->segment_table.buffer_start)[segment], SEEK_SET);
     local_arg->no_last_values=TRUE;
    }
    if (freiburg_get_segment(tinfo, &myarray)!=0) {
     array_free(&myarray);
     return NULL;
    }
   } while (myarray.message!=ARRAY_ENDOFSCAN);
  } while (--local_arg->fromepoch>0);
  snprintf(comment, MAX_COMMENTLEN, "%s %s %02d/%02d/%04d,%02d:%02d:%02d", (local_arg->continuous_type==SLEEP_BT_TYPE ? "sleep_BT" : "sleep_KL"), last_component, local_arg->btfile.start_month, local_arg->btfile.start_day, local_arg->btfile.start_year, local_arg->btfile.start_hour, local_arg->btfile.start_minute, local_arg->btfile.start_second);
  tinfo->sfreq=local_arg->sfreq;
  /*}}}  */
 } else
 /*{{{  Read an epoch from a single (epoched or averaged) file*/
 do {	/* Return here if trial was not accepted */
 trial_not_accepted=FALSE;

 if (local_arg->epochs--==0 || (local_arg->average_mode && local_arg->current_trigger>=1)) return NULL;

 if (local_arg->average_mode) {
  /*{{{  Prepare reading an averaged file*/
  if((infile=fopen(args[ARGS_IFILE].arg.s,"rb"))==NULL) {
   ERREXIT1(tinfo->emethods, "read_freiburg: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
  fseek(infile, 0, SEEK_END);
  length_of_data=ftell(infile)-FREIBURG_HEADER_LENGTH;
  local_arg->current_trigger++;
  snprintf(comment, MAX_COMMENTLEN, "Freiburg-avg %s", last_component);
  /*}}}  */
 } else {
  /*{{{  Build pathname of the single trial to read*/
  /* A directory name was given. */
  char const * const expnumber=args[ARGS_IFILE].arg.s+strlen(args[ARGS_IFILE].arg.s)-2;
#ifdef __GNUC__
#define NEWFILENAME_SIZE (strlen(args[ARGS_IFILE].arg.s)+strlen(last_component)+20)
#else
#define NEWFILENAME_SIZE MAX_PATHLEN
#endif
  char newfilename[NEWFILENAME_SIZE];
  do {	/* Files from which only the parameter part exists are rejected */
   int div100=local_arg->current_trigger/100, mod100=local_arg->current_trigger%100;
   snprintf(newfilename, NEWFILENAME_SIZE, "%s%c%s%02d%c%c%s%02d%02d", args[ARGS_IFILE].arg.s, PATHSEP, last_component, div100, PATHSEP, tolower(expnumber[-1]), expnumber, div100, mod100);
   TRACEMS1(tinfo->emethods, 1, "read_freiburg: Constructed filename %s\n", MSGPARM(newfilename));

   if((infile=fopen(newfilename,"rb"))==NULL) {
    if (local_arg->current_trigger==0) {
     ERREXIT1(tinfo->emethods, "read_freiburg: Can't open file %s\n", MSGPARM(newfilename));
    } else {
     return NULL;
    }
   }
   fseek(infile, 0, SEEK_END);
   length_of_data=ftell(infile)-FREIBURG_HEADER_LENGTH;
   local_arg->current_trigger++;
  } while (length_of_data==0 || --local_arg->fromepoch>0);
  snprintf(comment, MAX_COMMENTLEN, "Freiburg-raw %s", last_component);
  /*}}}  */
 }

 /*{{{  Read the 64-Byte header*/
 fseek(infile, 0, SEEK_SET);
 fread(header, 1, FREIBURG_HEADER_LENGTH, infile);
 for (i=0; i<18; i++) {
# ifdef LITTLE_ENDIAN
  Intel_short((unsigned short *)&header[i]);
# endif
  TRACEMS2(tinfo->emethods, 3, "read_freiburg: Header %02d: %d\n", MSGPARM(i), MSGPARM(header[i]));
 }
 /*}}}  */

 if (local_arg->average_mode) {
  if ((length_of_data/sizeof(short))%local_arg->nr_of_channels!=0) {
   ERREXIT2(tinfo->emethods, "read_freiburg: length_of_data=%d does not fit with nr_of_channels=%d\n", MSGPARM(length_of_data), MSGPARM(local_arg->nr_of_channels));
  }
  tinfo->nr_of_points=(length_of_data/sizeof(short))/local_arg->nr_of_channels;
  local_arg->sfreq=(args[ARGS_SFREQ].is_set ? args[ARGS_SFREQ].arg.d : 200.0); /* The most likely value */
 } else {
  local_arg->nr_of_channels=header[14];
  /* Note: In a lot of experiments, one point MORE is available in the data
   * than specified in the header, but this occurs erratically. We could only
   * check for this during decompression, but that's probably too much fuss. */
  tinfo->nr_of_points=header[16]/header[15];
  local_arg->sfreq=(args[ARGS_SFREQ].is_set ? args[ARGS_SFREQ].arg.d : 1000.0/header[15]);
 }
 tinfo->sfreq=local_arg->sfreq;
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */
 tinfo->beforetrig= -local_arg->offset;
 tinfo->aftertrig=tinfo->nr_of_points+local_arg->offset;

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->nr_of_channels;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_freiburg: Error allocating data\n");
 }
 tinfo->multiplexed=TRUE;
 /*}}}  */

 fseek(infile, FREIBURG_HEADER_LENGTH, SEEK_SET);
 if (local_arg->average_mode) {
  /*{{{  Read averaged epoch*/
  for (point=0; point<tinfo->nr_of_points; point++) {
#ifdef __GNUC__
   short buffer[tinfo->nr_of_channels];
#else
   short buffer[MAX_NR_OF_CHANNELS];
#endif
   if ((int)fread(buffer, sizeof(short), tinfo->nr_of_channels, infile)!=tinfo->nr_of_channels) {
    ERREXIT1(tinfo->emethods, "read_freiburg: Error reading data point %d\n", MSGPARM(point));
   }
   for (channel=0; channel<tinfo->nr_of_channels; channel++) {
#  ifdef LITTLE_ENDIAN
    Intel_short((unsigned short *)&buffer[channel]);
#  endif
    array_write(&myarray, (DATATYPE)buffer[channel]);
   }
  }
  /*}}}  */
 } else {
  /*{{{  Read compressed single trial*/
#  ifdef DISPLAY_ADJECTIVES
  char *adjektiv_ende=strchr(((char *)header)+36, ' ');
  if (adjektiv_ende!=NULL) *adjektiv_ende='\0';
  printf("Single trial: nr_of_points=%d, nr_of_channels=%d, Adjektiv=%s\n", tinfo->nr_of_points, tinfo->nr_of_channels, ((char *)header)+36);
#  endif
  local_arg->infile=infile;
  do {
   if (freiburg_get_segment(tinfo, &myarray)!=0) {
    TRACEMS1(tinfo->emethods, 0, "read_freiburg: Decompression out of bounds for trial %d - skipped\n", MSGPARM(local_arg->current_trigger-1));
    array_free(&myarray);
    trial_not_accepted=TRUE;
    break;
   }
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  /*}}}  */
 }
 fclose(infile);
 } while (trial_not_accepted);
 /*}}}  */

 /*{{{  Look for a Freiburg channel setup for this number of channels*/
 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, free_tinfo will free someone else's data ! */
 tinfo->channelnames=NULL; tinfo->probepos=NULL;
 if (local_arg->channelnames!=NULL) {
  copy_channelinfo(tinfo, local_arg->channelnames, NULL);
 } else {
  while (in_channels->nr_of_channels!=0 && in_channels->nr_of_channels!=tinfo->nr_of_channels) in_channels++;
  if (in_channels->nr_of_channels==0) {
   TRACEMS1(tinfo->emethods, 1, "read_freiburg: Don't have a setup for %d channels.\n", MSGPARM(tinfo->nr_of_channels));
  } else {
   copy_channelinfo(tinfo, in_channels->channelnames, NULL);
  }
 }
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */
 /*}}}  */
 
 if ((tinfo->comment=(char *)malloc(strlen(comment)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_freiburg: Error allocating comment memory\n");
 }
 strcpy(tinfo->comment, comment);

 if (local_arg->zero_idiotism_warned==FALSE && local_arg->zero_idiotism_encountered) {
  TRACEMS(tinfo->emethods, 0, "read_freiburg: All-zero data points have been encountered and omitted!\n");
  local_arg->zero_idiotism_warned=TRUE;
 }
 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_freiburg_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_freiburg_exit(transform_info_ptr tinfo) {
 struct read_freiburg_storage *local_arg=(struct read_freiburg_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_CONTINUOUS].is_set) {
  fclose(local_arg->infile);
 }

 freiburg_get_segment_free(tinfo);

 if (local_arg->channelnames!=NULL) {
  free_pointer((void **)&local_arg->channelnames[0]);
  free_pointer((void **)&local_arg->channelnames);
 }
 free_pointer((void **)&local_arg->uV_per_bit);
 growing_buf_free(&local_arg->segment_table);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_freiburg(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_freiburg(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_freiburg_init;
 tinfo->methods->transform= &read_freiburg;
 tinfo->methods->transform_exit= &read_freiburg_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_freiburg";
 tinfo->methods->method_description=
  "Get-epoch method to read data in Freiburg (Krieger, Lis) format.\n"
  " If a directory name was given, (compressed) single trials are read from\n"
  " files arranged in its subdirectories.\n"
  " If the name points to a file, then this file is read as an average\n"
  " (uncompressed single epoch). The nr_of_channels argument is only\n"
  " used for average files, since the information is missing from these.\n"
  " If -c is given, an epoch length must be specified instead of nr_of_channels.\n"
  " In this case, if Inputfile exists, it is taken as an OS/9 (KL) file,\n"
  " and if Inputfile.co exists, it is taken as an MSDOS (BT) file.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_freiburg_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
