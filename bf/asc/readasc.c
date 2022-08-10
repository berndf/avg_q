/*
 * Copyright (C) 2008,2010-2015,2017,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * readasc.c module to read data from an ascii file
 *	-- Bernd Feige 20.01.1993
 *
 * This module allocates any memory necessary to hold channel names,
 * positions and data entries itself.
 *
 * If (tinfo->filename eq "stdin") tinfo->fileptr=stdin;
 *
 * For the binary format, the byte-swap issue (little vs big endian) is
 * handled by readasc, providing read compatibility between Intel and other
 * processors. writeasc only writes the byte order of the local machine.
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "Intel_compat.h"
#include "ascfile.h"
/*}}}  */

/*{{{  #defines*/
/* Each single data line must fit into this buffer */
#define INBUFSIZE (16*1024)

LOCAL const char *zaxis_delimiter=ZAXIS_DELIMITER;
LOCAL const char *keywords[]={
 "Sfreq=",
 "BeforeTrig=",
 "Nr_of_averages=",
 "Leaveright=",
 "Condition=",
 "Triggers=",
};
enum keyword_numbers {
 KEY_SFREQ=0, KEY_BEFORETRIG, KEY_NROFAVERAGES, KEY_LEAVERIGHT,
 KEY_CONDITION,
 KEY_TRIGGERS,
 NR_OF_KEYWORDS
};
enum ARGS_ENUM {
 ARGS_CLOSE=0, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Close and reopen the file for each epoch", "c", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"}
};
/*}}}  */

#ifdef FLOAT_DATATYPE
#define C_OTHERDATATYPE double
#endif
#ifdef DOUBLE_DATATYPE
#define C_OTHERDATATYPE float
#endif

/*{{{  Definition of readasc_storage*/
/* This structure has to be defined to carry the sampling freq over epochs */
struct readasc_storage {
 FILE *ascfile;
 DATATYPE sfreq;
 int beforetrig;
 int offset;
 int leaveright;
 Bool Intel_Fix;
 Bool binary;
 Bool otherdatatype; /* Indicates float data where DATATYPE==double or vice versa */
 int fromepoch;
 int epochs;
 int current_epoch;
 int nrofaverages;
};
#define ASCFILEPTR (local_arg->ascfile)
/*}}}  */

/*{{{  assign_key_value(transform_info_ptr tinfo, enum keyword_numbers keynum, char *where) {*/
LOCAL void
assign_key_value(transform_info_ptr tinfo, enum keyword_numbers keynum, char *where) {
 switch (keynum) {
  case KEY_SFREQ:
   tinfo->sfreq=atof(where);
   break;
  case KEY_BEFORETRIG: {
   struct readasc_storage * const local_arg=(struct readasc_storage *)tinfo->methods->local_storage;
   tinfo->beforetrig=atoi(where)-local_arg->offset;
   }
   break;
  case KEY_NROFAVERAGES:
   tinfo->nrofaverages=atoi(where);
   break;
  case KEY_LEAVERIGHT:
   tinfo->leaveright=atoi(where);
   break;
  case KEY_CONDITION:
   tinfo->condition=atoi(where);
   break;
  case KEY_TRIGGERS: {
   char *inbuf;
   long pos=strtol(where, &inbuf, 10);
   int code;

   /* Allocate trigger memory, if necessary, and set file position: */
   push_trigger(&tinfo->triggers, pos, -1, NULL);
   while (*inbuf==',') {
    pos=strtol(inbuf+1, &inbuf, 10);
    if (*inbuf!=':') {
     ERREXIT1(tinfo->emethods, "assign_key_value: Malformed trigger list >%s<\n", MSGPARM(where));
    }
    code=strtol(inbuf+1, &inbuf, 10);
    push_trigger(&tinfo->triggers, pos, code, NULL);
   }
   if (*inbuf!='\0') {
    ERREXIT1(tinfo->emethods, "assign_key_value: Malformed trigger list >%s<\n", MSGPARM(where));
   }
   push_trigger(&tinfo->triggers, 0L, 0, NULL); /* End of list */
   }
  case NR_OF_KEYWORDS:
   break;
 }
}
/*}}}  */

/*{{{  Local open_file and close_file routines*/
/*{{{  fskip_lines(FILE *filep, int count): Local function to skip count lines*/
LOCAL void
fskip_lines(FILE *filep, int count) {
 signed char c;

 while ((c=getc(filep))!=EOF) {
  if (c=='\n') {
   if (--count<=0) break;
  }
 }
}
/*}}}  */

/*{{{  readasc_open_file(transform_info_ptr tinfo) {*/
LOCAL void
readasc_open_file(transform_info_ptr tinfo) {
 struct readasc_storage *local_arg=(struct readasc_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char inbuf[INBUFSIZE];
 uint16_t shortbuf, lensum=0;
 uint16_t magic;
 int i, len, skipepochs=local_arg->fromepoch-1;
 Bool Intel_Fix=FALSE;
 long save_filepos;
 FILE *ascfileptr;

 TRACEMS1(tinfo->emethods, 1, "readasc: Opening file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
 if (strcmp(args[ARGS_IFILE].arg.s, "stdin")==0) ascfileptr=stdin;
 else if ((ascfileptr=fopen(args[ARGS_IFILE].arg.s, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "readasc_init: Can't open %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 ASCFILEPTR=ascfileptr;
 
 fread((void *)&magic, 2, 1, ascfileptr);
 local_arg->binary=(magic==OLD_ASC_BF_MAGIC || magic==ASC_BF_FLOAT_MAGIC || magic==ASC_BF_DOUBLE_MAGIC);
 if (!local_arg->binary) {
  Intel_int16(&magic);	/* Try swapping */
  if (magic==ASC_BF_FLOAT_MAGIC || magic==ASC_BF_DOUBLE_MAGIC) {
   local_arg->binary=Intel_Fix=TRUE;
  }
 }
 if (local_arg->binary) {
  /* Binary file: Read length of keyword section */
  fread((void *)&shortbuf, 2, 1, ascfileptr);
  if (Intel_Fix) Intel_int16(&shortbuf);
# ifdef FLOAT_DATATYPE
  local_arg->otherdatatype= (magic==ASC_BF_DOUBLE_MAGIC);
# endif
# ifdef DOUBLE_DATATYPE
  local_arg->otherdatatype= (magic==ASC_BF_FLOAT_MAGIC);
# endif
 } else {
  fseek(ascfileptr, -2, 1);
 }

 /*{{{  Parse arguments that can be in seconds*/
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */
 
 tinfo->sfreq=1.0;
 tinfo->beforetrig=0;
 tinfo->leaveright=0;
 tinfo->condition=0;
 tinfo->nrofaverages=1;
 while (TRUE) {
  if (local_arg->binary && lensum>=shortbuf) break;
  save_filepos=ftell(ascfileptr);
  fgets(inbuf, INBUFSIZE, ascfileptr);	/* An assignment header line */
  lensum+=strlen(inbuf);
  for (i=0; i<NR_OF_KEYWORDS; i++) {
   len=strlen(keywords[i]);
   /* A comment line may now also start with an assignment, but should be
    * processed in the transform section. Because a semicolon terminates the
    * assignment in this case, we can just stop on lines with a semicolon. */
   if (strncmp(inbuf, keywords[i], len)==0 && strchr(inbuf, ';')==NULL) break;
  }
  if (i<NR_OF_KEYWORDS) {
   assign_key_value(tinfo, (enum keyword_numbers)i, inbuf+len);
  } else {
   /* This is the end-of-variable-header condition with ascfiles. */
   if (local_arg->binary) {
    ERREXIT1(tinfo->emethods, "readasc_init: Unknown keyword in >%s<\n", MSGPARM(inbuf));
   } else {
    fseek(ascfileptr, save_filepos, 0);
   }
   break;
  }
 }
 /* Transfer the defaults possibly modified by assign_key_value to local_arg */
 local_arg->sfreq=tinfo->sfreq;
 local_arg->beforetrig=tinfo->beforetrig;
 local_arg->leaveright=tinfo->leaveright;
 local_arg->nrofaverages=tinfo->nrofaverages;
 
 /*{{{  Skip epochs if fromepoch>1*/
 while (skipepochs>0) {
  int nr_of_channels, nr_of_points, itemsize;
  if (local_arg->binary) {
   /*{{{  Skip binary data set*/
   int length_of_string_section;

   fread((void *)&nr_of_channels, sizeof(int), 1, ascfileptr);
   if (feof(ascfileptr)) {
    ERREXIT(tinfo->emethods, "readasc_init: Error seeking first epoch\n");
   }
   fread((void *)&nr_of_points, sizeof(int), 1, ascfileptr);
   fread((void *)&itemsize, sizeof(int), 1, ascfileptr);
   fseek(ascfileptr, (long)sizeof(int), 1);	/* Skip multiplexed value */
   fread((void *)&length_of_string_section, sizeof(int), 1, ascfileptr);
   if (Intel_Fix) {
    Intel_int((unsigned int *)&nr_of_channels);
    Intel_int((unsigned int *)&nr_of_points);
    Intel_int((unsigned int *)&itemsize);
    Intel_int((unsigned int *)&length_of_string_section);
   }

   fseek(ascfileptr, (long)(length_of_string_section+3*nr_of_channels*sizeof(double)+ (nr_of_channels*itemsize+1)*nr_of_points*(local_arg->otherdatatype ? sizeof(C_OTHERDATATYPE) : sizeof(DATATYPE))+sizeof(int)), 1);
   /*}}}  */
  } else {
   /*{{{  Skip ascii  data set*/
   fskip_lines(ascfileptr, 1);	/* Skip comment line */

   /*{{{  Process the size line*/
   itemsize=1;	/* The default value */
   /* If fscanf finds no match, it leaves all values untouched. So in order
    * to see if it fails or not, preset the values to something illegal: */
   nr_of_channels=nr_of_points=0;
   fscanf(ascfileptr, "channels=%d, points=%d, itemsize=%d", &nr_of_channels, &nr_of_points, &itemsize);
   if (nr_of_channels<1 || nr_of_points<1 || itemsize<1) {
    ERREXIT(tinfo->emethods, "readasc_init: Skip: Invalid data size indicators\n");
   }
   /*}}}  */

   /* Skip the size+channel names lines, 3 probe pos lines and the data lines */
   fskip_lines(ascfileptr, 5+nr_of_points);
   /*}}}  */
  }
  skipepochs--;
 }
 /*}}}  */

 local_arg->Intel_Fix=Intel_Fix;
}
/*}}}  */

/*{{{  readasc_close_file(transform_info_ptr tinfo) {*/
LOCAL void
readasc_close_file(transform_info_ptr tinfo) {
 struct readasc_storage *local_arg=(struct readasc_storage *)tinfo->methods->local_storage;
 if (ASCFILEPTR!=stdin) fclose(ASCFILEPTR);
 ASCFILEPTR=NULL;
}
/*}}}  */
/*}}}  */

/*{{{  readasc_init(transform_info_ptr tinfo) {*/
METHODDEF void
readasc_init(transform_info_ptr tinfo) {
 struct readasc_storage *local_arg=(struct readasc_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->current_epoch=local_arg->fromepoch-1;
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 if (!args[ARGS_CLOSE].is_set) readasc_open_file(tinfo);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  assignval(int elem_size, void *elem_ptr, double value) {*/
LOCAL void
assignval(int elem_size, void *elem_ptr, double value) {
 if (elem_size==sizeof(double)) {
  *(double *)elem_ptr=value;
 } else {
  *(float *)elem_ptr=value;
 }
}
/*}}}  */

/*{{{  readvalues(char *readbuf, int elem_size, int elem_skip, int itemsize, void *x_element, void *y_elements) {*/
/*
 * The function readvalues reads an x element and y elements from a text input 
 * line into the given memory locations; the y elements are seperated in memory 
 * by (elem_skip*elem_size*itemsize) bytes.
 * This function works with double and float targets as indicated by the
 * elem_size parameter.
 * The return value is the number of y_elements read. Words that aren't numbers
 * are counted as elements, but not assigned (Example: '-' above).
 * If absolutely NOTHING is found on the input line, the return value is -1.
 */

LOCAL int 
readvalues(char *readbuf, int elem_size, int elem_skip, int itemsize, void *x_element, void *y_elements) {
 int i= -1, itempart=0;
 char *inreadbuf, *endvalstr;
 double invalue;

 inreadbuf=readbuf;
 for (;;) {	/* Exiting is done through a break */
  /* Scan to next promising string. This is done to get the loop termination
   * and the i count right. Else, strtod could do this for us. */
  while (strchr(" \t()", *inreadbuf)!=NULL) inreadbuf++;
  if (*inreadbuf=='\n') break;

  invalue=strtod(inreadbuf, &endvalstr);
  if (inreadbuf!=endvalstr) {
   inreadbuf=endvalstr;
   if (i==-1) {
    assignval(elem_size, x_element, invalue);
    i++;
   } else {
    assignval(elem_size, ((char *)y_elements)+(i*elem_skip*itemsize+itempart)*elem_size, invalue);
    if (++itempart==itemsize) {
     itempart=0;
     i++;
    }
   }
  } else {
   i++;	/* A sole minus sign, for example, denotes a left-out value */
  }
  /* Skip whatever non-blank characters might remain */
  while (*inreadbuf!=' ' && *inreadbuf!='\t' && *inreadbuf!='\n') inreadbuf++;
 }

 return i;
}
/*}}}  */

/*{{{  read_datatype(struct readasc_storage *local_arg, DATATYPE *data, long length) {*/
/* This function performs the task of reading float or double data,
 * appropriately byte-swap and transfer it to DATATYPE *data */
LOCAL void
read_datatype(struct readasc_storage *local_arg, DATATYPE *data, long length) {
 if (local_arg->otherdatatype) {
  /* Data was written with the other DATATYPE: Read in a temp buffer,
   * then transfer the data converting the data type */
  C_OTHERDATATYPE *usestart=calloc(length,sizeof(C_OTHERDATATYPE));
  long i;
  fread((void *)usestart, sizeof(C_OTHERDATATYPE), length, ASCFILEPTR);
  for (i=0; i<length; i++) {
   if (local_arg->Intel_Fix) {
#   ifdef FLOAT_DATATYPE
    Intel_double(usestart+i);
#   else
    Intel_float(usestart+i);
#   endif
   }
   data[i]=usestart[i];
  }
  free(usestart);
 } else {
  fread((void *)data, sizeof(DATATYPE), length, ASCFILEPTR);
  if (local_arg->Intel_Fix) {
   long i;
   for (i=0; i<length; i++) {
#   ifdef FLOAT_DATATYPE
    Intel_float(data+i);
#   else
    Intel_double(data+i);
#   endif
   }
  }
 }
}
/*}}}  */

/*{{{  readasc(transform_info_ptr tinfo) {*/
/*
 * The method readasc() returns a pointer to the allocated tsdata memory
 * location or NULL if End Of File encountered. EOF within the data set
 * results in an ERREXIT; NULL is returned ONLY if the first read resulted
 * in an EOF condition. This allows programs to look for multiple data sets
 * in a single file.
 */

METHODDEF DATATYPE *
readasc(transform_info_ptr tinfo) {
 struct readasc_storage *local_arg=(struct readasc_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int i, length, lastnonblank;
 const Bool Intel_Fix=local_arg->Intel_Fix;
 char buf[INBUFSIZE], *inbuf=buf, *ininbuf, *namebuf, *innamebuf, *inchannelnames, *laststart;
 char *new_namebuf;
 FILE *ascfileptr=ASCFILEPTR;

 if (local_arg->epochs-- ==0) return NULL;

 if (args[ARGS_CLOSE].is_set) {
  readasc_open_file(tinfo);
  ascfileptr=ASCFILEPTR;
 }

 /* Initialize values also modifiable via local attributes from global defaults */
 tinfo->file_start_point=local_arg->current_epoch*tinfo->nr_of_points;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->leaveright=local_arg->leaveright;
 tinfo->nrofaverages=local_arg->nrofaverages;

 if (local_arg->binary) {
  /*{{{  Read binary data set*/
  int length_of_string_section;

  fread((void *)&tinfo->nr_of_channels, sizeof(int), 1, ascfileptr);
  if (feof(ascfileptr)) return NULL;
  fread((void *)&tinfo->nr_of_points, sizeof(int), 1, ascfileptr);
  fread((void *)&tinfo->itemsize, sizeof(int), 1, ascfileptr);
  fread((void *)&tinfo->multiplexed, sizeof(int), 1, ascfileptr);
  fread((void *)&length_of_string_section, sizeof(int), 1, ascfileptr);
  if (Intel_Fix) {
   /*{{{  Byte-swap the values just read*/
   Intel_int((unsigned int *)&tinfo->nr_of_channels);
   Intel_int((unsigned int *)&tinfo->nr_of_points);
   Intel_int((unsigned int *)&tinfo->itemsize);
   Intel_int((unsigned int *)&tinfo->multiplexed);
   Intel_int((unsigned int *)&length_of_string_section);
   /*}}}  */
  }

  tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
  if ((tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
      (tinfo->probepos=(double *)malloc(tinfo->nr_of_channels*3*sizeof(double)))==NULL ||
      (tinfo->xdata=(DATATYPE *)malloc(tinfo->nr_of_points*sizeof(DATATYPE)))==NULL ||
      (tinfo->tsdata=(DATATYPE *)malloc(tinfo->length_of_output_region*sizeof(DATATYPE)))==NULL ||
      (namebuf=(char *)malloc(length_of_string_section))==NULL) {
   ERREXIT(tinfo->emethods, "readasc: malloc error on string section or data/probe position buffers\n");
  }

  fread((void *)namebuf, sizeof(char), length_of_string_section, ascfileptr);

  innamebuf=namebuf+strlen(namebuf)+1;
  tinfo->xchannelname=innamebuf; /* Will be changed to the correct position shortly */
  innamebuf+=strlen(innamebuf)+1;
  length=innamebuf-namebuf; /* Length of comment+z_label+z_value+xchannelname */
  /*{{{  Process comment line*/
  new_namebuf=namebuf;
  do {
   int len;
   char *semipos=NULL;
   for (i=0; i<NR_OF_KEYWORDS; i++) {
    len=strlen(keywords[i]);
    if (strncmp(new_namebuf, keywords[i], len)==0 &&
        (semipos=strchr(new_namebuf,';'))!=NULL) {
     *semipos='\0';
     break;
    }
   }
   assign_key_value(tinfo, (enum keyword_numbers)i, new_namebuf+len);
   if (semipos!=NULL) {
    length-=semipos+1-new_namebuf;
    new_namebuf=semipos+1;
   }
  } while (i<NR_OF_KEYWORDS);

  if ((tinfo->comment=(char *)malloc(length))==NULL) {
   ERREXIT(tinfo->emethods, "readasc: malloc error on comment\n");
  }
  memcpy(tinfo->comment, new_namebuf, length);
  if ((ininbuf=strstr(tinfo->comment, zaxis_delimiter))!=NULL &&
      (laststart=strchr(ininbuf, '='))!=NULL) {
   *laststart++ = *ininbuf='\0';
   tinfo->z_label=ininbuf+strlen(zaxis_delimiter);
   tinfo->z_value=atof(laststart);
  } else {
   tinfo->z_label=NULL;
  }
  /*}}}  */
  tinfo->xchannelname=tinfo->comment+(tinfo->xchannelname-new_namebuf);

  if ((inchannelnames=(char *)malloc(length_of_string_section-length))==NULL) {
   ERREXIT(tinfo->emethods, "readasc: malloc error on channelnames\n");
  }
  i=0;
  while (innamebuf-namebuf<length_of_string_section) {
   tinfo->channelnames[i]=inchannelnames;
   strcpy(inchannelnames, innamebuf);
   inchannelnames+=strlen(inchannelnames)+1;
   innamebuf+=strlen(innamebuf)+1;
   i++;
  }
  free(namebuf);

  fread((void *)tinfo->probepos, sizeof(double), 3*tinfo->nr_of_channels, ascfileptr);
  read_datatype(local_arg, tinfo->xdata, tinfo->nr_of_points);
  read_datatype(local_arg, tinfo->tsdata, tinfo->length_of_output_region);
  /* Read the backwards skip count: */
  fread((void *)&length_of_string_section, sizeof(int), 1, ascfileptr);
  if (Intel_Fix) {
   /*{{{  Byte-swap the values just read*/
   Intel_int((unsigned int *)&length_of_string_section);
   for (i=0; i<3*tinfo->nr_of_channels; i++) {
    Intel_double(tinfo->probepos+i);
   }
   /*}}}  */
  }
  /*}}}  */
 } else {
 /*{{{  Read ascii data set*/
 int xchannelnamelength;
 /*{{{  Process comment line*/
 fgets(inbuf, INBUFSIZE, ascfileptr);	/* The comment line */
 if (feof(ascfileptr)) return NULL;
 new_namebuf=inbuf;
 do {
  int len;
  char *semipos=NULL;
  for (i=0; i<NR_OF_KEYWORDS; i++) {
   len=strlen(keywords[i]);
   if (strncmp(new_namebuf, keywords[i], len)==0 &&
       (semipos=strchr(new_namebuf,';'))!=NULL) {
    *semipos='\0';
    break;
   }
  }
  assign_key_value(tinfo, (enum keyword_numbers)i, new_namebuf+len);
  if (semipos!=NULL) {
   new_namebuf=semipos+1;
  }
 } while (i<NR_OF_KEYWORDS);

 length=strlen(new_namebuf)-1;	/* Throw away the '\n' */
 new_namebuf[length]='\0';
 i=0;	/* This means that no z axis label was found */
 if ((ininbuf=strstr(new_namebuf, zaxis_delimiter))!=NULL &&
     (innamebuf=strchr(ininbuf, '='))!=NULL) {
  *innamebuf++ = *ininbuf='\0';
  i=(length=ininbuf-new_namebuf)+1;	/* Offset tinfo->z_label-tinfo->comment */
  ininbuf+=strlen(zaxis_delimiter);
  length+=strlen(ininbuf)+1;
 }
 if ((tinfo->comment=(char *)malloc(length+1))==NULL) {
  ERREXIT(tinfo->emethods, "readasc: malloc error on comment\n");
 }
 strcpy(tinfo->comment, new_namebuf);
 if (i>0) {
  tinfo->z_label=tinfo->comment+i;
  strcpy(tinfo->z_label, ininbuf);
  tinfo->z_value=atof(innamebuf);
 } else {
  tinfo->z_label=NULL;
 }
 /*}}}  */

 /*{{{  Process the size line*/
 fgets(inbuf, INBUFSIZE, ascfileptr);	/* The size line */
 tinfo->itemsize=1;	/* The default value */
 /* If sscanf finds no match, it leaves all values untouched. So in order
  * to see if it fails or not, preset the values to something illegal: */
 tinfo->nr_of_channels=tinfo->nr_of_points=0;
 sscanf(inbuf, "channels=%d, points=%d, itemsize=%d", &tinfo->nr_of_channels, &tinfo->nr_of_points, &tinfo->itemsize);
 if (tinfo->nr_of_channels<1 || tinfo->nr_of_points<1 || tinfo->itemsize<1) {
  ERREXIT(tinfo->emethods, "readasc: Invalid data size indicators\n");
 }
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 if ((tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
     (tinfo->probepos=(double *)malloc(tinfo->nr_of_channels*3*sizeof(double)))==NULL ||
     (tinfo->xdata=(DATATYPE *)malloc(tinfo->nr_of_points*sizeof(DATATYPE)))==NULL ||
     (tinfo->tsdata=(DATATYPE *)malloc(tinfo->length_of_output_region*sizeof(DATATYPE)))==NULL) {
  ERREXIT(tinfo->emethods, "readasc: malloc error on data/probe position buffers\n");
 }
 /*}}}  */

 /*{{{  Process the channel names line*/
 fgets(inbuf, INBUFSIZE, ascfileptr);	/* The channel names line */

 /* First, count how many bytes are necessary to hold the names strings */
 length=lastnonblank=0; ininbuf=inbuf;
 do {
  if (strchr(" \t\n", *ininbuf)==NULL) {
   length++;
   lastnonblank=1;
  } else {
   if (lastnonblank) length++;
   lastnonblank=0;
  }
 } while (*ininbuf++!='\n');
 if ((namebuf=(char *)malloc(length))==NULL) {
  ERREXIT(tinfo->emethods, "readasc: malloc error on names buffer\n");
 }

 /* Now do the actual transfer */
 /* xchannelname has to be at the end because the allocation standard
  * specifies that channelnames[0] is free()able */
 xchannelnamelength=strcspn(inbuf, " \t\n");
 tinfo->xchannelname=namebuf+length-xchannelnamelength-1;
 strncpy(tinfo->xchannelname, inbuf, xchannelnamelength);
 tinfo->xchannelname[xchannelnamelength]='\0';
 inbuf+=xchannelnamelength+1;

 laststart=innamebuf=namebuf;
 i=0; lastnonblank=0; ininbuf=inbuf;
 do {
  if (strchr(" \t\n", *ininbuf)==NULL) {
   *innamebuf++ = *ininbuf;
   lastnonblank=1;
  } else {
   if (lastnonblank) {
    *innamebuf++='\0';
    tinfo->channelnames[i]=laststart;
    i++;
    laststart=innamebuf;
   }
   lastnonblank=0;
  }
 } while (*ininbuf++!='\n');
 /*}}}  */
 inbuf=buf;

 /*{{{  Read the three probe position lines*/
 for (i=0; i<3; i++) {
  fgets(inbuf, INBUFSIZE, ascfileptr);	/* The probe position lines */
  if (readvalues(inbuf, sizeof(double), 3, 1, tinfo->probepos+i, tinfo->probepos+i)!=tinfo->nr_of_channels) {
   ERREXIT(tinfo->emethods, "readasc: Read error on probe positions\n");
  }
 }
 /*}}}  */

 /*{{{  Read the data lines*/
 for (i=0; i<tinfo->nr_of_points; i++) {
  fgets(inbuf, INBUFSIZE, ascfileptr);	/* The data lines */
  if (readvalues(inbuf, sizeof(DATATYPE), tinfo->nr_of_points, tinfo->itemsize, tinfo->xdata+i, tinfo->tsdata+i*tinfo->itemsize)!=tinfo->nr_of_channels) {
   ERREXIT(tinfo->emethods, "readasc: Read error on data\n");
  }
 }
 /*}}}  */

 tinfo->multiplexed=FALSE;	/* The data just read is non-multiplexed */
 /*}}}  */
 }

 if (args[ARGS_CLOSE].is_set) readasc_close_file(tinfo);

 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->data_type=TIME_DATA;
 local_arg->current_epoch++;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  readasc_exit(transform_info_ptr tinfo) {*/
METHODDEF void
readasc_exit(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;

 if (!args[ARGS_CLOSE].is_set) readasc_close_file(tinfo);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_readasc(transform_info_ptr tinfo) {*/
GLOBAL void
select_readasc(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &readasc_init;
 tinfo->methods->transform= &readasc;
 tinfo->methods->transform_exit= &readasc_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="readasc";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from asc files.\n";
 tinfo->methods->local_storage_size=sizeof(struct readasc_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
