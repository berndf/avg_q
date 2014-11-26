/*
 * Copyright (C) 1996-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_vitaport.c module to read data from the binary vitaport format
 *	-- Bernd Feige 17.12.1996
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <ctype.h>
#include <read_struct.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
#include "vitaport.h"
/*}}}  */

#define MAX_COMMENTLEN 1024
#define MAX_BUFFERED_TRIGGERS 3
#define TRIGCODE_FOR_STARTMARKER 1
#define TRIGCODE_FOR_ENDMARKER 2

enum ARGS_ENUM {
 ARGS_CONTINUOUS=0, 
 ARGS_TRIGTRANSFER, 
 ARGS_MARKER, 
 ARGS_TRIGFILE,
 ARGS_TRIGLIST, 
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Use marker table at the file end rather than the MARKER channel", "M", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.vpd"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
};

/*{{{  Definition of read_vitaport_storage*/
struct read_vitaport_storage {
 struct vitaport_fileheader fileheader;
 struct vitaportI_fileheader fileheaderI;
 struct vitaportII_fileheader fileheaderII;
 struct vitaportII_fileext fileext;
 enum vitaport_filetypes vitaport_filetype;
 struct vitaport_channelheader *channelheaders;
 struct vitaportIIrchannelheader *channelheadersII;
 uint16_t checksum;
 FILE *infile;
 FILE *triggerfile;
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 long sum_dlen;
 long current_point;
 long current_triggerpoint;
 long SizeofHeader;
 long points_in_file;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;

 unsigned int min_sfac;
 unsigned int max_sfac;
 unsigned int bytes_per_max_sfac;
 DATATYPE ceil_sfreq;	/* The sampling frequency from which channel rates are derived by division */
 DATATYPE max_sfreq;
 char **channelnames;
 unsigned int channelnames_length;
 unsigned int *sampling_step;

 DATATYPE *channelbuffer;	/* This is to buffer the interlaced values in the VITAPORT2RFILE format */
};
/*}}}  */

/*{{{  actual_fieldlength(char *thisentry, char *nextentry) {*/
LOCAL int 
actual_fieldlength(char *thisentry, char *nextentry) {
 char *endchar=nextentry-1;
 while (endchar>=thisentry && (*endchar=='\0' || strchr(" \t\r\n", *endchar)!=NULL)) endchar--;
 return (endchar-thisentry+1);
}
/*}}}  */

/*{{{  Single point interface - Not very useful with this format */

/*{{{  read_vitaport_get_filestrings: Allocate and set strings and probepos array*/
LOCAL void
read_vitaport_get_filestrings(transform_info_ptr tinfo) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;
 char *innamebuf;
 int channel;

 tinfo->xdata=NULL;
 tinfo->probepos=NULL;
 if ((tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
     (tinfo->comment=(char *)malloc(MAX_COMMENTLEN))==NULL ||
     (innamebuf=(char *)malloc(local_arg->channelnames_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_vitaport: Error allocating channelnames\n");
 }
 memcpy(innamebuf, local_arg->channelnames[0], local_arg->channelnames_length);
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  tinfo->channelnames[channel]=innamebuf+(local_arg->channelnames[channel]-local_arg->channelnames[0]);
 }
 create_channelgrid(tinfo);
}
/*}}}  */

/*}}}  */

/*{{{  read_vitaport_nexttrigger(transform_info_ptr tinfo, long *trigpoint) {*/
/*{{{  Maintaining the triggers list*/
LOCAL void 
read_vitaport_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}

/*{{{  read_vitaport_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_vitaport_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_vitaport_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_vitaport_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  int channel, marker_channel= -1;
  int const NoOfChannels=local_arg->fileheader.knum;
  /* Hmmm, first we saw only the 'MARKER' variant but then also 'Marker' was
   * floating around... Better use strncasecmp since this would be too dumb
   * a reason to not be able to read files we encounter... */
  for (channel=0; channel<NoOfChannels; channel++) {
   if (strncasecmp(local_arg->channelheaders[channel].kname, VP_MARKERCHANNEL_NAME, VP_CHANNELNAME_LENGTH)==0) {
    marker_channel=channel;
   }
  }
  if (local_arg->vitaport_filetype==VITAPORT2_FILE && (marker_channel<0 || args[ARGS_MARKER].is_set)) {
   Bool found_markertable=FALSE;
   if (marker_channel<0 && !args[ARGS_MARKER].is_set) {
    TRACEMS(tinfo->emethods, 0, "read_vitaport_build_trigbuffer: No marker channel. Trying Vitaport II event table...\n");
   }
   TRACEMS(tinfo->emethods, 1, "read_vitaport_build_trigbuffer: Reading Vitaport II event table\n");
   fseek(local_arg->infile, local_arg->sum_dlen, SEEK_CUR);
   while (1) {
    char label[VP_TABLEVAR_LENGTH+1];
    uint32_t length;
    struct vitaport_idiotic_multiplemarkertablenames *markertable_entry;
    label[VP_TABLEVAR_LENGTH]='\0';
    fread(label, 1, VP_TABLEVAR_LENGTH, local_arg->infile);
    if (feof(local_arg->infile)) break;
    fread(&length, sizeof(length), 1, local_arg->infile);
#ifdef LITTLE_ENDIAN
    Intel_int32((uint32_t *)&length);
#endif
    for (markertable_entry=markertable_names; markertable_entry->markertable_name!=NULL && strcmp(label, markertable_entry->markertable_name)!=0; markertable_entry++);
    if (markertable_entry->markertable_name!=NULL) {
     uint32_t millisec;
     int tagno;
     int const ntags=length/sizeof(millisec);
     found_markertable=TRUE;
     for (tagno=0; tagno<ntags; tagno++) {
      fread(&millisec, sizeof(millisec), 1, local_arg->infile);
#ifdef LITTLE_ENDIAN
      Intel_int32((uint32_t *)&millisec);
#endif
      if (millisec<0) {
       /* Usually, there are 100 tags reserved and the unused at the end
	* filled with -1 */
       break;
      }
      push_trigger(&local_arg->triggers, (long)rint(millisec*tinfo->sfreq/1000.0), (millisec%2==0 ? TRIGCODE_FOR_STARTMARKER : TRIGCODE_FOR_ENDMARKER), NULL);
     }
    } else {
     fseek(local_arg->infile, length, SEEK_CUR);
    }
   }
   if (!found_markertable) {
    TRACEMS(tinfo->emethods, 0, "read_vitaport_build_trigbuffer: No event table found!\n");
   }
  } else {
   if (marker_channel<0) {
    TRACEMS(tinfo->emethods, 0, "read_vitaport_build_trigbuffer: No marker channel found!\n");
   } else {
    unsigned int const this_size=local_arg->channelheaders[marker_channel].dasize+1;
    unsigned int const sstep=local_arg->sampling_step[marker_channel];
    float thispoint=((float)local_arg->current_triggerpoint)/sstep;
    long lastpoint=(long)thispoint;
    long current_triggerpoint=0;
    int code=0;
    TRACEMS(tinfo->emethods, 1, "read_vitaport_build_trigbuffer: Analyzing marker channel\n");
    fseek(local_arg->infile, local_arg->SizeofHeader+local_arg->channelheadersII[marker_channel].doffs+lastpoint*this_size, SEEK_SET);
    while (current_triggerpoint<local_arg->points_in_file) {
     switch (this_size) {
      case 1: {
       unsigned char s;
       fread(&s, sizeof(s), 1, local_arg->infile);
       code=s;
       }
       break;
      case 2: {
       uint16_t s;
       fread(&s, sizeof(s), 1, local_arg->infile);
#ifdef LITTLE_ENDIAN
       Intel_int16(&s);
#endif
       code=s;
       }
       break;
      default:
       continue;
     }
     if (code!=0) {
      push_trigger(&local_arg->triggers, current_triggerpoint, code, NULL);
      while (current_triggerpoint<local_arg->points_in_file) {
       do {
	current_triggerpoint++;
	thispoint=((float)current_triggerpoint)/sstep;
       } while (lastpoint==(int)thispoint);
       lastpoint=(int)thispoint;
       code=0;
       switch (this_size) {
	case 1: {
	 unsigned char s;
	 fread(&s, sizeof(s), 1, local_arg->infile);
	 code=s;
	 }
	 break;
	case 2: {
	 uint16_t s;
	 fread(&s, sizeof(s), 1, local_arg->infile);
#ifdef LITTLE_ENDIAN
	 Intel_int16(&s);
#endif
	 code=s;
	 }
	 break;
	default:
	 continue;
       }
       if (code==0) break;
      }
     }
     do {
      current_triggerpoint++;
      thispoint=((float)current_triggerpoint)/sstep;
     } while (lastpoint==(long)thispoint);
     lastpoint=(long)thispoint;
    }
   }
  }
 }
}
/*}}}  */

/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_vitaport_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_vitaport_build_trigbuffer(tinfo);
 }
 {
 int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);

 if (local_arg->current_trigger<nevents) {
  struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+local_arg->current_trigger;
  *position=intrig->position;
   code    =intrig->code;
  if (descriptionp!=NULL) *descriptionp=intrig->description;
 }
 }
 local_arg->current_trigger++;
 return code;
}
/*}}}  */
/*}}}  */

/*{{{  read_vitaport_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_vitaport_init(transform_info_ptr tinfo) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel, NoOfChannels;
 char *innames;
 unsigned long max_NumSamples=0;

 growing_buf_init(&local_arg->triggers);

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 if((local_arg->infile=fopen(args[ARGS_IFILE].arg.s,"rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 if (read_struct((char *)&local_arg->fileheader, sm_vitaport_fileheader, local_arg->infile)==0) {
  ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't read header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifdef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->fileheader, sm_vitaport_fileheader);
#endif
 if (local_arg->fileheader.Sync!=VITAPORT_SYNCMAGIC) {
  ERREXIT1(tinfo->emethods, "read_vitaport_init: Sync value is %4x, not 4afc\n", MSGPARM(local_arg->fileheader.Sync));
 }
 switch (local_arg->fileheader.hdtyp) {
  case HDTYP_VITAPORT1:
   TRACEMS1(tinfo->emethods, 1, "Opening VitaPort I file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
   if (read_struct((char *)&local_arg->fileheaderI, sm_vitaportI_fileheader, local_arg->infile)==0) {
    ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't read header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
   }
#ifdef LITTLE_ENDIAN
   change_byteorder((char *)&local_arg->fileheaderI, sm_vitaportI_fileheader);
#endif
   Intel_int16((uint16_t *)&local_arg->fileheaderI.blckslh);	/* These is in low-high order, ie always reversed */
   break;
  case HDTYP_VITAPORT2:
  case HDTYP_VITAPORT2P1:
   TRACEMS1(tinfo->emethods, 1, "Opening VitaPort II file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
   if (read_struct((char *)&local_arg->fileheaderII, sm_vitaportII_fileheader, local_arg->infile)==0) {
    ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't read header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
   }
#ifdef LITTLE_ENDIAN
   change_byteorder((char *)&local_arg->fileheaderII, sm_vitaportII_fileheader);
#endif
   convert_fromhex(&local_arg->fileheaderII.Hour);
   convert_fromhex(&local_arg->fileheaderII.Minute);
   convert_fromhex(&local_arg->fileheaderII.Second);
   convert_fromhex(&local_arg->fileheaderII.Day);
   convert_fromhex(&local_arg->fileheaderII.Month);
   convert_fromhex(&local_arg->fileheaderII.Year);
   switch (local_arg->fileheader.chnoffs) {
    case 22:
     local_arg->vitaport_filetype=VITAPORT1_FILE;
     local_arg->ceil_sfreq=400.0;	/* Isitso? */
     break;
    case 36:
     local_arg->vitaport_filetype=((local_arg->fileheader.hdlen&511)==0 ? VITAPORT2RFILE : VITAPORT2_FILE);
     if (read_struct((char *)&local_arg->fileext, sm_vitaportII_fileext, local_arg->infile)==0) {
      ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't read header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
     }
#ifdef LITTLE_ENDIAN
     change_byteorder((char *)&local_arg->fileext, sm_vitaportII_fileext);
#endif
     local_arg->ceil_sfreq=VITAPORT_CLOCKRATE/local_arg->fileheaderII.scnrate;
     break;
    default:
     ERREXIT1(tinfo->emethods, "read_vitaport_init: Unknown Vitaport II channel offset %d\n", MSGPARM(local_arg->fileheader.chnoffs));
   }
   break;
  default:
   ERREXIT1(tinfo->emethods, "read_vitaport_init: Unknown header type value %d\n", MSGPARM(local_arg->fileheader.hdtyp));
 }
 NoOfChannels=local_arg->fileheader.knum;
 if ((local_arg->channelheaders  =(struct vitaport_channelheader *)malloc(NoOfChannels*sizeof(struct vitaport_channelheader)))==NULL
  || (local_arg->channelheadersII=(struct vitaportIIrchannelheader *)malloc(NoOfChannels*sizeof(struct vitaportIIrchannelheader)))==NULL) {
  ERREXIT(tinfo->emethods, "read_vitaport_init: Error allocating channel headers\n");
 }
 local_arg->channelnames_length=0;
 local_arg->sum_dlen=0;
 local_arg->max_sfac=1;
 local_arg->min_sfac=SHRT_MAX;
 local_arg->bytes_per_max_sfac=0;
 for (channel=0; channel<NoOfChannels; channel++) {
  unsigned int this_sfac,this_size;

  if (read_struct((char *)&local_arg->channelheaders[channel], sm_vitaport_channelheader, local_arg->infile)==0) {
   ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't read channel header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifdef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->channelheaders[channel], sm_vitaport_channelheader);
#endif

  /* Let datype be either VP_DATYPE_SIGNED or VP_DATYPE_UNSIGNED so that the reading
   * code knows what to do.
   * The earlier heuristic to disregard datype and treat channels as signed if
   * offset==0 didn't always work, hopefully this is better...
   * I've seen datype's of both 4 and 5 for unsigned values, and we use 0 for signed.
   * Values for the marker channel vary, I've seen 130 (kunit='mrk') and 154
   * (kunit='bit', raw files). */
  if (local_arg->channelheaders[channel].datype!=VP_DATYPE_SIGNED) {
   local_arg->channelheaders[channel].datype=VP_DATYPE_UNSIGNED;
  }
  this_sfac=local_arg->channelheaders[channel].scanfc*local_arg->channelheaders[channel].stofac;
  this_size=local_arg->channelheaders[channel].dasize+1;
  if (this_size<1 || this_size>2) {
   /* In the reading routines, only 1 and 2 are handled right now and we will loop
    * forever there if a different value is found... Which was not the case for
    * any correct files I saw! */
   TRACEMS2(tinfo->emethods, 0, "read_vitaport_init Warning: Unsupported dasize %d in channel %d, will be set to 0!\n", MSGPARM(local_arg->channelheaders[channel].dasize), MSGPARM(channel+1));
  }
  if (this_sfac>local_arg->max_sfac) {
   local_arg->bytes_per_max_sfac=(local_arg->bytes_per_max_sfac*this_sfac)/local_arg->max_sfac;
   local_arg->max_sfac=this_sfac;
  }
  if (this_sfac<local_arg->min_sfac) {
   local_arg->min_sfac=this_sfac;
  }
  local_arg->bytes_per_max_sfac+=(this_size*local_arg->max_sfac)/this_sfac;

  switch (local_arg->vitaport_filetype) {
   case VITAPORT1_FILE:
    break;
   case VITAPORT2_FILE:
   case VITAPORT2RFILE:
    if (read_struct((char *)&local_arg->channelheadersII[channel], sm_vitaportIIrchannelheader, local_arg->infile)==0) {
     ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't read channel header in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
    }
#ifdef LITTLE_ENDIAN
    change_byteorder((char *)&local_arg->channelheadersII[channel], sm_vitaportIIrchannelheader);
#endif
    local_arg->sum_dlen+=local_arg->channelheadersII[channel].dlen;
    {
    /* This is the number of samples as measured in units of ceil_sfreq: */
    unsigned long const this_NumSamples=local_arg->channelheadersII[channel].dlen/this_size*this_sfac;
    if (this_NumSamples>max_NumSamples) max_NumSamples=this_NumSamples;
    }
    break;
  }
  local_arg->channelnames_length+=actual_fieldlength(local_arg->channelheaders[channel].kname, local_arg->channelheaders[channel].kname+VP_CHANNELNAME_LENGTH)+1;
 }
 fread(&local_arg->checksum, sizeof(uint16_t), 1, local_arg->infile);
#ifdef LITTLE_ENDIAN
 Intel_int16((uint16_t *)&local_arg->checksum);
#endif
 local_arg->SizeofHeader = ftell(local_arg->infile);

 /* Now we need to make sure that the min_sfac is commensurate with all sfac's...
  * Otherwise, we ultimatively have to set min_sfac to 1, which will cause our
  * global sfreq to be identical to the Vitaport global scan rate. */
 while (TRUE) {
  Bool again=FALSE;
  for (channel=0; channel<NoOfChannels; channel++) {
   unsigned int const this_sfac=local_arg->channelheaders[channel].scanfc*local_arg->channelheaders[channel].stofac;
   unsigned int const remain=this_sfac%local_arg->min_sfac;
   if (remain!=0) {
    /* This is a very crude try-it-out largest divisor finder: */
    unsigned int new_min_sfac=1, test_min_sfac;
    for (test_min_sfac=2; test_min_sfac<local_arg->min_sfac; test_min_sfac++) {
     if (this_sfac%test_min_sfac==0) new_min_sfac=test_min_sfac;
    }
    TRACEMS4(tinfo->emethods, 1, " min_sfac %d not commensurate with channel %d sfac %d: Correcting to %d\n", MSGPARM(local_arg->min_sfac), MSGPARM(channel+1), MSGPARM(this_sfac), MSGPARM(new_min_sfac));
    local_arg->min_sfac=new_min_sfac;
    again=TRUE;
    break;
   }
  }
  if (!again) break;
 }
 local_arg->max_sfreq=local_arg->ceil_sfreq/local_arg->min_sfac;
 switch (local_arg->vitaport_filetype) {
  case VITAPORT1_FILE:
   break;
  case VITAPORT2_FILE:
   /* If fileheaderII.dlen was always filled out correctly, we could as well
    * use the statement for VITAPORT2RFILE below... */
   local_arg->points_in_file=max_NumSamples/local_arg->min_sfac;
   break;
  case VITAPORT2RFILE:
   if (!args[ARGS_CONTINUOUS].is_set || local_arg->fromepoch!=1) {
    ERREXIT(tinfo->emethods, "read_vitaport_init: Only continuous, full reading is currently supported on RAW files!\n");
   }
   if (args[ARGS_TRIGTRANSFER].is_set) {
    ERREXIT(tinfo->emethods, "read_vitaport_init: Trigger transfer is not currently supported on RAW files!\n");
   }
   /* Looks strange, eh? But for long files, the `long' data type can overflow within this operation... */
   local_arg->points_in_file=(long)rint(((double)local_arg->fileheaderII.dlen-local_arg->fileheader.hdlen)*local_arg->max_sfac/local_arg->min_sfac/local_arg->bytes_per_max_sfac);
   /* For VITAPORT2RFILE, this deviates from the current file position... */
   local_arg->SizeofHeader = local_arg->fileheader.hdlen;
   TRACEMS(tinfo->emethods, 1, " We'll assume that this is a RAW file...\n");
   if ((local_arg->channelbuffer=(DATATYPE *)malloc(local_arg->fileheader.knum*sizeof(DATATYPE)))==NULL) {
    ERREXIT(tinfo->emethods, "read_vitaport_init: Error allocating channelbuffer memory\n");
   }
   fseek(local_arg->infile, local_arg->SizeofHeader, SEEK_SET);
   break;
 }
 tinfo->points_in_file=local_arg->points_in_file;

 /*{{{  Allocate and fill local arrays*/
 if ((local_arg->channelnames=(char **)malloc(NoOfChannels*sizeof(char *)))==NULL ||
     (innames=(char *)malloc(local_arg->channelnames_length))==NULL) {
  ERREXIT(tinfo->emethods, "read_vitaport_init: Error allocating channelnames memory\n");
 }
 if ((local_arg->sampling_step=(unsigned int *)malloc(NoOfChannels*sizeof(float)))==NULL) {
  ERREXIT(tinfo->emethods, "read_vitaport_init: Error allocating sampling_step array\n");
 }

 for (channel=0; channel<NoOfChannels; channel++) {
  int n=actual_fieldlength(local_arg->channelheaders[channel].kname, local_arg->channelheaders[channel].kname+VP_CHANNELNAME_LENGTH);
  unsigned int const this_sfac=local_arg->channelheaders[channel].scanfc*local_arg->channelheaders[channel].stofac;
  local_arg->channelnames[channel]=innames;
  strncpy(innames, local_arg->channelheaders[channel].kname, n);
  innames[n]='\0';
  innames+=n+1;

  local_arg->sampling_step[channel]=this_sfac/local_arg->min_sfac;
 }
 /*}}}  */
 
 tinfo->sfreq=local_arg->max_sfreq;
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 if (local_arg->beforetrig==0 && local_arg->aftertrig==0) {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Read the whole file as one epoch: */
   local_arg->aftertrig=local_arg->points_in_file;
   TRACEMS1(tinfo->emethods, 1, "read_vitaport_init: Reading ALL data (%d points)\n", MSGPARM(local_arg->aftertrig));
  } else {
   ERREXIT(tinfo->emethods, "read_vitaport_init: Zero epoch length.\n");
  }
 }
 if (args[ARGS_TRIGFILE].is_set) {
  if ((local_arg->triggerfile=fopen(args[ARGS_TRIGFILE].arg.s, "r"))==NULL) {
   ERREXIT1(tinfo->emethods, "read_vitaport_init: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
 } else {
  local_arg->triggerfile=NULL;
 }
 if (args[ARGS_TRIGLIST].is_set) {
  local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
  if (local_arg->trigcodes==NULL) {
   ERREXIT(tinfo->emethods, "read_vitaport_init: Error allocating triglist memory\n");
  }
 } else {
  local_arg->trigcodes=NULL;
 }

 read_vitaport_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;
 local_arg->current_triggerpoint=0;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_vitaport(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_vitaport(transform_info_ptr tinfo) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 FILE *infile=local_arg->infile;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->fileheader.knum;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_vitaport: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /*{{{  Find the next window that fits into the actual data*/
 /* This is just for the Continuous option (no trigger file): */
 file_end_point=local_arg->current_point-1;
 do {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* Simulate a trigger at current_point+beforetrig */
   file_start_point=file_end_point+1;
   trigger_point=file_start_point+tinfo->beforetrig;
   file_end_point=trigger_point+tinfo->aftertrig-1;
   if (file_end_point>=local_arg->points_in_file) return NULL;
   local_arg->current_trigger++;
   local_arg->current_point+=tinfo->nr_of_points;
   tinfo->condition=0;
  } else 
  do {
   tinfo->condition=read_vitaport_read_trigger(tinfo, &trigger_point, &description);
   if (tinfo->condition==0) return NULL;	/* No more triggers in file */
   file_start_point=trigger_point-tinfo->beforetrig+local_arg->offset;
   file_end_point=trigger_point+tinfo->aftertrig-1-local_arg->offset;
   
   if (local_arg->trigcodes==NULL) {
    not_correct_trigger=FALSE;
   } else {
    int trigno=0;
    not_correct_trigger=TRUE;
    while (local_arg->trigcodes[trigno]!=0) {
     if (local_arg->trigcodes[trigno]==tinfo->condition) {
      not_correct_trigger=FALSE;
      break;
     }
     trigno++;
    }
   }
  } while (not_correct_trigger || file_start_point<0 || file_end_point>=local_arg->points_in_file);
 } while (--local_arg->fromepoch>0);
 TRACEMS3(tinfo->emethods, 1, "read_vitaport: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_vitaport_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_vitaport_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
    trigs_in_epoch++;
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /*{{{  Configure myarray*/
 myarray.element_skip=tinfo->itemsize=1;
 myarray.nr_of_elements=tinfo->nr_of_points;
 myarray.nr_of_vectors=tinfo->nr_of_channels;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_vitaport: Error allocating data\n");
 }
 /* Byte order is nonmultiplexed, ie points fastest */
 tinfo->multiplexed=FALSE;
 /*}}}  */

 read_vitaport_get_filestrings(tinfo);
 if (description==NULL) {
  char const *last_sep=strrchr(args[ARGS_IFILE].arg.s, PATHSEP);
  char const *last_component=(last_sep==NULL ? args[ARGS_IFILE].arg.s : last_sep+1);
  snprintf(tinfo->comment, MAX_COMMENTLEN, "read_vitaport %s %02d/%02d/%02d,%02d:%02d:%02d", last_component, local_arg->fileheaderII.Month, local_arg->fileheaderII.Day, local_arg->fileheaderII.Year, local_arg->fileheaderII.Hour, local_arg->fileheaderII.Minute, local_arg->fileheaderII.Second);
 } else {
  snprintf(tinfo->comment, MAX_COMMENTLEN, "%s", description);
 }
 if (local_arg->vitaport_filetype!=VITAPORT2RFILE) {
  do { /* For each channel */
   unsigned int const this_size=local_arg->channelheaders[myarray.current_vector].dasize+1;
   unsigned int const sstep=local_arg->sampling_step[myarray.current_vector];
   unsigned char const datype=local_arg->channelheaders[myarray.current_vector].datype;
   unsigned short const offset=local_arg->channelheaders[myarray.current_vector].offset;
   float const factor=((float)local_arg->channelheaders[myarray.current_vector].mulfac)/local_arg->channelheaders[myarray.current_vector].divfac;
   long current_point=file_start_point;
   float thispoint=((float)current_point)/sstep;
   long lastpoint=(int)thispoint;
   fseek(infile, local_arg->SizeofHeader+local_arg->channelheadersII[myarray.current_vector].doffs+lastpoint*this_size, SEEK_SET);
   do { /* For each point */
    DATATYPE dat;
    int n=0;
    if (datype==VP_DATYPE_SIGNED) {
     switch (this_size) {
      case 1: {
       char s;
       n=fread(&s, sizeof(s), 1, infile);
       dat=s;
       }
       break;
      case 2: {
       int16_t s;
       n=fread(&s, sizeof(s), 1, infile);
#ifdef LITTLE_ENDIAN
       Intel_int16((uint16_t *)&s);
#endif
       dat=s;
       }
       break;
      default: {
       unsigned char s[8];
       n=fread(&s, this_size, 1, infile);
       dat=0;
       }
       break;
     }
    } else {
     switch (this_size) {
      case 1: {
       unsigned char s;
       n=fread(&s, sizeof(s), 1, infile);
       dat=s;
       }
       break;
      case 2: {
       uint16_t s;
       n=fread(&s, sizeof(s), 1, infile);
#ifdef LITTLE_ENDIAN
       Intel_int16(&s);
#endif
       dat=s;
       }
       break;
      default: {
       unsigned char s[8];
       n=fread(&s, this_size, 1, infile);
       dat=0;
       }
       break;
     }
    }
    if (n!=1) {
     ERREXIT(tinfo->emethods, "read_vitaport: Error reading data\n");
    }
    dat=(dat-offset)*factor;
    do {
     array_write(&myarray, dat);
     current_point++;
     thispoint=((float)current_point)/sstep;
    } while (myarray.message==ARRAY_CONTINUE && lastpoint==(int)thispoint);
    lastpoint=(int)thispoint;
   } while (myarray.message==ARRAY_CONTINUE);
  } while (myarray.message!=ARRAY_ENDOFSCAN);
 } else {
  /* VITAPORT2RFILE */
  long current_point=file_start_point;
  array_transpose(&myarray);
  do { /* For each point */
   do { /* For each channel */
   unsigned int const this_size=local_arg->channelheaders[myarray.current_element].dasize+1;
   unsigned int const sstep=local_arg->sampling_step[myarray.current_element];
   unsigned char const datype=local_arg->channelheaders[myarray.current_element].datype;
   unsigned short const offset=local_arg->channelheaders[myarray.current_element].offset;
   float const factor=((float)local_arg->channelheaders[myarray.current_element].mulfac)/local_arg->channelheaders[myarray.current_element].divfac;
    DATATYPE dat;
    int n=0;
    if (current_point%sstep==0) {
     if (datype==VP_DATYPE_SIGNED) {
      switch (this_size) {
       case 1: {
	char s;
	n=fread(&s, sizeof(s), 1, infile);
	dat=s;
	}
	break;
       case 2: {
	int16_t s;
	n=fread(&s, sizeof(s), 1, infile);
#ifdef LITTLE_ENDIAN
	Intel_int16((unsigned short *)&s);
#endif
	dat=s;
	}
	break;
       default:
	continue;
      }
     } else {
      switch (this_size) {
       case 1: {
	unsigned char s;
	n=fread(&s, sizeof(s), 1, infile);
	dat=s;
	}
	break;
       case 2: {
	uint16_t s;
	n=fread(&s, sizeof(s), 1, infile);
#ifdef LITTLE_ENDIAN
	Intel_int16(&s);
#endif
	dat=s;
	}
	break;
       default:
	continue;
      }
      dat-=offset;
     }
     if (n!=1) {
      ERREXIT(tinfo->emethods, "read_vitaport: Error reading data\n");
     }
     dat*=factor;
     local_arg->channelbuffer[myarray.current_element]=dat;
    } else {
     dat=local_arg->channelbuffer[myarray.current_element];
    }
    array_write(&myarray, dat);
   } while (myarray.message==ARRAY_CONTINUE);
   current_point++;
  } while (myarray.message!=ARRAY_ENDOFSCAN);
 }

 tinfo->file_start_point=file_start_point;
 tinfo->z_label=NULL;
 tinfo->sfreq=local_arg->max_sfreq;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_vitaport_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_vitaport_exit(transform_info_ptr tinfo) {
 struct read_vitaport_storage *local_arg=(struct read_vitaport_storage *)tinfo->methods->local_storage;

 if (local_arg->triggerfile!=NULL) {
  if (local_arg->triggerfile!=stdin) fclose(local_arg->triggerfile);
  local_arg->triggerfile=NULL;
 }
 fclose(local_arg->infile);
 free_pointer((void **)&local_arg->trigcodes);
 free_pointer((void **)&local_arg->sampling_step);
 if (local_arg->channelnames!=NULL) {
  free_pointer((void **)&local_arg->channelnames[0]);
  free_pointer((void **)&local_arg->channelnames);
 }
 free_pointer((void **)&local_arg->channelheaders);
 free_pointer((void **)&local_arg->channelheadersII);
 growing_buf_free(&local_arg->triggers);

 if (local_arg->vitaport_filetype==VITAPORT2RFILE) free_pointer((void **)&local_arg->channelbuffer);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_vitaport(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_vitaport(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_vitaport_init;
 tinfo->methods->transform= &read_vitaport;
 tinfo->methods->transform_exit= &read_vitaport_exit;
 tinfo->methods->get_filestrings= &read_vitaport_get_filestrings;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_vitaport";
 tinfo->methods->method_description=
  "Get-epoch method to read binary files in the `vitaport' format.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_vitaport_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
