/*
 * Copyright (C) 2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_nke.c module to read data from Nihon Kohden (2110/1100) formats
 *	-- Bernd Feige 03.08.2022
 * Based in part on nk2edf Copyright (C) 2007 - 2019 Teunis van Beelen
 *
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <read_struct.h>
#ifndef LITTLE_ENDIAN
#include <Intel_compat.h>
#endif
#include "nke_format.h"
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_CONTINUOUS=0,
 ARGS_TRIGTRANSFER, 
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
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these condition codes. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
};

LOCAL char const *const accepted_signatures[]={
 "EEG-1100A V01.00",
 "EEG-1100B V01.00",
 "EEG-1100C V01.00",
 "QI-403A   V01.00",
 "QI-403A   V02.00",
 "EEG-2100  V01.00",
 "EEG-2100  V02.00",
 "DAE-2100D V01.30",
 "DAE-2100D V02.00",
 "EEG-1100A V02.00",
 "EEG-1100B V02.00",
 "EEG-1100C V02.00",
 "EEG-1100A V02.0",
 NULL
};
LOCAL Bool check_signature(FILE *infile, long where) {
 char signature[17];
 int i;
 fseek(infile, where, SEEK_SET);
 fread(signature, 16, 1, infile);
 signature[16]=(char)0;
 for (i=0; accepted_signatures[i]!=NULL && strcmp(signature,accepted_signatures[i])!=0; i++);
 return accepted_signatures[i]!=NULL;
}
#define ELECTRODE_TAG "[ELECTRODE]"
#define REFERENCE_TAG "[REFERENCE]"
#define ELECTRODE_UNTAG "["
#define ELECTRODE_NAME_MAXLEN 256
#define COMMENT_MAXLEN 100
/* Compatible with synamps_sm.c */
#define trigcode_STARTSTOP 256

/*{{{  Definition of read_nke_storage*/
/* Per wfm_block we need to record the start address and data duration
 * Note that we allow only fixed sfreq and channels across blocks */
struct wfm_block {
 unsigned long data_start;
 unsigned long points_in_block;
};
/* These are stored in local_arg->nke21e_channels by read_21e 
 * until channel assignment is complete */
struct nke21e_channel {
 int chan;
 char channelname[ELECTRODE_NAME_MAXLEN];
};
struct read_nke_storage {
 DATATYPE *factor;	/* Factor to multiply the raw data with to obtain microvolts (per channel) */
 FILE *infile;
 int n_blocks;
 struct wfm_block *wfm_blocks;
 char comment[COMMENT_MAXLEN];
 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
 int nr_of_channels;
 growing_buf channelnames;
 growing_buf nke21e_channels;
 long stringlength;	/* Bytes to allocate for channel names */
 long points_in_file;
 long current_point;
 long beforetrig;
 long aftertrig;
 long offset;
 long fromepoch;
 long epochs;
 float sfreq;
};
/*}}}  */

LOCAL void read_21e(char const *eegfilename, transform_info_ptr tinfo) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 growing_buf filename;
 FILE *file21e;

 growing_buf_init(&filename);
 growing_buf_takethis(&filename,eegfilename);
 char *const dot=strrchr(filename.buffer_start,'.');
 if (dot!=NULL) {
  filename.current_length=dot-filename.buffer_start+1;
 }
 growing_buf_appendstring(&filename,".21e");
 file21e=fopen(filename.buffer_start, "rb");
 if (file21e!=NULL) {
  char buffer[ELECTRODE_NAME_MAXLEN];
  Bool seeking=TRUE;
  growing_buf_allocate(&local_arg->nke21e_channels,1024);
  TRACEMS1(tinfo->emethods, 1, "read_nke read_21e: Reading channel names from %s\n", MSGPARM(filename.buffer_start));
  while (!feof(file21e)) {
   if (fgets(buffer, ELECTRODE_NAME_MAXLEN-1, file21e)==NULL) break;
   if (seeking) {
    /* Still looking for ELECTRODE_TAG */
    if (strncmp(buffer, ELECTRODE_TAG, strlen(ELECTRODE_TAG)) == 0) seeking=FALSE;
   } else {
    if (strncmp(buffer, ELECTRODE_UNTAG, strlen(ELECTRODE_UNTAG)) == 0) break;
    /* Remove EOL */
    for (char *inbuf=buffer; *inbuf!='\0'; inbuf++) {
     if (*inbuf=='\r' || *inbuf=='\n') *inbuf='\0';
    }
    char *const equalsign=strchr(buffer,'=');
    if (equalsign==NULL) continue;
    *equalsign='\0';
    struct nke21e_channel nke21e_chan;
    nke21e_chan.chan=atoi(buffer);
    strncpy(nke21e_chan.channelname,equalsign+1,ELECTRODE_NAME_MAXLEN-1);
    growing_buf_append(&local_arg->nke21e_channels, (const char *)&nke21e_chan, sizeof(struct nke21e_channel));
   }
  }
  fclose(file21e);
 } else {
  TRACEMS(tinfo->emethods, 1, "read_nke read_21e: No .21e file available, using standard labels\n");
 }
 growing_buf_free(&filename);
}
LOCAL void add_channelname(transform_info_ptr tinfo, int chan) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 /* This function will add the requested channel name to the channelnames growing_buf */
 char *label=NULL;
 if (local_arg->nke21e_channels.can_be_freed) {
  /* Data from a 21e file is available */
  for (struct nke21e_channel *nke21e_chan=(struct nke21e_channel *)local_arg->nke21e_channels.buffer_start;
    nke21e_chan<(struct nke21e_channel *)(local_arg->nke21e_channels.buffer_start+local_arg->nke21e_channels.current_length);
    nke21e_chan++) {
   if (nke21e_chan->chan==chan) {
    label=nke21e_chan->channelname;
    break;
   }
  }
  if (label!=NULL) {
   growing_buf_append(&local_arg->channelnames,label,strlen(label)+1);
  }
 }
 /* We fall through here, setting standard labels if the .21e file doesn't contain them.
  * This sounds strange but from example files this seems the way to go... */
 if (label==NULL) {
  /* Set standard labels */
  switch (chan) {
   case 0:
    label="Fp1";
    break;
   case 1:
    label="Fp2";
    break;
   case 2:
    label="F3";
    break;
   case 3:
    label="F4";
    break;
   case 4:
    label="C3";
    break;
   case 5:
    label="C4";
    break;
   case 6:
    label="P3";
    break;
   case 7:
    label="P4";
    break;
   case 8:
    label="O1";
    break;
   case 9:
    label="O2";
    break;
   case 10:
    label="F7";
    break;
   case 11:
    label="F8";
    break;
   case 12:
    label="T3";
    break;
   case 13:
    label="T4";
    break;
   case 14:
    label="T5";
    break;
   case 15:
    label="T6";
    break;
   case 16:
    label="Fz";
    break;
   case 17:
    label="Cz";
    break;
   case 18:
    label="Pz";
    break;
   case 19:
    label="E";
    break;
   case 20:
    label="PG1";
    break;
   case 21:
    label="PG2";
    break;
   case 22:
    label="A1";
    break;
   case 23:
    label="A2";
    break;
   case 24:
    label="T1";
    break;
   case 25:
    label="T2";
    break;
   case 35:
    label="X10";
    break;
   case 36:
    label="X11";
    break;
   case 74:
    label="BN1";
    break;
   case 75:
    label="BN2";
    break;
   case 76:
    label="Mark1";
    break;
   case 77:
    label="Mark2";
    break;
   case 100:
    label="X12/BP1";
    break;
   case 101:
    label="X13/BP2";
    break;
   case 102:
    label="X14/BP3";
    break;
   case 103:
    label="X15/BP4";
    break;
   case 255:
    label="Z";
    break;
   default:
    break;
  }
  if (label!=NULL) {
   growing_buf_append(&local_arg->channelnames,label,strlen(label)+1);
  } else {
   char tmplabel[ELECTRODE_NAME_MAXLEN];
   if (chan>=26 && chan<35) {
    sprintf(tmplabel, "X%i", chan - 25);
   } else if (chan>=42 && chan<74) {
    sprintf(tmplabel, "DC%02i", chan - 41);
   } else if (chan>=104 && chan<188) {
    sprintf(tmplabel, "X%i", chan - 88);
   } else if (chan>=188 && chan<254) {
    sprintf(tmplabel, "X%i", chan - 88);
   } else {
    ERREXIT1(tinfo->emethods, "read_nke add_channelname: No label for channel ID %d\n", MSGPARM(chan));
   }
   growing_buf_append(&local_arg->channelnames,tmplabel,strlen(tmplabel)+1);
  }
 }
}

/*{{{  Maintaining the triggers list*/
LOCAL void 
read_nke_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}
/*}}}  */
/*{{{  read_nke_build_trigbuffer(transform_info_ptr tinfo) {*/
/* This function has all the knowledge about events in the various file types */
LOCAL void 
read_nke_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_nke_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_nke_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
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
  growing_buf logfilename;

  growing_buf_init(&logfilename);
  growing_buf_takethis(&logfilename,args[ARGS_IFILE].arg.s);
  char *const dot=strrchr(logfilename.buffer_start,'.');
  if (dot!=NULL) {
   logfilename.current_length=dot-logfilename.buffer_start+1;
  }
  growing_buf_appendstring(&logfilename,".log");
  FILE *logfile=fopen(logfilename.buffer_start, "rb");
  if (logfile!=NULL) {
   check_signature(logfile,0L);
   if (!check_signature(logfile,0L)) {
    ERREXIT1(tinfo->emethods, "read_nke_build_trigbuffer: Invalid signature in >%s<\n", MSGPARM(logfilename.buffer_start));
   }
   fseeko(logfile, 0x0091L, SEEK_SET);
   int log_block_cnt = fgetc(logfile);
   /* log_block_cnt can be at most 22; Afterwards "subevents" may follow with milliseconds information */
   Bool read_subevents=TRUE;
   for (int lg_block=0; lg_block<log_block_cnt; lg_block++) {
    fseeko(logfile, 0x0092L+lg_block*sm_nke_control_block[0].offset, SEEK_SET);
    struct nke_control_block clog_block,sclog_block;
    if (read_struct((char *)&clog_block, sm_nke_control_block, logfile)==0) {
     ERREXIT1(tinfo->emethods, "read_nke_build_trigbuffer: Can't read header in file >%s<\n", MSGPARM(logfilename.buffer_start));
    }
#ifndef LITTLE_ENDIAN
    change_byteorder((char *)&clog_block, sm_nke_control_block);
#endif
    // Read datablock_cnt which is a member of the first data block...
    fseeko(logfile, clog_block.address+18, SEEK_SET);
    int const data_block_cnt = fgetc(logfile);
    if (read_subevents) {
     /* Read subevents */
     if (fseeko(logfile, 0x0092L+(lg_block+22)*sm_nke_control_block[0].offset, SEEK_SET)) {
      read_subevents=FALSE;
     } else {
      if (read_struct((char *)&sclog_block, sm_nke_control_block, logfile)==0) {
       ERREXIT1(tinfo->emethods, "read_nke_build_trigbuffer: Can't read header in file >%s<\n", MSGPARM(logfilename.buffer_start));
      }
#ifndef LITTLE_ENDIAN
      change_byteorder((char *)&sclog_block, sm_nke_control_block);
#endif
      // Read datablock_cnt which is a member of the first data block...
      fseeko(logfile, sclog_block.address+18, SEEK_SET);
      int const subdata_block_cnt = fgetc(logfile);
      if (subdata_block_cnt!=data_block_cnt) {
       read_subevents=FALSE;
      }
     }
    }
    for (int dta_block=0; dta_block<data_block_cnt; dta_block++) {
     int code=1;
     char description[NKE_DESCRIPTION_LENGTH+1];
     fseeko(logfile, clog_block.address+20+dta_block*sm_nke_log_block[0].offset, SEEK_SET);
     struct nke_log_block log_block;
     if (read_struct((char *)&log_block, sm_nke_log_block, logfile)==0) {
      ERREXIT1(tinfo->emethods, "read_nke_build_trigbuffer: Can't read header in file >%s<\n", MSGPARM(logfilename.buffer_start));
     }
#ifndef LITTLE_ENDIAN
     change_byteorder((char *)&log_block, sm_nke_log_block);
#endif
     strncpy(description,(char *)&log_block.description,NKE_DESCRIPTION_LENGTH);
     description[NKE_DESCRIPTION_LENGTH]='\0';
     if (strncmp(description,"REC START",9)==0 ||
         strncmp(description,"Recording Gap",13)==0) {
      code=trigcode_STARTSTOP;
     }
     /* Format of timestamp[:6] is HHMMSS starting from recording start */
    /* Format of timestamp[:6] is HHMMSS starting from recording start */
    float marker_time_s=
     36000L*(log_block.timestamp[0]-'0')+
      3600L*(log_block.timestamp[1]-'0')+
       600L*(log_block.timestamp[2]-'0')+
        60L*(log_block.timestamp[3]-'0')+
        10L*(log_block.timestamp[4]-'0')+
            (log_block.timestamp[5]-'0');
     if (read_subevents) {
      /* Read the corresponding "sub event" */
      fseeko(logfile, sclog_block.address+20+dta_block*sm_nke_log_block[0].offset, SEEK_SET);
      struct nke_log_block slog_block;
      if (read_struct((char *)&slog_block, sm_nke_log_block, logfile)==0) {
       ERREXIT1(tinfo->emethods, "read_nke_build_trigbuffer: Can't read header in file >%s<\n", MSGPARM(logfilename.buffer_start));
      }
#ifndef LITTLE_ENDIAN
      change_byteorder((char *)&slog_block, sm_nke_log_block);
#endif
      marker_time_s+=
	 0.1*(slog_block.timestamp[4]-'0')+
	0.01*(slog_block.timestamp[5]-'0')+
       0.001*(slog_block.timestamp[6]-'0');
     }
     push_trigger(&local_arg->triggers, marker_time_s*local_arg->sfreq, code, description);
    }
   }
   fclose(logfile);
  } else {
   TRACEMS(tinfo->emethods, 0, "read_nke_build_trigbuffer: No trigger source known.\n");
  }
  growing_buf_free(&logfilename);
 }
}
/*}}}  */
/*{{{  read_nke_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_nke_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.buffer_start==NULL) {
  /* Load the event information */
  read_nke_build_trigbuffer(tinfo);
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

/*{{{  read_nke_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_nke_init(transform_info_ptr tinfo) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 growing_buf_init(&local_arg->triggers);
 growing_buf_init(&local_arg->channelnames);
 growing_buf_allocate(&local_arg->channelnames,1024);
 local_arg->channelnames.delimiters=""; /* Only \0 is a delimiter */

 local_arg->points_in_file=0L;
 if ((local_arg->infile=fopen(args[ARGS_IFILE].arg.s, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "read_nke_init: Can't open EEG file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 if (!check_signature(local_arg->infile,0L) || !check_signature(local_arg->infile,0x0081L)) {
  ERREXIT1(tinfo->emethods, "read_nke_init: Invalid signature in >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }

 /* Populate local_arg->nke21e_channels if a .21e file is available.
  * This is used only until the last add_channelname() is done, here in
  * read_nke_init. */
 growing_buf_init(&local_arg->nke21e_channels);
 read_21e(args[ARGS_IFILE].arg.s, tinfo);

 fseeko(local_arg->infile, 0x0091L, SEEK_SET);
 int const ctl_block_cnt = fgetc(local_arg->infile);
 local_arg->n_blocks=0; /* First pass through the data blocks to count the wfm_blocks */
 for (int ctl_block=0; ctl_block<ctl_block_cnt; ctl_block++) {
  fseeko(local_arg->infile, 0x0092L+ctl_block*sm_nke_control_block[0].offset, SEEK_SET);
  struct nke_control_block control_block;
  if (read_struct((char *)&control_block, sm_nke_control_block, local_arg->infile)==0) {
   ERREXIT1(tinfo->emethods, "read_nke_init: Can't read header in file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&control_block, sm_nke_control_block);
#endif
  fseeko(local_arg->infile, control_block.address+17, SEEK_SET);
  int const data_block_cnt = fgetc(local_arg->infile);
  local_arg->n_blocks+=data_block_cnt;
 }
 if ((local_arg->wfm_blocks=(struct wfm_block *)calloc(local_arg->n_blocks, sizeof(struct wfm_block)))==NULL) {
  ERREXIT(tinfo->emethods, "read_nke_init: Error allocating wfm_blocks memory\n");
 }
 int current_block=0;
 for (int ctl_block=0; ctl_block<ctl_block_cnt; ctl_block++) {
  fseeko(local_arg->infile, 0x0092L+ctl_block*sm_nke_control_block[0].offset, SEEK_SET);
  struct nke_control_block control_block;
  if (read_struct((char *)&control_block, sm_nke_control_block, local_arg->infile)==0) {
   ERREXIT1(tinfo->emethods, "read_nke_init: Can't read header in file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
  }
#ifndef LITTLE_ENDIAN
  change_byteorder((char *)&control_block, sm_nke_control_block);
#endif
  // Read datablock_cnt which is a member of the first data block...
  fseeko(local_arg->infile, control_block.address+17, SEEK_SET);
  int const data_block_cnt = fgetc(local_arg->infile);
  for (int dta_block=0; dta_block<data_block_cnt; dta_block++) {
   fseeko(local_arg->infile, control_block.address+dta_block*sm_nke_control_block[0].offset, SEEK_SET);
   struct nke_data_block data_block;
   if (read_struct((char *)&data_block, sm_nke_data_block, local_arg->infile)==0) {
    ERREXIT1(tinfo->emethods, "read_nke_init: Can't read header in file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&data_block, sm_nke_data_block);
#endif

   fseeko(local_arg->infile, data_block.wfmblock_address, SEEK_SET);
   struct nke_wfm_block wfm_block;
   if (read_struct((char *)&wfm_block, sm_nke_wfm_block, local_arg->infile)==0) {
    ERREXIT1(tinfo->emethods, "read_nke_init: Can't read header in file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
   }
#ifndef LITTLE_ENDIAN
   change_byteorder((char *)&wfm_block, sm_nke_wfm_block);
#endif
   wfm_block.sfreq&=0x3fff; /* Patch-up sfreq in place */
   long int const points_in_block=wfm_block.record_duration*wfm_block.sfreq/10L; /* 0.1s units */
   local_arg->points_in_file+=points_in_block;
   local_arg->wfm_blocks[current_block].data_start=data_block.wfmblock_address+sm_nke_wfm_block[0].offset+wfm_block.channels*sm_nke_channel_block[0].offset;
   local_arg->wfm_blocks[current_block].points_in_block=points_in_block;
   if (ctl_block==0 && dta_block==0) {
    /* Only prepare channel memory etc. once because we disallow changing parameters across blocks */
    if ((local_arg->factor=(DATATYPE *)calloc(wfm_block.channels,sizeof(DATATYPE)))==NULL) {
     ERREXIT(tinfo->emethods, "read_nke_init: Error allocating factor\n");
    }
    sprintf(local_arg->comment, "read_nke %02x/%02x/%02x,%02x:%02x:%02x", wfm_block.month, wfm_block.day, wfm_block.year, wfm_block.hour, wfm_block.minute, wfm_block.second);
    for (int channel=0; channel<wfm_block.channels; channel++) {
     fseeko(local_arg->infile, data_block.wfmblock_address+sm_nke_wfm_block[0].offset+channel*sm_nke_channel_block[0].offset, SEEK_SET);
     struct nke_channel_block channel_block;
     if (read_struct((char *)&channel_block, sm_nke_channel_block, local_arg->infile)==0) {
      ERREXIT1(tinfo->emethods, "read_nke_init: Can't read header in file >%s<\n", MSGPARM(args[ARGS_IFILE].arg.s));
     }
#ifndef LITTLE_ENDIAN
     change_byteorder((char *)&channel_block, sm_nke_channel_block);
#endif
     Bool const type1=(channel_block.chan<42 || channel_block.chan>73) && channel_block.chan!=76 && channel_block.chan!=77;
     local_arg->factor[channel]=type1 ? 0.09765625 : 0.3662994;
     add_channelname(tinfo,channel_block.chan);
    }
    growing_buf_free(&local_arg->nke21e_channels);
    local_arg->sfreq=wfm_block.sfreq;
    local_arg->nr_of_channels=wfm_block.channels;
   } else {
    if (wfm_block.sfreq!=local_arg->sfreq) {
     ERREXIT(tinfo->emethods, "read_nke_init: Blocks differ in sfreq\n");
    }
    if (wfm_block.channels!=local_arg->nr_of_channels) {
     ERREXIT(tinfo->emethods, "read_nke_init: Blocks differ in channels\n");
    }
   }
   current_block++;
  }
 }
 tinfo->points_in_file=local_arg->points_in_file;
 tinfo->sfreq=local_arg->sfreq;

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */
 /*{{{  Parse arguments that can be in seconds*/
 local_arg->beforetrig=tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->aftertrig=tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 /*}}}  */

 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_nke_init: Error allocating triglist memory\n");
   }
  }
 } else {
  if (local_arg->aftertrig==0) {
   /* Continuous mode: If aftertrig==0, automatically read up to the end of file */
   local_arg->aftertrig=local_arg->points_in_file-local_arg->beforetrig;
  }
 }
 TRACEMS3(tinfo->emethods, 1, "read_nke_init: Opened nke file %s with %d channels, Sfreq=%d.\n", MSGPARM(args[ARGS_IFILE].arg.s), MSGPARM(local_arg->nr_of_channels), MSGPARM(local_arg->sfreq));

 read_nke_reset_triggerbuffer(tinfo);
 local_arg->current_trigger=0;
 local_arg->current_point=0;

 tinfo->filetriggersp=&local_arg->triggers;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_nke(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_nke(transform_info_ptr tinfo) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;
 FILE *const infile=local_arg->infile;
 Bool not_correct_trigger=FALSE;
 long trigger_point, file_start_point, file_end_point;
 char *innamebuf;
 int channel;
 char *description=NULL;

 if (local_arg->epochs--==0) return NULL;
 tinfo->beforetrig=local_arg->beforetrig;
 tinfo->aftertrig=local_arg->aftertrig;
 tinfo->nr_of_points=local_arg->beforetrig+local_arg->aftertrig;
 tinfo->nr_of_channels=local_arg->nr_of_channels;
 tinfo->nrofaverages=1;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_nke: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
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
   tinfo->condition=read_nke_read_trigger(tinfo, &trigger_point, &description);
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
  } while (not_correct_trigger || file_start_point<0 || (file_end_point>=local_arg->points_in_file));
 } while (--local_arg->fromepoch>0);
 if (description==NULL) {
  TRACEMS3(tinfo->emethods, 1, "read_nke: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 } else {
  TRACEMS4(tinfo->emethods, 1, "read_nke: Reading around tag %d at %d, condition=%d, description=%s\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition), MSGPARM(description));
 }

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int trigs_in_epoch, code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  /* First trigger entry holds file_start_point */
  push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  read_nke_reset_triggerbuffer(tinfo);
  for (trigs_in_epoch=1; (code=read_nke_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
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
 tinfo->multiplexed=TRUE;
 myarray.nr_of_elements=tinfo->nr_of_channels;
 myarray.nr_of_vectors=tinfo->nr_of_points;
 if (array_allocate(&myarray)==NULL || 
    (tinfo->channelnames=(char **)malloc(tinfo->nr_of_channels*sizeof(char *)))==NULL ||
    (innamebuf=(char *)malloc(local_arg->channelnames.current_length))==NULL ||
    (tinfo->comment=(char *)malloc(strlen(local_arg->comment)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_nke: Error allocating data\n");
 }
 strcpy(tinfo->comment,local_arg->comment);
 /*}}}  */

 channel=0;
 local_arg->channelnames.current_token=local_arg->channelnames.buffer_start;
 while (channel<tinfo->nr_of_channels) {
  strcpy(innamebuf, local_arg->channelnames.current_token);
  tinfo->channelnames[channel]=innamebuf;
  innamebuf+=strlen(innamebuf)+1;
  if (!growing_buf_get_nexttoken(&local_arg->channelnames,NULL)) break;
  channel++;
 }

 long int thispoint=file_start_point;
 do {
  /* Find correct block and point within that block for thispoint */
  int current_block=0;
  int point_in_block=thispoint;
  for (; current_block<local_arg->n_blocks && point_in_block>=local_arg->wfm_blocks[current_block].points_in_block; current_block++) {
   point_in_block-=local_arg->wfm_blocks[current_block].points_in_block;
  }
  /* There is an extra channel at the end of each point that is discarded */
  fseek(infile,local_arg->wfm_blocks[current_block].data_start+point_in_block*(tinfo->nr_of_channels+1)*2,SEEK_SET);
  do {
   short tmpval;
   if (fread(&tmpval, 2, 1, infile)!=1) {
    ERREXIT(tinfo->emethods, "read_nke: Error reading data\n");
   }
   *(((char *)&tmpval)+1)+=128;
#ifndef LITTLE_ENDIAN
   Intel_int16((uint16_t *)&tmpval);
#endif
   array_write(&myarray, tmpval*local_arg->factor[myarray.current_element]);
  } while (myarray.message==ARRAY_CONTINUE);
  thispoint++;
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 /* Force create_channelgrid to really allocate the channel info anew.
  * Otherwise, deepfree_tinfo will free someone else's data ! */
 tinfo->probepos=NULL;
 tinfo->xdata=NULL;
 create_channelgrid(tinfo); /* Create defaults for any missing channel info */

 tinfo->file_start_point=file_start_point;
 tinfo->z_label=NULL;
 tinfo->sfreq=local_arg->sfreq;
 tinfo->tsdata=myarray.start;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points;
 tinfo->leaveright=0;
 tinfo->data_type=TIME_DATA;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_nke_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_nke_exit(transform_info_ptr tinfo) {
 struct read_nke_storage *local_arg=(struct read_nke_storage *)tinfo->methods->local_storage;

 fclose(local_arg->infile);
 local_arg->infile=NULL;
 free_pointer((void **)&local_arg->trigcodes);

 if (local_arg->triggers.buffer_start!=NULL) {
  clear_triggers(&local_arg->triggers);
  growing_buf_free(&local_arg->triggers);
 }
 free_pointer((void **)&local_arg->wfm_blocks);
 free_pointer((void **)&local_arg->factor);
 growing_buf_free(&local_arg->channelnames);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_nke(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_nke(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_nke_init;
 tinfo->methods->transform= &read_nke;
 tinfo->methods->transform_exit= &read_nke_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_nke";
 tinfo->methods->method_description=
  "Get-epoch method to read Nihon Kohden 2110/1100 formats.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_nke_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
