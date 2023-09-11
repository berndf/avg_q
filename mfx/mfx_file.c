/*
 * Copyright (C) 1992-1998,2003,2007,2011-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfx_file.c i/o module to handle the multi-channel MFX (M"unster file exchange)
 * format.					Bernd Feige 23.11.1992
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include "mfx_file.h"
#include "mfx_old_formats.h"

#ifdef LITTLE_ENDIAN
#include "Intel_compat.h"
#include "fix_headers.h"
#endif
/*}}}  */

/*{{{  #defines*/
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/*}}}  */

/*{{{  Global variables*/
/*
 * The first global variable we define holds the last error number
 * (error numbers and messages defined in mfx_file.h);
 * The second the automatically generated RCS version information
 */

mfx_errtype mfx_lasterr;
char *mfx_version="$Header: /home/charly/.cvsroot/avg_q/mfx/mfx_file.c,v 1.37 2007/05/21 12:48:15 charly Exp $";
/*}}}  */

/*{{{  int mfx_get_channel_number(MFX_FILE* mfxfile, char* channelname)*/
/*
 * mfx_get_channel_number returns the number of the
 * channel named channelname
 */
int mfx_get_channel_number(MFX_FILE* mfxfile, char const * channelname) {
 int i;
 for (i=0; i<mfxfile->fileheader.total_chans && 
	   strncmp(mfxfile->channelheaders[i].channel_name, channelname, MFX_CHANNELNAMELENGTH-1)!=0; i++);
 if (i==mfxfile->fileheader.total_chans) return 0; /* Not found */
 return i+1;
}
/*}}}  */

/*{{{  mfx_open(char *filename, char *filemode, enum mfx_datatypes datatype)*/
/*
 * mfx_open returns an MFX_FILE pointer if successful.
 * datatype is MFX_SHORTS, MFX_FLOATS or MFX_DOUBLES (the output
 * data type)
 */

MFX_FILE*
mfx_open(char const *filename, char const *filemode, enum mfx_datatypes datatype) {
 MFX_FILE* mfxfile;

 if ((mfxfile=(MFX_FILE*)malloc(sizeof(struct MFX_FILE_STRUCT)))==NULL) {
  mfx_lasterr=MFX_NOMEM1;
  return (MFX_FILE*)NULL;
 }
 if ((mfxfile->mfxstream=fopen(filename, filemode))==NULL) {
  free((void*)mfxfile);
  mfx_lasterr=MFX_NOFILE;
  return (MFX_FILE*)NULL;
 }
 if (fread(&mfxfile->typeheader, sizeof(struct mfx_header_format_data), 1, mfxfile->mfxstream)!=1) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile);
  mfx_lasterr=MFX_READERR1;
  return (MFX_FILE*)NULL;
 }
#ifdef LITTLE_ENDIAN
 fix_typeheader(&mfxfile->typeheader);
#endif
 if (mfxfile->typeheader.mfx_header_format!=1) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile);
  mfx_lasterr=MFX_UNKNOWNHTYPE;
  return (MFX_FILE*)NULL;
 }
 if (mfxfile->typeheader.mfx_data_format!=1) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile);
  mfx_lasterr=MFX_UNKNOWNDTYPE;
  return (MFX_FILE*)NULL;
 }
 if (fread(&mfxfile->fileheader, sizeof(struct mfx_header_data001), 1, mfxfile->mfxstream)!=1) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile);
  mfx_lasterr=MFX_READERR2;
  return (MFX_FILE*)NULL;
 }
#ifdef LITTLE_ENDIAN
 fix_fileheader(&mfxfile->fileheader);
#endif
 if ((mfxfile->channelheaders=(struct mfx_channel_data001*)
      malloc(mfxfile->fileheader.total_chans*sizeof(struct mfx_header_data001)))==NULL) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile);
  mfx_lasterr=MFX_NOMEM2;
  return (MFX_FILE*)NULL;
 }
 if ((int)fread(mfxfile->channelheaders, sizeof(struct mfx_channel_data001), 
	mfxfile->fileheader.total_chans, mfxfile->mfxstream)!=mfxfile->fileheader.total_chans) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile->channelheaders);
  free((void*)mfxfile);
  mfx_lasterr=MFX_READERR3;
  return (MFX_FILE*)NULL;
 }
#ifdef LITTLE_ENDIAN
 {int channel;
  for (channel=0; channel<mfxfile->fileheader.total_chans; channel++) {
   fix_channelheader(&mfxfile->channelheaders[channel]);
  }
 }
#endif
 mfxfile->data_offset=ftell(mfxfile->mfxstream);	/* Save data start for seeking */
 fseek(mfxfile->mfxstream, 0, 2);			/* Find the actual file length */
 mfxfile->mfxlen=(ftell(mfxfile->mfxstream)-mfxfile->data_offset)/sizeof(int16_t)/
		  mfxfile->fileheader.total_chans;
 mfxfile->subversion=SUBVERSION_PAST1192;
 if (mfxfile->mfxlen!=mfxfile->fileheader.total_epochs*mfxfile->fileheader.pts_in_epoch) {
  /* Does the file size difference indicate this is an old file ? */
  if ((mfxfile->fileheader.total_epochs*mfxfile->fileheader.pts_in_epoch-
      mfxfile->mfxlen)*sizeof(int16_t)==
      sizeof(struct mfx_channel_data001)-sizeof(struct mfx_channel_data001a)) {
   int channel;
   struct mfx_channel_data001a *oldchannelp=
	((struct mfx_channel_data001a*)mfxfile->channelheaders)+
	mfxfile->fileheader.total_chans-1;
   struct mfx_channel_data001  *newchannelp=
	mfxfile->channelheaders+mfxfile->fileheader.total_chans-1;

   for (channel=mfxfile->fileheader.total_chans-1; channel>=0; channel--) {
    /* First copy the body of the structure if necessary */
    if (channel!=0)
     memcpy(newchannelp, oldchannelp, sizeof(struct mfx_channel_data001a));
    /* Then reposition the tail part of the structure */
    memmove(newchannelp->position,
    	((struct mfx_channel_data001a*)newchannelp)->position,
    	22*sizeof(double));
#   ifdef LITTLE_ENDIAN
    fix_channelheader(newchannelp);
#   endif

    /* This sets the correct values for conv_factor and shift_'factor' */
    WRITECONVSTRUCT((*newchannelp), newchannelp->ymin, newchannelp->ymax);
    oldchannelp--; newchannelp--;
   }
   /* Adjust the data_offset position and mfxlen accordingly */
   mfxfile->data_offset-=(sizeof(struct mfx_channel_data001)-sizeof(struct mfx_channel_data001a))*mfxfile->fileheader.total_chans;
   mfxfile->mfxlen=mfxfile->fileheader.total_epochs*
		   mfxfile->fileheader.pts_in_epoch;
   mfxfile->subversion=SUBVERSION_TILL1192;
  } else {
   fclose(mfxfile->mfxstream);
   free((void*)mfxfile->channelheaders);
   free((void*)mfxfile);
   mfx_lasterr=MFX_FILELEN;
   return (MFX_FILE*)NULL;
  }
 }
 if ((mfxfile->input_buffer=(int16_t*)
      malloc(mfxfile->fileheader.total_chans*sizeof(int16_t)))==NULL) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile->channelheaders);
  free((void*)mfxfile);
  mfx_lasterr=MFX_NOMEM2;
  return (MFX_FILE*)NULL;
 }
 fseek(mfxfile->mfxstream, mfxfile->data_offset, 0);
 mfxfile->mfxpos=0;
 mfxfile->datatype=datatype;
 mfxfile->nr_of_channels_selected=0;
 mfxfile->selected_channels=(int*)NULL;
 strcpy(mfxfile->filename, filename); 
 strcpy(mfxfile->filemode, filemode); 
 mfxfile->was_just_created=FALSE;
 mfx_lasterr=MFX_NOERR;
 return mfxfile;
}
/*}}}  */

/*{{{  mfx_create(char *filename, int nr_of_channels, enum mfx_datatypes datatype)*/
/*
 * mfx_create returns an MFX_FILE pointer if successful.
 * The number of channels has to be determined at this stage;
 * Additional file and channel information can be given by
 * mfx_describefile and mfx_describechannel.
 */

MFX_FILE*
mfx_create(char const *filename, int nr_of_channels, enum mfx_datatypes datatype) {
 MFX_FILE* mfxfile;
 const time_t myClock=time(NULL);
 if ((mfxfile=(MFX_FILE*)calloc(1, sizeof(struct MFX_FILE_STRUCT)))==NULL) {
  mfx_lasterr=MFX_NOMEM1;
  return (MFX_FILE*)NULL;
 }
 if ((mfxfile->mfxstream=fopen(filename, "w+b"))==NULL) {
  free((void*)mfxfile);
  mfx_lasterr=MFX_NOFILE;
  return (MFX_FILE*)NULL;
 }
 mfxfile->typeheader.mfx_header_format=1;
 mfxfile->typeheader.mfx_data_format=1;
 mfxfile->fileheader.total_chans=nr_of_channels;
 if ((mfxfile->channelheaders=(struct mfx_channel_data001*)
      calloc(mfxfile->fileheader.total_chans, sizeof(struct mfx_header_data001)))==NULL) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile);
  mfx_lasterr=MFX_NOMEM2;
  return (MFX_FILE*)NULL;
 }
 mfxfile->subversion=SUBVERSION_PAST1192;
 if ((mfxfile->input_buffer=(int16_t*)
      malloc(mfxfile->fileheader.total_chans*sizeof(int16_t)))==NULL) {
  fclose(mfxfile->mfxstream);
  free((void*)mfxfile->channelheaders);
  free((void*)mfxfile);
  mfx_lasterr=MFX_NOMEM2;
  return (MFX_FILE*)NULL;
 }
 mfxfile->mfxlen=0;
 mfxfile->mfxpos=0;
 mfxfile->datatype=datatype;
 mfxfile->nr_of_channels_selected=0;
 mfxfile->selected_channels=(int*)NULL;
 strcpy(mfxfile->filename, filename); 
 strcpy(mfxfile->filemode, "w+b"); 
 strcpy(mfxfile->fileheader.time_stamp, ctime(&myClock)); 
 strcpy(mfxfile->fileheader.file_descriptor, "File created by mfx_create");
 mfxfile->was_just_created=TRUE;
 mfx_lasterr=MFX_NOERR;
 return mfxfile;
}
/*}}}  */

/*{{{  static mfx_errtype mfx_flushheaders(MFX_FILE* mfxfile)*/
/*
 * Internal mfx_flushheaders writes the file headers if the file has
 * been created and it is closed or something gets written to it
 */
static mfx_errtype
mfx_flushheaders(MFX_FILE* mfxfile) {
 long sizeofchannelheader=sizeof(struct mfx_channel_data001);
 int channel;
 struct mfx_header_format_data fixed_typeheader=mfxfile->typeheader;
 struct mfx_header_data001 fixed_fileheader=mfxfile->fileheader;
 struct mfx_channel_data001 fixed_channelheader;

 fseek(mfxfile->mfxstream, 0, 0);
#ifdef LITTLE_ENDIAN
 fix_typeheader(&fixed_typeheader);
 fix_fileheader(&fixed_fileheader);
#endif
 if (fwrite(&fixed_typeheader, sizeof(struct mfx_header_format_data), 1, 
	    mfxfile->mfxstream)!=1 ||
     fwrite(&fixed_fileheader, sizeof(struct mfx_header_data001), 1, 
	    mfxfile->mfxstream)!=1) {
  return mfx_lasterr=MFX_HEADERFLUSHERR;
 }
 for (channel=0; channel<mfxfile->fileheader.total_chans; channel++) {
  fixed_channelheader=mfxfile->channelheaders[channel];
# ifdef LITTLE_ENDIAN
  fix_channelheader(&fixed_channelheader);
# endif
  if (mfxfile->subversion==SUBVERSION_TILL1192) {
   memmove(((struct mfx_channel_data001a *)&fixed_channelheader)->position, fixed_channelheader.position, 22*sizeof(double));
   sizeofchannelheader=sizeof(struct mfx_channel_data001a);
  }
  if (fwrite(&fixed_channelheader, sizeofchannelheader, 
	     1, mfxfile->mfxstream)!=1) {
   return mfx_lasterr=MFX_HEADERFLUSHERR;
  }
 }
 mfxfile->data_offset=ftell(mfxfile->mfxstream);
 mfxfile->was_just_created=FALSE;
 return mfx_lasterr=MFX_NOERR;
}
/*}}}  */

/*{{{  mfx_describefile(MFX_FILE* mfxfile, char *file_descriptor, float sample_period, int pts_per_epoch, float epoch_begin_latency)*/
/*
 * mfx_describefile sets the file-wide description variables that can be
 * freely set. If pts_per_epoch==0, the file is set to continuous mode.
 * Only applicable after create.
 */

mfx_errtype
mfx_describefile(MFX_FILE* mfxfile, char const *file_descriptor, float sample_period, int pts_per_epoch, float epoch_begin_latency) {
 if (!mfxfile->was_just_created) return mfx_lasterr=MFX_HEADERACCESS;
 strncpy(mfxfile->fileheader.file_descriptor, file_descriptor, MFX_FILEDESCRIPTORLENGTH-1);
 mfxfile->fileheader.epoch_begin_latency=epoch_begin_latency;
 mfxfile->fileheader.sample_period=sample_period;
 /* Our way to decide that we are in continuous mode is that total_epochs==1 */
 if ((mfxfile->fileheader.pts_in_epoch=pts_per_epoch)==0) {
  mfxfile->fileheader.total_epochs=1;
 } else {
  mfxfile->fileheader.total_epochs=0;
 }
 mfxfile->fileheader.input_epochs=mfxfile->fileheader.total_epochs;
 return mfx_lasterr=MFX_NOERR;
} 
/*}}}  */

/*{{{  mfx_describechannel(MFX_FILE* mfxfile, int channel_nr, char *channelname, char *yaxis_label, unsigned short data_type, double ymin, double ymax)*/
/*
 * mfx_describechannel sets the channel parameters that can be freely set.
 * Only applicable after create.
 */

mfx_errtype
mfx_describechannel(MFX_FILE* mfxfile, int channel_nr, char const *channelname, char const *yaxis_label, unsigned short data_type, double ymin, double ymax) {
 struct mfx_channel_data001* channelstructp=mfxfile->channelheaders+(channel_nr-1);
 if (!mfxfile->was_just_created) return mfx_lasterr=MFX_HEADERACCESS;
 channelstructp->data_type=data_type;
 strncpy(channelstructp->channel_name, channelname, MFX_CHANNELNAMELENGTH-1);
 strncpy(channelstructp->yaxis_label, yaxis_label, MFX_YAXISLABELLENGTH-1);
 WRITECONVSTRUCT((*channelstructp), ymin, ymax);
 return mfx_lasterr=MFX_NOERR;
}
/*}}}  */

/*{{{  mfx_close(MFX_FILE* mfxfile)*/
/*
 * mfx_close closes the input stream and frees the structure memory.
 * If the number of points in file has changed (ie, there were points
 * added), the point count in the header is calculated and saved.
 * In this process, multi-epoch files are recognized and extended to
 * whole epochs if necessary.
 */

mfx_errtype 
mfx_close(MFX_FILE* mfxfile) {
 if (mfxfile->mfxlen!=mfxfile->fileheader.total_epochs*mfxfile->fileheader.pts_in_epoch) {
  if (mfxfile->fileheader.total_epochs==1) {
   mfxfile->fileheader.pts_in_epoch=mfxfile->mfxlen;
  } else {
   long leftover=mfxfile->mfxlen%mfxfile->fileheader.pts_in_epoch;
   mfxfile->fileheader.total_epochs=mfxfile->mfxlen/mfxfile->fileheader.pts_in_epoch;
   if (leftover>0) {
    /* There are data rows left to write to complete the last epoch */
    if (mfx_seek(mfxfile, 0L, SEEK_END)!=MFX_NOERR) return mfx_lasterr;
    memset(mfxfile->input_buffer, 0, sizeof(int16_t)*mfxfile->fileheader.total_chans);
    leftover=mfxfile->fileheader.pts_in_epoch-leftover;
    while (leftover--) {
     if ((int)fwrite(mfxfile->input_buffer, sizeof(int16_t), mfxfile->fileheader.total_chans, 
		mfxfile->mfxstream)!=mfxfile->fileheader.total_chans) {
      return mfx_lasterr=MFX_WRITEERR4;
     }
    }
    mfxfile->fileheader.total_epochs++;
   }
   mfxfile->fileheader.input_epochs=mfxfile->fileheader.total_epochs;
  }
  mfx_flushheaders(mfxfile);
 }
 if (mfxfile->was_just_created) mfx_flushheaders(mfxfile);
 fclose(mfxfile->mfxstream);
 free((void*)mfxfile->channelheaders);
 mfxfile->nr_of_channels_selected=0;
 free((void*)mfxfile->selected_channels);
 free((void*)mfxfile->input_buffer);
 free((void*)mfxfile);
 return mfx_lasterr=MFX_NOERR;
}
/*}}}  */

/*{{{  mfx_cselect(MFX_FILE* mfxfile, char* channelnames)*/
/*{{{  Name expansion*/
/*
 * Internal startexpand(inlist) initializes the expansion of a 'list'
 * string like "FG12-FG15, tt, TRIGGER, t2-t5" into "FG12", "FG13", ...
 * The expanded strings are returned with each call to nextexpand.
 * nextexpand returns NULL if no further elements are available.
 */
static struct {
 char const *currpos;	/* The interpreter position in the input sring */
 int  endcount;
 int  count;
 char *numpos;	/* The position at which numbers are printed in outbuf */
 char outbuf[80];
} expandinfo;

static void
startexpand(char const * inlist) {
 expandinfo.currpos=inlist;
 expandinfo.endcount=0;	/* This signals that no expansion occurs now */
}
static char*
nextexpand(void) {
 char const *current;
 char *tobuf;

 if (expandinfo.endcount) {	/* Expansion in progress */
  sprintf(expandinfo.numpos, "%d", expandinfo.count);
  if (expandinfo.count++==expandinfo.endcount) expandinfo.endcount=0;
  mfx_lasterr=MFX_NOERR;
  return expandinfo.outbuf;
 }
 /* Normal reading: */
 current=expandinfo.currpos;
 expandinfo.count=0;
 while (1) {
  /* Skip white space, commas and hyphens we were possibly left on */
  while (*current && strchr(" \t,-", *current)) current++;
  if (*current=='\0') {
   /* expandinfo.count is set if the starting value of a range was read */
   mfx_lasterr=(expandinfo.count==0 ? MFX_NOERR : MFX_EXPANDUNTERMINATED);
   return (char*)NULL;
  }
  /* Now copy characters to outbuf up to next delimiter */
  {int escaped=FALSE;
  for (tobuf=expandinfo.outbuf, expandinfo.numpos=NULL; 
       (escaped==TRUE || strchr("-,", *current)==NULL)
	&& *current!='\0'; current++) {
   if (escaped==FALSE && *current=='\\') {
    escaped=TRUE;
    continue;
   }
   if (*current>='0' && *current<='9') {
    if (expandinfo.numpos==NULL) {
     expandinfo.numpos=tobuf;
    }
   } else {
    expandinfo.numpos=NULL;
   }
   *tobuf++=*current;
   escaped=FALSE;
  }
  }
  /* Terminate tobuf string and decide what to do */
  *tobuf='\0';
  expandinfo.currpos=current;
  switch (*current) {
   case '\0':
   case ',':
    /* Was there a range started ? If not, just return outbuf */
    if (expandinfo.count==0) {
     mfx_lasterr=MFX_NOERR;
     return expandinfo.outbuf;
    }
    /* Else: End the specified interval */
    if (expandinfo.numpos==NULL) {
     mfx_lasterr=MFX_EXPANDNONUM;
     return (char*)NULL;
    }
    expandinfo.endcount=atoi(expandinfo.numpos);
    if (expandinfo.endcount<expandinfo.count) {
     mfx_lasterr=MFX_EXPANDINTERVAL;
     return (char*)NULL;
    }
    return nextexpand();
    break;
   case '-':	/* Start of an interval */
    if (expandinfo.numpos==NULL) {
     mfx_lasterr=MFX_EXPANDNONUM;
     return (char*)NULL;
    }
    expandinfo.count=atoi(expandinfo.numpos);
    break;
  }
 }
}
/*}}}  */

/*
 * mfx_cselect
 *  is used to select the channels to be read by channel names
 *  channelnames is a string of the form "A2-A5,A9, t3"
 *  As a special request, "ALL" means that all channels in file get selected.
 */

int
mfx_cselect(MFX_FILE* mfxfile, char const * channelnames) {
 int const nr_of_channels=mfxfile->fileheader.total_chans;
 char* channelname;
 int argcnt, channelnr;
 Bool ignoreNA=FALSE, invertselection=FALSE;

 /*{{{  Handle "ALL" request*/
 if (strcmp(channelnames, "ALL")==0) {
  if ((mfxfile->selected_channels=(int *)malloc(nr_of_channels*sizeof(int)))==NULL) {
   mfx_lasterr=MFX_CHANSELNOMEM;
   return 0;
  }
  for (argcnt=0; argcnt<nr_of_channels; argcnt++) {
   mfxfile->selected_channels[argcnt]=argcnt+1;
  }
  mfxfile->nr_of_channels_selected=argcnt;
  mfx_lasterr=MFX_NOERR;
  return argcnt;
 }
 /*}}}  */

 while (TRUE) {
  switch(*channelnames) {
   case '?':
    /* If names string begins with this char, a missing channel name
     * is not a fatal condition (ignore if not available) */
    ignoreNA=TRUE;
    channelnames++;
    continue;
   case '!':
    /* If names string begins with this char, all channels EXCEPT those 
     * mentioned are selected */
    invertselection=TRUE;
    channelnames++;
    continue;
   default:
    break;
  }
  break;
 }

 if (invertselection) {
  Bool *channelmap=(Bool *)calloc(nr_of_channels, sizeof(Bool));
  if (channelmap==NULL) {
   mfx_lasterr=MFX_CHANSELNOMEM;
   return 0;
  }
  /* We create a channel map where we mark channels for deletion and
   * then return a list of the remaining */
  startexpand(channelnames);
  while ((channelname=nextexpand())!=(char *)NULL) {
   if ((channelnr=mfx_get_channel_number(mfxfile, channelname))==0) {
    if (!ignoreNA) {
     mfx_lasterr=MFX_WRONGCHANNELNAME;
     return 0;
    }
   } else {
    channelmap[channelnr-1]=TRUE;
   }
  }
  if (mfx_lasterr!=MFX_NOERR) return 0;
  argcnt=0;
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (!channelmap[channelnr]) argcnt++;
  }
  free((void*)mfxfile->selected_channels);
  mfxfile->nr_of_channels_selected=0;
  if ((mfxfile->selected_channels=(int *)malloc((argcnt+1)*sizeof(int)))==NULL) {
   mfx_lasterr=MFX_CHANSELNOMEM;
   return 0;
  }
  argcnt=0;
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (!channelmap[channelnr]) mfxfile->selected_channels[argcnt++]=channelnr+1;
  }
  free(channelmap);
 } else {

 startexpand(channelnames);	/* First traversal to count selected channels */
 for (argcnt=0; (channelname=nextexpand())!=(char *)NULL; ) {
  Bool found_one=FALSE;
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (strncmp(mfxfile->channelheaders[channelnr].channel_name, channelname, MFX_CHANNELNAMELENGTH-1)==0) {
    argcnt++;
    found_one=TRUE;
   }
  }
  if (!found_one) {
   if (!ignoreNA) {
    mfx_lasterr=MFX_WRONGCHANNELNAME;
    return 0;
   }
  }
 }
 if (mfx_lasterr!=MFX_NOERR) return 0;
 free((void*)mfxfile->selected_channels);
 mfxfile->nr_of_channels_selected=0;
 if ((mfxfile->selected_channels=(int *)malloc(argcnt*sizeof(int)))==NULL) {
  mfx_lasterr=MFX_CHANSELNOMEM;
  return 0;
 }
 startexpand(channelnames);	/* Now read the channel numbers */
 for (argcnt=0; (channelname=nextexpand())!=(char *)NULL; ) {
  for (channelnr=0; channelnr<nr_of_channels; channelnr++) {
   if (strncmp(mfxfile->channelheaders[channelnr].channel_name, channelname, MFX_CHANNELNAMELENGTH-1)==0) {
    mfxfile->selected_channels[argcnt++]=channelnr+1;
   }
  }
 }

 }
 mfxfile->nr_of_channels_selected=argcnt;
 mfx_lasterr=MFX_NOERR;
 return argcnt;
}
/*}}}  */

/*{{{  mfx_read(void *tobuffer, long count, MFX_FILE *mfxfile)*/
/*
 * mfx_read reads count values of the selected channels, transforms
 * them into the requested data type and writes them into the user-supplied
 * buffer
 */

mfx_errtype
mfx_read(void *tobuffer, long count, MFX_FILE *mfxfile) {
 char *bufptr;
 int i, channel;
 int16_t datum;
 if (mfxfile->nr_of_channels_selected==0) {
  return mfx_lasterr=MFX_NOCHANNEL;
 }
 bufptr=(char *)tobuffer;
 while (count--) {
  if ((int)fread(mfxfile->input_buffer, sizeof(int16_t), mfxfile->fileheader.total_chans, 
	    mfxfile->mfxstream)!=mfxfile->fileheader.total_chans) {
   return mfx_lasterr=MFX_READERR4;
  }
  mfxfile->mfxpos++;
  for (i=0; i<mfxfile->nr_of_channels_selected; i++) {
   channel=mfxfile->selected_channels[i];
   datum=mfxfile->input_buffer[--channel];
#  ifdef LITTLE_ENDIAN
   Intel_int16((uint16_t *)&datum);
#  endif
   switch (mfxfile->datatype) {
    case MFX_SHORTS:
     *(int16_t*)bufptr=datum;
     bufptr+=sizeof(int16_t);
     break;
    case MFX_FLOATS:
     *(float*)bufptr=DATACONV(mfxfile->channelheaders[channel], datum);
     bufptr+=sizeof(float);
     break;
    case MFX_DOUBLES:
     *(double*)bufptr=DATACONV(mfxfile->channelheaders[channel], datum);
     bufptr+=sizeof(double);
     break;
   }
  }
 }
 return mfx_lasterr=MFX_NOERR;
}
/*}}}  */

/*{{{  mfx_write(void *frombuffer, long count, MFX_FILE *mfxfile)*/
/*
 * mfx_write writes count values of the selected channels, transforming
 * the requested data type from the user-supplied buffer into mfx shorts.
 * Only the selected channels of the written time slices are modified;
 * the other channel values remain untouched if an existing timeslice
 * is overwritten and set to zero if new timeslices are appended.
 * Note that the flags for mfx_open should be "r+b" (ie, update mode)
 * to use mfx_write !
 */

mfx_errtype
mfx_write(void *frombuffer, long count, MFX_FILE *mfxfile) {
 char *bufptr;
 int i, channel;
 int16_t datum;
 if (mfxfile->was_just_created) mfx_flushheaders(mfxfile);
 if (mfxfile->nr_of_channels_selected==0) {
  return mfx_lasterr=MFX_NOCHANNEL;
 }
 bufptr=(char *)frombuffer;
 while (count--) {
  /* First, read the current values for this time slice. Zero the buffer if
   * a new time slice is appended. 
   */
  if (mfxfile->mfxpos<mfxfile->mfxlen) {
   if ((int)fread(mfxfile->input_buffer, sizeof(int16_t), mfxfile->fileheader.total_chans, 
	     mfxfile->mfxstream)!=mfxfile->fileheader.total_chans) {
    return mfx_lasterr=MFX_READERR4;
   }
   fseek(mfxfile->mfxstream, -sizeof(int16_t)*mfxfile->fileheader.total_chans, 1);
  } else memset(mfxfile->input_buffer, 0, sizeof(int16_t)*mfxfile->fileheader.total_chans);
  for (i=0; i<mfxfile->nr_of_channels_selected; i++) {
   channel=mfxfile->selected_channels[i]-1;
   switch (mfxfile->datatype) {
    case MFX_SHORTS:
     datum=*(int16_t*)bufptr;
     bufptr+=sizeof(int16_t);
     break;
    case MFX_FLOATS:
     datum=CONVDATA(mfxfile->channelheaders[channel], *(float*)bufptr);
     bufptr+=sizeof(float);
     break;
    case MFX_DOUBLES:
     datum=CONVDATA(mfxfile->channelheaders[channel], *(double*)bufptr);
     bufptr+=sizeof(double);
     break;
   }
#  ifdef LITTLE_ENDIAN
   Intel_int16((uint16_t *)&datum);
#  endif
   mfxfile->input_buffer[channel]=datum;
  }
  if ((int)fwrite(mfxfile->input_buffer, sizeof(int16_t), mfxfile->fileheader.total_chans, 
	    mfxfile->mfxstream)!=mfxfile->fileheader.total_chans) {
   return mfx_lasterr=MFX_WRITEERR4;
  }
  mfxfile->mfxpos++;
  if (mfxfile->mfxpos>mfxfile->mfxlen) mfxfile->mfxlen=mfxfile->mfxpos;
  /* This seems to be necessary to resyncronize the read and write pointers: */
  ftell(mfxfile->mfxstream);
 }
 return mfx_lasterr=MFX_NOERR;
}
/*}}}  */

/*{{{  mfx_tell(MFX_FILE* mfxfile)*/
/*
 * mfx_tell returns the present data row number
 */

long 
mfx_tell(MFX_FILE* mfxfile) {
 mfx_lasterr=MFX_NOERR;
 return mfxfile->mfxpos;
}
/*}}}  */

/*{{{  mfx_seek(MFX_FILE* mfxfile, long offset, int whence)*/
/*
 * mfx_seek seeks to the requested data row (whence=0),
 * adjusts it starting from the present position (whence=1)
 * or from end of file (whence=2)
 */

mfx_errtype
mfx_seek(MFX_FILE* mfxfile, long offset, int whence) {
 long newoffset=0, newpos=0;
 if (mfxfile->was_just_created) mfx_flushheaders(mfxfile);
 switch (whence) {
  case SEEK_SET:
   newpos=offset;
   newoffset=mfxfile->data_offset;
   break;
  case SEEK_CUR:
   newpos=mfxfile->mfxpos+offset;
   break;
  case SEEK_END:
   newpos=mfxfile->mfxlen+offset;
   break;
 }
 if (newpos<0) return mfx_lasterr=MFX_NEGSEEK;
 if (newpos>mfxfile->mfxlen) return mfx_lasterr=MFX_PLUSSEEK;
 newoffset+=offset*mfxfile->fileheader.total_chans*sizeof(int16_t);
 if (fseek(mfxfile->mfxstream, newoffset, whence)!=0) {
  return mfx_lasterr=MFX_SEEKERROR;
 }
 mfxfile->mfxpos=(ftell(mfxfile->mfxstream)-mfxfile->data_offset)/
		  sizeof(int16_t)/mfxfile->fileheader.total_chans;
 if (mfxfile->mfxpos!=newpos) return mfx_lasterr=MFX_SEEKINCONSISTENT;
 return mfx_lasterr=MFX_NOERR;
}
/*}}}  */

/*{{{  mfx_eof(MFX_FILE* mfxfile)*/
/*
 * mfx_eof returns true if read past end of file was attempted
 */

int 
mfx_eof(MFX_FILE* mfxfile) {
 mfx_lasterr=MFX_NOERR;
 return feof(mfxfile->mfxstream);
}
/*}}}  */

/*{{{  mfx_getchannelname(MFX_FILE* mfxfile, int number)*/
/*
 * mfx_getchannelname returns a pointer to the name of the
 * number'th selected channel. number starts with 0.
 * Returns NULL if number is out of range.
 */

char *
mfx_getchannelname(MFX_FILE* mfxfile, int number) {
 char *retval=(char *)NULL;

 if (mfxfile->nr_of_channels_selected==0) {
  mfx_lasterr=MFX_NOCHANNEL;
 } else {
  mfx_lasterr=MFX_NOERR;
  if (number>=0 && number<mfxfile->nr_of_channels_selected) {
   retval=mfxfile->channelheaders[mfxfile->selected_channels[number]-1].channel_name;
  }
 }
 return retval;
}
/*}}}  */

/*
 * Now for higher-level functions:
 */

/*{{{  mfx_alloc(MFX_FILE* mfxfile, long count)*/
/*
 * mfx_alloc allocates enough memory to hold count
 * entries of the requested data type
 */

void* 
mfx_alloc(MFX_FILE* mfxfile, long count) {
 void *memstart;
 int elementsize=0;
 switch (mfxfile->datatype) {
  case MFX_SHORTS:
   elementsize=sizeof(int16_t);
   break;
  case MFX_FLOATS:
   elementsize=sizeof(float);
   break;
  case MFX_DOUBLES:
   elementsize=sizeof(double);
   break;
 }
 if ((memstart=malloc(count*elementsize*mfxfile->nr_of_channels_selected))==NULL) 
	mfx_lasterr=MFX_ALLOCERR;
 return memstart;
}
/*}}}  */

/*{{{  static int nexttrigcode(MFX_FILE* mfxfile, int channel)*/
/*
 * Internal: nexttrigcode
 * Returns triggercode of next datum or 0
 */
static int nexttrigcode(MFX_FILE* mfxfile, int channel) {
 int16_t datum;
 if ((int)fread(mfxfile->input_buffer, sizeof(datum), mfxfile->fileheader.total_chans, 
	   mfxfile->mfxstream)!=mfxfile->fileheader.total_chans) {
  mfx_lasterr=MFX_READERR4;
  return 0;
 }
 mfxfile->mfxpos++;
 datum=mfxfile->input_buffer[channel-1];
#ifdef LITTLE_ENDIAN
 Intel_int16((uint16_t *)&datum);
#endif
 /* The following is necessary for compatibility with the old style triggers */
 /* Be aware of round-off problems */
 datum=(int16_t)(DATACONV(mfxfile->channelheaders[channel-1], datum)+0.1);
 mfx_lasterr=MFX_NOERR;
 if (IS_TRIGGER(datum)==0) return 0;
 return TRIGGERCODE(datum);
}
/*}}}  */

/*{{{  mfx_seektrigger(MFX_FILE* mfxfile, char const * triggerchannel, int triggercode)*/
/*
 * mfx_seektrigger
 * Seeks to next trigger (i.e., either next trigger onset or, given NULL as
 * triggerchannel name, to next epoch start). If the current file position
 * is ON a trigger, it seeks the NEXT trigger.
 * If triggercode==0, it returns on ANY trigger; else, returns on the specified
 * trigger only.
 * Returns the trigger value found; 1 for fixed epochs; 0 on error.
 */

int
mfx_seektrigger(MFX_FILE* mfxfile, char const * triggerchannel, int triggercode) {
 int tchannel, trig;
 if (triggerchannel!=NULL) {	/* Select epochs by trigger */
  if ((tchannel=mfx_get_channel_number(mfxfile, triggerchannel))==0) {
   mfx_lasterr=MFX_WRONGCHANNELNAME;
   return 0;
  }
  /* Seek to first non-trigger */
  while (nexttrigcode(mfxfile, tchannel) && mfx_lasterr==MFX_NOERR);
  /* Now seek to next trigger onset+1 */
  while (((trig=nexttrigcode(mfxfile, tchannel))==0 || 
	  (triggercode!=0 && trig!=triggercode)) && 
	 mfx_lasterr==MFX_NOERR);
  /* Seek backwards to trigger onset */
  if (mfx_lasterr==MFX_NOERR) mfx_seek(mfxfile, -1, SEEK_CUR);
 } else {			/* Use fixed epoch lengths  */
  mfx_seek(mfxfile, (mfx_tell(mfxfile)/mfxfile->fileheader.pts_in_epoch+1)*
	mfxfile->fileheader.pts_in_epoch, SEEK_SET);
  trig=1;
 }
 if (mfx_lasterr==MFX_NOERR) 
  return trig;
 else
  return 0;
}
/*}}}  */

/*{{{  mfx_getepoch(MFX_FILE* mfxfile, long* epochlenp, long beforetrig, long aftertrig, char const * triggerchannel, int triggercode)*/
/*
 * mfx_getepoch
 * gets a buffer with the values of the next epoch, beforetrig values
 * before epoch start and aftertrig values after (if aftertrig==0, gets
 * the whole epoch up to next epoch-beforetrig; if no next epoch exists
 * in the file, up to EOF-beforetrig).
 * If the current file position is ON an trigger, gets epoch starting at
 * THIS trigger.
 * If triggercode==0 (and epochs are selected by trigger) then the epoch
 * around ANY next trigger will be loaded, analogous to the function of
 * mfx_seektrigger.
 * If the epochs are selected by trigger, no epochs will be selected that
 * overlap with a fixed recording epoch boundary.
 */

#define IS_THISTRIGGER(t) (triggercode==0 ? (t)!=0 : (t)==triggercode)

static long
epoch_number(MFX_FILE* mfxfile, long position) {
 return (position/mfxfile->fileheader.pts_in_epoch);
}

void*
mfx_getepoch(MFX_FILE* mfxfile, long* epochlenp, long beforetrig, 
	     long aftertrig, char const * triggerchannel, int triggercode) {
 void* memstart;
 long savepos, trigpos, startpos, epochlength;
 int tchannel;

 savepos=mfx_tell(mfxfile);
 if (triggerchannel!=NULL) {
  /*{{{  Select epochs by trigger*/
  int again;
  if ((tchannel=mfx_get_channel_number(mfxfile, triggerchannel))==0) {
   mfx_lasterr=MFX_WRONGCHANNELNAME;
   return NULL;
  }
  do {
   again=FALSE;
   if (IS_THISTRIGGER(nexttrigcode(mfxfile, tchannel))) {
    /* Already on trigger: seek back to trigger onset */
    while (mfx_seek(mfxfile, -2, SEEK_CUR)==MFX_NOERR && IS_THISTRIGGER(nexttrigcode(mfxfile, tchannel)));
   } else {
    if (mfx_seektrigger(mfxfile, triggerchannel, triggercode)==0) return NULL;
   }
   trigpos=mfx_tell(mfxfile);
   if (aftertrig==0) {
    mfx_seektrigger(mfxfile, triggerchannel, triggercode);
    epochlength=mfx_tell(mfxfile)-trigpos;
   } else epochlength=beforetrig+aftertrig;
   startpos=trigpos-beforetrig;

   if (epoch_number(mfxfile, startpos)!=epoch_number(mfxfile, startpos+epochlength-1)) {
    /* Try next trigger if this 'epoch' overlaps a fixed epoch boundary */
    /* First, go away from this trigger */
    while (IS_THISTRIGGER(nexttrigcode(mfxfile, tchannel)));
    again=TRUE;
   }
  } while(again);
  /*}}}  */
 } else {
  /*{{{  Use fixed epoch lengths*/
  epochlength=(aftertrig==0 ? mfxfile->fileheader.pts_in_epoch : beforetrig+aftertrig);
  startpos=((savepos-1+mfxfile->fileheader.pts_in_epoch)/mfxfile->fileheader.pts_in_epoch)*
            mfxfile->fileheader.pts_in_epoch-beforetrig;
  /*}}}  */
 }
 if (epochlength<1) {
  mfx_lasterr=MFX_EMPTYEPOCH;
  return NULL;
 }
 if ((memstart=mfx_alloc(mfxfile, epochlength))!=NULL) {
  if (mfx_seek(mfxfile, startpos, SEEK_SET)!=MFX_NOERR ||
      mfx_read(memstart, epochlength, mfxfile)!=MFX_NOERR) {
   free(memstart);
   return NULL;
  }
 }
 if (triggerchannel!=NULL) {
  /* If trigger codes used: allow for overlapping epochs by leaving the file
   * pointer directly behind the trigger code for this epoch
   */
  mfx_seek(mfxfile, trigpos+1, SEEK_SET);
  /* Seek to first non-trigger */
  while (nexttrigcode(mfxfile, tchannel) && mfx_lasterr==MFX_NOERR);
 }
 *epochlenp=epochlength;
 return memstart;
}
/*}}}  */
