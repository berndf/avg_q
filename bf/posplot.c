/*
 * Copyright (C) 1996-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * posplot.c module to plot transform data at probe positions
 *	-- Bernd Feige 20.01.1993
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifdef __linux__
/* Needed with glibc 2.1 to have NAN defined (from math.h) */
#define __USE_ISOC9X 1
/* Needed with glibc 2.2 to have NAN defined (sigh...) */
#define __USE_ISOC99 1
#endif
#include <math.h>
#include <float.h>
#include <array.h>
#include <biomag.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"

#ifdef INCLUDE_DIPOLE_FIT
struct dip_fit_storage {
 long pointno;
 struct do_leastsquares_args fixedargs;
};
#endif

#ifdef SGI
#include "gl.h"
#include "device.h"
#else
#define originx 0
#define originy 0
typedef short Int16;
typedef float Float32;
#include "vogl.h"
#include "vodevice.h"
#endif
/*}}}  */

/*{{{  Arguments*/
enum ARGS_ENUM {
 ARGS_QUIT=0,
 ARGS_RUN,
 ARGS_RUNFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Quit after plotting the epoch - never go interactive", "q", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Run the event replay file `posplot.rec'", "R", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "Run the named event replay file", "r", ARGDESC_UNUSED, NULL}
};
/*}}}  */

/*{{{  #defines*/
#define CHAR_WIDTH  char_width
#define CHAR_HEIGHT char_height
#define DATASET_MAP (CHAR_WIDTH*5)
#define LINESTYLE_NORMAL 0
#define LINESTYLE_DASHED 1
#define LINESTYLECOUNT 4
/* The line styles are:
 * 1__  __  __  __  2____    ____    3___  _  ___  _  4_______   ______
 */
LOCAL Linestyle linepatterns[LINESTYLECOUNT]=
	{ 0xcccc, 0xf0f0, 0xe4e4, 0xfe3f};

/* Background color */
LOCAL Colorindex BACKGROUND=BLACK;
LOCAL Colorindex FOREGROUND=WHITE;
LOCAL Colorindex COORDSYSTEM=YELLOW;
#define LINECOLOR_NORMAL 0
#define LINECOLORCOUNT 7
#define FGLINECOLORINDEX 6
LOCAL Colorindex linecolors[LINECOLORCOUNT]=
	{ GREEN, RED, YELLOW, BLUE, MAGENTA, CYAN, WHITE };
LOCAL void background_black(void) {
 BACKGROUND=BLACK;
 FOREGROUND=WHITE;
 COORDSYSTEM=YELLOW;
 linecolors[FGLINECOLORINDEX]=WHITE;
}
LOCAL void background_white(void) {
 BACKGROUND=WHITE;
 FOREGROUND=BLACK;
 COORDSYSTEM=BLACK;
 linecolors[FGLINECOLORINDEX]=BLACK;
}
/* The NEWBORDER 'device' to signal a reinitialization including viewing distance */
#define NEWBORDER 999
/* The NEWDATA 'device' to signal that a plotting boundary was changed */
#define NEWDATA 998

typedef enum tsdata_functions {
 TS_RAW=0, TS_EXP, TS_LOG, TS_EXP10, TS_LOG10, TS_EXPDB, TS_LOGDB, TS_POWER, TS_ABS, TS_PHASE
} tsdata_function;
LOCAL const char *tsfunction_names[]={
 "Raw", "Exp", "Log", "Exp10", "Log10", "Exp_dB", "Log_dB", "Power", "Abs", "Phase"
};
enum position_modes {
 /* This first mode is 'unusual' in that it allows changing the position 
  * using the uppercase rotation keys... */
 POSITION_MODE_CXYZ= -1,
 POSITION_MODE_XYZ,
 POSITION_MODE_VERTICAL,
 POSITION_MODE_OVERLAY,
 NR_OF_POSITION_MODES
};
LOCAL const char *position_mode_names[]={
 "MODIFY",
 "channel",
 "vertical",
 "overlay",
};

extern void TriAn_init(transform_info_ptr tinfo);
extern void TriAn_display(transform_info_ptr tinfo);

/* Length of stringbuffer and inputbuffer */
#define STRBUFLEN 1024
#define MESSAGELEN 1024
#define INPUTBUFLEN 80
#define SMALL_DISTANCE 0.001	/* Used if a distance is actually 0, to avoid singularity */
#define CHANNELBOX_ENLARGE 0.2 /* Add this fraction to each channelbox dimension */
#define VERTMODE_XFRACT 0.9
#define VERTMODE_YFRACT 0.9
/* The file name used to record and replay events */
#define RECORD_FILENAME "posplot_rec"
#define WRITEASC_OUTFILE "posplot_out.asc"
#define POSTSCRIPT_OUTFILE "posplot_out.ps"
#define POSTSCRIPT_ENVNAME "VPOSTSCRIPT"
#define POSTSCRIPT_DEFDEV "cps"
/* This is for the color height mapping: */
#define NR_OF_GRAYLEVELS 64

#ifdef SGI
#define swapbuffers()
#define doublebuffer()
#endif

enum vertical_scale_locks {
 NO_VSCALE_LOCK=0, INCREMENTAL_VSCALE_LOCK, FIXED_VSCALE_LOCK
};
LOCAL const char *vertical_scale_lock_descriptions[]={
 "None", "Incremental", "Fixed"
};
LOCAL const char *linestyle_types[]={
 "color", "dashed", "single"
};
LOCAL const double nullpos[3]={0.0, 0.0, 0.0};

typedef struct {
 char *channelname;
 char *channelname_to_show;
 Bool selected;
 double pos[3];
} channel_map_entry;

enum linestyles {LST_TYPE_COLOR=0, LST_TYPE_DASHED, LST_TYPE_NONE, LST_TYPE_LAST};
struct posplot_storage {
 int linestyle_offset;
 enum linestyles linestyle_type;
 DATATYPE lowxboundary, highxboundary; /* Markers for the part of the x axis to show */
 DATATYPE lastselected;
 long lastsel_pos;
 Bool auto_subsampling;
 int sampling_step;
 Bool singleitems;
 Bool showcoordsys, shownames, showplots, showdist, showmarker;
 Bool showtriggers;
 Bool colored_triggers; /* If true, cycle colors depending on trigger code */
 Bool show_info;
 Bool quit_mode;
 enum position_modes position_mode;
 int itempart;
 int gid; /* Window ID */
 int dev;
 FILE *replay_file, *record_file;
 float plfact;	/* Factor for the plotting rectangle size */
 float displaysign; /* Display negative down (+1) or up (-1) */
 double xposmin, xposmax, yposmin, yposmax, zposmin, zposmax;
 DATATYPE center_pos[3];
 Angle fov, azimuth, incidence, twist;
 Coord distance, orgdistance;
 DATATYPE ydmin, ydmax;
 DATATYPE yclicked;
 enum vertical_scale_locks lock_vscale;
 tsdata_function function;
 growing_buf channel_map;	/* Scratchpad to note where channels are displayed and whether they are shown */
 channel_map_entry *lastsel_entry;
 growing_buf selection;	/* Scratchpad to note which data sets are selected */
 char messagebuffer[MESSAGELEN];
 char *red_message;
};

/*{{{  Definition of the head outline to display when standard positioning is requested*/
Matrix bspline = {
 { -1.0/6.0,  3.0/6.0, -3.0/6.0, 1.0/6.0 },
 {  3.0/6.0, -6.0/6.0,  3.0/6.0, 0.0     },
 { -3.0/6.0,  0.0,      3.0/6.0, 0.0     },
 {  1.0/6.0,  4.0/6.0,  1.0/6.0, 0.0     }
};

#define BASIS_ID_FOR_CURVES 2
#define BASIS_MATRIX_FOR_CURVES bspline

/* This was converted from a postscript file output by Tgif using the
 * 'postscript_curveto_to_GL_bspline' script. The coordinates are
 * centered around (0,0) and have span 1, ie (-0.5,-0.5):(0.5,0.5). */
#define HEAD_OUTLINE_SIZE 55
Coord head_outline[HEAD_OUTLINE_SIZE][2]={
{0.06292, -0.5},
{0.04461, -0.4365},
{0.05147, -0.3929},
{0.08352, -0.369},
{0.1156, -0.3452},
{0.1568, -0.3333},
{0.2071, -0.3333},
{0.2574, -0.3333},
{0.2986, -0.3314},
{0.3307, -0.3274},
{0.3627, -0.3234},
{0.3764, -0.3095},
{0.3718, -0.2857},
{0.3673, -0.2619},
{0.3718, -0.244},
{0.3856, -0.2321},
{0.3993, -0.2202},
{0.4108, -0.1964},
{0.4199, -0.1607},
{0.429, -0.125},
{0.4451, -0.1032},
{0.468, -0.09524},
{0.4908, -0.08731},
{0.4954, -0.0734},
{0.4817, -0.05357},
{0.468, -0.03374},
{0.4588, -0.003976},
{0.4542, 0.03571},
{0.4496, 0.0754},
{0.4542, 0.1032},
{0.468, 0.119},
{0.4817, 0.1349},
{0.4908, 0.1667},
{0.4954, 0.2143},
{0.5, 0.2619},
{0.4657, 0.3214},
{0.3924, 0.3929},
{0.3192, 0.4643},
{0.1842, 0.5},
{-0.01259, 0.5},
{-0.2094, 0.5},
{-0.333, 0.4405},
{-0.3833, 0.3214},
{-0.4336, 0.2024},
{-0.4428, 0.1071},
{-0.4108, 0.03571},
{-0.3787, -0.03571},
{-0.3536, -0.09921},
{-0.3352, -0.1548},
{-0.3169, -0.2103},
{-0.3169, -0.256},
{-0.3352, -0.2917},
{-0.3536, -0.3274},
{-0.4085, -0.373},
{-0.5, -0.4286}
};
/*}}}  */
/*}}}  */

/*{{{  Channel map local functions*/
LOCAL void
clear_channel_map (struct posplot_storage * const local_arg) {
 if (local_arg->channel_map.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->channel_map, 0);
 } else {
  /* Free the channel name memory: */
  int const n_entries= local_arg->channel_map.current_length/sizeof(channel_map_entry);
  int entry_number=0;
  channel_map_entry *entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
  while (entry_number<n_entries) {
   if (entry->channelname_to_show!=entry->channelname) {
    free_pointer((void **)&entry->channelname_to_show);
   }
   free_pointer((void **)&entry->channelname);
   entry++;
   entry_number++;
  }
 }
 growing_buf_clear(&local_arg->channel_map);
}
LOCAL channel_map_entry *
locate_channel_map_entry(struct posplot_storage * const local_arg, char * const channelname) {
 int const n_entries= local_arg->channel_map.current_length/sizeof(channel_map_entry);
 int entry_number=0;
 channel_map_entry * entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 while (entry_number<n_entries && strcmp(entry->channelname_to_show,channelname)!=0) {
  entry++;
  entry_number++;
 }
 if (entry_number==n_entries) entry=NULL;
 return entry;
}
LOCAL channel_map_entry *
locate_channel_map_entry_with_position_n(struct posplot_storage * const local_arg, transform_info_ptr tinfo, int channel, int *n_with_same_name) {
 int const n_entries= local_arg->channel_map.current_length/sizeof(channel_map_entry);
 int entry_number=0;
 channel_map_entry * entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 (*n_with_same_name)=0;
 while (entry_number<n_entries) {
  if (strcmp(entry->channelname,tinfo->channelnames[channel])==0) {
   (*n_with_same_name)++;
   if (tinfo->probepos[3*channel]==entry->pos[0] 
    && tinfo->probepos[3*channel+1]==entry->pos[1] 
    && tinfo->probepos[3*channel+2]==entry->pos[2]) {
    break;
   }
  }
  entry++;
  entry_number++;
 }
 if (entry_number==n_entries) entry=NULL;
 return entry;
}
LOCAL channel_map_entry *
locate_channel_map_entry_with_position(struct posplot_storage * const local_arg, transform_info_ptr tinfo, int channel) {
 int n_with_same_name;
 return locate_channel_map_entry_with_position_n(local_arg, tinfo, channel, &n_with_same_name);
}
LOCAL int
find_channel_number_for_entry(transform_info_ptr tinfo, channel_map_entry *entry) {
 char ** const channelnames=tinfo->channelnames;
 int const nr_of_channels=tinfo->nr_of_channels;
 int channel=0;

 while (channel<nr_of_channels) {
  if (strcmp(channelnames[channel], entry->channelname)==0 
   && tinfo->probepos[3*channel]==entry->pos[0] 
   && tinfo->probepos[3*channel+1]==entry->pos[1] 
   && tinfo->probepos[3*channel+2]==entry->pos[2]) {
   break;
  }
  channel++;
 }
 if (channel==nr_of_channels) return -1;	/* Not found */
 return channel;
}
LOCAL void
add_channel_map_entry(struct posplot_storage * const local_arg, transform_info_ptr tinfo, int channel) {
 int n_with_same_name;
 if (locate_channel_map_entry_with_position_n(local_arg, tinfo, channel, &n_with_same_name)==NULL) {
  channel_map_entry newentry;
  if ((newentry.channelname=(char *)malloc(strlen(tinfo->channelnames[channel])+1))==NULL) {
   return;
  }
  strcpy(newentry.channelname, tinfo->channelnames[channel]);
  if (n_with_same_name>0) {
   if ((newentry.channelname_to_show=(char *)malloc(strlen(tinfo->channelnames[channel])+6))==NULL) {
    return;
   }
   sprintf(newentry.channelname_to_show, "%s(%d)", tinfo->channelnames[channel], n_with_same_name+1);
  } else {
   newentry.channelname_to_show=newentry.channelname;
  }
  newentry.pos[0]=tinfo->probepos[3*channel];
  newentry.pos[1]=tinfo->probepos[3*channel+1];
  newentry.pos[2]=tinfo->probepos[3*channel+2];
  newentry.selected=TRUE;
  growing_buf_append(&local_arg->channel_map, (char *)&newentry, sizeof(channel_map_entry));
 }
}
LOCAL void
add_channel_map_entries(struct posplot_storage * const local_arg, transform_info_ptr tinfo) {
 int channel=0;
 Bool pos_was_modified=FALSE;
 while (channel<tinfo->nr_of_channels) {
  /*{{{  Make sure to have a unique set of (name, position): */
  int channel2=channel+1;
  while (channel2<tinfo->nr_of_channels) {
   if (channel2!=channel
    && strcmp(tinfo->channelnames[channel], tinfo->channelnames[channel2])==0 
    && tinfo->probepos[3*channel]==tinfo->probepos[3*channel2] 
    && tinfo->probepos[3*channel+1]==tinfo->probepos[3*channel2+1] 
    && tinfo->probepos[3*channel+2]==tinfo->probepos[3*channel2+2]) {
    tinfo->probepos[3*channel2]+=0.123;
    pos_was_modified=TRUE;
   }
   channel2++;
  }
  /*}}}  */
  add_channel_map_entry(local_arg, tinfo, channel);
  channel++;
 }
 if (pos_was_modified) TRACEMS(tinfo->emethods, 0, "posplot: Channels with identical names and positions were made unique by adding 0.123 to the x position.\n");
}
LOCAL channel_map_entry *
move_entry_up(struct posplot_storage * const local_arg, channel_map_entry * move_this) {
 channel_map_entry const stored_entry= *move_this;
 channel_map_entry *entry, *top_entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 if (!move_this->selected) return top_entry;
 while (!top_entry->selected) top_entry++;
 if (move_this==top_entry) return top_entry;
 entry=move_this-1;
 do {
  *(entry+1)= *entry;
  entry--;
 } while (entry>=top_entry && !(entry+1)->selected);
 *(entry+1)= stored_entry;
 return (entry+1);
}
LOCAL channel_map_entry *
move_entry_down(struct posplot_storage * const local_arg, channel_map_entry * move_this) {
 channel_map_entry const stored_entry= *move_this;
 channel_map_entry *entry, *bottom_entry=((channel_map_entry *)(local_arg->channel_map.buffer_start+local_arg->channel_map.current_length))-1;
 if (!move_this->selected) return bottom_entry;
 while (!bottom_entry->selected) bottom_entry--;
 if (move_this==bottom_entry) return bottom_entry;
 entry=move_this+1;
 do {
  *(entry-1)= *entry;
  entry++;
 } while (entry<=bottom_entry && !(entry-1)->selected);
 *(entry-1)= stored_entry;
 return (entry-1);
}
LOCAL int
count_selected_channels(struct posplot_storage * const local_arg) {
 int const n_entries= local_arg->channel_map.current_length/sizeof(channel_map_entry);
 int entry_number=0, n_selected=0;
 channel_map_entry * entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 while (entry_number<n_entries) {
  if (entry->selected) n_selected++;
  entry++;
  entry_number++;
 }
 return n_selected;
}
LOCAL void
select_channels(struct posplot_storage * const local_arg, channel_map_entry * select_this) {
 int const n_entries= local_arg->channel_map.current_length/sizeof(channel_map_entry);
 int entry_number=0;
 channel_map_entry * entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 while (entry_number<n_entries) {
  if (select_this==NULL) {
   /* Select all channels */
   entry->selected=TRUE;
  } else {
   entry->selected=(entry==select_this);
  }
  entry++;
  entry_number++;
 }
}
LOCAL channel_map_entry *
get_first_selected_channel(struct posplot_storage * const local_arg) {
 int const n_entries= local_arg->channel_map.current_length/sizeof(channel_map_entry);
 int entry_number=0;
 channel_map_entry * entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 while (entry_number<n_entries) {
  if (entry->selected) return entry;
  entry++;
  entry_number++;
 }
 return (channel_map_entry *)NULL;
}
/*}}}  */

/*{{{  first_x, last_x, get_selectedpoint*/
LOCAL int
first_x(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 int xpos=0;
 while (xpos<tinfo->nr_of_points && tinfo->xdata[xpos]<local_arg->lowxboundary) {
  xpos++;
 }
 return xpos;
}
LOCAL int
last_x(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 int xpos=tinfo->nr_of_points-1;
 while (xpos>0 && tinfo->xdata[xpos]>local_arg->highxboundary) {
  xpos--;
 }
 return xpos;
}
LOCAL int 
get_selectedpoint(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 int whichpoint=local_arg->lastsel_pos;
 if (whichpoint<0 || whichpoint>=tinfo->nr_of_points ||
     tinfo->xdata[whichpoint]!=local_arg->lastselected) {
  whichpoint=find_pointnearx(tinfo, local_arg->lastselected);
 }
 return whichpoint;
}
/* First argument is always from where to look, second is the boundary up to which to look */
LOCAL struct trigger *find_trigger_after(struct trigger *starttrig, int left, int right) {
 struct trigger *intrig=starttrig;
 struct trigger *mintrig=NULL;
 while (intrig->code!=0) {
  if (intrig->position>left && intrig->position<=right && (mintrig==NULL || intrig->position<mintrig->position)) mintrig=intrig;
  intrig++;
 }
 return mintrig;
}
LOCAL struct trigger *find_trigger_before(struct trigger *starttrig, int right, int left) {
 struct trigger *intrig=starttrig;
 struct trigger *mintrig=NULL;
 while (intrig->code!=0) {
  if (intrig->position<right && intrig->position>=left && (mintrig==NULL || intrig->position>mintrig->position)) mintrig=intrig;
  intrig++;
 }
 return mintrig;
}
/*}}}  */

/*{{{  clear_event_queue*/
LOCAL void
clear_event_queue(void) {
 short val;
 while (qtest()) {
  qread(&val);
 }
}
/*}}}  */

/*{{{  show_Input_line*/
LOCAL void
show_Input_line(char * const inputbuffer) {
 int newminx, newminy;
 Screencoord minx, maxx, miny, maxy;

 frontbuffer(1);
 getviewport(&minx, &maxx, &miny, &maxy);
 pushviewport();
 pushmatrix();
 pushattributes();
 font((Int16)1);
 newminx=minx+(maxx-minx)/2;
 newminy=miny+(maxy-miny)/20;
 viewport(newminx, maxx, newminy, (Screencoord)(newminy+getheight()));
 ortho2((Coord)newminx, (Coord)maxx, (Coord)newminy, (Coord)(newminy+getheight()));
 cmov2((Coord)(newminx+getwidth()), (Coord)newminy);
 color(BACKGROUND);
 clear();
 color(FOREGROUND);
 charstr("INPUT:>");
 charstr(inputbuffer);
 charstr("<");
 popattributes();
 popmatrix();
 popviewport();
}
/*}}}  */

/*{{{  Replay/record file local functions*/
LOCAL FILE *
open_record_file(transform_info_ptr tinfo, char const * const filename) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 if (strcmp(filename, "stdout")==0) local_arg->record_file=stdout;
 else if ((local_arg->record_file=fopen(filename, "w"))==NULL) {
  snprintf(local_arg->messagebuffer, MESSAGELEN, "Couldn't open record file >%s<", filename);
  local_arg->red_message=local_arg->messagebuffer;
 }
 return local_arg->record_file;
}
LOCAL void
close_record_file(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 if (local_arg->record_file!=NULL && local_arg->record_file!=stdout) fclose(local_arg->record_file);
 local_arg->record_file=NULL;
 local_arg->red_message="Recording finished.";
}
LOCAL FILE *
open_replay_file(transform_info_ptr tinfo, char const * const filename) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 if (strcmp(filename, "stdin")==0) local_arg->replay_file=stdin;
 else if ((local_arg->replay_file=fopen(filename, "r"))==NULL) {
  snprintf(local_arg->messagebuffer, MESSAGELEN, "Couldn't open replay file >%s<", filename);
  local_arg->red_message=local_arg->messagebuffer;
 }
 return local_arg->replay_file;
}
LOCAL void
close_replay_file(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 if (local_arg->replay_file!=NULL && local_arg->replay_file!=stdin) fclose(local_arg->replay_file);
 local_arg->replay_file=NULL;
 local_arg->red_message="Replay finished.";
 if (!local_arg->quit_mode) clear_event_queue();
}
/*}}}  */

/*{{{  posplot_init(transform_info_ptr tinfo)*/
METHODDEF void
posplot_init(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int i;

 /*{{{  Initialize local_arg variables*/
 local_arg->singleitems=FALSE;
 local_arg->itempart=0;
 local_arg->fov=200;	/* Field of vision, 20 deg initially */
 local_arg->azimuth=0; local_arg->incidence=0; local_arg->twist=0;
 local_arg->plfact=0.0; /* Set when number of channels in channel map is known */
 local_arg->displaysign=1.0; /* Plot negative down by default */
 local_arg->showcoordsys=FALSE;
 local_arg->shownames=TRUE;
 local_arg->showplots=FALSE;
 local_arg->showdist=FALSE;
 local_arg->showmarker=FALSE;
 local_arg->showtriggers=FALSE;
 local_arg->colored_triggers=FALSE;
 local_arg->show_info=FALSE;
 local_arg->position_mode=POSITION_MODE_XYZ;
 local_arg->replay_file=NULL;
 local_arg->record_file=NULL;
 local_arg->lastsel_entry=NULL;
 local_arg->lock_vscale=NO_VSCALE_LOCK;
 local_arg->linestyle_offset=0; 
 local_arg->linestyle_type=0; 
 local_arg->function=TS_RAW;
 local_arg->dev=NEWBORDER;
 local_arg->red_message="This is posplot (c) 1993-2014 by Bernd Feige.";

 if (tinfo->data_type==FREQ_DATA) tinfo->nr_of_points=tinfo->nroffreq;
 local_arg->lastselected= 0.0;
 local_arg->lastsel_pos= -1;
 local_arg->lowxboundary= -HUGE_VAL; 
 local_arg->highxboundary= HUGE_VAL;
 local_arg->auto_subsampling=TRUE;
 local_arg->sampling_step=1;

 growing_buf_init(&local_arg->channel_map);
 clear_channel_map(local_arg);
 growing_buf_init(&local_arg->selection);
 if (local_arg->selection.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->selection, 0);
 }
 /*}}}  */

 local_arg->quit_mode=args[ARGS_QUIT].is_set;

 local_arg->gid=winopen("posplot");
#ifdef SGI
 winposition(100,1100,100,900);
 reshapeviewport();
#endif
 /* Gobble up the redraw event that's sent automatically - GL sends three ! */
 if (!local_arg->quit_mode) clear_event_queue();
 doublebuffer();
 gconfig();

#ifdef SGI
 /*{{{  Redefine GL cursor etc.*/
 /* Disliking the thick red arrow GL cursor, we define our thin white cross */
 static Uint16 cross[] = { 
  0x0080, 0x0080, 0x0080, 0x0080,
  0x0080, 0x0080, 0x0080, 0x0080,
  0xffff, 0x0080, 0x0080, 0x0080,
  0x0080, 0x0080, 0x0080, 0x0080};
 curstype(C16X1);
 drawmode(CURSORDRAW);
 mapcolor(1, 255, 255, 255); /* White */
 mapcolor(2, 0, 0, 255);
 mapcolor(3, 255, 255, 255);
 defcursor(1, cross);
 curorigin(1, 8, 8);
 setcursor(1, 0, 0);
 drawmode(NORMALDRAW);
 loadXfont(1, "-adobe-courier-medium-r-normal--24-240-75-75-m-150-iso8859-1");
 /*}}}  */
#endif

 /*{{{  Set up own line styles and the curve basis*/
 for (i=0; i<LINESTYLECOUNT; i++) {
  deflinestyle(LINESTYLE_DASHED+i, linepatterns[i]);
 }

 curveprecision((short)20);
 defbasis((short)BASIS_ID_FOR_CURVES, BASIS_MATRIX_FOR_CURVES);
 curvebasis((short)BASIS_ID_FOR_CURVES);
 /*}}}  */

 /*{{{  Set up grey color map*/
 mapcolor((Colorindex)8, (Int16)0, (Int16)0, (Int16)0);
 {int colorno=1;
 for (; colorno<=NR_OF_GRAYLEVELS; colorno++) {
  i=(int)rint(((float)(255*colorno))/NR_OF_GRAYLEVELS);
  mapcolor((Colorindex)colorno+8, (Int16)i, (Int16)i, (Int16)i);
 }
 }
 /*}}}  */

 /*{{{  Select devices*/
 unqdevice(INPUTCHANGE);
 qdevice(REDRAW);
 qdevice(ESCKEY);
 qdevice(LEFTMOUSE);
 qdevice(MIDDLEMOUSE);
 qdevice(RIGHTMOUSE);	/* Right mouse button */
 qenter(REDRAW, local_arg->gid);
 qdevice(KEYBD);
 /*}}}  */

 /*{{{  Open replay file if told so by posplot_args*/
 if (args[ARGS_RUN].is_set || args[ARGS_RUNFILE].is_set) {
  char const *filename=RECORD_FILENAME;
  if (args[ARGS_RUNFILE].is_set) filename=args[ARGS_RUNFILE].arg.s;
  open_replay_file(tinfo, filename);
 }
 /*}}}  */

 tinfo->methods->local_storage=(void *)local_arg;

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  Local functions*/

#define dataset_is_shown(dataset) (((Bool *)local_arg->selection.buffer_start)[2*(dataset)])
#define dataset_is_fixed(dataset) (((Bool *)local_arg->selection.buffer_start)[2*(dataset)+1])

/* A local function to toggle the show status of a given linked list dataset */
LOCAL Bool 
dataset_toggle_shown(transform_info_ptr tinfo, unsigned int dataset) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 unsigned int const ndatasets=local_arg->selection.current_length/(2*sizeof(Bool));
 if (dataset<=0 || dataset>ndatasets) {
  return FALSE;
 } else {
  Bool *const in_selection=((Bool *)local_arg->selection.buffer_start)+2*(dataset-1);
  *in_selection= !*in_selection;
  *(in_selection+1)= FALSE; /* Automatically un-fix the dataset */
 }
 return TRUE;
}
/* A local function to toggle the fixed status of a given linked list dataset */
LOCAL Bool 
dataset_toggle_fixed(transform_info_ptr tinfo, unsigned int dataset) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 unsigned int const ndatasets=local_arg->selection.current_length/(2*sizeof(Bool));
 if (dataset<=0 || dataset>ndatasets) {
  return FALSE;
 } else {
  Bool *const in_selection=((Bool *)local_arg->selection.buffer_start)+2*(dataset-1)+1;
  *in_selection= !*in_selection;
 }
 return TRUE;
}

/* tsdata() returns part itempart of the data item (channel, point) */
LOCAL DATATYPE
tsdata(transform_info_ptr tinfo, int itempart, tsdata_function function, int channel, int point) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 DATATYPE * const datap=tinfo->tsdata+(tinfo->multiplexed ? (point*tinfo->nr_of_channels+channel) : (channel*tinfo->nr_of_points+point))*tinfo->itemsize+itempart;
 DATATYPE value;

 switch (function) {
  case TS_RAW:
   value= *datap;
   break;
  case TS_EXP:
   value=exp(*datap);
   break;
  case TS_LOG:
   value=log(*datap);
   break;
  case TS_EXP10:
   value=pow(10.0, *datap);
   break;
  case TS_LOG10:
   value=log10(*datap);
   break;
  case TS_EXPDB:
   value=pow(10.0, *datap/10.0);
   break;
  case TS_LOGDB:
   value=log10(*datap)*10.0;
   break;
  case TS_POWER:
   value= (tinfo->itemsize>1 && !local_arg->singleitems ?
    c_power(*((complex *)(datap-itempart%2))) : (*datap)*(*datap));
   break;
  case TS_ABS:
   value= (tinfo->itemsize>1 && !local_arg->singleitems ?
    c_abs(*((complex *)(datap-itempart%2))) : fabs(*datap));
   break;
  case TS_PHASE:
   value= (tinfo->itemsize>1 ?
    c_phase(*((complex *)(datap-itempart%2)))/M_PI*180 : 0.0);
   break;
  default:
   value=0.0;
   break;
 }
 return value;
}

/* Find out into which of the plotting rectangles the mouse pointed */
LOCAL channel_map_entry *
which_channel_below(transform_info_ptr tinfo, Screencoord mousex, Screencoord mousey, Screencoord plotlength, Screencoord plotheight, DATATYPE xdmin, DATATYPE xdmax, DATATYPE ydmin, DATATYPE ydmax, int *xpos, DATATYPE *yclicked, Bool allchannels) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 unsigned int entry_number; 
 int displayed_channel=0;
 int const nchannels=count_selected_channels(local_arg);
 channel_map_entry *entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 Screencoord minx, maxx, miny, maxy;

 getviewport(&minx, &maxx, &miny, &maxy);

 for (entry_number=0; entry_number<local_arg->channel_map.current_length/sizeof(channel_map_entry); entry_number++, entry++) {
  double const *thispos=entry->pos;
  Screencoord newminx, newminy;
  /* In vertical mode, allchannels makes no sense */
  if ((local_arg->position_mode==POSITION_MODE_VERTICAL || !allchannels)
    && !entry->selected) continue;
  if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
   newminx=(Screencoord)(minx+((1.0-VERTMODE_XFRACT)/2)*(maxx-minx));
   newminy=(Screencoord)(maxy-(((1.0-VERTMODE_YFRACT)/2)+VERTMODE_YFRACT*((displayed_channel+1.0)/nchannels))*(maxy-miny));
  } else {
  if (local_arg->position_mode==POSITION_MODE_OVERLAY) thispos=nullpos;

  cmov(thispos[0], thispos[1], thispos[2]);
  getcpos(&newminx, &newminy);
  newminx-=plotlength/2;
  newminy-=plotheight/2;
  }
  displayed_channel++;
  if (mousex>newminx && mousex<newminx+plotlength &&
      mousey>newminy && mousey<newminy+plotheight) {
   DATATYPE guessx;
   int const left=first_x(tinfo), right=last_x(tinfo);
   guessx=xdmin+(xdmax-xdmin)*(mousex-newminx)/plotlength;
   for (*xpos=left; *xpos<tinfo->nr_of_points && *xpos<=right && tinfo->xdata[*xpos]<guessx; (*xpos)++);
   if (guessx-tinfo->xdata[*xpos-1]<tinfo->xdata[*xpos]-guessx) (*xpos)--;
   if (local_arg->displaysign>0) {
    *yclicked=ydmin+(ydmax-ydmin)*(mousey-newminy)/plotheight;
   } else {
    *yclicked=ydmax-(ydmax-ydmin)*(mousey-newminy)/plotheight;
   }
   return entry;
  }
 }
 return NULL;
}

/* Find out near which of the plotting centers the mouse pointed */
LOCAL channel_map_entry *
which_channel_near(transform_info_ptr tinfo, Screencoord mousex, Screencoord mousey, Bool allchannels) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 unsigned int entry_number;
 int displayed_channel=0;
 int const nchannels=count_selected_channels(local_arg);
 channel_map_entry *nearest_entry=NULL, *entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 double this_distance, smallest_distance=HUGE_VAL;
 Screencoord minx, maxx, miny, maxy;

 getviewport(&minx, &maxx, &miny, &maxy);

 for (entry_number=0; entry_number<local_arg->channel_map.current_length/sizeof(channel_map_entry); entry_number++, entry++) {
  double const *thispos=entry->pos;
  Screencoord middlex, middley;
  /* In vertical mode, allchannels makes no sense */
  if ((local_arg->position_mode==POSITION_MODE_VERTICAL || !allchannels)
    && !entry->selected) continue;
  if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
   middlex=(Screencoord)(minx+(((1.0-VERTMODE_XFRACT)/2)+VERTMODE_XFRACT*0.5)*(maxx-minx));
   middley=(Screencoord)(maxy-(((1.0-VERTMODE_YFRACT)/2)+VERTMODE_YFRACT*((displayed_channel+0.5)/nchannels))*(maxy-miny));
  } else {
   if (local_arg->position_mode==POSITION_MODE_OVERLAY) thispos=nullpos;

   cmov(thispos[0], thispos[1], thispos[2]);
   getcpos(&middlex, &middley);
  }
  displayed_channel++;
  
  this_distance=(middlex-mousex)*(middlex-mousex)+(middley-mousey)*(middley-mousey);
  if (this_distance<smallest_distance) {
   smallest_distance=this_distance;
   nearest_entry=entry;
  }
 }
 return nearest_entry;
}

LOCAL void
move_channel_xy(transform_info_ptr tinfo, Screencoord dx, Screencoord dy) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 /* What we accomplish here is the following:
  * We move the currently selected channel dx points (in screen coords) in the x 
  * direction and dy points in the y direction. So far so simple. But this is
  * done by changing the 3D position of the channel. This means that we have
  * to move the point in 3D space within the plane perpendicular to the line of 
  * sight. Mathematically, what we need is the solution to an underdetermined linear
  * system, and we do that by using our approved array_svd_solution function. */
 if (local_arg->lastsel_entry!=NULL) {
  double const realunit=local_arg->distance/10;
  array origin, a, b, x;
  Screencoord screenx, screeny;
  origin.element_skip=a.element_skip=b.element_skip=x.element_skip=1;
  origin.nr_of_vectors=b.nr_of_vectors=x.nr_of_vectors=1;
  origin.nr_of_elements=a.nr_of_elements=b.nr_of_elements=2;
  a.nr_of_vectors=x.nr_of_elements=3;
  if (array_allocate(&origin)==NULL || array_allocate(&a)==NULL || array_allocate(&b)==NULL || array_allocate(&x)==NULL) {
   return;
  }

  /* First, we assess 4x2 values, the screen coords of the origin and those of
   * points a distance `realunit' in the 3 directions of space.
   * One difficulty here is that these are screen coordinates, and care must be
   * taken that valid screen coordinates exist for all points polled. */
  cmov(local_arg->center_pos[0], local_arg->center_pos[1], local_arg->center_pos[2]);
  getcpos(&screenx, &screeny);
  array_write(&origin, screenx); array_write(&origin, screeny);

  cmov(local_arg->center_pos[0]+realunit, local_arg->center_pos[1], local_arg->center_pos[2]);
  getcpos(&screenx, &screeny);
  array_write(&a, screenx); array_write(&a, screeny);

  cmov(local_arg->center_pos[0], local_arg->center_pos[1]+realunit, local_arg->center_pos[2]);
  getcpos(&screenx, &screeny);
  array_write(&a, screenx); array_write(&a, screeny);

  cmov(local_arg->center_pos[0], local_arg->center_pos[1], local_arg->center_pos[2]+realunit);
  getcpos(&screenx, &screeny);
  array_write(&a, screenx); array_write(&a, screeny);

  /* Subtract the origin from the three coordinates: */
  do {
   DATATYPE * const currval=ARRAY_ELEMENT(&a);
   *currval=array_subtract(&a, &origin);
  } while (a.message!=ARRAY_ENDOFSCAN);

  /* b contains the known inhomogenity (A*x=b) */
  array_write(&b, dx); array_write(&b, dy);

  /* Now solve the equation and use the result. */
  array_svd_solution(&a, &b, &x);

  array_reset(&x);

  /* Immediately write the new position into all data sets: */
  {
  transform_info_ptr tinfoptr;
  double const p1=local_arg->lastsel_entry->pos[0], p2=local_arg->lastsel_entry->pos[1], p3=local_arg->lastsel_entry->pos[2];
  double const pn1=p1+realunit*array_scan(&x), pn2=p2+realunit*array_scan(&x), pn3=p3+realunit*array_scan(&x);
  for (tinfoptr=tinfo; tinfoptr!=NULL; tinfoptr=tinfoptr->next) {
   int const channel=find_channel_number_for_entry(tinfoptr, local_arg->lastsel_entry);
   if (channel>=0) {
    tinfoptr->probepos[3*channel  ]= pn1;
    tinfoptr->probepos[3*channel+1]= pn2;
    tinfoptr->probepos[3*channel+2]= pn3;
   }
  }
  local_arg->lastsel_entry->pos[0]= pn1;
  local_arg->lastsel_entry->pos[1]= pn2;
  local_arg->lastsel_entry->pos[2]= pn3;
  }

  array_free(&origin);
  array_free(&a);
  array_free(&b);
  array_free(&x);
 }
}
/*}}}  */

/*{{{  posplot(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
posplot(transform_info_ptr tinfo) {
 /*{{{  local variables*/
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
 transform_info_ptr top_tinfo, tinfoptr, swaptinfo=NULL;
 transform_info_ptr tinfo_to_use;
 transform_info_ptr const orig_tinfo=tinfo;
 Screencoord minx, maxx, miny, maxy, plotlength, plotheight, mousex, mousey;
 DATATYPE xdmin, xdmax, data;
 double xposmin, xposmax, yposmin, yposmax, zposmin, zposmax, posdata;
 char stringbuffer[STRBUFLEN], inputbuffer[INPUTBUFLEN];
 int channel, dev=local_arg->dev, i; 
 int max_nr_of_channels;
 int max_nr_of_points;
 int nr_of_selected_channels;
 int displayed_channel;
 int rightleftview=0;	/* Indicates view unknown (0), from right (1), left (2) */
 int top_in_dataset_map=0;	/* Number of data set with which DS map starts */
 Bool postscript_output=FALSE;
 Bool get_string=FALSE;
 Bool have_triangulation=FALSE;
 Bool return_null=FALSE;

#ifdef INCLUDE_DIPOLE_FIT
 DATATYPE dipole_pos[3]={0.0, 0.0, 0.0}, dipole_vec[3];
#endif
 struct transform_methods_struct swap_methods_struct, *org_methods_ptr;
 const int char_width=getwidth(), char_height=getheight();
 channel_map_entry *entry;
 unsigned int entry_number;
 /*}}}  */

 if (tinfo->data_type==FREQ_DATA) {
  tinfo->nr_of_points=tinfo->nroffreq;
  max_nr_of_points=tinfo->nr_of_points;
  if (tinfo->nrofshifts>1 && tinfo->next==NULL) {
   tinfo->next=(struct transform_info_struct *)malloc((tinfo->nrofshifts-1)*sizeof(struct transform_info_struct));
   if (tinfo->next==NULL) {
    ERREXIT(tinfo->emethods, "posplot: Error allocating transform_info_struct array\n");
   }
   tinfo->previous=NULL;
   tinfoptr=tinfo;	/* This holds the previous tinfo */
   tinfoptr->z_label="Lat[ms]";
   tinfoptr->z_value=((tinfo->windowsize-tinfo->beforetrig)/tinfo->sfreq)*1000;
   for (i=0; i<tinfo->nrofshifts-1; i++) {
    tinfoptr->next=tinfo->next+i;
    memcpy(tinfoptr->next, tinfo, sizeof(struct transform_info_struct));
    tinfoptr->next->previous=tinfoptr;
    tinfoptr->next->tsdata=tinfoptr->tsdata+tinfo->nr_of_channels*tinfo->nroffreq*tinfo->itemsize;
    tinfoptr->next->z_label=tinfoptr->z_label;
    tinfoptr->next->z_value=((i+1)*tinfo->basetime+(tinfo->windowsize-tinfo->beforetrig)/tinfo->sfreq)*1000;
    tinfoptr=tinfoptr->next;
   }
   tinfoptr->next=NULL;
  }
 }

do { /* Repeat from here if dev==NEWBORDER || dev==NEWDATA */
 int current_dataset, ndatasets=0;
 max_nr_of_channels=0;
 max_nr_of_points=0;
 for (tinfoptr=tinfo; tinfoptr!=NULL; tinfoptr=tinfoptr->next) {
  /* Synchronize the methods pointer across all linked epochs - note that
   * `posplot' is only written into the first tinfo structure by setup_queue! */
  tinfoptr->methods=tinfo->methods;
  if (tinfoptr->nr_of_channels>max_nr_of_channels) max_nr_of_channels=tinfoptr->nr_of_channels;
  if (tinfoptr->nr_of_points>max_nr_of_points) max_nr_of_points=tinfoptr->nr_of_points;
  add_channel_map_entries(local_arg, tinfoptr);
  ndatasets++;
 }
 /* Heuristic to set the plotting rectangle size depending on the number of channels displayed */
 if (local_arg->plfact==0.0) local_arg->plfact=3.3/sqrt(local_arg->channel_map.current_length/sizeof(channel_map_entry));
 nr_of_selected_channels=count_selected_channels(local_arg);
 /* Possibly extend the dataset selection buffer */
 while (local_arg->selection.current_length/(int)(2*sizeof(Bool))<ndatasets) {
  Bool const f[2]={FALSE, FALSE};
  growing_buf_append(&local_arg->selection, (char *)f, 2*sizeof(Bool));
 }

 /*{{{  Find the max and min values of the sensor positions*/
 xposmin=yposmin=zposmin=  FLT_MAX; 
 xposmax=yposmax=zposmax= -FLT_MAX;

 for (i=1, tinfoptr=tinfo; tinfoptr!=NULL; i++, tinfoptr=tinfoptr->next) {
  if (tinfoptr->xdata==NULL) create_xaxis(tinfoptr, NULL);
  if (tinfoptr->z_label==NULL) {
   tinfoptr->z_label="Nr";
   tinfoptr->z_value=i;
  }
 }
 entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
 for (entry_number=0; entry_number<local_arg->channel_map.current_length/sizeof(channel_map_entry); entry_number++, entry++) {
  const double *thispos=entry->pos;
  if (!entry->selected) continue;
  if (local_arg->position_mode==POSITION_MODE_OVERLAY) thispos=nullpos;
  posdata=thispos[0];
  if (posdata<xposmin) xposmin=posdata;
  if (posdata>xposmax) xposmax=posdata;
  posdata=thispos[1];
  if (posdata<yposmin) yposmin=posdata;
  if (posdata>yposmax) yposmax=posdata;
  posdata=thispos[2];
  if (posdata<zposmin) zposmin=posdata;
  if (posdata>zposmax) zposmax=posdata;
 }

 posdata=(xposmax-xposmin)*CHANNELBOX_ENLARGE;
 if (posdata==0.0) posdata=SMALL_DISTANCE;
 xposmax+=posdata; xposmin-=posdata;
 posdata=(yposmax-yposmin)*CHANNELBOX_ENLARGE;
 if (posdata==0.0) posdata=SMALL_DISTANCE;
 yposmax+=posdata; yposmin-=posdata;
 posdata=(zposmax-zposmin)*CHANNELBOX_ENLARGE;
 if (posdata==0.0) posdata=SMALL_DISTANCE;
 zposmax+=posdata; zposmin-=posdata;

 local_arg->center_pos[0]=(xposmax+xposmin)/2;
 local_arg->center_pos[1]=(yposmax+yposmin)/2;
 local_arg->center_pos[2]=(zposmax+zposmin)/2;
 /*}}}  */

 /* The following positioning normalization is annoying between datasets: */
 if (dev==NEWBORDER) {
 /* A default for the viewing distance */
 posdata=xposmax-xposmin;
 if (yposmax-yposmin>posdata) posdata=yposmax-yposmin;
 if (zposmax-zposmin>posdata) posdata=zposmax-zposmin;
 local_arg->distance=local_arg->orgdistance=posdata*2.8;
 }

 /* Find the first selected data set */
 for (current_dataset=0, top_tinfo=tinfo; top_tinfo!=NULL && !dataset_is_shown(current_dataset); current_dataset++, top_tinfo=top_tinfo->next);
 /* This is the tinfo to use for x data access etc.: */
 tinfo_to_use=(top_tinfo!=NULL ? top_tinfo : tinfo);

 /*{{{  Find the max and min values of the data*/
 xdmin=  FLT_MAX;
 xdmax= -FLT_MAX;
 if (local_arg->lock_vscale==NO_VSCALE_LOCK) {
  local_arg->ydmin=  FLT_MAX;
  local_arg->ydmax= -FLT_MAX;
 }

 for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
  int const left=first_x(tinfoptr), right=last_x(tinfoptr);
  /* Note that the min and max values of the x axis are determined globally since
   * they can be set using the marker; the y values are determined only for
   * the selected epochs. This is useful should nr_of_points vary across epochs. */
  for (i=left; i<tinfoptr->nr_of_points && i<=right; i+=1) {
   data=tinfoptr->xdata[i];
   if (data<xdmin) xdmin=data;
   if (data>xdmax) xdmax=data;
  }

  if (!dataset_is_shown(current_dataset)) continue;

  if (local_arg->lock_vscale!=FIXED_VSCALE_LOCK) {
   for (channel=0; channel<tinfoptr->nr_of_channels; channel++) {
    entry=locate_channel_map_entry_with_position(local_arg, tinfoptr, channel);
    if (!entry->selected) continue;
    for (i=left; i<tinfoptr->nr_of_points && i<=right; i+=1) {
     data=tsdata(tinfoptr, local_arg->itempart, local_arg->function, channel, i);
     if (isnan(data) || isinf(data)) continue;
     if (data<local_arg->ydmin) local_arg->ydmin=data;
     if (data>local_arg->ydmax) local_arg->ydmax=data;
    }
   }
  }
 }
 /* Corrections if data span is zero or if no data was seen, to stay sane */
 if (xdmin==FLT_MAX) {
  /* No data was seen */
  xdmax=1.0; xdmin= -1.0;
 } else if (xdmax-xdmin==0.0) {
  /* All data points had the same value.
   * If the value is, say, 1e8, then adding 0.5 does not lead to a different
   * value... So we have to adapt the offset to the exponent of our number.*/
  DATATYPE const e10=(local_arg->ydmax==0.0 ? -100.0 : log10(fabs(local_arg->ydmax)));
  if (e10>DATATYPE_DIGITS) {
   xdmax+=0.5*pow(10.0,e10-DATATYPE_DIGITS); xdmin-=0.5*pow(10.0,e10-DATATYPE_DIGITS);
  } else {
   xdmax+=0.5; xdmin-=0.5;
  }
 }
 if (local_arg->ydmin==FLT_MAX) {
  /* No data was seen */
  local_arg->ydmax=1.0; local_arg->ydmin= -1.0;
 } else if (local_arg->ydmax-local_arg->ydmin==0.0) {
  /* All data points had the same value; Cf. above explanation. */
  DATATYPE const e10=(local_arg->ydmax==0.0 ? -100.0 : log10(fabs(local_arg->ydmax)));
  if (e10>DATATYPE_DIGITS) {
   local_arg->ydmax+=0.5*pow(10.0,e10-DATATYPE_DIGITS); local_arg->ydmin-=0.5*pow(10.0,e10-DATATYPE_DIGITS);
  } else {
   local_arg->ydmax+=0.5; local_arg->ydmin-=0.5;
  }
 }
 /*}}}  */

 do { /* Repeat from here if dev==REDRAW */
  Bool leave=FALSE;
  DATATYPE selmin, selmax;
  int lastsel_changed=(local_arg->lastsel_entry!=NULL); /* sel[min,max] have to be reset */
#ifdef SGI
  Int32 originx, originy;
  /* On AIX GL, viewport minx and miny are always 0, but getcpos() returns
   * ABSOLUTE screen coordinates. */
  getorigin(&originx, &originy);
#endif

  reshapeviewport();
  /* This is done anew each time because the window may be resized */
  getviewport(&minx, &maxx, &miny, &maxy);

  if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
   plotlength=(Screencoord)(VERTMODE_XFRACT*(maxx-minx));
   plotheight=(Screencoord)(VERTMODE_YFRACT*(maxy-miny)/nr_of_selected_channels);
  } else {
   plotlength=(Screencoord)(local_arg->plfact*(maxx-minx)*CHANNELBOX_ENLARGE);
   plotheight=(Screencoord)(local_arg->plfact*(maxy-miny)*CHANNELBOX_ENLARGE);
  }

  /* Auto-subsampling algorithm */
  if (local_arg->auto_subsampling) {
   int const npoints=last_x(tinfo_to_use)-first_x(tinfo_to_use)+1;
   local_arg->sampling_step=rint(((float)npoints)/plotlength+0.5);
  }

  if (lastsel_changed) {
   /*{{{  Recalculate selmin and selmax*/
   if (local_arg->showdist && local_arg->lastsel_entry!=NULL && top_tinfo!=NULL) {
    int const whichpoint=get_selectedpoint(top_tinfo);
     /* We want to show color coded channel distribution for one x value;
      * To maximize the usable color range, find out the value range */
     selmin=FLT_MAX;
     selmax= -FLT_MAX;
     for (channel=0; channel<top_tinfo->nr_of_channels; channel++) {
      entry=locate_channel_map_entry_with_position(local_arg, top_tinfo, channel);
      if (!entry->selected) continue;
      data=tsdata(top_tinfo, local_arg->itempart, local_arg->function, channel, whichpoint);
      if (isnan(data) || isinf(data)) continue;
      if (data<selmin) selmin=data;
      if (data>selmax) selmax=data;
     }
   }
   lastsel_changed=FALSE;
   /*}}}  */
  }

  /*{{{  Set up the viewing transformation*/
  if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
   ortho2((Coord)0, (Coord)1, (Coord)0, (Coord)1);
  } else {
   perspective(local_arg->fov, (Float32)1, (Coord)0, (Coord)100);
   polarview(local_arg->distance, local_arg->azimuth, local_arg->incidence, local_arg->twist);
   /* Set the origin to center_pos */
   translate(-local_arg->center_pos[0], -local_arg->center_pos[1], -local_arg->center_pos[2]);
  }
  /*}}}  */

  backbuffer(1);
  color(BACKGROUND);
  clear();

  displayed_channel=0;
  entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
  for (entry_number=0; entry_number<local_arg->channel_map.current_length/sizeof(channel_map_entry); entry_number++, entry++) {
   /*{{{  Plot into small flat channel screens*/
   const double *thispos=entry->pos;
   Screencoord newminx, newminy;
   if (!entry->selected) continue;
   if (local_arg->position_mode==POSITION_MODE_OVERLAY) thispos=nullpos;

   if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
    newminx=(Screencoord)(minx+((1.0-VERTMODE_XFRACT)/2)*(maxx-minx));
    newminy=(Screencoord)(maxy-(((1.0-VERTMODE_YFRACT)/2)+VERTMODE_YFRACT*((displayed_channel+1.0)/nr_of_selected_channels))*(maxy-miny));
   } else {
    /* Let the system calculate the new viewport positions for us */
    cmov(thispos[0], thispos[1], thispos[2]);
    getcpos(&newminx, &newminy);
#    ifdef SGI
    newminx-=originx;
    newminy-=originy;
#    endif
    newminx-=plotlength/2;
    newminy-=plotheight/2;
   }
   if (newminx>=minx && newminx+plotlength<=maxx && newminy>=miny && newminy+plotheight<=maxy) {
    /*{{{  Save viewport and matrix and set up small 2d screens*/
    pushviewport();
    pushmatrix();
    viewport(newminx, newminx+plotlength, newminy, newminy+plotheight);
    if (local_arg->displaysign>0.0) {
     ortho2((Coord)xdmin, (Coord)xdmax, (Coord)local_arg->ydmin, (Coord)local_arg->ydmax);
    } else {
     ortho2((Coord)xdmin, (Coord)xdmax, (Coord)local_arg->ydmax, (Coord)local_arg->ydmin);
    }

    if (local_arg->showdist && local_arg->lastsel_entry!=NULL && nr_of_selected_channels>1 && top_tinfo!=NULL) {
     int const whichpoint=get_selectedpoint(top_tinfo);
     /*{{{  */
     if ((channel=find_channel_number_for_entry(top_tinfo, entry))>=0) {
      if (local_arg->lock_vscale==FIXED_VSCALE_LOCK) {
       i=(int)rint((tsdata(top_tinfo, local_arg->itempart, local_arg->function, channel, whichpoint)-local_arg->ydmin)/(local_arg->ydmax-local_arg->ydmin)*NR_OF_GRAYLEVELS);
       if (i<0) i=0;
       if (i>NR_OF_GRAYLEVELS) i=NR_OF_GRAYLEVELS;
      } else {
       i=(int)rint((tsdata(top_tinfo, local_arg->itempart, local_arg->function, channel, whichpoint)-selmin)/(selmax-selmin)*NR_OF_GRAYLEVELS);
      }
      if (local_arg->displaysign== -1) i=NR_OF_GRAYLEVELS-i;
      color(i+8);
      clear();
     }
     /*}}}  */
    }

    /* Avoid drawing triggers, coordinate system and marker multiple times in overlay mode */
    if (local_arg->position_mode!=POSITION_MODE_OVERLAY || displayed_channel==0) {
    if (local_arg->showtriggers) {
     /*{{{  */
     for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
      struct trigger *intrig=(struct trigger *)tinfoptr->triggers.buffer_start+1;
      if (tinfoptr->triggers.buffer_start==NULL || !dataset_is_shown(current_dataset)) continue;
      channel=find_channel_number_for_entry(tinfoptr, entry);
      if (channel<0) continue;
      while (intrig->code!=0) {
       if (intrig->position<0 || intrig->position>=tinfoptr->nr_of_points) {
	TRACEMS(tinfo->emethods, 0, "posplot: Trigger point outside epoch!\n");
       } else {
	if (local_arg->colored_triggers) {
	 int const colorindex=intrig->code%LINECOLORCOUNT;
	 /* For negative values, '%' is negative in C... Produce a continuous sequence anyway. */
         color(linecolors[colorindex>=0 ? colorindex : LINECOLORCOUNT+colorindex]);
	} else {
	 /* Stimuli are shown as CYAN, responses as MAGENTA and keyboard markers FOREGROUND */
	 color(intrig->code>0 ? CYAN : (intrig->code<= -16 ? FOREGROUND : MAGENTA));
	}
	bgnline();
	data=tinfoptr->xdata[intrig->position];
	move2((Coord)data, (Coord)local_arg->ydmin);
	draw2((Coord)data, (Coord)local_arg->ydmax);
	endline();
       }
       intrig++;
      }
     }
     /*}}}  */
    }

    if (local_arg->showcoordsys) {
     /*{{{  */
     DATATYPE barpos;
     color(COORDSYSTEM);
     bgnline();
     move2((Coord)0, (Coord)local_arg->ydmin);
     draw2((Coord)0, (Coord)local_arg->ydmax);

     if (local_arg->ydmin>0 && local_arg->ydmin<100 && local_arg->ydmax>100) {
      /* Suppose it's more useful to display a bar at y==100 (for percentages) */
      color(BLUE);
      barpos=100.0;
     } else if (local_arg->ydmin>0 && local_arg->ydmin<1 && local_arg->ydmax>1) {
      /* Suppose it's more useful to display a bar at y==1 (for ratios) */
      color(BLUE);
      barpos=1.0;
     } else barpos=0;
     move2((Coord)xdmin, (Coord)barpos);
     draw2((Coord)xdmax, (Coord)barpos);
     endline();
     /*}}}  */
    }

    if (local_arg->showmarker && local_arg->lastsel_entry!=NULL && local_arg->lastselected>=xdmin && local_arg->lastselected<=xdmax) {
     /*{{{  */
     color(BLUE);
     bgnline();
     move2((Coord)local_arg->lastselected, (Coord)local_arg->ydmin);
     draw2((Coord)local_arg->lastselected, (Coord)local_arg->ydmax);
     endline();
     /*}}}  */
    }
    }

    if (local_arg->showplots) {
     /*{{{  */
     Float32 vector2[2];
     color(GREEN);
     dev=LINESTYLE_NORMAL;
     for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next, dev++) {
      int const left=first_x(tinfoptr), right=last_x(tinfoptr);
      if (!dataset_is_shown(current_dataset)) continue;
      channel=find_channel_number_for_entry(tinfoptr, entry);
      if (channel<0) continue;

      /* Now draw one channel */
      switch (local_arg->linestyle_type) {
       case LST_TYPE_DASHED:
        setlinestyle((dev+local_arg->linestyle_offset-LINESTYLE_NORMAL)%(LINESTYLECOUNT+1)+LINESTYLE_NORMAL);
	break;
       case LST_TYPE_COLOR:
        color(linecolors[(dev+local_arg->linestyle_offset-LINECOLOR_NORMAL)%LINECOLORCOUNT+LINECOLOR_NORMAL]);
	break;
       default:
	break;
      }
      bgnline();
      for (i=left; i<tinfoptr->nr_of_points && i<=right; i+=local_arg->sampling_step) {
       if (local_arg->sampling_step>1) {
	long mini= -1;
	long maxi= -1;
	DATATYPE minval=FLT_MAX; 
	DATATYPE maxval= -FLT_MAX;
	long j=i;
	long endj=i+local_arg->sampling_step-1;
	if (endj>right) endj=right;
	for (;j<=endj;j++) {
	 DATATYPE const val=tsdata(tinfoptr, local_arg->itempart, local_arg->function, channel, j);
         if (isnan(val) || isinf(val)) continue;
	 if (val<minval) { minval=val; mini=j; }
	 if (val>maxval) { maxval=val; maxi=j; }
	}
	if (mini>=0) {
	 /* This means that minval and maxval are not +-FLT_MAX any more... */
	 if (mini<maxi) {
	  vector2[0]=tinfoptr->xdata[mini];
	  vector2[1]=minval;
	  v2f(vector2);
	  vector2[0]=tinfoptr->xdata[maxi];
	  vector2[1]=maxval;
	  v2f(vector2);
	 } else {
	  vector2[0]=tinfoptr->xdata[maxi];
	  vector2[1]=maxval;
	  v2f(vector2);
	  vector2[0]=tinfoptr->xdata[mini];
	  vector2[1]=minval;
	  v2f(vector2);
	 }
	} else {
	 endline();
         bgnline();
	}
       } else {
	DATATYPE const val=tsdata(tinfoptr, local_arg->itempart, local_arg->function, channel, i);
        if (!isnan(val) && !isinf(val)) {
	 vector2[0]=tinfoptr->xdata[i];
	 vector2[1]=val;
         v2f(vector2);
	} else {
	 endline();
         bgnline();
	}
       }
      }
      endline();
     }
     setlinestyle(LINESTYLE_NORMAL);
     /*}}}  */
    }

    popviewport();
    popmatrix();
    /*}}}  */
    }

   if (local_arg->shownames) {
    /* Show the channel names at the probe positions */
    color(RED);
    if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
     cmov2(0.5, 1.0-((1.0-VERTMODE_YFRACT)/2+VERTMODE_YFRACT*((displayed_channel+0.5)/nr_of_selected_channels)));
    } else {
    cmov(thispos[0], thispos[1], thispos[2]);
    }
    charstr(entry->channelname_to_show);
   }
   displayed_channel++;
   /*}}}  */
  }
  if (have_triangulation) TriAn_display(tinfo_to_use);
#ifdef INCLUDE_DIPOLE_FIT
  /*{{{  Print dipole position*/
  if (dipole_pos[0]!=0.0) {
   color(CYAN);
   cmov(dipole_pos[0], dipole_pos[1], dipole_pos[2]);
   charstr("x");
   move(dipole_pos[0], dipole_pos[1], dipole_pos[2]);
   rdr(dipole_vec[0], dipole_vec[1], dipole_vec[2]);
  }
  /*}}}  */
#endif

  /*{{{  Show information at a fixed screen position*/
  pushmatrix();
  ortho2((Coord)0, (Coord)1, (Coord)0, (Coord)1);

  /*{{{  Comment, data range and selected value*/
  /* Look for the first selected data set and print its comment */
  if (top_tinfo!=NULL) {
   color(FOREGROUND);
   cmov2((Coord)CHAR_WIDTH/(maxx-minx), (Coord)(maxy-CHAR_HEIGHT)/(maxy-miny));
   for (i=0; top_tinfo->comment[i]!='\0'; i++) {
    stringbuffer[i]=top_tinfo->comment[i];
    stringbuffer[i+1]='\0';
    if (strwidth(stringbuffer)>=(maxx-minx)*0.5) break;
   }
   stringbuffer[i]='\0';
   charstr(stringbuffer);
   snprintf(stringbuffer, STRBUFLEN, "; %s=%g", top_tinfo->z_label, top_tinfo->z_value);
   charstr(stringbuffer);
   if (tinfo->itemsize>1) {
    snprintf(stringbuffer, STRBUFLEN, "; %c%d", local_arg->singleitems ? 'S' : '#', local_arg->itempart);
    charstr(stringbuffer);
   }
   if (local_arg->function!=TS_RAW) {
    snprintf(stringbuffer, STRBUFLEN, "; %s", tsfunction_names[local_arg->function]);
    charstr(stringbuffer);
   }
  }
  color(FOREGROUND);
#  define XPOS_C 0.065
#  define XLEN_C 0.05
#  define YPOS_C 0.05
#  define YLEN_C 0.05
  bgnline();
  move2((Coord)XPOS_C, (Coord)YPOS_C);
  rdr2((Coord)XLEN_C, (Coord)0);
  rmv2((Coord)-XLEN_C, (Coord)(-YLEN_C/2));
  rdr2((Coord)0, (Coord)YLEN_C);
  endline();
  snprintf(stringbuffer, STRBUFLEN, "%6g", xdmin);
  cmov2((Coord)(XPOS_C-((float)strwidth(stringbuffer))/(maxx-minx)), (Coord)YPOS_C);
  charstr(stringbuffer);
  cmov2((Coord)(XPOS_C+XLEN_C), (Coord)YPOS_C);
  snprintf(stringbuffer, STRBUFLEN, "%g", xdmax);
  charstr(stringbuffer);
  cmov2((Coord)XPOS_C, (Coord)(YPOS_C-YLEN_C/2));
  snprintf(stringbuffer, STRBUFLEN, "%g", local_arg->displaysign>0 ? local_arg->ydmin : local_arg->ydmax);
  charstr(stringbuffer);
  cmov2((Coord)XPOS_C, (Coord)(YPOS_C+YLEN_C/2));
  snprintf(stringbuffer, STRBUFLEN, "%g", local_arg->displaysign>0 ? local_arg->ydmax : local_arg->ydmin);
  charstr(stringbuffer);
#  undef XPOS_C
#  undef XLEN_C
#  undef YPOS_C
#  undef YLEN_C
  /*}}}  */

  if (local_arg->position_mode==POSITION_MODE_XYZ && rightleftview!=0) {
   /*{{{  Show a head view*/
   Coord tmpcurve[HEAD_OUTLINE_SIZE][3];
   float xfactor=0.1, yfactor=0.15;
#   define HEADMID_X 0.94;
#   define HEADMID_Y 0.1;

   color(FOREGROUND);
   bgnline();
   if (rightleftview==2) xfactor*= -1.0; /* Let Head look to the left */
   for (i = 0; i < HEAD_OUTLINE_SIZE; i++) {
    tmpcurve[i][0]=head_outline[i][0]*xfactor+HEADMID_X;
    tmpcurve[i][1]=head_outline[i][1]*yfactor+HEADMID_Y;
    tmpcurve[i][2]=(Coord)0;
   }
   crvn(HEAD_OUTLINE_SIZE, tmpcurve);
   endline();
#   undef HEADMID_X
#   undef HEADMID_Y
   /*}}}  */
  }

  if (!postscript_output) {
   /*{{{  Print selected data sets and the marked position*/
   Screencoord cposx, cposy;
   int max_datasets_on_screen;
   /* Print selected data sets and the marked position only if output goes 
    * to screen. First, print the dataset map at the right border: */
   cmov2((Coord)1, (Coord)1);
   getcpos(&cposx, &cposy);
   max_datasets_on_screen=cposy/CHAR_HEIGHT;
#   ifdef SGI
   cposx-=originx;
   cposy-=originy;
#   endif
   cposx-=DATASET_MAP;
   cposy-=CHAR_HEIGHT;

   if (ndatasets<=max_datasets_on_screen) {
    /* Everything fits on the screen: Start at top. */
    top_in_dataset_map=0;
   } else if (top_tinfo!=NULL) {
    int firstsel= -1, lastsel= -1;
    /* Try to show the data sets in a useful manner */
    for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
     if (dataset_is_shown(current_dataset)) {
      if (firstsel<0) firstsel=current_dataset;
      lastsel=current_dataset;
     }
    }
    if (lastsel-firstsel+1>(int)max_datasets_on_screen) {
     /* If selected data sets cannot all be shown, show the first one in any case */
     top_in_dataset_map=firstsel;
    } else {
     top_in_dataset_map=(firstsel+lastsel-max_datasets_on_screen+1)/2;
     /* Stop scrolling as we would start the page earlier as the first or stop
      * the page later than the last dataset... */
     if (top_in_dataset_map<0) top_in_dataset_map=0;
     if (top_in_dataset_map>(int)ndatasets-(int)max_datasets_on_screen) top_in_dataset_map=ndatasets-max_datasets_on_screen;
    }
   }
   for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL && current_dataset<top_in_dataset_map; current_dataset++, tinfoptr=tinfoptr->next);
   for (; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next, cposy-=CHAR_HEIGHT) {
    cmov2((Coord)cposx/(maxx-minx), (Coord)cposy/(maxy-miny));
    snprintf(stringbuffer, STRBUFLEN, "[%3g]", tinfoptr->z_value);
    color(dataset_is_shown(current_dataset) ? (dataset_is_fixed(current_dataset) ? YELLOW : GREEN) : (dataset_is_fixed(current_dataset) ? RED : FOREGROUND));
    charstr(stringbuffer);
   }
   if (local_arg->lastsel_entry!=NULL) {
    color(FOREGROUND);
    snprintf(stringbuffer, STRBUFLEN, "Marked: %s=(%g %g), ", tinfo_to_use->xchannelname, local_arg->lastselected, local_arg->yclicked);
    cmov2((Coord)(maxx-strwidth(stringbuffer)-(DATASET_MAP+10*CHAR_WIDTH))/(maxx-minx), (Coord)(maxy-CHAR_HEIGHT)/(maxy-miny));
    charstr(stringbuffer);
    getcpos(&cposx, &cposy);
#   ifdef SGI
    cposx-=originx;
    cposy-=originy;
#   endif
    for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL && current_dataset<top_in_dataset_map; current_dataset++, tinfoptr=tinfoptr->next);
    for (; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next, cposy-=CHAR_HEIGHT) {
     if (dataset_is_shown(current_dataset)) {
      int const lastsel_channel=find_channel_number_for_entry(tinfoptr, local_arg->lastsel_entry);
      cmov2((Coord)cposx/(maxx-minx), (Coord)cposy/(maxy-miny));
      if (lastsel_channel>=0) {
       snprintf(stringbuffer, STRBUFLEN, "%s=%g", local_arg->lastsel_entry->channelname_to_show, tsdata(tinfoptr, local_arg->itempart, local_arg->function, lastsel_channel, get_selectedpoint(tinfoptr)));
       charstr(stringbuffer);
      }
     }
    }
   }
   /*}}}  */
   /*{{{  Print the subsampling step indicator if necessary*/
   if (local_arg->sampling_step!=1) {
    color(GREEN);
    cmov2((Coord)CHAR_WIDTH/(maxx-minx), (Coord)(maxy-2*CHAR_HEIGHT)/(maxy-miny));
    snprintf(stringbuffer, STRBUFLEN, "%sSubsampling, step %d", (local_arg->auto_subsampling ? "Auto " : ""), local_arg->sampling_step);
    charstr(stringbuffer);
   }
   /*}}}  */
   /*{{{  Print the vertical lock indicator if necessary*/
   if (local_arg->lock_vscale!=NO_VSCALE_LOCK) {
    color(YELLOW);
    cmov2((Coord)20*CHAR_WIDTH/(maxx-minx), (Coord)(maxy-2*CHAR_HEIGHT)/(maxy-miny));
    snprintf(stringbuffer, STRBUFLEN, "Vertical scale lock: %s", vertical_scale_lock_descriptions[local_arg->lock_vscale]);
    charstr(stringbuffer);
   }
   /*}}}  */
   /*{{{  Set 'red_message' to indicators if otherwise unset*/
   if (local_arg->red_message==NULL) {
    if (local_arg->record_file!=NULL) {
     local_arg->red_message="RECORDING";
    } else if (local_arg->position_mode==POSITION_MODE_CXYZ) {
     if (local_arg->lastsel_entry!=NULL) {
      snprintf(local_arg->messagebuffer, MESSAGELEN, "Move channel %s using HJKL", local_arg->lastsel_entry->channelname_to_show);
      local_arg->red_message=local_arg->messagebuffer;
     } else {
      local_arg->red_message="Select a channel to change its position";
     }
    }
   }
   /*}}}  */
   /*{{{  Print 'red_message' if given*/
   if (local_arg->red_message!=NULL) {
    color(RED);
    cmov2((Coord)CHAR_WIDTH/(maxx-minx), (Coord)(maxy-3*CHAR_HEIGHT)/(maxy-miny));
    charstr(local_arg->red_message);
    local_arg->red_message=NULL;
   }
   /*}}}  */
   /*{{{  Print some epoch info if the user wants it*/
   if (top_tinfo!=NULL && local_arg->show_info) {
    int xpos=4;
#define DISPLAY_INFOLINE cmov2((Coord)CHAR_WIDTH/(maxx-minx), (Coord)(maxy-(xpos++)*CHAR_HEIGHT)/(maxy-miny)); charstr(stringbuffer);
    color(FOREGROUND);
    snprintf(stringbuffer, STRBUFLEN, "nr_of_channels=%d %s", top_tinfo->nr_of_channels, (top_tinfo->data_type==FREQ_DATA ? "FREQ_DATA" : (top_tinfo->multiplexed ? "MULTIPLEXED" : "NONMULTIPLEXED")));
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "nr_of_points=%d", top_tinfo->nr_of_points);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "itemsize=%d", top_tinfo->itemsize);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "sfreq=%g", top_tinfo->sfreq);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "beforetrig=%d", top_tinfo->beforetrig);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "leaveright=%d", top_tinfo->leaveright);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "nrofaverages=%d", top_tinfo->nrofaverages);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "accepted_epochs=%d", top_tinfo->accepted_epochs);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "rejected_epochs=%d", top_tinfo->rejected_epochs);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "failed_assertions=%d", top_tinfo->failed_assertions);
    DISPLAY_INFOLINE;
    snprintf(stringbuffer, STRBUFLEN, "condition=%d", top_tinfo->condition);
    DISPLAY_INFOLINE;
#undef DISPLAY_INFOLINE
    if (local_arg->lastsel_entry!=NULL && tinfo_to_use->triggers.buffer_start!=NULL && ((struct trigger *)tinfo_to_use->triggers.buffer_start)[1].code!=0) {
     /* Find if we're on a trigger */
     int const whichpoint=get_selectedpoint(tinfo_to_use);
     struct trigger *trig=(struct trigger *)tinfo_to_use->triggers.buffer_start+1;
     while (trig->code!=0) {
      if (trig->position==whichpoint) break;
      trig++;
     }
     cmov2((Coord)CHAR_WIDTH/(maxx-minx), (Coord)(maxy-12*CHAR_HEIGHT)/(maxy-miny));
     if (trig->code!=0) {
      snprintf(stringbuffer, STRBUFLEN, "trigger=%d", trig->code);
     } else {
      strcpy(stringbuffer, "trigger=NONE");
     }
     charstr(stringbuffer);
    }
   }
   /*}}}  */
  }
  popmatrix();
  /*}}}  */

  if (!postscript_output) swapbuffers();

  while (!leave) {
   /*{{{  The main event loop*/
   short val;
   if (dev!=KEYBD && !get_string) *inputbuffer='\0';	/* Mark input buffer as empty */
   if (postscript_output) {
    char * const interactive_dev=getenv("VDEVICE");
    vnewdev(interactive_dev);	/* Select interactive device again */
    /* We have to flush the event queue again, because of redraw events and such... */
    if (!local_arg->quit_mode) clear_event_queue();
    doublebuffer();
    dev=REDRAW;
    postscript_output=FALSE;
    setlocale(LC_NUMERIC, ""); /* Reset locale to environment */
   } else {
    if (local_arg->replay_file!=NULL) {
     int state=1;
     while (state!=0) {
      val=fgetc(local_arg->replay_file);
      if (state==2) {
       /* Something preceded by a backslash */
       state=0;
       switch (val) {
        case '\\':
	 dev=KEYBD;
	 break;
	case 'n':
	 val=RETKEY;
	 dev=KEYBD;
	 break;
	case 'L':
	 dev=LEFTMOUSE;
	 val=1;
	 break;
	case 'M':
	 dev=MIDDLEMOUSE;
	 val=1;
	 break;
	case 'R':
	 dev=RIGHTMOUSE;
	 val=1;
	 break;
	default:
         ERREXIT1(tinfo->emethods, "posplot: Unknown replay escape \\%c\n", MSGPARM(val));
	 continue;
       }
      } else {
       switch (val) {
	case EOF:
	 state=0;
	 break;
	case '\\':
	 state=2;
	 break;
	case '\n':
	 /* Ignore \n's */
	 state=1;
	 break;
	default:
	 dev=KEYBD;
	 state=0;
	 break;
       }
      }
     }
     if (feof(local_arg->replay_file)) {
      close_replay_file(tinfo);
      dev=REDRAW;
     }
    } else {
     if (local_arg->quit_mode) {
      dev=KEYBD;
      val='q';
     } else {
      dev=qread(&val);
     }
     if (local_arg->record_file!=NULL) {
      switch (dev) {
       case KEYBD:
        switch (val) {
	 case RETKEY:
          fprintf(local_arg->record_file, "\\n");
	  break;
	 case '\\':
          fprintf(local_arg->record_file, "\\\\");
	  break;
	 default:
          fputc(val, local_arg->record_file);
	  break;
	}
	break;
       case LEFTMOUSE:
        fprintf(local_arg->record_file, "\\L");
	break;
       case MIDDLEMOUSE:
        fprintf(local_arg->record_file, "\\M");
	break;
       case RIGHTMOUSE:
        fprintf(local_arg->record_file, "\\R");
	break;
      }
     }
    }
   }
   /* Make RETURN synonymous to a right mouse press: DOS GRX doesn't
    * handle the third button */
   if (!get_string && dev==KEYBD && val==RETKEY) {
    dev=RIGHTMOUSE; val=1;
   }
   switch (dev) {
    case KEYBD:
     /*{{{  */
     if (get_string) {
      /*{{{  Accumulate chars in inputbuffer*/
      if (val==RETKEY) {
       get_string=FALSE;
      } else {
       int inputlen=strlen(inputbuffer);
       switch (val) {
        case BACKSPACEKEY:
         if (inputlen>0) inputbuffer[inputlen-1]='\0';
         break;
        default:
         inputbuffer[inputlen]=(char)val;
         inputbuffer[inputlen+1]='\0';
         break;
       }
       if (local_arg->replay_file==NULL) show_Input_line(inputbuffer);
      }
      /*}}}  */
      break;
     }
     if (*inputbuffer!='\0') show_Input_line(inputbuffer);

     dev=REDRAW;
     if (val>='0' && val<='9' && dataset_toggle_shown(tinfo, val-'0'+(val=='0' ? 10 : 0))) {
      dev=NEWDATA; leave=TRUE;
      break;
     }
     switch (val) {
      case '(':
       /*{{{  Move all selected z ranges up by one */
       /* No redraw if nothing changed */
       dev=KEYBD;
       /* Look for the first non-fixed dataset... */ 
       for (current_dataset=0; current_dataset<ndatasets-1 && dataset_is_fixed(current_dataset); current_dataset++);
       /* ... and do nothing if it is already shown */
       if (current_dataset<ndatasets-1 && !dataset_is_shown(current_dataset)) {
	Bool something_changed=FALSE;
	for (current_dataset++; current_dataset<ndatasets; current_dataset++) {
	 if (dataset_is_shown(current_dataset) && !dataset_is_fixed(current_dataset)) {
	  int target_dataset;
	  /* Look for the target, skip fixed datasets */
	  for (target_dataset=current_dataset-1; dataset_is_fixed(target_dataset); target_dataset--);
	  dataset_is_shown(target_dataset)=TRUE;
	  dataset_is_shown(current_dataset)=FALSE;
	  something_changed=TRUE;
	 }
	}
	if (something_changed) {
	 dev=NEWDATA; leave=TRUE;
	}
       }
       /*}}}  */
       break;
      case ')':
       /*{{{  Move all selected z ranges down by one */
       /* No redraw if nothing changed */
       dev=KEYBD;
       /* Look for the last non-fixed dataset... */ 
       for (current_dataset=ndatasets-1; current_dataset>0 && dataset_is_fixed(current_dataset); current_dataset--);
       /* ... and do nothing if it is already shown */
       if (current_dataset>0 && !dataset_is_shown(current_dataset)) {
	Bool something_changed=FALSE;
	for (current_dataset--; current_dataset>=0; current_dataset--) {
	 if (dataset_is_shown(current_dataset) && !dataset_is_fixed(current_dataset)) {
	  int target_dataset;
	  /* Look for the target, skip fixed datasets */
	  for (target_dataset=current_dataset+1; dataset_is_fixed(target_dataset); target_dataset++);
	  dataset_is_shown(target_dataset)=TRUE;
	  dataset_is_shown(current_dataset)=FALSE;
	  something_changed=TRUE;
	 }
	}
	if (something_changed) {
	 dev=NEWDATA; leave=TRUE;
	}
       }
       /*}}}  */
       break;
      case '*':
       /*{{{  Invert selection of data sets*/
       for (current_dataset=0; current_dataset<ndatasets; current_dataset++) {
	if (!dataset_is_fixed(current_dataset))
	dataset_is_shown(current_dataset)= !dataset_is_shown(current_dataset);
       }
       dev=NEWDATA; leave=TRUE;
       /*}}}  */
       break;
#     ifndef SGI
      case 'o':
      case 'O': {
       char const * const postscript_outfile=(*inputbuffer!='\0' ? inputbuffer : POSTSCRIPT_OUTFILE);
       char const * const post_dev=getenv(POSTSCRIPT_ENVNAME);
       /* This sets the VOGL system up to output postscript to POSTSCRIPT_OUTFILE */
       voutput(postscript_outfile);
       if (val=='o') {
        *stringbuffer='\0';
       } else {
        strcpy(stringbuffer, "p");	/* Prefix for postscript mode */
       }
       strcat(stringbuffer, (post_dev==NULL ? POSTSCRIPT_DEFDEV : post_dev));
       vnewdev(stringbuffer);	/* Select postscript device */
       postscript_output=TRUE;
       /* Postscript output needs to use the decimal point, not comma */
       setlocale(LC_NUMERIC, "C");
       /* This message is printed only after postscript_output is off again: */
       snprintf(local_arg->messagebuffer, MESSAGELEN, "Postscript plotted to file %s", postscript_outfile);
       local_arg->red_message=local_arg->messagebuffer;
       }
       break;
#     endif
      case 'y':
      case 'Y':
       /* Dump the channel x-y screen positions and selected values to stdout */
       if (top_tinfo==NULL || local_arg->lastsel_entry==NULL) {
        dev=KEYBD;
       } else {
       int const whichpoint=get_selectedpoint(top_tinfo);
       if (val=='y') {	/* Output compatible with array_dump */
        snprintf(stringbuffer, STRBUFLEN, "# ARRAY_DUMP ASCII\n# 3 %d\n", top_tinfo->nr_of_channels);
	TRACEMS(tinfo->emethods, -1, stringbuffer);
       }
       displayed_channel=0;
       entry=(channel_map_entry *)local_arg->channel_map.buffer_start;
       for (entry_number=0; entry_number<local_arg->channel_map.current_length/sizeof(channel_map_entry); entry_number++, entry++) {
        const double *thispos=entry->pos;
        Screencoord newminx, newminy;
        if (!entry->selected
          ||(channel=find_channel_number_for_entry(top_tinfo, entry))<0) 
	 continue;
	if (local_arg->position_mode==POSITION_MODE_VERTICAL) {
	 newminx=(Screencoord)(minx+((1.0-VERTMODE_XFRACT)/2)*(maxx-minx));
	 newminy=(Screencoord)(maxy-(((1.0-VERTMODE_YFRACT)/2)+VERTMODE_YFRACT*((displayed_channel+1.0)/nr_of_selected_channels))*(maxy-miny));
	} else {
        if (local_arg->position_mode==POSITION_MODE_OVERLAY) thispos=nullpos;
        cmov(thispos[0], thispos[1], thispos[2]);
        getcpos(&newminx, &newminy);
#     ifdef SGI
        newminx-=originx;
        newminy-=originy;
#     endif
	}
        if (val=='Y') {
         snprintf(stringbuffer, STRBUFLEN, "%s: %d %d = %g\n", top_tinfo->channelnames[channel], newminx, newminy, tsdata(top_tinfo, local_arg->itempart, local_arg->function, channel, whichpoint));
        } else {
         snprintf(stringbuffer, STRBUFLEN, "%d %d %g\n", newminx, newminy, tsdata(top_tinfo, local_arg->itempart, local_arg->function, channel, whichpoint));
        }
	TRACEMS(tinfo->emethods, -1, stringbuffer);
	displayed_channel++;
       }
       }
       break;
      case 'v':
       if (local_arg->lock_vscale!=NO_VSCALE_LOCK && *inputbuffer=='\0') {	/* Unlock vert. scale on second press of 'v' */
	if (local_arg->lock_vscale==INCREMENTAL_VSCALE_LOCK) {
         local_arg->lock_vscale=FIXED_VSCALE_LOCK;
	} else {
         local_arg->lock_vscale=NO_VSCALE_LOCK;
	}
        dev=NEWDATA; leave=TRUE;
        break;
       }
       local_arg->lock_vscale=INCREMENTAL_VSCALE_LOCK;
       if (*inputbuffer!='\0') {
	/* The check for 'i' is for compatibility with earlier behavior */
     	if (*inputbuffer!='i') {
     	 char *endpointer;
         local_arg->lock_vscale=FIXED_VSCALE_LOCK;	/* Don't recalculate scales automatically */
     	 local_arg->ydmin=strtod(inputbuffer, &endpointer);
     	 local_arg->ydmax=atof(endpointer+1);
        }
       }
       break;
      case 'p':
       local_arg->showplots=!local_arg->showplots;
       break;
      case 'x':
       local_arg->showcoordsys=!local_arg->showcoordsys;
       break;
      case 'n':
       local_arg->shownames=!local_arg->shownames;
       break;
      case 'd':
       local_arg->showdist=!local_arg->showdist;
       lastsel_changed=local_arg->showdist;
       break;
      case '_':
       if (strcmp(inputbuffer, "change")==0) {
        local_arg->position_mode=POSITION_MODE_CXYZ;
	/* Don't annoy by recalculating the viewing distance now... */
	dev=NEWDATA; 
       } else {
        local_arg->position_mode=(enum position_modes)((local_arg->position_mode+1)%NR_OF_POSITION_MODES);
	dev=NEWBORDER; 
       }
       snprintf(local_arg->messagebuffer, MESSAGELEN, "Entered %s position mode", position_mode_names[local_arg->position_mode+1]);
       local_arg->red_message=local_arg->messagebuffer;
       leave=TRUE;
       break;
      case 'b':
       if (BACKGROUND==BLACK) {
	background_white();
       } else {
	background_black();
       }
       break;
      case 'u':
       local_arg->linestyle_type=(enum linestyles)(local_arg->linestyle_type+1);
       if (local_arg->linestyle_type==LST_TYPE_LAST) local_arg->linestyle_type=0;
       local_arg->linestyle_offset=0;
       snprintf(local_arg->messagebuffer, MESSAGELEN, "Entered %s linestyle mode", linestyle_types[local_arg->linestyle_type]);
       local_arg->red_message=local_arg->messagebuffer;
       break;
      case 'U':
       for (current_dataset=0; current_dataset<ndatasets && !dataset_is_shown(current_dataset); current_dataset++);
       if (current_dataset<ndatasets) {
	switch (local_arg->linestyle_type) {
	 case LST_TYPE_DASHED:
	  local_arg->linestyle_offset=LINESTYLECOUNT+1-(current_dataset%(LINESTYLECOUNT+1));
	  break;
	 case LST_TYPE_COLOR:
	  local_arg->linestyle_offset=LINECOLORCOUNT-(current_dataset%LINECOLORCOUNT);
	  break;
	 default:
	  break;
	}
       }
       break;
      case 'm':
       local_arg->showmarker=!local_arg->showmarker;
       if (local_arg->showmarker) {
        local_arg->red_message="Showing marker.";
       } else {
        local_arg->red_message="Not showing marker.";
       }
       break;
      case 'M':
       switch(inputbuffer[0]) {
	case 'c':
	 local_arg->colored_triggers=TRUE;
	 break;
	case 'u':
	 local_arg->colored_triggers=FALSE;
	 break;
	default:
	 local_arg->showtriggers=!local_arg->showtriggers;
	 break;
       }
       if (local_arg->showtriggers) {
	if (local_arg->colored_triggers) {
         local_arg->red_message="Showing colored triggers.";
	} else {
         local_arg->red_message="Showing triggers.";
	}
       } else {
        local_arg->red_message="Not showing triggers.";
       }
       break;
      case 'N':
       /* Define a new or remove an existing trigger */
       if (local_arg->lastsel_entry!=NULL) {
	/* First find out whether the marker is placed on an existing trigger */
        int const whichpoint=get_selectedpoint(tinfo_to_use);
        struct trigger *intrig=(struct trigger *)tinfo_to_use->triggers.buffer_start;
	if (intrig!=NULL) {
	 for (intrig++; intrig->code!=0 && intrig->position!=whichpoint; intrig++);
	}
	if (intrig!=NULL && intrig->code!=0) {
	 snprintf(local_arg->messagebuffer, MESSAGELEN, "Deleted trigger at %ld code %d", intrig->position, intrig->code);
	 /* Delete a marker */
	 free_pointer((void *)&intrig->description);
	 do {
	  intrig++;
	  *(intrig-1)= *intrig;
	 } while (intrig->code!=0);
	 tinfo_to_use->triggers.current_length-=sizeof(struct trigger);
	} else {
	 int const code=(*inputbuffer!='\0' ? atoi(inputbuffer) : 1);
	 if (code!=0) {
	  if (tinfo_to_use->triggers.buffer_start==NULL) {
	   /* Create file position entry */
	   push_trigger(&tinfo_to_use->triggers, 0L, -1, NULL);
	  } else {
	   /* Delete the end marker */
	   tinfo_to_use->triggers.current_length-=sizeof(struct trigger);
	  }
	  push_trigger(&tinfo_to_use->triggers, whichpoint, code, NULL);
	  push_trigger(&tinfo_to_use->triggers, 0L, 0, NULL); /* End of list */
	  snprintf(local_arg->messagebuffer, MESSAGELEN, "Inserted trigger code %d at %d", code, whichpoint);
	 } else {
	  strcpy(local_arg->messagebuffer,"Trigger code 0 not inserted.");
	 }
	}
	local_arg->red_message=local_arg->messagebuffer;
       }
       break;
      case '\'':
       local_arg->singleitems=!local_arg->singleitems;
       dev=NEWDATA; leave=TRUE;
       break;
      case '+':
       local_arg->distance-=local_arg->distance/10;
      case '>':
       local_arg->plfact+=local_arg->plfact/10;
       break;
      case '-':
       local_arg->distance+=local_arg->distance/10;
      case '<':
       local_arg->plfact-=local_arg->plfact/10;
       break;
      case '~':
       local_arg->displaysign*= -1.0;
       break;
      case 'c':
       if (*inputbuffer!='\0') {
	/* Set the condition code */
	tinfo_to_use->condition=atoi(inputbuffer);
	snprintf(local_arg->messagebuffer, MESSAGELEN, "Epoch condition code set to %d", tinfo_to_use->condition);
	local_arg->red_message=local_arg->messagebuffer;
       } else {
       local_arg->fov=200;
       local_arg->twist=local_arg->azimuth=local_arg->incidence=0;
       local_arg->distance=local_arg->orgdistance;
       local_arg->plfact=0.8;
       local_arg->center_pos[0]=(xposmax+xposmin)/2;
       local_arg->center_pos[1]=(yposmax+yposmin)/2;
       local_arg->center_pos[2]=(zposmax+zposmin)/2;
       local_arg->red_message="Cleared viewing parameters.";
       }
       break;
      case 'C':
       local_arg->lowxboundary= -HUGE_VAL; 
       local_arg->highxboundary= HUGE_VAL;
       clear_channel_map(local_arg);
       /* Use tinfo_to_use as starting point for the channel ordering: */
       add_channel_map_entries(local_arg, tinfo_to_use);
       local_arg->red_message="Channel map was reset.";
       dev=NEWBORDER; leave=TRUE;
       break;
      case 'Z':
       if (local_arg->lastsel_entry!=NULL) {
     	local_arg->center_pos[0]=local_arg->lastsel_entry->pos[0];
     	local_arg->center_pos[1]=local_arg->lastsel_entry->pos[1];
     	local_arg->center_pos[2]=local_arg->lastsel_entry->pos[2];
	snprintf(local_arg->messagebuffer, MESSAGELEN, "Centered channel %s.", local_arg->lastsel_entry->channelname_to_show);
	local_arg->red_message=local_arg->messagebuffer;
       }
       break;
      case 'z':
       if (have_triangulation) {
	have_triangulation=FALSE;
       } else {
	if (tinfo_to_use->nr_of_channels<3) {
         snprintf(local_arg->messagebuffer, MESSAGELEN, "Need at least 3 channels for triangulation!");
         local_arg->red_message=local_arg->messagebuffer;
	} else {
	 TriAn_init(tinfo_to_use);
	 have_triangulation=TRUE;
	}
       }
       break;
      case 'i': {
       int const left=first_x(tinfo_to_use), right=last_x(tinfo_to_use);
       int whichpoint=(local_arg->lastsel_entry==NULL ? left : get_selectedpoint(tinfo_to_use));
       if (local_arg->showtriggers && tinfo_to_use->triggers.buffer_start!=NULL && ((struct trigger *)tinfo_to_use->triggers.buffer_start)[1].code!=0) {
	struct trigger * const trigstart=(struct trigger *)tinfo_to_use->triggers.buffer_start+1;
	/* Jump to the next trigger */
	struct trigger *mintrig=find_trigger_after(trigstart,whichpoint,right); 
	if (mintrig==NULL) mintrig=find_trigger_after(trigstart,left-1,right);
	if (mintrig==NULL) {
	 whichpoint= -1;
	} else {
	 whichpoint=mintrig->position;
	 if (mintrig->description==NULL) {
	  snprintf(local_arg->messagebuffer, MESSAGELEN, "Trigger code %d", mintrig->code);
	 } else {
	  snprintf(local_arg->messagebuffer, MESSAGELEN, "Trigger code %d \"%s\"", mintrig->code, mintrig->description);
	 }
	 local_arg->red_message=local_arg->messagebuffer;
	}
       } else {
	/* Jump to the next data point */
	if (local_arg->lastsel_entry!=NULL) {
	 if (whichpoint+local_arg->sampling_step<=left+((right-left)/local_arg->sampling_step)*local_arg->sampling_step) {
	  whichpoint+=local_arg->sampling_step;
	 } else {
	  whichpoint= -1;
	 }
	}
       }
       local_arg->lastsel_pos=whichpoint;
       if (whichpoint<0) {
	local_arg->lastsel_entry=NULL;
       } else {
	if (local_arg->lastsel_entry==NULL) local_arg->lastsel_entry=get_first_selected_channel(local_arg);
	local_arg->lastselected=tinfo_to_use->xdata[whichpoint];
       }
       }
       lastsel_changed=TRUE;
       dev=KEYBD;
       break;
      case 'I': {
       int const left=first_x(tinfo_to_use), right=last_x(tinfo_to_use);
       int whichpoint=(local_arg->lastsel_entry==NULL ? left+((right-left)/local_arg->sampling_step)*local_arg->sampling_step : get_selectedpoint(tinfo_to_use));
       if (local_arg->showtriggers && tinfo_to_use->triggers.buffer_start!=NULL && ((struct trigger *)tinfo_to_use->triggers.buffer_start)[1].code!=0) {
	struct trigger * const trigstart=(struct trigger *)tinfo_to_use->triggers.buffer_start+1;
	/* Jump to the previous trigger */
	struct trigger *mintrig=find_trigger_before(trigstart,whichpoint,left); 
	if (mintrig==NULL) mintrig=find_trigger_before(trigstart,right+1,left);
	if (mintrig==NULL) {
	 whichpoint= -1;
	} else {
	 whichpoint=mintrig->position;
	 if (mintrig->description==NULL) {
	  snprintf(local_arg->messagebuffer, MESSAGELEN, "Trigger code %d", mintrig->code);
	 } else {
	  snprintf(local_arg->messagebuffer, MESSAGELEN, "Trigger code %d \"%s\"", mintrig->code, mintrig->description);
	 }
	 local_arg->red_message=local_arg->messagebuffer;
	}
       } else {
	/* Jump to the previous data point */
	if (local_arg->lastsel_entry!=NULL) {
	 if (whichpoint-local_arg->sampling_step>=left) {
	  whichpoint-=local_arg->sampling_step;
	 } else {
	  whichpoint= -1;
	 }
	}
       }
       local_arg->lastsel_pos=whichpoint;
       if (whichpoint<0) {
	local_arg->lastsel_entry=NULL;
       } else {
	if (local_arg->lastsel_entry==NULL) local_arg->lastsel_entry=get_first_selected_channel(local_arg);
	local_arg->lastselected=tinfo_to_use->xdata[whichpoint];
       }
       }
       lastsel_changed=TRUE;
       dev=KEYBD;
       break;
      case '[':
       if (*inputbuffer!='\0') {
        /*{{{  Set lowxboundary using the input buffer*/
        if (*inputbuffer=='#') {
	 i=gettimeslice(tinfo_to_use, inputbuffer+1);
	 if (i>=0 && i<tinfo_to_use->nr_of_points) {
          local_arg->lowxboundary=tinfo_to_use->xdata[i];
	 }
        } else {
	 local_arg->lowxboundary=(DATATYPE)atof(inputbuffer);
        }
        /*}}}  */
        dev=NEWDATA; leave=TRUE;
       } else if (local_arg->lastsel_entry!=NULL) {
	/* If lowxboundary is already at the mark, we assume that the
	 * user instead wants to maximally enlarge the view in that direction */
        if (local_arg->lowxboundary==local_arg->lastselected) {
         local_arg->lowxboundary= -HUGE_VAL;
	} else {
         local_arg->lowxboundary=local_arg->lastselected;
	}
        dev=NEWDATA; leave=TRUE;
       } else dev=KEYBD;
       break;
      case ']':
       if (*inputbuffer!='\0') {
        /*{{{  Set highxboundary using the input buffer*/
        if (*inputbuffer=='#') {
	 i=gettimeslice(tinfo_to_use, inputbuffer+1);
	 if (i>=0 && i<tinfo_to_use->nr_of_points) {
          local_arg->highxboundary=tinfo_to_use->xdata[i];
	 }
        } else {
	 local_arg->highxboundary=(DATATYPE)atof(inputbuffer);
        }
        /*}}}  */
        dev=NEWDATA; leave=TRUE;
       } else if (local_arg->lastsel_entry!=NULL) {
	if (local_arg->highxboundary==local_arg->lastselected) {
	 local_arg->highxboundary=HUGE_VAL;
	} else {
         local_arg->highxboundary=local_arg->lastselected;
	}
        dev=NEWDATA; leave=TRUE;
       } else dev=KEYBD;
       break;
      case '{':
       if (local_arg->itempart>0) {
        local_arg->itempart--;
        dev=NEWDATA; leave=TRUE;
       } else dev=KEYBD;
       break;
      case '}':
       if (tinfo_to_use->itemsize>local_arg->itempart+1) {
        local_arg->itempart++;
        dev=NEWDATA; leave=TRUE;
       } else dev=KEYBD;
       break;
      case 'P':
       local_arg->function=(local_arg->function==TS_POWER ? TS_RAW : TS_POWER);
       dev=NEWDATA; leave=TRUE;
       break;
      case 'a':
       local_arg->function=(local_arg->function==TS_ABS ? TS_RAW : TS_ABS);
       dev=NEWDATA; leave=TRUE;
       break;
      case 'A':
       local_arg->function=(local_arg->function==TS_PHASE ? TS_RAW : TS_PHASE);
       dev=NEWDATA; leave=TRUE;
       break;
      case 'e':
       switch(local_arg->function) {
	case TS_EXP:
	case TS_EXP10:
	 local_arg->function=(enum tsdata_functions)(local_arg->function+2);
	 break;
	case TS_EXPDB:
	 local_arg->function=TS_RAW;
	 break;
	default:
	 local_arg->function=TS_EXP;
	 break;
       }
       dev=NEWDATA; leave=TRUE;
       break;
      case 'E':
       switch(local_arg->function) {
	case TS_LOG:
	case TS_LOG10:
	 local_arg->function=(enum tsdata_functions)(local_arg->function+2);
	 break;
	case TS_LOGDB:
	 local_arg->function=TS_RAW;
	 break;
	default:
	 local_arg->function=TS_LOG;
	 break;
       }
       dev=NEWDATA; leave=TRUE;
       break;
      case '?':
       local_arg->show_info=(local_arg->show_info==FALSE);
       break;
#ifdef INCLUDE_DIPOLE_FIT
      case 'F':
       if (top_tinfo!=NULL && local_arg->lastsel_entry!=NULL) {
       struct do_leastsquares_args *fixedargsp;
       DATATYPE hold;
       growing_buf buf;
       growing_buf_init(&buf);
       snprintf(stringbuffer, STRBUFLEN, "%ld", get_selectedpoint(top_tinfo));
       growing_buf_takethis(&buf, stringbuffer);

       org_methods_ptr=top_tinfo->methods;	/* Don't modify ->methods */
       top_tinfo->methods= &swap_methods_struct;
       select_dip_fit(top_tinfo);
       setup_method(top_tinfo, &buf);
       growing_buf_free(&buf);
       (*top_tinfo->methods->transform_init)(top_tinfo);
       (*top_tinfo->methods->transform)(top_tinfo);
       fixedargsp= &((struct dip_fit_storage *)top_tinfo->methods->local_storage)->fixedargs;
       /*{{{  Extract position*/
       array_reset(&fixedargsp->position);
       for (i=0; i<3; i++) {
        hold=array_scan(&fixedargsp->position);
        dipole_pos[i]=hold/100;
       }
       /*}}}  */
       /*{{{  Extract momentum*/
       array_reset(&fixedargsp->x3d);
       hold=array_abs(&fixedargsp->x3d);
       for (i=0; i<3; i++) {
        dipole_vec[i]=array_scan(&fixedargsp->x3d)/hold/33.3;	/* Scale to 3cm */
       }
       /*}}}  */
       (*top_tinfo->methods->transform_exit)(top_tinfo);
       top_tinfo->methods=org_methods_ptr;
       }
       break;
#endif
      case 'D':
       for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
	if (!dataset_is_shown(current_dataset)) continue;
	org_methods_ptr=tinfoptr->methods;
	tinfoptr->methods= &swap_methods_struct;
	select_detrend(tinfoptr);
	setup_method(tinfoptr, NULL);
	(*tinfoptr->methods->transform_init)(tinfoptr);
	(*tinfoptr->methods->transform)(tinfoptr);
	(*tinfoptr->methods->transform_exit)(tinfoptr);
	tinfoptr->methods=org_methods_ptr;
       }
       local_arg->red_message="Selected data sets detrended.";
       dev=NEWDATA; leave=TRUE;
       break;
      case 'W': {
       char const * const writeasc_outfile=(*inputbuffer!='\0' ? inputbuffer : WRITEASC_OUTFILE);
       int n_written=0;
       growing_buf buf;
       growing_buf_init(&buf);
       snprintf(stringbuffer, STRBUFLEN, "-a -b %s", writeasc_outfile);
       growing_buf_takethis(&buf, stringbuffer);
       org_methods_ptr=tinfo->methods;
       tinfo->methods= &swap_methods_struct;
       select_writeasc(tinfo);
       setup_method(tinfo, &buf);
       growing_buf_free(&buf);
       (*tinfo->methods->transform_init)(tinfo);
       tinfo->methods=org_methods_ptr;
       for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
	/* Save all datasets if none is selected, or only the selected datasets */
        if (top_tinfo!=NULL && !dataset_is_shown(current_dataset)) continue;
        org_methods_ptr=tinfoptr->methods;
        tinfoptr->methods= &swap_methods_struct;
        (*tinfoptr->methods->transform)(tinfoptr);
        tinfoptr->methods=org_methods_ptr;
	n_written++;
       }
       org_methods_ptr=tinfo->methods;
       tinfo->methods= &swap_methods_struct;
       (*tinfo->methods->transform_exit)(tinfo);
       tinfo->methods=org_methods_ptr;
       snprintf(local_arg->messagebuffer, MESSAGELEN, "%d dataset%s written to %s", n_written, (n_written==1 ? "" : "s"), writeasc_outfile);
       local_arg->red_message=local_arg->messagebuffer;
       }
       break;
      case '$':
       {
       int whichpoint;
       growing_buf buf;
       growing_buf_init(&buf);
       if (local_arg->lastsel_entry==NULL) local_arg->lastselected=local_arg->lowxboundary;

       for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
	if (!dataset_is_shown(current_dataset)) continue;
	whichpoint=get_selectedpoint(tinfoptr);
	snprintf(stringbuffer, STRBUFLEN, "-o %d", whichpoint);
	growing_buf_takethis(&buf, stringbuffer);
	org_methods_ptr=tinfoptr->methods;
	tinfoptr->methods= &swap_methods_struct;
	select_detrend(tinfoptr);
	setup_method(tinfoptr, &buf);
	(*tinfoptr->methods->transform_init)(tinfoptr);
	(*tinfoptr->methods->transform)(tinfoptr);
	(*tinfoptr->methods->transform_exit)(tinfoptr);
	tinfoptr->methods=org_methods_ptr;
       }
       growing_buf_free(&buf);
       local_arg->red_message="Current point value subtracted from selected datasets.";
       dev=NEWDATA; leave=TRUE;
       }
       break;
      case '&':
       for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
	if (!dataset_is_shown(current_dataset)) continue;
	org_methods_ptr=tinfoptr->methods;
	tinfoptr->methods= &swap_methods_struct;
	select_integrate(tinfoptr);
	setup_method(tinfoptr, NULL);
	(*tinfoptr->methods->transform_init)(tinfoptr);
	(*tinfoptr->methods->transform)(tinfoptr);
	(*tinfoptr->methods->transform_exit)(tinfoptr);
	tinfoptr->methods=org_methods_ptr;
       }
       local_arg->red_message="Selected data sets integrated.";
       dev=NEWDATA; leave=TRUE;
       break;
      case '%':
       for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
	if (!dataset_is_shown(current_dataset)) continue;
        org_methods_ptr=tinfoptr->methods;
	tinfoptr->methods= &swap_methods_struct;
	select_differentiate(tinfoptr);
	setup_method(tinfoptr, NULL);
	(*tinfoptr->methods->transform_init)(tinfoptr);
	(*tinfoptr->methods->transform)(tinfoptr);
	(*tinfoptr->methods->transform_exit)(tinfoptr);
	tinfoptr->methods=org_methods_ptr;
       }
       local_arg->red_message="Selected data sets differentiated.";
       dev=NEWDATA; leave=TRUE;
       break;
      case 'S':
       if (swaptinfo==NULL) {
        tinfo=swap_xz(swaptinfo=tinfo);
       } else {
        tinfoptr=tinfo;
        tinfo=swaptinfo;
        swaptinfo=tinfoptr;
       }
       local_arg->lowxboundary= -HUGE_VAL; 
       local_arg->highxboundary= HUGE_VAL;
       dev=NEWDATA; leave=TRUE;
       break;
      case 's': {
       growing_buf buf;
       growing_buf_init(&buf);
       growing_buf_takethis(&buf, "-d");
       for (current_dataset=0, tinfoptr=tinfo; tinfoptr!=NULL; current_dataset++, tinfoptr=tinfoptr->next) {
	if (!dataset_is_shown(current_dataset)) continue;
	org_methods_ptr=tinfoptr->methods;
	tinfoptr->methods= &swap_methods_struct;
	select_normalize_channelbox(tinfoptr);
	setup_method(tinfoptr, &buf);
	(*tinfoptr->methods->transform_init)(tinfoptr);
	(*tinfoptr->methods->transform)(tinfoptr);
	(*tinfoptr->methods->transform_exit)(tinfoptr);
	tinfoptr->methods=org_methods_ptr;
       }
       growing_buf_free(&buf);
       rightleftview=(*((Bool*)swap_methods_struct.local_storage) ? 2:1);
       dev=NEWBORDER; leave=TRUE;
       }
       break;
      case 'G':
       free_pointer((void **)&tinfo_to_use->probepos);
       if (*inputbuffer!='\0') {
        create_channelgrid_ncols(tinfo_to_use, atoi(inputbuffer));
       } else {
        create_channelgrid(tinfo_to_use);
       }
       clear_channel_map(local_arg);
       /* Ensure that these gridded positions are used in the channel map: */
       add_channel_map_entries(local_arg, tinfo_to_use);
       local_arg->red_message="Channels arranged on a grid.";
       dev=NEWBORDER; leave=TRUE;
       break;
      case '/':
       if (local_arg->fov<1600) local_arg->fov+=200;
       break;
      case '\\':
       if (local_arg->fov>200) local_arg->fov-=200;
       break;
      case 'h':
       local_arg->azimuth+=(Angle)22.5;
       break;
      case 'j':
       local_arg->incidence+=(Angle)22.5;
       break;
      case 'k':
       local_arg->incidence-=(Angle)22.5;
       break;
      case 'l':
       local_arg->azimuth-=(Angle)22.5;
       break;
      case 'H':
       if (local_arg->position_mode==POSITION_MODE_CXYZ) {
	move_channel_xy(tinfo, -1, 0);
	break;
       }
       local_arg->azimuth+=180;
       break;
      case 'J':
       switch (local_arg->position_mode) {
	case POSITION_MODE_CXYZ:
	 move_channel_xy(tinfo, 0, -1);
	 break;
	case POSITION_MODE_VERTICAL:
         if (local_arg->lastsel_entry!=NULL) {
	  local_arg->lastsel_entry=move_entry_down(local_arg, local_arg->lastsel_entry);
	 }
	 break;
	default:
         local_arg->incidence+=180;
	 break;
       };
       break;
      case 'K':
       switch (local_arg->position_mode) {
	case POSITION_MODE_CXYZ:
	 move_channel_xy(tinfo, 0, 1);
	 break;
	case POSITION_MODE_VERTICAL:
         if (local_arg->lastsel_entry!=NULL) {
	  local_arg->lastsel_entry=move_entry_up(local_arg, local_arg->lastsel_entry);
	 }
	 break;
	default:
         local_arg->incidence-=180;
	 break;
       };
       break;
      case 'L':
       if (local_arg->position_mode==POSITION_MODE_CXYZ) {
	move_channel_xy(tinfo, 1, 0);
	break;
       }
       local_arg->azimuth-=180;
       break;
      case 't':
       local_arg->twist+=(Angle)22.5;
       break;
      case 'T':
       local_arg->twist-=(Angle)22.5;
       break;
      case 'r':
       if (local_arg->record_file!=NULL) {
        fputc('\n', local_arg->record_file);
	close_record_file(tinfo);
       } else if (local_arg->replay_file!=NULL) {
        close_replay_file(tinfo);
       } else {
	char const * const filename=(*inputbuffer!='\0' ? inputbuffer : RECORD_FILENAME);
	if (open_record_file(tinfo, filename)==NULL) {
         dev=REDRAW;
        }
       }
       break;
      case 'R':
       dev=KEYBD;
       if (local_arg->replay_file!=NULL) {
        close_replay_file(tinfo);
       } else if (local_arg->record_file!=NULL) {
        local_arg->red_message="Replay not possible while recording!";
       } else {
	char const * const filename=(*inputbuffer!='\0' ? inputbuffer : RECORD_FILENAME);
	if (open_replay_file(tinfo, filename)==NULL) {
	 dev=REDRAW;
        }
       }
       break;
      case '=':	/* Initialize gathering of a new string in inputbuffer */
       get_string=TRUE;
       *inputbuffer='\0';
       if (local_arg->replay_file==NULL) show_Input_line(inputbuffer);
       dev=KEYBD;
       break;
      case '.':
       if (*inputbuffer!='\0') {
        /*{{{  Set lastselected using the input buffer*/
        if (*inputbuffer=='#') {
	 i=gettimeslice(tinfo_to_use, inputbuffer+1);
	 if (i>=0 && i<tinfo_to_use->nr_of_points) {
          local_arg->lastselected=tinfo_to_use->xdata[i];
          local_arg->lastsel_pos=i;
	 }
        } else {
	 local_arg->lastselected=(DATATYPE)atof(inputbuffer);
         local_arg->lastsel_pos=find_pointnearx(tinfo_to_use, local_arg->lastselected);
        }
	if (local_arg->lastsel_entry==NULL) local_arg->lastsel_entry=get_first_selected_channel(local_arg);
        lastsel_changed=TRUE;
        /*}}}  */
        dev=KEYBD;
       } else {
        local_arg->red_message="Please enter the argument first.";
       }
       break;
      case ',':
       if (*inputbuffer!='\0') {
        /*{{{  Select dataset using the input buffer*/
        if (*inputbuffer=='#') {
         current_dataset=atoi(inputbuffer+1)-1;
        } else {
         DATATYPE guessz;
         guessz=(DATATYPE)atof(inputbuffer);
         for (current_dataset=0, tinfoptr=tinfo; tinfoptr->z_value<guessz && tinfoptr->next!=NULL; current_dataset++, tinfoptr=tinfoptr->next);
         if (tinfoptr->previous!=NULL && guessz-tinfoptr->previous->z_value<tinfoptr->z_value-guessz) current_dataset--;
        }
	if (inputbuffer[strlen(inputbuffer)-1]=='F') {
	 dataset_is_fixed(current_dataset)= !dataset_is_fixed(current_dataset);
	} else {
	 dataset_is_shown(current_dataset)= !dataset_is_shown(current_dataset);
	}
        dev=NEWDATA; leave=TRUE;
        /*}}}  */
       } else {
        local_arg->red_message="Please enter the argument first.";
       }
       break;
      case '@':
       if (*inputbuffer!='\0') {
        /*{{{  Set the sampling step using the input buffer*/
        /* Note: Subsampling will only affect data display. Undisplayed points may
         * be selected by pointing or #input anyway. */
	char *endptr;
	i=strtol(inputbuffer,&endptr,10);
	if (*endptr=='\0' && i>0) local_arg->sampling_step=i;
	local_arg->auto_subsampling=FALSE;
        /*}}}  */
       } else {
        /*{{{  Toggle auto-subsampling*/
	if (local_arg->auto_subsampling) {
         local_arg->sampling_step=1;
	 local_arg->auto_subsampling=FALSE;
	} else {
	 local_arg->auto_subsampling=TRUE;
	}
        /*}}}  */
       }
       dev=NEWDATA; leave=TRUE;
       break;
      case BACKSPACEKEY:
       /* Toggle channel selection */
       if (*inputbuffer!='\0') {
        if (*inputbuffer=='#') {
	 char *endptr;
	 channel=strtol(inputbuffer+1,&endptr,10);
	 if (*endptr=='\0' && channel>=0 && channel<tinfo_to_use->nr_of_channels) {
          entry=locate_channel_map_entry_with_position(local_arg, tinfo_to_use, channel);
	 }
	} else {
         entry=locate_channel_map_entry(local_arg, inputbuffer);
	}
       } else {
	mousex=getvaluator(MOUSEX);
	mousey=getvaluator(MOUSEY);
	entry=which_channel_near(tinfo_to_use, mousex, mousey, TRUE);
       }
       if (entry==NULL || (entry->selected && nr_of_selected_channels<=1)) break;
       entry->selected=!entry->selected;
       dev=NEWBORDER; leave=TRUE;
       break;
      case 12:	/* CTRL-L */
       dev=NEWDATA; leave=TRUE;
       break;
      case 'X':
       if (swaptinfo!=NULL) {
	if (tinfo!=orig_tinfo) {
	 /* Swap swaptinfo and tinfo; assuming swaptinfo==orig_tinfo */
	 tinfoptr=tinfo;
	 tinfo=swaptinfo;
	 swaptinfo=tinfoptr;
	}
	swap_xz_free(swaptinfo);
       }
       if (tinfo->data_type==FREQ_DATA) {
	free_pointer((void **)&tinfo->next);
       }
       ERREXIT(tinfo->emethods, "posplot: Terminated by user.\n");
       break;
      case 'V':
       /* Quit and stop iterated queue */
       tinfo->stopsignal=TRUE;
      case 'Q':
       /* Quit and return NULL: reject epoch (in avg_q) */
       return_null=TRUE;
      case 'q':
       leave=TRUE;
      default:
       dev=KEYBD;
       break;
     }
     /*}}}  */
     if (dev!=REDRAW) break;
    case REDRAW:
     leave=TRUE;
     break;
    case LEFTMOUSE:
    case MIDDLEMOUSE:
    case RIGHTMOUSE:
     /*{{{  */
     if (val==0) break;
     if (local_arg->replay_file!=NULL) {
      if (fscanf(local_arg->replay_file, "%hd %hd", &mousex, &mousey)!=2) break;
     } else {
      mousex=getvaluator(MOUSEX);
      mousey=getvaluator(MOUSEY);
      if (local_arg->record_file!=NULL) fprintf(local_arg->record_file, " %d %d\n", mousex, mousey);
      /* VOGL places lots of on/off events in the queue as long as you press the
       * button: gobble them up */
      if (!local_arg->quit_mode) clear_event_queue();
     }
     if (mousex-originx>maxx-DATASET_MAP && mousex-originx<=maxx) {
      /*{{{  Handle clicks into the dataset map on the right of the screen */
      Bool retval=FALSE;

      current_dataset=(maxy-mousey+originy)/CHAR_HEIGHT+top_in_dataset_map+1;
      switch (dev) {
       case LEFTMOUSE:
        retval=dataset_toggle_shown(tinfo, current_dataset);
	break;
       case MIDDLEMOUSE:
	break;
       case RIGHTMOUSE:
        retval=dataset_toggle_fixed(tinfo, current_dataset);
	break;
      }
      if (retval) {
       dev=NEWDATA; leave=TRUE;
      }
      break;
      /*}}}  */
     }

     if (*inputbuffer!='\0') {
      if (*inputbuffer=='#') {
       char *endptr;
       channel=strtol(inputbuffer+1,&endptr,10);
       if (*endptr=='\0' && channel>=0 && channel<tinfo_to_use->nr_of_channels) {
	entry=locate_channel_map_entry_with_position(local_arg, tinfo_to_use, channel);
       } else {
	entry=NULL;
	*inputbuffer='\0';
       }
      } else {
       entry=locate_channel_map_entry(local_arg, inputbuffer);
      }
     } else {
      if (dev==RIGHTMOUSE) {
       entry=which_channel_near(tinfo_to_use, mousex, mousey, FALSE);
      } else {
       entry=which_channel_below(tinfo_to_use, mousex, mousey, plotlength, plotheight, xdmin, xdmax, local_arg->ydmin, local_arg->ydmax, &i, &local_arg->yclicked, FALSE);
      }
     }
     if (entry==NULL) break;
     /* In any event, the channel selection is changed to the channel
      * last clicked at; This also makes sense when switching to a single
      * channel display: The shown value is automatically the one of the
      * current channel.
      */
     local_arg->lastsel_entry=entry; lastsel_changed=TRUE;
     if (dev==LEFTMOUSE) {
      /*{{{  Select this point*/
      local_arg->lastselected=tinfo_to_use->xdata[i];
      /*}}}  */
     } else if (dev==MIDDLEMOUSE) {
      /*{{{  Output the value of the selected channel at the selected point*/
#define BUF_SIZE 200
      char buf[BUF_SIZE];
      if ((channel=find_channel_number_for_entry(tinfo_to_use, entry))<0) break;
      if (local_arg->lastsel_entry!=NULL) {
       snprintf(buf, BUF_SIZE, "%s(%s(%g).#%d)=%g", tsfunction_names[local_arg->function], entry->channelname_to_show, local_arg->lastselected, local_arg->itempart, tsdata(tinfo_to_use, local_arg->itempart, local_arg->function, channel, get_selectedpoint(tinfo_to_use)));
       TRACEMS(tinfo->emethods, -1, buf);
      }
      snprintf(buf, BUF_SIZE, ";%s(%s(%g).#%d)=%g\n", tsfunction_names[local_arg->function], entry->channelname_to_show, tinfo_to_use->xdata[i], local_arg->itempart, local_arg->yclicked);
      TRACEMS(tinfo->emethods, -1, buf);
#undef BUF_SIZE
      /*}}}  */
     } else {
      /*{{{  RIGHTMOUSE: toggle single-channel display*/
      if (nr_of_selected_channels==1 && entry->selected) {
       select_channels(local_arg, NULL);
       local_arg->red_message="All channels selected.";
      } else {
       select_channels(local_arg, entry);
       snprintf(local_arg->messagebuffer, MESSAGELEN, "Channel %s selected.", entry->channelname_to_show);
       local_arg->red_message=local_arg->messagebuffer;
      }
      dev=NEWBORDER; leave=TRUE;
      /*}}}  */
     }
     /*}}}  */
     break;
    case ESCKEY:
     leave=(val==0);	/* Leave on release of key */
     break;
   }
   if (dev!=NEWBORDER && dev!=NEWDATA && lastsel_changed) {
    /* Redraw */
    leave=TRUE;
    dev=REDRAW;
   }

   if (leave && !postscript_output) {
    if (local_arg->replay_file!=NULL) {
     /* In case of replay from a file, do NEWBORDERS, because subsequent events
      * may depend upon the newly calculated border/max/min values. */
     if (dev==REDRAW) leave=FALSE;
    } else {
     /* Don't do a redraw if another event is pending. */
     if ((dev==REDRAW || dev==NEWBORDER || dev==NEWDATA) && qtest()) {
      leave=FALSE;
     }
    }
   }
   /*}}}  */
  }
  if (!postscript_output) {
   /*{{{  Show "Plotting..."*/
   int newminx, newminy;
   frontbuffer(1);
   pushviewport();
   pushmatrix();
   newminx=minx;
   newminy=(maxy-3*CHAR_HEIGHT);
   viewport(newminx, maxx, newminy, (Screencoord)(newminy+CHAR_HEIGHT));
   ortho2((Coord)newminx, (Coord)maxx, (Coord)newminy, (Coord)(newminy+CHAR_HEIGHT));
   cmov2((Coord)(newminx+CHAR_WIDTH), (Coord)newminy);
   color(BACKGROUND);
   clear();
   color(RED);
   charstr("Plotting...");
   popmatrix();
   popviewport();
   /*}}}  */
  }
 } while (dev==REDRAW);
} while (dev==NEWBORDER || dev==NEWDATA);

 if (swaptinfo!=NULL) {
  if (tinfo!=orig_tinfo) {
   /* Swap swaptinfo and tinfo; assuming swaptinfo==orig_tinfo */
   tinfoptr=tinfo;
   tinfo=swaptinfo;
   swaptinfo=tinfoptr;
  }
  swap_xz_free(swaptinfo);
 }
 if (tinfo->data_type==FREQ_DATA) {
  free_pointer((void **)&tinfo->next);
 }
 local_arg->dev=NEWDATA;/* In case there will be another call to tinfo */
 return (return_null ? NULL : tinfo->tsdata);
}
/*}}}  */

/*{{{  posplot_exit(transform_info_ptr tinfo)*/
METHODDEF void
posplot_exit(transform_info_ptr tinfo) {
 struct posplot_storage * const local_arg=(struct posplot_storage * const)tinfo->methods->local_storage;
#ifdef SGI
 winclose(local_arg->gid);
#endif
 gexit();

 if (local_arg->record_file!=NULL) {
  fputc('\n', local_arg->record_file);
  close_record_file(tinfo);
 }
 if (local_arg->replay_file!=NULL) close_replay_file(tinfo);
 growing_buf_free(&local_arg->selection);
 clear_channel_map(local_arg);
 growing_buf_free(&local_arg->channel_map);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_posplot(transform_info_ptr tinfo)*/
GLOBAL void
select_posplot(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &posplot_init;
 tinfo->methods->transform= &posplot;
 tinfo->methods->transform_exit= &posplot_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="posplot";
 tinfo->methods->method_description=
  "Method to display data sets at positions corresponding\n"
  "to the sensor positions.\n";
 tinfo->methods->local_storage_size=sizeof(struct posplot_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
