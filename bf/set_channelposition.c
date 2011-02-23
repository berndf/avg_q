/*
 * Copyright (C) 1996-2003,2007-2010 Bernd Feige
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
 * set_channelposition method to set the channel positions for a number of
 * channels of the incoming epoch.		-- Bernd Feige 21.04.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_SETNAMES=0, 
 ARGS_ARGS,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Set channel names and positions rather than identify channels by names", "s", FALSE, NULL},
 {T_ARGS_TAKES_SENTENCE, "=builtin_set or @pos_filename or channelname1 posx posy posz [channelname2...]", "", ARGDESC_UNUSED, NULL}
};

#define INBUF_SIZE 80

LOCAL char *builtin_sets[]={
 "grid",
 "PSG",
 "EEG",
 NULL
};
enum BUILTIN_SET_ENUM {
 BUILTIN_NONE=0,
 BUILTIN_GRID,
 BUILTIN_PSG,
 BUILTIN_EEG,
};
struct positions_for_channelnames {
 char *channelname;
 double x;
 double y;
 double z;
};
LOCAL struct positions_for_channelnames positions_PSG[]={
 {"vEOG",	1.0,	5.0,	0.0},
 {"hEOG",	-1.0,	5.0,	0.0},
 {"hEOGl",	-2.0,	5.0,	0.0},
 {"hEOGr",	-1.0,	5.0,	0.0},
 {"Fp1",	-1.0,	4.0,	0.0},
 {"Fpz",	0.0,	4.0,	0.0},
 {"Fp2",	1.0,	4.0,	0.0},
 {"F7",	-2.0,	3.0,	0.0},
 {"F3",	-1.0,	3.0,	0.0},
 {"Fz",	0.0,	3.0,	0.0},
 {"F4",	1.0,	3.0,	0.0},
 {"F8",	2.0,	3.0,	0.0},
 {"T3",	-2.0,	2.0,	0.0},
 {"C3",	-1.0,	2.0,	0.0},
 {"Cz",	0.0,	2.0,	0.0},
 {"C4",	1.0,	2.0,	0.0},
 {"T4",	2.0,	2.0,	0.0},
 {"T5",	-2.0,	1.0,	0.0},
 {"P3",	-1.0,	1.0,	0.0},
 {"Pz",	0.0,	1.0,	0.0},
 {"P4",	1.0,	1.0,	0.0},
 {"T6",	2.0,	1.0,	0.0},
 {"O1",	-1.0,	0.0,	0.0},
 {"Oz",	0.0,	0.0,	0.0},
 {"O2",	1.0,	0.0,	0.0},
 {"A1",	-3.0,	2.0,	0.0},
 {"A2",	3.0,	2.0,	0.0},
 {"EKG",	 -2,-1.0,	0.0},
 {"POLY1",	-1.0,	-1.0,	0.0},
 {"POLY2",	-0.0,	-1.0,	0.0},
 {"K25",	-2.0,	-2.0,	0.0},
 {"K26",	-1.0,	-2.0,	0.0},
 {"K27",	0.0,	-2.0,	0.0},
 {"EMG",	1.0,	-2.0,	0.0},
 {"ATM",	2.0,	-2.0,	0.0},
 /* The NULL end entry tells where unknown channels are placed (i*x, y, z) */
 {NULL,	0.5,	-3.0,	0.0}
};
LOCAL struct positions_for_channelnames positions_EEG[]={
 {"Nas",	0,	2.4,	0.0},

 {"IO1",	-0.628435,	2.5,	0.0},
 {"IO2",	0.628435,	2.5,	0.0},
 {"Fp1",	-0.628435,	1.93413,	0.0},
 {"Fpz",	0,	1.93413,	0.0},
 {"Fp2",	0.628435,	1.93413,	0.0},
 {"AF7",	-1.2,	1.5,	0.0},
 {"AF3",	-0.628435,	1.5,	0.0},
 {"AFz",	0,	1.5,	0.0},
 {"AF4",	0.628435,	1.5,	0.0},
 {"AF8",	1.2,	1.5,	0.0},

 {"F9",	-2,	1.4,	0.0},
 {"F7",	-1.64527,	1.19536,	0.0},
 {"F5",	-1.2,	1,	0.0},
 {"F3",	-0.817511,	1.00954,	0.0},
 {"F1",	-0.5,	1.00954,	0.0},
 {"Fz",	0,	0.938984,	0.0},
 {"F2",	0.5,	1.00954,	0.0},
 {"F4",	0.817511,	1.00954,	0.0},
 {"F6",	1.2,	1,	0.0},
 {"F8",	1.64527,	1.19536,	0.0},
 {"F10",	2,	1.4,	0.0},

 {"T7",	-2.03366,	-0,	0.0},
 {"C5",	-1.5,	-0,	0.0},
 {"C3",	-0.938984,	-0,	0.0},
 {"C1",	-0.5,	0,	0.0},
 {"Cz",	0,	0,	0.0},
 {"C2",	0.5,	0,	0.0},
 {"C4",	0.938984,	0,	0.0},
 {"C6",	1.5,	0,	0.0},
 {"T8",	2.03366,	0,	0.0},

 /* Some use T3,T4 where we use T7,T8 */
 {"T3",	-2.03366,	-0,	0.0},
 {"T4",	2.03366,	0,	0.0},
 /* Some use T5,T6 where we use P7,P8 */
 {"T5",	-1.64527,	-1.19536,	0.0},
 {"T6",	1.64527,	-1.19536,	0.0},

 {"P9",	-2.03449,	-1.5,	0.0},
 {"P7",	-1.64527,	-1.19536,	0.0},
 {"P5",	-1.2,	-1.00954,	0.0},
 {"P3",	-0.817511,	-1.00954,	0.0},
 {"P1",	-0.4,	-1.00954,	0.0},
 {"P2",	0.4,	-1.00954,	0.0},
 {"Pz",	0,	-0.938984,	0.0},
 {"P4",	0.817511,	-1.00954,	0.0},
 {"P6",	1.2,	-1.00954,	0.0},
 {"P8",	1.64527,	-1.19536,	0.0},
 {"P10",	2.03449,	-1.5,	0.0},

 {"PO9",	-1.5,	-2,	0.0},
 {"PO7",	-1.2,	-1.4,	0.0},
 {"PO3",	-0.5,	-1.4,	0.0},
 {"POz",	0,	-1.4,	0.0},
 {"PO4",	0.5,	-1.4,	0.0},
 {"PO8",	1.2,	-1.4,	0.0},
 {"PO10",	1.5,	-2,	0.0},

 {"O9",	-0.8,	-2.2,	0.0},
 {"O1",	-0.628435,	-1.93413,	0.0},
 {"Oz",	0,	-1.93413,	0.0},
 {"O2",	0.628435,	-1.93413,	0.0},
 {"O10",	0.8,	-2.2,	0.0},

 {"Iz",	0,	-2.4,	0.0},

 {"FT9",	-2.3,	0.5,	0.0},
 {"FT7",	-2,	0.576335,	0.0},
 {"FC5",	-1.5014,	0.576335,	0.0},
 {"FC3",	-0.9,	0.431647,	0.0},
 {"FC1",	-0.431647,	0.431647,	0.0},
 {"FCz",	0.0,	0.431647,	0.0},
 {"FC2",	0.431647,	0.431647,	0.0},
 {"FC4",	0.9,	0.431647,	0.0},
 {"FC6",	1.5014,	0.576335,	0.0},
 {"FT8",	2,	0.576335,	0.0},
 {"FT10",	2.3,	0.5,	0.0},

 {"TP9",	-2.03449,	-0.740495,	0.0},
 {"TP7",	-1.75,	-0.6,	0.0},
 {"CP5",	-1.5014,	-0.576335,	0.0},
 {"CP3",	-1,	-0.5,	0.0},
 {"CP1",	-0.431647,	-0.431647,	0.0},
 {"CPz",	0,	-0.431647,	0.0},
 {"CP2",	0.431647,	-0.431647,	0.0},
 {"CP4",	1,	-0.5,	0.0},
 {"CP6",	1.5014,	-0.576335,	0.0},
 {"TP8",	1.75,	-0.6,	0.0},
 {"TP10",	2.03449,	-0.740495,	0.0},
 {"VeogO",	1.237002,	2.36252,	0.0},
 {"VeogU",	0.618501,	2.36252,	0.0},
 {"Eog",	0.618501,	2.36252,	0.0},
 {"EOGL",	-0.618501,	2.36252,	0.0},
 {"EOGR",	0.618501,	2.36252,	0.0},
 {"Ekg1",	-1.78836,	2.34242,	0.0},
 {"Ekg2",	-1.77885,	1.94955,	0.0},
 {"Ekg",	-1.78836,	2.2,	0.0},
 {"ECG",	-1.78836,	2.2,	0.0},
 {"BEMG1",	2.0,	2.34242,	0.0},
 {"BEMG2",	2.0,	1.94955,	0.0},
 {"EDA",	0.0,	0.0,	0.0},
 /* The NULL end entry tells where unknown channels are placed (i*x, y, z) */
 {NULL,	0.5,	-3.0,	0.0}
};
LOCAL void
set_positions_from_struct(transform_info_ptr tinfo, struct positions_for_channelnames *positions) {
 int channel;
 int unknown_channel=0;

 /*{{{  If no probepos space in epoch, allocate one*/
 if (tinfo->probepos==NULL) {
  if ((tinfo->probepos=(double *)calloc(3*tinfo->nr_of_channels, sizeof(double)))==NULL) {
   ERREXIT(tinfo->emethods, "set_channelposition: Error allocating channel position memory\n");
  }
 }
 /*}}}  */

 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  struct positions_for_channelnames *in_struct=positions;
  for (in_struct=positions; in_struct->channelname!=NULL; in_struct++) {
   if (strcasecmp(in_struct->channelname, tinfo->channelnames[channel])==0) {
    tinfo->probepos[3*channel  ]=in_struct->x;
    tinfo->probepos[3*channel+1]=in_struct->y;
    tinfo->probepos[3*channel+2]=in_struct->z;
    break;
   }
  }
  if (in_struct->channelname==NULL) {
   /* Unknown name: Place outside regular region so that this can be seen */
   tinfo->probepos[3*channel  ]=unknown_channel*in_struct->x;
   tinfo->probepos[3*channel+1]=in_struct->y;
   tinfo->probepos[3*channel+2]=in_struct->z;
   unknown_channel++;
  }
 }
}

typedef struct {
 int nr_of_channels;
 char **channelnames;
 double *positions;
 int wholelen;	/* The number of bytes needed for the string section */
 enum BUILTIN_SET_ENUM use_builtin_set;
} set_channelposition_selection;

/*{{{  set_channelposition_init(transform_info_ptr tinfo) {*/
METHODDEF void
set_channelposition_init(transform_info_ptr tinfo) {
 set_channelposition_selection *selection=(set_channelposition_selection *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char *innames;
 Bool havearg;
 int string_space=0, channel;
 growing_buf buf;
 growing_buf_init(&buf);
 growing_buf_takethis(&buf, args[ARGS_ARGS].arg.s);
 buf.delim_protector='\\';

 havearg=growing_buf_firsttoken(&buf);
 if (!havearg) {
  ERREXIT(tinfo->emethods, "set_channelposition_init: No arguments found!\n");
 }

 selection->use_builtin_set=BUILTIN_NONE;
 if (*buf.current_token=='=') {
  /*{{{  Use builtin set*/
  char **in_builtin_sets;
  if (args[ARGS_SETNAMES].is_set) {
   ERREXIT(tinfo->emethods, "set_channelposition_init: -s cannot be used with builtin sets.\n");
  }
  for (in_builtin_sets=builtin_sets; *in_builtin_sets!=NULL; in_builtin_sets++) {
   if (strcmp(buf.current_token+1, *in_builtin_sets)==0) {
    selection->use_builtin_set=(enum BUILTIN_SET_ENUM)(in_builtin_sets-builtin_sets)+1;
   }
  }
  if (selection->use_builtin_set==BUILTIN_NONE) {
   ERREXIT1(tinfo->emethods, "set_channelposition_init: Unknown builtin set >%s<\n",MSGPARM(buf.current_token+1));
  }
  /*}}}  */
 } else if (*buf.current_token=='@') {
  /*{{{  Read channels and positions from ascii file*/
  char inbuf[INBUF_SIZE], *ininbuf;
  int nr_of_channels=0, i;
  FILE *pos_file=fopen(buf.current_token+1, "r");

  if (pos_file==NULL) {
   ERREXIT1(tinfo->emethods, "set_channelposition_init: Can't open position file >%s<\n", MSGPARM(buf.current_token+1));
  }

  /*{{{  Count lines and neccessary string space*/
  while (fgets(inbuf, INBUF_SIZE, pos_file)!=NULL) {
   nr_of_channels++;
   for (ininbuf=inbuf; isspace(*ininbuf); ininbuf++);
   for (; !isspace(*ininbuf); ininbuf++,string_space++);
   string_space++;
  }
  /*}}}  */

  if ((selection->channelnames=(char **)malloc(nr_of_channels*sizeof(char *)))==NULL
    ||(innames=(char *)malloc(string_space))==NULL
    ||(selection->positions=(double *)malloc(3*nr_of_channels*sizeof(double)))==NULL) {
   ERREXIT(tinfo->emethods, "set_channelposition_init: Can't malloc data\n");
  }
  fseek(pos_file, 0, 0);
  channel=0;
  while (fgets(inbuf, INBUF_SIZE, pos_file)!=NULL) {
   for (ininbuf=inbuf; isspace(*ininbuf); ininbuf++);
   selection->channelnames[channel]=innames;
   while (!isspace(*ininbuf)) *innames++ = *ininbuf++;
   *innames++='\0';
   for (i=0; i<3; i++) {
    while (isspace(*ininbuf)) ininbuf++;
    selection->positions[3*channel+i]=atof(ininbuf);
    while (!isspace(*ininbuf)) ininbuf++;
   }
   channel++;
  }
  selection->nr_of_channels=nr_of_channels;
  fclose(pos_file);
  /*}}}  */
 } else {
  /*{{{  Determine nr_of_channels*/
  if (buf.nr_of_tokens%4!=0) {
   ERREXIT(tinfo->emethods, "set_channelposition_init: Not 3 positions for each channel\n");
  }
  selection->nr_of_channels=buf.nr_of_tokens/4;
  if (selection->nr_of_channels==0) {
   ERREXIT(tinfo->emethods, "set_channelposition_init: Need at least 4 arguments.\n");
  }
  /*}}}  */

  /*{{{  Determine neccessary string space*/
  for (channel=0; havearg; channel++) {
   string_space+=strlen(buf.current_token)+1;
   growing_buf_nexttoken(&buf);
   growing_buf_nexttoken(&buf);
   growing_buf_nexttoken(&buf);
   havearg=growing_buf_nexttoken(&buf);
  }
  /*}}}  */

  /*{{{  Allocate memory*/
  if ((selection->channelnames=(char **)malloc(selection->nr_of_channels*sizeof(char *)))==NULL
    ||(innames=(char *)malloc(string_space))==NULL
    ||(selection->positions=(double *)malloc(3*selection->nr_of_channels*sizeof(double)))==NULL) {
   ERREXIT(tinfo->emethods, "set_channelposition_init: Can't malloc selection buffers\n");
  }
  /*}}}  */

  /*{{{  Note the names and positions*/
  havearg=growing_buf_firsttoken(&buf);
  for (channel=0; havearg; channel++) {
   strcpy(innames, buf.current_token);
   selection->channelnames[channel]=innames;
   innames+=strlen(innames)+1;
   growing_buf_nexttoken(&buf);
   selection->positions[3*channel  ]=atof(buf.current_token);
   growing_buf_nexttoken(&buf);
   selection->positions[3*channel+1]=atof(buf.current_token);
   growing_buf_nexttoken(&buf);
   selection->positions[3*channel+2]=atof(buf.current_token);
   havearg=growing_buf_nexttoken(&buf);
  }
  /*}}}  */
 }
 selection->wholelen=string_space;
 growing_buf_free(&buf);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  set_channelposition(transform_info_ptr tinfo) {*/
LOCAL void
set_1channelposition(transform_info_ptr tinfo, int channel, int whichselection) {
 set_channelposition_selection *selection=(set_channelposition_selection *)tinfo->methods->local_storage;
 /* NOTE: channel starts with 0 here! */
 /*{{{  If no probepos space in epoch, allocate one*/
 if (tinfo->probepos==NULL) {
  if ((tinfo->probepos=(double *)calloc(3*tinfo->nr_of_channels, sizeof(double)))==NULL) {
   ERREXIT(tinfo->emethods, "set_channelposition: Error allocating channel position memory\n");
  }
 }
 /*}}}  */
 memcpy(tinfo->probepos+3*channel, selection->positions+3*whichselection, 3*sizeof(double));
}

METHODDEF DATATYPE *
set_channelposition(transform_info_ptr tinfo) {
 set_channelposition_selection *selection=(set_channelposition_selection *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int i;

 char *in_channelnames=NULL;
 if (selection->use_builtin_set!=0) {
  switch (selection->use_builtin_set) {
   case BUILTIN_GRID:
    free_pointer((void **)&tinfo->probepos);
    create_channelgrid(tinfo);
    break;
   case BUILTIN_PSG:
    set_positions_from_struct(tinfo, positions_PSG);
    break;
   case BUILTIN_EEG:
    set_positions_from_struct(tinfo, positions_EEG);
    break;
   default:
    break;
  }
 } else
 if (args[ARGS_SETNAMES].is_set) {
  char * const old_channelspace=tinfo->channelnames[0];
  long space_needed=selection->wholelen;
  /* Add how many bytes will be transferred from the old names */
  for (i=selection->nr_of_channels; i<tinfo->nr_of_channels; i++) {
   space_needed+=strlen(tinfo->channelnames[i])+1;
  }
  if ((in_channelnames=(char *)malloc(space_needed))==NULL) {
   ERREXIT(tinfo->emethods, "set_channelposition: Can't malloc string section\n");
  }
  for (i=0; i<selection->nr_of_channels && i<tinfo->nr_of_channels; i++) {
   strcpy(in_channelnames, selection->channelnames[i]);
   tinfo->channelnames[i]=in_channelnames;
   in_channelnames+=strlen(in_channelnames)+1;
  }
  /* Copy the rest of the strings if not all channels receive new names */
  for (i=selection->nr_of_channels; i<tinfo->nr_of_channels; i++) {
   strcpy(in_channelnames, tinfo->channelnames[i]);
   tinfo->channelnames[i]=in_channelnames;
   in_channelnames+=strlen(in_channelnames)+1;
  }
  free(old_channelspace);
 }
 for (i=0; i<selection->nr_of_channels; i++) {
  if (args[ARGS_SETNAMES].is_set) {
   /* Protect against overflow */
   if (i>=tinfo->nr_of_channels) break;
   set_1channelposition(tinfo, i, i);
  } else {
   char * const channel_name=selection->channelnames[i];
   int channel=0;
   do {
    if (strcmp(tinfo->channelnames[channel], channel_name)==0) {
     set_1channelposition(tinfo, channel, i);
    }
    channel++;
   } while (channel<tinfo->nr_of_channels);
  }
 }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  set_channelposition_exit(transform_info_ptr tinfo) {*/
METHODDEF void
set_channelposition_exit(transform_info_ptr tinfo) {
 set_channelposition_selection *selection=(set_channelposition_selection *)tinfo->methods->local_storage;

 if (selection->channelnames!=NULL) free_pointer((void **)&selection->channelnames[0]);
 free_pointer((void **)&selection->positions);
 free_pointer((void **)&selection->channelnames);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_set_channelposition(transform_info_ptr tinfo) {*/
GLOBAL void
select_set_channelposition(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &set_channelposition_init;
 tinfo->methods->transform= &set_channelposition;
 tinfo->methods->transform_exit= &set_channelposition_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="set_channelposition";
 tinfo->methods->method_description=
  "Transform method to set the position for a number of channels.\n"
  "Currently known builtin sets:\n"
  "grid, "
  "PSG, "
  "EEG";
 tinfo->methods->local_storage_size=sizeof(set_channelposition_selection);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
