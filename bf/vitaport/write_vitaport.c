/*
 * Copyright (C) 1996-2001,2003,2005-2008 Bernd Feige
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
 * write_vitaport.c module to write data to a vitaport file
 *  This put_epoch method first collects the channels in separate temp files
 *  and copies the channels together in the write_vitaport_exit, because the 
 *  Vitaport disk format is `points fastest'. The temp files '00000000.tmp', 
 *  '00000001.tmp' ... are created in the same directory as the output file 
 *  and are automatically removed.
 *  Extra `magic' goes on in finding the appropriate `mulfac' and `divfac' 
 *  values for the scaling. (Any idea why they didn't just write a single 
 *  float?)
 *	-- Bernd Feige 20.12.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <read_struct.h>
#include <Intel_compat.h>
#include "transform.h"
#include "bf.h"
#include "vitaport.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_OFILE=0,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, NULL}
};

/* This value defines the log optimal scale for the scaling shorts mulfac and divfac */
#define TARGET_LOG_SHORTVAL (log(SHRT_MAX))

/*{{{  struct write_vitaport_storage {*/
struct write_vitaport_storage {
 struct vitaport_fileheader fileheader;
 struct vitaportII_fileheader fileheaderII;
 struct vitaportII_fileext fileext;
 struct vitaport_channelheader *channelheaders;
 struct vitaportIIrchannelheader *channelheadersII;
 FILE *outfile;
 char *channelfilename;
 FILE *channelfile;
 DATATYPE *channelmax;
 DATATYPE *channelmin;
 DATATYPE sfreq;
 unsigned long total_points;
 growing_buf triggers;
};
/*}}}  */

/*{{{  write_vitaport_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_vitaport_init(transform_info_ptr tinfo) {
 struct write_vitaport_storage *local_arg=(struct write_vitaport_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel, NoOfChannels=tinfo->nr_of_channels;
 int tempnamelength, tempnum;
 short dd, mm, yy, yyyy, hh, mi, ss;
 char const *lastslash;
 struct stat statbuff;

 growing_buf_init(&local_arg->triggers);

 if ((local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, "wb"))==NULL) {
  ERREXIT1(tinfo->emethods, "write_vitaport_init: Can't open %s\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }
 
 /*{{{  Initialize the file header*/
 local_arg->fileheader.Sync=VITAPORT_SYNCMAGIC;
 local_arg->fileheader.chnoffs=sm_vitaport_fileheader[0].offset+sm_vitaportII_fileheader[0].offset+sm_vitaportII_fileext[0].offset;
 local_arg->fileext.chdlen=sm_vitaport_channelheader[0].offset+sm_vitaportIIrchannelheader[0].offset;
 local_arg->fileheader.hdlen=local_arg->fileheader.chnoffs+NoOfChannels*local_arg->fileext.chdlen+2;
 local_arg->fileheader.hdtyp=HDTYP_VITAPORT2;
 local_arg->fileheader.knum=NoOfChannels;
 if (comment2time(tinfo->comment, &dd, &mm, &yy, &yyyy, &hh, &mi, &ss)) {
  local_arg->fileheaderII.Hour=hh;
  local_arg->fileheaderII.Minute=mi;
  local_arg->fileheaderII.Second=ss;
  local_arg->fileheaderII.Day=dd;
  local_arg->fileheaderII.Month=mm;
  local_arg->fileheaderII.Year=yy;
  convert_tohex(&local_arg->fileheaderII.Hour);
  convert_tohex(&local_arg->fileheaderII.Minute);
  convert_tohex(&local_arg->fileheaderII.Second);
  convert_tohex(&local_arg->fileheaderII.Day);
  convert_tohex(&local_arg->fileheaderII.Month);
  convert_tohex(&local_arg->fileheaderII.Year);
 }
 local_arg->fileheaderII.Sysvers=56;
 local_arg->fileheaderII.scnrate=(short int)rint(VITAPORT_CLOCKRATE/tinfo->sfreq);
 local_arg->fileext.swvers=311;
 local_arg->fileext.hwvers=1286;
 local_arg->fileext.recorder= -1;
 local_arg->fileext.timer1=0;
 local_arg->fileext.sysflg12=512;
 /*}}}  */
 
 /*{{{  Initialize channel temp files and storage*/
 lastslash=strrchr(args[ARGS_OFILE].arg.s, PATHSEP);
 if (lastslash==NULL) lastslash=args[ARGS_OFILE].arg.s-1;
 /* Up to slash, 8.3 for name, 1 for null */
 tempnamelength=lastslash-args[ARGS_OFILE].arg.s+(1+8+1+3+1);
 if ((local_arg->channelfilename=(char *)malloc(tempnamelength))==NULL
  || (local_arg->channelmax=(DATATYPE *)malloc(NoOfChannels*sizeof(DATATYPE)))==NULL
  || (local_arg->channelmin=(DATATYPE *)malloc(NoOfChannels*sizeof(DATATYPE)))==NULL
  || (local_arg->channelheaders=(struct vitaport_channelheader *)malloc(NoOfChannels*sizeof(struct vitaport_channelheader)))==NULL
  || (local_arg->channelheadersII=(struct vitaportIIrchannelheader *)malloc(NoOfChannels*sizeof(struct vitaportIIrchannelheader)))==NULL) {
  ERREXIT(tinfo->emethods, "write_vitaport_init: Error allocating memory\n");
 }
 tempnum=0;
 do { /* While the targeted tempfile already exists... */
  strncpy(local_arg->channelfilename, args[ARGS_OFILE].arg.s, lastslash-args[ARGS_OFILE].arg.s+1);
  local_arg->channelfilename[lastslash-args[ARGS_OFILE].arg.s+1]='\0';
  sprintf(local_arg->channelfilename+strlen(local_arg->channelfilename), "%08d.tmp", tempnum);
  tempnum++;
 } while (stat(local_arg->channelfilename, &statbuff)==0);
 if ((local_arg->channelfile=fopen(local_arg->channelfilename, "wb"))==NULL) {
  ERREXIT1(tinfo->emethods, "write_vitaport_init: Can't open temp file %s\n", MSGPARM(local_arg->channelfilename));
 }
 for (channel=0; channel<NoOfChannels; channel++) {
  local_arg->channelmax[channel]= -FLT_MAX;
  local_arg->channelmin[channel]=  FLT_MAX;

  strncpy(local_arg->channelheaders[channel].kname, tinfo->channelnames[channel], 6);
  strcpy(local_arg->channelheaders[channel].kunit, "uV");
  local_arg->channelheaders[channel].datype=VP_DATYPE_SIGNED;
  local_arg->channelheaders[channel].dasize=VP_DATATYPE_SHORT;
  local_arg->channelheaders[channel].scanfc=1;
  local_arg->channelheaders[channel].fltmsk=0;
  local_arg->channelheaders[channel].stofac=1;
  local_arg->channelheaders[channel].resvd1=0;
  local_arg->channelheaders[channel].resvd2=0;
  local_arg->channelheadersII[channel].mval=1000;
  local_arg->channelheadersII[channel].chflg=1;
  local_arg->channelheadersII[channel].hpass=0;
  local_arg->channelheadersII[channel].ampl=1;
  local_arg->channelheadersII[channel].tpass=0;
 }
 /*}}}  */

 local_arg->total_points=0;
 local_arg->sfreq=tinfo->sfreq;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_vitaport(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_vitaport(transform_info_ptr tinfo) {
 struct write_vitaport_storage *local_arg=(struct write_vitaport_storage *)tinfo->methods->local_storage;
 array myarray;
 int channel;

 /*{{{  Assert that epoch size didn't change & itemsize==1*/
 if (tinfo->itemsize!=1) {
  ERREXIT(tinfo->emethods, "write_vitaport: Only itemsize=1 is supported.\n");
 }
 if (local_arg->fileheader.knum!=tinfo->nr_of_channels) {
  ERREXIT2(tinfo->emethods, "write_vitaport: nr_of_channels was %d, now %d\n", MSGPARM(local_arg->fileheader.knum), MSGPARM(tinfo->nr_of_channels));
 }
 /*}}}  */

 if (tinfo->data_type==FREQ_DATA) tinfo->nr_of_points=tinfo->nroffreq;
 
 tinfo_array(tinfo, &myarray);
 array_transpose(&myarray); /* channels are the elements: Temp file is interlaced */
 do {
  channel=0;
  do {
   DATATYPE dat=array_scan(&myarray);
   if (fwrite(&dat, sizeof(DATATYPE), 1, local_arg->channelfile)!=1) {
    ERREXIT(tinfo->emethods, "write_vitaport: Error writing temp file.\n");
   }
   /* Keep track of the scaling... */
   if (dat>local_arg->channelmax[channel]) local_arg->channelmax[channel]=dat;
   if (dat<local_arg->channelmin[channel]) local_arg->channelmin[channel]=dat;
   channel++;
  } while (myarray.message==ARRAY_CONTINUE);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
  while (intrig->code!=0) {
   push_trigger(&local_arg->triggers,local_arg->total_points+intrig->position,intrig->code,intrig->description);
   intrig++;
  }
 }

 local_arg->total_points+=tinfo->nr_of_points;

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_vitaport_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_vitaport_exit(transform_info_ptr tinfo) {
 struct write_vitaport_storage *local_arg=(struct write_vitaport_storage *)tinfo->methods->local_storage;
 int channel, NoOfChannels=local_arg->fileheader.knum;
 long point;
 long const channellen=local_arg->total_points*sizeof(short); 
 long const hdlen=local_arg->fileheader.hdlen;
 short checksum=0;

 local_arg->fileheaderII.dlen=hdlen+NoOfChannels*channellen; 
 /*{{{  Write the file headers*/
# ifdef LITTLE_ENDIAN
 change_byteorder((char *)&local_arg->fileheader, sm_vitaport_fileheader);
 change_byteorder((char *)&local_arg->fileheaderII, sm_vitaportII_fileheader);
 change_byteorder((char *)&local_arg->fileext, sm_vitaportII_fileext);
# endif
 write_struct((char *)&local_arg->fileheader, sm_vitaport_fileheader, local_arg->outfile);
 write_struct((char *)&local_arg->fileheaderII, sm_vitaportII_fileheader, local_arg->outfile);
 write_struct((char *)&local_arg->fileext, sm_vitaportII_fileext, local_arg->outfile);
 /*}}}  */

 for (channel=0; channel<NoOfChannels; channel++) {
  /*{{{  Write a channel header*/
  /* This is the factor as we'd like to have it... */
  float factor=((float)(local_arg->channelmax[channel]> -local_arg->channelmin[channel] ? local_arg->channelmax[channel] : -local_arg->channelmin[channel]))/SHRT_MAX;

  if (strncmp(local_arg->channelheaders[channel].kname, VP_MARKERCHANNEL_NAME, 6)==0) {
   strcpy(local_arg->channelheaders[channel].kunit, "mrk");
   local_arg->channelheaders[channel].datype=VP_DATYPE_MARKER;
   /* Since the marker channel is read without the conversion factors,
    * set the conversion to 1.0 for this channel */
   local_arg->channelheaders[channel].mulfac=local_arg->channelheaders[channel].divfac=1;
   local_arg->channelheaders[channel].offset=0;
  } else {
  float const logdiff=log(factor)-TARGET_LOG_SHORTVAL;

  local_arg->channelheaders[channel].offset=0;
  if (logdiff<0) {
   /*{{{  Factor is small: Better to fix divfac and calculate mulfac after that*/
   double const divfac=exp(-logdiff);
   if (divfac>SHRT_MAX) {
    local_arg->channelheaders[channel].divfac=SHRT_MAX;
   } else {
    local_arg->channelheaders[channel].divfac=(unsigned short)divfac;
   }
   /* The +1 takes care that we never get below the `ideal' factor computed above. */
   local_arg->channelheaders[channel].mulfac=(unsigned short)(factor*local_arg->channelheaders[channel].divfac+1);
   if (local_arg->channelheaders[channel].mulfac==0) {
    local_arg->channelheaders[channel].mulfac=1;
    local_arg->channelheaders[channel].divfac=(unsigned short)(1.0/factor-1);
   }
   /*}}}  */
  } else {
   /*{{{  Factor is large: Better to fix mulfac and calculate divfac after that*/
   double const mulfac=exp(logdiff);
   if (mulfac>SHRT_MAX) {
    local_arg->channelheaders[channel].mulfac=SHRT_MAX;
   } else {
    local_arg->channelheaders[channel].mulfac=(unsigned short)mulfac;
   }
   /* The -1 takes care that we never get below the `ideal' factor computed above. */
   local_arg->channelheaders[channel].divfac=(unsigned short)(local_arg->channelheaders[channel].mulfac/factor-1);
   if (local_arg->channelheaders[channel].divfac==0) {
    local_arg->channelheaders[channel].mulfac=(unsigned short)(factor+1);
    local_arg->channelheaders[channel].divfac=1;
   }
   /*}}}  */
  }
  }
  /* Now see which factor we actually got constructed */
  factor=((float)local_arg->channelheaders[channel].mulfac)/local_arg->channelheaders[channel].divfac;

  local_arg->channelheadersII[channel].doffs=channel*channellen;
  local_arg->channelheadersII[channel].dlen=channellen;
#  ifdef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->channelheaders[channel], sm_vitaport_channelheader);
  change_byteorder((char *)&local_arg->channelheadersII[channel], sm_vitaportIIrchannelheader);
#  endif
  write_struct((char *)&local_arg->channelheaders[channel], sm_vitaport_channelheader, local_arg->outfile);
  write_struct((char *)&local_arg->channelheadersII[channel], sm_vitaportIIrchannelheader, local_arg->outfile);
#  ifdef LITTLE_ENDIAN
  change_byteorder((char *)&local_arg->channelheaders[channel], sm_vitaport_channelheader);
  change_byteorder((char *)&local_arg->channelheadersII[channel], sm_vitaportIIrchannelheader);
#  endif
  /*}}}  */
 }
 fwrite(&checksum, sizeof(short), 1, local_arg->outfile);

 TRACEMS(tinfo->emethods, 1, "write_vitaport_exit: Copying channels to output file...\n");

 /* We close and re-open the channel file because buffering is much more
  * efficient at least with DJGPP if the file is either only read or written */
 fclose(local_arg->channelfile);
 if ((local_arg->channelfile=fopen(local_arg->channelfilename, "rb"))==NULL) {
  ERREXIT1(tinfo->emethods, "write_vitaport_exit: Can't open temp file %s\n", MSGPARM(local_arg->channelfilename));
 }

 for (channel=0; channel<NoOfChannels; channel++) {
  /*{{{  Copy a channel to the target file */
  float const factor=((float)local_arg->channelheaders[channel].mulfac)/local_arg->channelheaders[channel].divfac;
  unsigned short const offset=local_arg->channelheaders[channel].offset;

  for (point=0; point<local_arg->total_points; point++) {
   DATATYPE dat;
   short s;
   fseek(local_arg->channelfile, (point*NoOfChannels+channel)*sizeof(DATATYPE), SEEK_SET);
   if (fread(&dat, sizeof(DATATYPE), 1, local_arg->channelfile)!=1) {
    ERREXIT(tinfo->emethods, "write_vitaport_exit: Error reading temp file.\n");
   }
   s= (short int)rint(dat/factor)-offset;
#   ifdef LITTLE_ENDIAN
   Intel_short((unsigned short *)&s);
#   endif
   if (fwrite(&s, sizeof(short), 1, local_arg->outfile)!=1) {
    ERREXIT(tinfo->emethods, "write_vitaport_exit: Write error.\n");
   }
  }
  /*}}}  */
 }
 fclose(local_arg->channelfile);
 unlink(local_arg->channelfilename);

 if (local_arg->triggers.current_length!=0) {
  long tagno, length, n_events, trigpoint;
  int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);
  struct vitaport_idiotic_multiplemarkertablenames *markertable_entry;

  /* Try to keep a security space of at least 20 free events */
  for (markertable_entry=markertable_names; markertable_entry->markertable_name!=NULL && markertable_entry->idiotically_fixed_length<nevents+20; markertable_entry++);
  if (markertable_entry->markertable_name==NULL) {
   if ((markertable_entry-1)->idiotically_fixed_length<=nevents) {
    /* Drop the requirement for at least 20 free events */
    markertable_entry--;
   } else {
    /* No use... */
    ERREXIT1(tinfo->emethods, "write_vitaport_exit: %d events wouldn't fit into the fixed-length VITAGRAPH marker tables!\nBlame the Vitaport people!\n", MSGPARM(nevents));
   }
  }
  n_events=markertable_entry->idiotically_fixed_length;

  fwrite(markertable_entry->markertable_name, 1, VP_TABLEVAR_LENGTH, local_arg->outfile);
  length=n_events*sizeof(long);
#ifdef LITTLE_ENDIAN
  Intel_long((unsigned long *)&length);
#endif
  fwrite(&length, sizeof(long), 1, local_arg->outfile);
  for (tagno=0; tagno<n_events; tagno++) {
   if (tagno<nevents) {
    struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+tagno;
    /* Trigger positions are given in ms units */
    trigpoint=(long)rint(intrig->position*1000.0/local_arg->sfreq);
    /* Make trigpoint uneven if code%2==1 */
    trigpoint=trigpoint-trigpoint%2+(intrig->code-1)%2;
   } else {
    trigpoint= -1;
   }
#ifdef LITTLE_ENDIAN
   Intel_long((unsigned long *)&trigpoint);
#endif
   fwrite(&trigpoint, sizeof(long), 1, local_arg->outfile);
  }
  for (; tagno<n_events; tagno++) {
   trigpoint= -1;
#ifdef LITTLE_ENDIAN
   Intel_long((unsigned long *)&trigpoint);
#endif
   fwrite(&trigpoint, sizeof(long), 1, local_arg->outfile);
  }
 }

 fclose(local_arg->outfile);

 /*{{{  Free memory*/
 free_pointer((void **)&local_arg->channelfilename);
 free_pointer((void **)&local_arg->channelmax);
 free_pointer((void **)&local_arg->channelmin);
 free_pointer((void **)&local_arg->channelheaders);
 free_pointer((void **)&local_arg->channelheadersII);
 growing_buf_free(&local_arg->triggers);
 /*}}}  */

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_vitaport(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_vitaport(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_vitaport_init;
 tinfo->methods->transform= &write_vitaport;
 tinfo->methods->transform_exit= &write_vitaport_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_vitaport";
 tinfo->methods->method_description=
  "Put-epoch method to write data to a binary `vitaport' file\n";
 tinfo->methods->local_storage_size=sizeof(struct write_vitaport_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
