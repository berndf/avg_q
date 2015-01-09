/*
 * Copyright (C) 1996-2001,2003,2004,2006-2010,2012-2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_synamps.c module to read data from Neuroscan `SynAmps' files
 * part of this code was taken from Patrick Berg's `3sycorr.c'.
 *	-- Bernd Feige 14.06.1995
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <string.h>
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "transform.h"
#include "bf.h"
#include "neurohdr.h"
/*}}}  */

LOCAL char *read_synamps_version="$Header: /home/charly/.cvsroot/avg_q/bf/neuroscan/read_synamps.c,v 2.50 2007/05/21 12:48:04 charly Exp $";

/*{{{  Local defs and args*/
enum ARGS_ENUM {
 ARGS_NOREJECTED=0, 
 ARGS_NOBADCHANS, 
 ARGS_CONTINUOUS, 
 ARGS_TRIGTRANSFER, 
 ARGS_KN_REMAP, 
 ARGS_TRIGLIST, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_TRIGFILE,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Reject epochs marked as rejected (.EEG files only)", "r", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Ignore channels marked as Bad", "B", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Konstanz remapping of trigger_list values to 1...n", "K", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these `StimTypes'. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"}
};
/*}}}  */

/*{{{  Definition of read_synamps_storage*/
struct read_synamps_storage {
 FILE *SCAN;	/* Input file */
 SETUP EEG;	/* header info. */
 ELECTLOC *Channels;
 int bytes_per_sample;	/* Can be 2 or 4 for 16 or 32-bit raw data */
 int nchannels;
 long filesize;
 long SizeofHeader;          /* no. of bytes in header of source file */
 unsigned long BufferCount;	/* Buffer size */
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 long current_point;
 long first_point_in_buffer;
 long last_point_in_buffer;
 void *buffer;
 enum NEUROSCAN_SUBTYPES SubType; /* This tells, once and for all, the file type */

 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
};
/*}}}  */

/*{{{  Local functions to transform file offsets <-> point numbers*/
LOCAL long
offset2point(transform_info_ptr tinfo, long offset) {
 struct read_synamps_storage *local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 return (offset-local_arg->SizeofHeader)/local_arg->bytes_per_sample/local_arg->EEG.nchannels;
}
LOCAL long
point2offset(transform_info_ptr tinfo, long point) {
 struct read_synamps_storage *local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 return point*local_arg->EEG.nchannels*local_arg->bytes_per_sample+local_arg->SizeofHeader;
}
/*}}}  */

/*{{{  Single point interface*/

/*{{{  read_synamps_get_filestrings: Allocate and set strings and probepos array*/
LOCAL void
read_synamps_get_filestrings(transform_info_ptr tinfo) {
 struct read_synamps_storage *local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char *innamebuf;
 int channel, namelen=0;
 ELECTLOC *inChannels=local_arg->Channels;

 tinfo->xchannelname=tinfo->z_label=NULL;
 for (channel=0; channel<local_arg->nchannels; channel++) {
  if (args[ARGS_NOBADCHANS].is_set) {
   while (inChannels->bad) inChannels++;
  }
  namelen+=strlen(inChannels->lab)+1;
  inChannels++;
 }
 tinfo->xdata=NULL;
 if ((tinfo->channelnames=(char **)malloc(local_arg->nchannels*sizeof(char *)))==NULL ||
     (tinfo->probepos=(double *)malloc(local_arg->nchannels*3*sizeof(double)))==NULL ||
     (innamebuf=(char *)malloc(namelen))==NULL) {
  ERREXIT(tinfo->emethods, "read_synamps: Error allocating channelnames\n");
 }

 inChannels=local_arg->Channels;
 for (channel=0; channel<local_arg->nchannels; channel++) {
  if (args[ARGS_NOBADCHANS].is_set) {
   while (inChannels->bad) inChannels++;
  }
  strcpy(innamebuf, inChannels->lab);
  tinfo->channelnames[channel]=innamebuf;
  innamebuf+=strlen(innamebuf)+1;
  /* In the x-y channel positions, the y axis runs down from the top.
     Further, there is a scale mismatch of about 3:1 between x and y. */
  tinfo->probepos[3*channel  ]=inChannels->x_coord;
  tinfo->probepos[3*channel+1]= -3*inChannels->y_coord;
  tinfo->probepos[3*channel+2]=0.0;
  inChannels++;
 }
}
/*}}}  */

/*{{{  read_synamps_seek_point(transform_info_ptr tinfo, long point) {*/
/* This function not only seeks to the specified point, but also reads
 * the point into the buffer if necessary. The get_singlepoint function
 * then only transfers the data. */
LOCAL void
read_synamps_seek_point(transform_info_ptr tinfo, long point) {
 struct read_synamps_storage *local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;

 switch(local_arg->SubType) {
  case NST_CONT0:
  case NST_DCMES:
  case NST_CONTINUOUS:
  case NST_SYNAMPS: {
   /*{{{  */
   const int points_per_buf=local_arg->EEG.ChannelOffset/local_arg->bytes_per_sample; /* Note a value of 1 is fixed in read_synamps_init */
   long const buffer_boundary=(point/points_per_buf)*points_per_buf;
   long const new_last_point_in_buffer=buffer_boundary+points_per_buf-1;
   /* This format is neither points fastest nor channels fastest, but
    * points fastest in blocks. No idea why they didn't do channels fastest,
    * it would have rendered the whole file in one order, no matter what buffer
    * size is used internally in programs. This way we MUST use the given buffers. */
   if (new_last_point_in_buffer!=local_arg->last_point_in_buffer) {
    fseek(local_arg->SCAN,point2offset(tinfo,buffer_boundary),SEEK_SET);
    if (fread(local_arg->buffer,1,local_arg->BufferCount,local_arg->SCAN)!=local_arg->BufferCount) {
     ERREXIT(tinfo->emethods, "read_synamps_seek_point: Error reading data\n");
    }
    local_arg->first_point_in_buffer=buffer_boundary;
    local_arg->last_point_in_buffer=new_last_point_in_buffer;
    /*{{{  Swap byte order if necessary*/
#    ifndef LITTLE_ENDIAN
    {uint16_t *pdata=(uint16_t *)local_arg->buffer, *p_end=(uint16_t *)((char *)local_arg->buffer+local_arg->BufferCount);
     while (pdata<p_end) Intel_int16(pdata++);
    }
#    endif
    /*}}}  */
   }
   /*}}}  */
  }
  break;
 default:
  ERREXIT1(tinfo->emethods, "read_synamps_seek_point: Format `%s' is not yet supported\n", MSGPARM(neuroscan_subtype_names[local_arg->SubType]));
  break;
 }
 local_arg->current_point=point;
}
/*}}}  */

/*{{{  read_synamps_get_singlepoint(transform_info_ptr tinfo, array *toarray) {*/
LOCAL int
read_synamps_get_singlepoint(transform_info_ptr tinfo, array *toarray) {
 struct read_synamps_storage *local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->current_point>=local_arg->EEG.NumSamples) return -1;
 read_synamps_seek_point(tinfo, local_arg->current_point);
 switch(local_arg->SubType) {
  case NST_DCMES: {
   /* DC-MES data is unsigned, and the marker channel is skipped: */
   unsigned short *pdata=(unsigned short *)local_arg->buffer+local_arg->current_point-local_arg->first_point_in_buffer+1;
   int channel=0;
   do {
    *pdata^=0x8000;	/* Exclusive or to convert to signed... */
    if (args[ARGS_NOBADCHANS].is_set && local_arg->Channels[channel].bad) {
     toarray->message=ARRAY_CONTINUE;
    } else {
     array_write(toarray, NEUROSCAN_CONVSHORT(&local_arg->Channels[channel], *((signed short *)pdata)));
    }
    pdata+=local_arg->EEG.ChannelOffset/local_arg->bytes_per_sample;
    channel++;
   } while (toarray->message==ARRAY_CONTINUE);
  }
   break;
  case NST_CONT0:
  case NST_CONTINUOUS:
  case NST_SYNAMPS: {
   int channel=0;
   do {
    DATATYPE val;
    if (args[ARGS_NOBADCHANS].is_set && local_arg->Channels[channel].bad) {
     toarray->message=ARRAY_CONTINUE;
    } else {
     if (local_arg->bytes_per_sample==2) {
      int16_t * const pdata=((int16_t *)local_arg->buffer)+local_arg->current_point-local_arg->first_point_in_buffer+channel;
#   ifndef LITTLE_ENDIAN
      Intel_int16(pdata);
#   endif
      val=NEUROSCAN_CONVSHORT(&local_arg->Channels[channel],*pdata);
     } else if (local_arg->bytes_per_sample==4) {
      int32_t * const pdata=((int32_t *)local_arg->buffer)+local_arg->current_point-local_arg->first_point_in_buffer+channel;
#   ifndef LITTLE_ENDIAN
      Intel_int32(pdata);
#   endif
      val=NEUROSCAN_CONVSHORT(&local_arg->Channels[channel],*pdata);
     } else {
      ERREXIT(tinfo->emethods, "read_synamps: Unknown bytes_per_sample\n");
     }
     array_write(toarray, val);
    }
    channel++;
   } while (toarray->message==ARRAY_CONTINUE);
  }
   break;
  default:
   ERREXIT1(tinfo->emethods, "read_synamps_get_singlepoint: Format `%s' is not yet supported\n", MSGPARM(neuroscan_subtype_names[local_arg->SubType]));
   break;
 }
 local_arg->current_point++;
 return 0;
}
/*}}}  */
/*}}}  */

/*{{{  read_synamps_nexttrigger(transform_info_ptr tinfo, long *trigpoint) {*/
/*{{{  Maintaining the triggers list*/
LOCAL void 
read_synamps_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_synamps_storage * const local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */

/*{{{  read_synamps_push_keys(transform_info_ptr tinfo, long *trigpoint) {*/
/* This subroutine encapsulates the coding of the different event codes
 * into a unified signed code */
LOCAL void
read_synamps_push_keys(transform_info_ptr tinfo, long position, int TrigVal, int KeyPad, int KeyBoard, enum NEUROSCAN_ACCEPTVALUES Accept) {
 struct read_synamps_storage * const local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 int code;
 if (Accept!=0 && (Accept<NAV_DCRESET || Accept>NAV_STARTSTOP)) {
  TRACEMS2(tinfo->emethods, 0, "read_synamps_push_keys: Unknown Accept value %d at position %d!\n", MSGPARM(Accept), MSGPARM(position));
  return;
 }
 code=TrigVal-KeyPad+neuroscan_accept_translation[Accept];
 if (code==0) {
  /* At least in Edit 4.x, KeyPad F2 is written as KeyBoard=0 with everything else 0 too... */
  code= -(((KeyBoard&0xf)+1)<<4);
 }
 if (code==0) {
  TRACEMS1(tinfo->emethods, 0, "read_synamps_push_keys: Trigger with trigcode 0 at position %d!\n", MSGPARM(position));
 } else {
  push_trigger(&local_arg->triggers, position, code, NULL);
 }
}
/*}}}  */

/*{{{  read_synamps_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_synamps_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_synamps_storage * const local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_synamps_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_synamps_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
   free_pointer((void **)&description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  switch(local_arg->SubType) {
   case NST_CONTINUOUS:
   case NST_SYNAMPS: {
    /*{{{  Read the events header and array*/
    /* We handle errors through setting errormessage, because we want to continue
     * with a warning if we are in continuous mode or read from an event file */
    TEEG TagType;
    EVENT2 event;
    TRACEMS(tinfo->emethods, 1, "read_synamps_build_trigbuffer: Reading event table\n");
    fseek(local_arg->SCAN,local_arg->EEG.EventTablePos,SEEK_SET);
    /* Here we face two evils in one header: Coding enums as chars and
     * allowing longs at odd addresses. Well... */
    if (read_struct((char *)&TagType, sm_TEEG, local_arg->SCAN)==1) {
#    ifndef LITTLE_ENDIAN
     change_byteorder((char *)&TagType, sm_TEEG);
#    endif
     if (TagType.Teeg==TEEG_EVENT_TAB1 || TagType.Teeg==TEEG_EVENT_TAB2) {
      /*{{{  Read the event table*/
      struct_member * const sm_EVENT=(TagType.Teeg==TEEG_EVENT_TAB1 ? sm_EVENT1 : sm_EVENT2);
      int ntags, tag;

      ntags=TagType.Size/sm_EVENT[0].offset;	/* sm_EVENT[0].offset is the size of the structure in file. */
      for (tag=0; tag<ntags; tag++) {
       long trigger_position=0;
       int TrigVal=0, KeyPad=0, KeyBoard=0;
       enum NEUROSCAN_ACCEPTVALUES Accept=(enum NEUROSCAN_ACCEPTVALUES)0;
       if (read_struct((char *)&event, sm_EVENT, local_arg->SCAN)==0) {
	ERREXIT(tinfo->emethods, "read_synamps_build_trigbuffer: Can't read an event table entry.\n");
	break;
       }
#      ifndef LITTLE_ENDIAN
       change_byteorder((char *)&event, sm_EVENT);
#      endif
       trigger_position=offset2point(tinfo,event.Offset);
       TrigVal=event.StimType &0xff;
       KeyBoard=event.KeyBoard&0xf;
       KeyPad=Event_KeyPad_value(event);
       Accept=Event_Accept_value(event);
       if (!args[ARGS_NOREJECTED].is_set || Accept!=NAV_REJECT) {
	read_synamps_push_keys(tinfo, trigger_position, TrigVal, KeyPad, KeyBoard, Accept);
       }
      }
      /*}}}  */
     } else {
      ERREXIT1(tinfo->emethods, "read_synamps_build_trigbuffer: Unknown tag type %d.\n", MSGPARM(TagType.Teeg));
     }
    } else {
     ERREXIT(tinfo->emethods, "read_synamps_build_trigbuffer: Can't read the event table header.\n");
    }
    /*}}}  */
    }
    break;
   case NST_CONT0: {
    /*{{{  CONT0: Two trailing marker channels*/
    long current_triggerpoint=0;
    unsigned short *pdata;
    TRACEMS(tinfo->emethods, 1, "read_synamps_build_trigbuffer: Analyzing CONT0 marker channels\n");
    while (current_triggerpoint<local_arg->EEG.NumSamples) {
     int TrigVal=0, KeyPad=0, KeyBoard=0;
     enum NEUROSCAN_ACCEPTVALUES Accept=(enum NEUROSCAN_ACCEPTVALUES)0;
     read_synamps_seek_point(tinfo, current_triggerpoint);
     pdata=(unsigned short *)local_arg->buffer+current_triggerpoint-local_arg->first_point_in_buffer+local_arg->nchannels;
     TrigVal =pdata[0]&0xff; 
     KeyBoard=pdata[1]&0xf; if (KeyBoard>13) KeyBoard=0;
     if (TrigVal!=0 || KeyBoard!=0) {
      read_synamps_push_keys(tinfo, current_triggerpoint, TrigVal, KeyPad, KeyBoard, Accept);
      current_triggerpoint++;
      while (current_triggerpoint<local_arg->EEG.NumSamples) {
       int This_TrigVal, This_KeyBoard;
       read_synamps_seek_point(tinfo, current_triggerpoint);
       pdata=(unsigned short *)local_arg->buffer+current_triggerpoint-local_arg->first_point_in_buffer+local_arg->nchannels;
       This_TrigVal =pdata[0]&0xff; 
       This_KeyBoard=pdata[1]&0xf; if (This_KeyBoard>13) This_KeyBoard=0;
       if (This_TrigVal!=TrigVal || This_KeyBoard!=KeyBoard) break;
       current_triggerpoint++;
      }
     }
    }
    /*}}}  */
    }
    break;
   case NST_DCMES: { 
    /*{{{  DC-MES: Single, leading marker channel*/
    long current_triggerpoint=0;
    unsigned short *pdata;
    TRACEMS(tinfo->emethods, 1, "read_synamps_build_trigbuffer: Analyzing DCMES marker channel\n");
    while (current_triggerpoint<local_arg->EEG.NumSamples) {
     int TrigVal=0, KeyPad=0, KeyBoard=0;
     enum NEUROSCAN_ACCEPTVALUES Accept=(enum NEUROSCAN_ACCEPTVALUES)0;
     read_synamps_seek_point(tinfo, current_triggerpoint);
     pdata=(unsigned short *)local_arg->buffer+current_triggerpoint-local_arg->first_point_in_buffer;
     TrigVal= *pdata&0xff; 
     KeyBoard=(*pdata>>8)&0xf; if (KeyBoard>13) KeyBoard=0;
     if (TrigVal!=0 || KeyBoard!=0) {
      read_synamps_push_keys(tinfo, current_triggerpoint, TrigVal, KeyPad, KeyBoard, Accept);
      current_triggerpoint++;
      while (current_triggerpoint<local_arg->EEG.NumSamples) {
       int This_TrigVal, This_KeyBoard;
       read_synamps_seek_point(tinfo, current_triggerpoint);
       pdata=(unsigned short *)local_arg->buffer+current_triggerpoint-local_arg->first_point_in_buffer;
       This_TrigVal= *pdata&0xff; 
       This_KeyBoard=(*pdata>>8)&0xf; if (This_KeyBoard>13) This_KeyBoard=0;
       if (This_TrigVal!=TrigVal || This_KeyBoard!=KeyBoard) break;
       current_triggerpoint++;
      }
     }
    }
    /*}}}  */
    }
    break;
   default:
    break;
  }
 }
}
/*}}}  */

/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_synamps_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_synamps_storage *local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_synamps_build_trigbuffer(tinfo);
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

/*{{{  read_synamps_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_synamps_init(transform_info_ptr tinfo) {
 struct read_synamps_storage * const local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 FILE *SCAN=fopen(args[ARGS_IFILE].arg.s,"rb");
 int NoOfChannels, channel;
 struct stat statbuff;

 growing_buf_init(&local_arg->triggers);

 if(SCAN==NULL) {
  ERREXIT1(tinfo->emethods, "read_synamps_init: Can't open file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }

 if (read_struct((char *)&local_arg->EEG, sm_SETUP, SCAN)==0) {
  ERREXIT1(tinfo->emethods, "read_synamps_init: File header read error in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
#ifndef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->EEG, sm_SETUP);
#endif

 /*{{{  Allocate channel header*/
 if ((local_arg->Channels=(ELECTLOC *)malloc(local_arg->EEG.nchannels*sizeof(ELECTLOC)))==NULL) {
  ERREXIT(tinfo->emethods, "read_synamps_init: Error allocating Channels list\n");
 }
 /*}}}  */
 /* For minor_rev<4, 32 electrode structures were hardcoded - but the data is
  * there for all the channels... */
 NoOfChannels = (local_arg->EEG.minor_rev<4 ? 32 : local_arg->EEG.nchannels);
 for (channel=0; channel<NoOfChannels; channel++) {
  if (read_struct((char *)&local_arg->Channels[channel], sm_ELECTLOC, SCAN)==0) {
   ERREXIT1(tinfo->emethods, "read_synamps_init: Channel header read error in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->Channels[channel], sm_ELECTLOC);
#endif
 }
 local_arg->SizeofHeader = ftell(SCAN);
 if (local_arg->EEG.minor_rev<4) {
  NoOfChannels = local_arg->EEG.nchannels;
  /* Copy the channel descriptors over from the first 32: */
  for (; channel<NoOfChannels; channel++) {
   memcpy(&local_arg->Channels[channel], &local_arg->Channels[channel%32], sizeof(ELECTLOC));
  }
 }

 /*{{{  Determine the file size*/
 stat(args[ARGS_IFILE].arg.s,&statbuff);
 local_arg->filesize = statbuff.st_size;
 /*}}}  */

 /* The actual criteria to decide upon the file type (average, continuous etc)
  * by the header have been moved to neurohdr.h because it's black magic... */
 local_arg->SubType=(enum NEUROSCAN_SUBTYPES)NEUROSCAN_SUBTYPE(&local_arg->EEG);
 TRACEMS1(tinfo->emethods, 1, "read_synamps_init: File type is `%s'\n", MSGPARM(neuroscan_subtype_names[local_arg->SubType]));

 tinfo->sfreq=local_arg->EEG.rate;
 local_arg->nchannels=local_arg->EEG.nchannels;
 if (local_arg->SubType==NST_DCMES) {
  /* The first channel in this type is the `marker' channel */
  local_arg->nchannels--;
  local_arg->Channels++;	/* Point to the first data channel info */
 } else if (local_arg->SubType==NST_CONT0) {
  /* In CONT0 format, there are two trailing trigger channels for Stimulation and
   * KeyBoard, but the header does not count them; in DCMES, the header counts the
   * `marker' channel. To get this consistently, we make local_arg->EEG.nchannels to 
   * always count ALL channels, and local_arg->nchannels to count only the data
   * channels. */
  local_arg->EEG.nchannels+=2;
 }
 if (args[ARGS_NOBADCHANS].is_set) {
  int badchans=0;
  for (channel=0; channel<local_arg->nchannels; channel++) {
   if (local_arg->Channels[channel].bad) badchans++;
  }
  local_arg->nchannels-=badchans;
 }
 tinfo->nr_of_channels=local_arg->nchannels;
 tinfo->itemsize=1;
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 switch(local_arg->SubType) {
  case NST_CONTINUOUS:
  case NST_SYNAMPS:
   /*{{{  Determine bytes_per_sample and NumSamples */
   {float const bytes_per_sample=rint(((float)(local_arg->EEG.EventTablePos - local_arg->SizeofHeader))/NoOfChannels/local_arg->EEG.NumSamples);
    if (bytes_per_sample==2.0 || bytes_per_sample==4.0) {
     local_arg->bytes_per_sample=bytes_per_sample;
    } else {
     local_arg->bytes_per_sample=2;
     local_arg->EEG.NumSamples = (local_arg->EEG.EventTablePos - local_arg->SizeofHeader)/NoOfChannels/local_arg->bytes_per_sample;
    }
   }
   tinfo->points_in_file=local_arg->EEG.NumSamples;
   /*}}}  */
  break;
 case NST_CONT0:
 case NST_DCMES:
  /*{{{  Calculate NumSamples from the file size*/
  local_arg->bytes_per_sample=2;
  local_arg->EEG.NumSamples = (local_arg->filesize - local_arg->SizeofHeader)/
   local_arg->EEG.nchannels/local_arg->bytes_per_sample; /* NoOfChannels may be != nchannels ! */
  tinfo->points_in_file=local_arg->EEG.NumSamples;
  /*}}}  */
  break;
 case NST_EPOCHS:
  {float const bytes_per_sample=rint(((float)(local_arg->EEG.EventTablePos - local_arg->SizeofHeader))/NoOfChannels/local_arg->EEG.pnts/local_arg->EEG.nsweeps);
   if (bytes_per_sample==2.0 || bytes_per_sample==4.0) {
    local_arg->bytes_per_sample=bytes_per_sample;
   } else {
    local_arg->bytes_per_sample=2;
   }
  }
  break;
 case NST_AVERAGE:
  local_arg->bytes_per_sample=4; /* Floats */
  local_arg->offset-= (long int)rint(local_arg->EEG.xmin*tinfo->sfreq);
  break;
 default:
  ERREXIT1(tinfo->emethods, "read_synamps_init: Format `%s' is not yet supported\n", MSGPARM(neuroscan_subtype_names[local_arg->SubType]));
  break;
 }

 switch(local_arg->SubType) {
  case NST_CONTINUOUS:
  case NST_SYNAMPS:
  case NST_CONT0:
  case NST_DCMES:
   /* It appears that NeuroScan actually only uses ChannelOffset in SynAmps files.
    * When filtering such a file, an NST_CONTINUOUS file is written but the
    * ChannelOffset value is not modified, which would severely perturb the data
    * as read by read_synamps! */
   if (local_arg->EEG.ChannelOffset<=1 || local_arg->SubType!=NST_SYNAMPS) local_arg->EEG.ChannelOffset=local_arg->bytes_per_sample;
   local_arg->BufferCount = local_arg->EEG.ChannelOffset*NoOfChannels;
   if ((local_arg->buffer=malloc(local_arg->BufferCount))==NULL) {
    ERREXIT(tinfo->emethods, "read_synamps_init: Error allocating buffer memory\n");
   }
   break;
  default:
   local_arg->buffer=NULL;
   break;
 }

 if (local_arg->beforetrig==0 && local_arg->aftertrig==0 && local_arg->SubType!=NST_EPOCHS && local_arg->SubType!=NST_AVERAGE) {
  if (args[ARGS_CONTINUOUS].is_set) {
   /* For the two epoched types, the automatic length selection is done
    * in read_synamps; For the continuous types, we do it here: */
   local_arg->aftertrig=local_arg->EEG.NumSamples;
   TRACEMS1(tinfo->emethods, 1, "read_synamps_init: Reading ALL data (%d points)\n", MSGPARM(local_arg->aftertrig));
  } else {
   ERREXIT(tinfo->emethods, "read_synamps_init: Zero epoch length.\n");
  }
 }
 if (args[ARGS_TRIGLIST].is_set) {
  local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
  if (local_arg->trigcodes==NULL) {
   ERREXIT(tinfo->emethods, "read_synamps_init: Error allocating triglist memory\n");
  }
 } else {
  local_arg->trigcodes=NULL;
 }

 read_synamps_reset_triggerbuffer(tinfo);
 local_arg->SCAN=SCAN;
 local_arg->current_trigger=0;
 local_arg->current_point=0;
 local_arg->first_point_in_buffer=local_arg->last_point_in_buffer= -1;
 tinfo->channelnames=NULL;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_synamps(transform_info_ptr tinfo) {*/
/*
 * The method read_synamps() returns a pointer to the allocated tsdata memory
 * location or NULL if End Of File encountered. EOF within the data set
 * results in an ERREXIT; NULL is returned ONLY if the first read resulted
 * in an EOF condition. This allows programs to look for multiple data sets
 * in a single file.
 */

METHODDEF DATATYPE *
read_synamps(transform_info_ptr tinfo) {
 struct read_synamps_storage * const local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 int not_correct_trigger, marker;
 int start_point;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nrofaverages=1;
 switch(local_arg->SubType) {
  case NST_CONT0:
  case NST_DCMES:
  case NST_CONTINUOUS:
  case NST_SYNAMPS: {
   long trigger_point, file_start_point, file_end_point;
   /*{{{  Find the next window that fits into the actual data*/
   /* This is just for the Continuous option: */
   file_end_point=local_arg->current_point-1;
   do {
    if (args[ARGS_CONTINUOUS].is_set) {
     /* Simulate a trigger at current_point+beforetrig */
     file_start_point=file_end_point+1;
     trigger_point=file_start_point+tinfo->beforetrig;
     file_end_point=trigger_point+tinfo->aftertrig-1;
     if (file_end_point>=local_arg->EEG.NumSamples) return NULL;
     local_arg->current_trigger++;
     marker=0;
    } else 
    do {
     marker=read_synamps_read_trigger(tinfo, &trigger_point, &description);
     if (marker==0) return NULL;	/* No more triggers in file */
     file_start_point=trigger_point-tinfo->beforetrig+local_arg->offset;
     file_end_point=trigger_point+tinfo->aftertrig-1-local_arg->offset;
     
     if (local_arg->trigcodes==NULL) {
      not_correct_trigger=FALSE;
     } else {
      int trigno=0;
      not_correct_trigger=TRUE;
      while (local_arg->trigcodes[trigno]!=0) {
       if (local_arg->trigcodes[trigno]==marker) {
   	not_correct_trigger=FALSE;
   	if (args[ARGS_KN_REMAP].is_set) marker=trigno+1; /* Remap trigger codes */
   	break;
       }
       trigno++;
      }
     }
    } while (not_correct_trigger || file_start_point<0 || file_end_point>=local_arg->EEG.NumSamples);
   } while (--local_arg->fromepoch>0);
   if (description==NULL) {
    TRACEMS3(tinfo->emethods, 1, "read_synamps: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(marker));
   } else {
    TRACEMS4(tinfo->emethods, 1, "read_synamps: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(marker), MSGPARM(description));
   }

   tinfo->condition=marker;
   /*}}}  */

   /*{{{  Handle triggers within the epoch (option -T)*/
   if (args[ARGS_TRIGTRANSFER].is_set) {
    int trigs_in_epoch, code;
    long trigpoint;
    long const old_current_trigger=local_arg->current_trigger;
    char *thisdescription;

    /* First trigger entry holds file_start_point */
    push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
    read_synamps_reset_triggerbuffer(tinfo);
    for (trigs_in_epoch=1; (code=read_synamps_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
     if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
      push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
      trigs_in_epoch++;
     }
    }
    push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
    local_arg->current_trigger=old_current_trigger;
   }
   /*}}}  */

   /*{{{  Setup and allocate myarray*/
   myarray.element_skip=tinfo->itemsize=1;
   myarray.nr_of_vectors=tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
   myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->nchannels;
   if (tinfo->nr_of_points<=0) {
    ERREXIT1(tinfo->emethods, "read_synamps: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
   }
   tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
   if (array_allocate(&myarray)==NULL) {
    ERREXIT(tinfo->emethods, "read_synamps: Error allocating data\n");
   }
   /* Byte order is multiplexed, ie channels fastest */
   tinfo->multiplexed=TRUE;
   /*}}}  */

   local_arg->current_point=file_start_point;
   do {
    read_synamps_get_singlepoint(tinfo, &myarray);
   } while (myarray.message!=ARRAY_ENDOFSCAN);
   tinfo->file_start_point=file_start_point;
   }
   break;
  case NST_EPOCHS: {
   NEUROSCAN_EPOCHED_SWEEP_HEAD sweephead;
   void *buffer;
   
   /*{{{  Find a suitable epoch*/
   do {
    do {
     if (local_arg->current_trigger>=local_arg->EEG.compsweeps) return NULL;
     /* sm_NEUROSCAN_EPOCHED_SWEEP_HEAD[0].offset is sizeof(NEUROSCAN_EPOCHED_SWEEP_HEAD) on those other systems... */
     fseek(local_arg->SCAN,local_arg->current_trigger*(sm_NEUROSCAN_EPOCHED_SWEEP_HEAD[0].offset+local_arg->EEG.pnts*local_arg->EEG.nchannels*local_arg->bytes_per_sample)+local_arg->SizeofHeader,SEEK_SET);
     if (read_struct((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD, local_arg->SCAN)==0) {
      ERREXIT1(tinfo->emethods, "read_synamps_init: Sweep header read error in file %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
     }
#   ifndef LITTLE_ENDIAN
     change_byteorder((char *)&sweephead, sm_NEUROSCAN_EPOCHED_SWEEP_HEAD);
#   endif
     marker=(sweephead.type!=0 ? sweephead.type : -sweephead.response);
     if (local_arg->trigcodes==NULL) {
      not_correct_trigger=FALSE;
     } else {
      int trigno=0;
      not_correct_trigger=TRUE;
      while (local_arg->trigcodes[trigno]!=0) {
       if (local_arg->trigcodes[trigno]==marker) {
        not_correct_trigger=FALSE;
        if (args[ARGS_KN_REMAP].is_set) marker=trigno+1; /* Remap trigger codes */
        break;
       }
       trigno++;
      }
     }
     if (args[ARGS_NOREJECTED].is_set && sweephead.accept==0) not_correct_trigger=TRUE;
     local_arg->current_trigger++;
    } while (not_correct_trigger);
   } while (--local_arg->fromepoch>0);
   /*}}}  */

   /* If beforetrig or aftertrig are set to 0, the range is automatically 
    * extended to the start or end of the epoch, respectively. */
   if (tinfo->beforetrig==0) tinfo->beforetrig= local_arg->offset;
   start_point= -tinfo->beforetrig+local_arg->offset;
   if (tinfo->aftertrig==0) tinfo->aftertrig=local_arg->EEG.pnts-tinfo->beforetrig-start_point;
   /*{{{  Check the epoch sizes */
   if (tinfo->beforetrig>local_arg->offset) {
    ERREXIT(tinfo->emethods, "read_synamps: beforetrig>offset in epoch mode\n");
   }
   if (tinfo->beforetrig+tinfo->aftertrig>local_arg->EEG.pnts) {
    ERREXIT1(tinfo->emethods, "read_synamps: file epoch length is %d points, shorter than requested\n", MSGPARM(local_arg->EEG.pnts));
   }
   /*}}}  */

   /*{{{  Setup and allocate myarray*/
   myarray.element_skip=tinfo->itemsize=1;
   myarray.nr_of_vectors=tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
   myarray.nr_of_elements=tinfo->nr_of_channels=local_arg->nchannels;
   if (tinfo->nr_of_points<=0) {
    ERREXIT1(tinfo->emethods, "read_synamps: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
   }
   tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
   if (array_allocate(&myarray)==NULL
    || (buffer=malloc(local_arg->EEG.nchannels*local_arg->bytes_per_sample))==NULL) {
    ERREXIT(tinfo->emethods, "read_synamps: Error allocating data\n");
   }
   /* EEG order is multiplexed, ie channels fastest */
   tinfo->multiplexed=TRUE;
   /*}}}  */
   /*{{{  Read data*/
   TRACEMS2(tinfo->emethods, 1, "read_synamps: Reading epoch %d, StimType=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(marker));
   fseek(local_arg->SCAN,start_point*local_arg->EEG.nchannels*local_arg->bytes_per_sample,SEEK_CUR);
   do {
    int channel=0;
    if (fread(buffer,local_arg->bytes_per_sample,local_arg->EEG.nchannels,local_arg->SCAN)!=local_arg->EEG.nchannels) {
     ERREXIT(tinfo->emethods, "read_synamps: Error reading epoched data\n");
    }
    do {
     DATATYPE val;
     if (args[ARGS_NOBADCHANS].is_set && local_arg->Channels[channel].bad) {
      myarray.message=ARRAY_CONTINUE;
     } else {
      if (local_arg->bytes_per_sample==2) {
       int16_t * const pdata=((int16_t *)buffer)+channel;
#   ifndef LITTLE_ENDIAN
       Intel_int16(pdata);
#   endif
       val=NEUROSCAN_CONVSHORT(&local_arg->Channels[channel],*pdata);
      } else if (local_arg->bytes_per_sample==4) {
       int32_t * const pdata=((int32_t *)buffer)+channel;
#   ifndef LITTLE_ENDIAN
       Intel_int32(pdata);
#   endif
       val=NEUROSCAN_CONVSHORT(&local_arg->Channels[channel],*pdata);
      } else {
       ERREXIT(tinfo->emethods, "read_synamps: Unknown bytes_per_sample\n");
      }
      array_write(&myarray, val);
     }
     channel++;
    } while (myarray.message==ARRAY_CONTINUE);
   } while (myarray.message==ARRAY_ENDOFVECTOR);
   /*}}}  */
   free(buffer);
   tinfo->condition=marker;
   }
   break;
  case NST_AVERAGE:
   if (local_arg->current_trigger++>=1) return NULL;

   /* If beforetrig or aftertrig are set to 0, the range is automatically 
    * extended to the start or end of the epoch, respectively. */
   if (tinfo->beforetrig==0) tinfo->beforetrig= local_arg->offset;
   start_point= -tinfo->beforetrig+local_arg->offset;
   if (tinfo->aftertrig==0) tinfo->aftertrig=local_arg->EEG.pnts-tinfo->beforetrig-start_point;
   /*{{{  Check the epoch sizes */
   if (tinfo->beforetrig>local_arg->offset) {
    ERREXIT(tinfo->emethods, "read_synamps: beforetrig>offset in epoch mode\n");
   }
   if (tinfo->beforetrig+tinfo->aftertrig>local_arg->EEG.pnts) {
    ERREXIT1(tinfo->emethods, "read_synamps: file epoch length is %d points, shorter than requested\n", MSGPARM(local_arg->EEG.pnts));
   }
   /*}}}  */

   /*{{{  Setup and allocate myarray*/
   myarray.element_skip=tinfo->itemsize=1;
   myarray.nr_of_elements=tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
   myarray.nr_of_vectors=tinfo->nr_of_channels=local_arg->nchannels;
   if (tinfo->nr_of_points<=0) {
    ERREXIT1(tinfo->emethods, "read_synamps: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
   }
   tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
   if (array_allocate(&myarray)==NULL) {
    ERREXIT(tinfo->emethods, "read_synamps: Error allocating data\n");
   }
   /* AVG order is non-multiplexed, ie points fastest: vectors are the channels */
   tinfo->multiplexed=FALSE;
   /*}}}  */
   /*{{{  Read data*/
   {
   /* Here, we don't use automatic (stack) memory for the buffer, because the 
    * number of points can be large more easily than the number of channels... */
   float *pdata, *buffer=(float *)malloc(myarray.nr_of_elements*sizeof(float));
   int channel=0;
   if (buffer==NULL) {
    ERREXIT(tinfo->emethods, "read_synamps: Error allocating AVG buffer memory\n");
   }
   do {
    if (args[ARGS_NOBADCHANS].is_set && local_arg->Channels[channel].bad) {
     myarray.message=ARRAY_ENDOFVECTOR;
    } else {
    fseek(local_arg->SCAN,5+start_point*sizeof(float)+channel*(5+local_arg->EEG.pnts*sizeof(float))+local_arg->SizeofHeader,SEEK_SET);
    if ((int)fread(buffer,sizeof(float),myarray.nr_of_elements,local_arg->SCAN)!=myarray.nr_of_elements) {
     ERREXIT(tinfo->emethods, "read_synamps: Error reading averaged data\n");
    }
    pdata=buffer;
    do {
#   ifndef LITTLE_ENDIAN
     Intel_float(pdata);
#   endif
     array_write(&myarray, NEUROSCAN_CONVFLOAT(&local_arg->Channels[channel], *pdata));
     pdata++;
    } while (myarray.message==ARRAY_CONTINUE);
    }
    channel++;
   } while (myarray.message==ARRAY_ENDOFVECTOR);
   free(buffer);
   }
   /*}}}  */
   tinfo->condition=0;
   tinfo->nrofaverages=local_arg->Channels[0].n;
   break;
  default:
   ERREXIT1(tinfo->emethods, "read_synamps: Format `%s' is not yet supported\n", MSGPARM(neuroscan_subtype_names[local_arg->SubType]));
   break;
 }

 read_synamps_get_filestrings(tinfo);
 /* Construct comment */
 if (description==NULL) {
  int const datelen=(strlen(local_arg->EEG.date)>sizeof(local_arg->EEG.date) ? sizeof(local_arg->EEG.date) : strlen(local_arg->EEG.date));
  if ((tinfo->comment=(char *)malloc(strlen(local_arg->EEG.rev)+datelen+strlen(local_arg->EEG.time)+strlen(local_arg->EEG.id)+strlen(local_arg->EEG.patient)+5))==NULL) {
   ERREXIT(tinfo->emethods, "read_synamps: Error allocating comment\n");
  }
  strcpy(tinfo->comment, local_arg->EEG.rev); strcat(tinfo->comment, ",");
  strncat(tinfo->comment, local_arg->EEG.date, datelen); strcat(tinfo->comment, ",");
  strcat(tinfo->comment, local_arg->EEG.time); strcat(tinfo->comment, ",");
  strcat(tinfo->comment, local_arg->EEG.id); strcat(tinfo->comment, ","); 
  strcat(tinfo->comment, local_arg->EEG.patient);
 } else {
  if ((tinfo->comment=(char *)malloc(strlen(description)+1))==NULL) {
   ERREXIT(tinfo->emethods, "read_synamps: Error allocating comment\n");
  }
  strcpy(tinfo->comment, description);
 }

 tinfo->tsdata=myarray.start;
 tinfo->sfreq=local_arg->EEG.rate;
 tinfo->leaveright=0;
 if (local_arg->EEG.domain==1) {
  tinfo->data_type=FREQ_DATA;
  tinfo->windowsize=tinfo->nr_of_points;
  tinfo->nrofshifts=1;
  tinfo->nroffreq=tinfo->nr_of_points;
  tinfo->shiftwidth=0;
  /* I know that this field is supposed to contain the taper window size in %,
     but we need some way to store basefreq and 'rate' is only integer... */
  tinfo->basefreq=local_arg->EEG.SpectWinLength;
  if (tinfo->basefreq==0) {
   TRACEMS(tinfo->emethods, 1, "read_synamps: basefreq=SpectWinLength was zero for freq domain data, corrected to 1!\n");
   tinfo->basefreq=1;
  }
  tinfo->basetime=0;
 } else {
  tinfo->data_type=TIME_DATA;
 }

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_synamps_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_synamps_exit(transform_info_ptr tinfo) {
 struct read_synamps_storage * const local_arg=(struct read_synamps_storage *)tinfo->methods->local_storage;

 /* For DCMES, local_arg->Channels was incremented, and a wrong pointer
  * given to free() will lead to strange crashes */
 if (local_arg->SubType==NST_DCMES) local_arg->Channels--;

 free_pointer((void **)&local_arg->Channels);
 free_pointer((void **)&local_arg->buffer);
 free_pointer((void **)&local_arg->trigcodes);
 if (local_arg->triggers.buffer_start!=NULL) {
  clear_triggers(&local_arg->triggers);
  growing_buf_free(&local_arg->triggers);
 }
 fclose(local_arg->SCAN);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_synamps(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_synamps(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_synamps_init;
 tinfo->methods->transform= &read_synamps;
 tinfo->methods->transform_exit= &read_synamps_exit;
 tinfo->methods->get_singlepoint= &read_synamps_get_singlepoint;
 tinfo->methods->seek_point= &read_synamps_seek_point;
 tinfo->methods->get_filestrings= &read_synamps_get_filestrings;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_synamps";
 tinfo->methods->method_description=
  "Get-epoch method to read data from any of the NeuroScan formats\n"
  " `.CNT' (continuous), `.EEG' (epoched), or `.AVG' (averaged)\n";
 tinfo->methods->local_storage_size=sizeof(struct read_synamps_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
