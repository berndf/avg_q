/*
 * Copyright (C) 1996-2001,2004,2010,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * project.c method to project the incoming epochs onto a number of
 * directions read from a second file
 *	-- Bernd Feige 23.09.1994
 *
 * This method needs a side_tinfo to be set with a selected get_epoch
 * method; It will use this method to read maps from epochs when initialized
 * and then project each incoming epoch to the epochs read, after normalizing
 * (but not orthogonalizing !) them. It will only use one vector of scalars
 * per epoch (ie, only the first item).
 * There are two modi: Either the output is just one channel named 'projected'
 * containing the projection of the incoming vectors onto the maps read
 * previously (one output item for each map per incoming item) or the
 * output is the input projected onto the subspace spanned by the maps
 * (signal-space projection). In the latter case, the size of the epoch
 * remains the same.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_SUBSPACE=0, 
 ARGS_SUBTRACT_SUBSPACE, 
 ARGS_MULTIPLY, 
 ARGS_CORRELATION, 
 ARGS_USE_CHANNELS, 
 ARGS_NONORMALIZATION, 
 ARGS_ORTHOGONALIZE, 
 ARGS_AT_XVALUE,
 ARGS_ZERO_UNUSEDCOEFFICIENTS,
 ARGS_CHANNELNAMES, 
 ARGS_DUMPVECTORS, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_POINTS,
 ARGS_NROFITEM,
 ARGS_PROJECTFILE,
 ARGS_NROFPOINT,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Subspace mode. incoming maps are projected onto the project_file subspace", "s", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Subtract-Subspace mode. projected maps are subtracted from data", "S", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Multiply mode. The projection phase is skipped, the current epoch contains the weights, maps*weights is returned.", "m", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Correlation. Subtract the mean from project_file maps first", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Channels are used for scalars instead of different items", "C", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "No normalization. Do NOT normalize the maps from project_file", "n", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Orthogonalize the maps from project_file. Recommended for Subspace !", "o", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Select x value of the map to use instead of nr_of_point", "x", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Set project_file channels not in the -N list to zero (protects these input channels from modification in Subtract-Subspace mode)", "z", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict scalar product and normalization to these channel names", "N", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "vectorfile: Dump vectors, after preprocessing, to an ASCII (matlab) file", "D", ARGDESC_UNUSED, (const char *const *)"*.mat"},
 {T_ARGS_TAKES_LONG, "fromepoch: Start with epoch number fromepoch (>=1)", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Number of vectors to read from the project_file (default: 1)", "e", 1, NULL},
 {T_ARGS_TAKES_LONG, "points: Number of points to read from each epoch (default: 1)", "p", 1, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: Read vector from this item # (>=0; default: 0)", "i", 0, NULL},
 {T_ARGS_TAKES_FILENAME, "project_file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"},
 {T_ARGS_TAKES_STRING_WORD, "nr_of_point", "", ARGDESC_UNUSED, NULL}
};

/*{{{  Local definitions*/
enum project_modes {
 PROJECT_MODE_SCALAR, PROJECT_MODE_SSP, PROJECT_MODE_SSPS, PROJECT_MODE_MULTIPLY
};

struct project_args_struct {
 struct transform_info_struct side_tinfo;
 struct transform_info_struct save_side_tinfo;
 struct transform_methods_struct methods;
 array vectors;
 int nr_of_point;
 int nr_of_item;
 int orthogonalize_vectors_first;
 enum project_modes project_mode;
 int epochs;
 int points;
 int *channel_list;
};
/*}}}  */

/*{{{  project_init(transform_info_ptr tinfo)*/
METHODDEF void
project_init(transform_info_ptr tinfo) {
 struct project_args_struct *project_args=(struct project_args_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &project_args->side_tinfo;
 array *vectors= &project_args->vectors;
 growing_buf buf;
#define BUFFER_SIZE 80
 char buffer[BUFFER_SIZE];

 side_tinfo->methods= &project_args->methods;
 side_tinfo->emethods=tinfo->emethods;
 select_readasc(side_tinfo);
 growing_buf_init(&buf);
 growing_buf_allocate(&buf, 0);
 if (args[ARGS_FROMEPOCH].is_set) {
  snprintf(buffer, BUFFER_SIZE, "-f %ld ", args[ARGS_FROMEPOCH].arg.i);
  growing_buf_appendstring(&buf, buffer);
 }
 if (args[ARGS_EPOCHS].is_set) {
  project_args->epochs=args[ARGS_EPOCHS].arg.i;
  if (project_args->epochs<=0) {
   ERREXIT(tinfo->emethods, "project_init: The number of epochs must be positive.\n");
  }
 } else {
  project_args->epochs=1;
 }
 snprintf(buffer, BUFFER_SIZE, "-e %d ", project_args->epochs);
 growing_buf_appendstring(&buf, buffer);
 growing_buf_appendstring(&buf, args[ARGS_PROJECTFILE].arg.s);
 if (!buf.can_be_freed || !setup_method(side_tinfo, &buf)) {
  ERREXIT(tinfo->emethods, "project_init: Error setting readasc arguments.\n");
 }

 project_args->points=(args[ARGS_POINTS].is_set ? args[ARGS_POINTS].arg.i : 1);
 project_args->nr_of_item=(args[ARGS_NROFITEM].is_set ? args[ARGS_NROFITEM].arg.i : 0);
 project_args->orthogonalize_vectors_first=args[ARGS_ORTHOGONALIZE].is_set;
 if (args[ARGS_SUBSPACE].is_set) {
  project_args->project_mode=PROJECT_MODE_SSP;
 } else if (args[ARGS_SUBTRACT_SUBSPACE].is_set) {
  project_args->project_mode=PROJECT_MODE_SSPS;
 } else if (args[ARGS_MULTIPLY].is_set) {
  project_args->project_mode=PROJECT_MODE_MULTIPLY;
 } else {
  project_args->project_mode=PROJECT_MODE_SCALAR;
 }
 if (args[ARGS_CHANNELNAMES].is_set) {
  project_args->channel_list=expand_channel_list(tinfo, args[ARGS_CHANNELNAMES].arg.s);
  if (project_args->channel_list==NULL) {
   ERREXIT(tinfo->emethods, "project_init: Zero channels were selected by -N!\n");
  }
 } else {
  project_args->channel_list=NULL;
 }

 (*side_tinfo->methods->transform_init)(side_tinfo);

 /*{{{  Read first project_file epoch and allocate vectors array accordingly*/
 if ((side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo))==NULL) {
  ERREXIT(tinfo->emethods, "project_init: Can't get the first project epoch.\n");
 }
 if (project_args->project_mode==PROJECT_MODE_MULTIPLY) {
  /* Save channel names and positions for later */
  project_args->save_side_tinfo.nr_of_channels=side_tinfo->nr_of_channels;
  copy_channelinfo(&project_args->save_side_tinfo, side_tinfo->channelnames, side_tinfo->probepos);
 }
 if (project_args->points==0) project_args->points=side_tinfo->nr_of_points;
 if (project_args->points>side_tinfo->nr_of_points) {
  ERREXIT1(tinfo->emethods, "project_init: There are only %d points in the project_file epoch.\n", MSGPARM(side_tinfo->nr_of_points));
 }
 if (args[ARGS_AT_XVALUE].is_set) {
  project_args->nr_of_point=find_pointnearx(side_tinfo, (DATATYPE)atof(args[ARGS_NROFPOINT].arg.s));
 } else {
 project_args->nr_of_point=gettimeslice(side_tinfo, args[ARGS_NROFPOINT].arg.s);
 }
 vectors->nr_of_vectors=project_args->epochs*project_args->points;
 vectors->nr_of_elements=side_tinfo->nr_of_channels;
 vectors->element_skip=1;
 if (array_allocate(vectors)==NULL) {
  ERREXIT(tinfo->emethods, "project_init: Error allocating vectors memory\n");
 }
 /*}}}  */

 do {
  /*{{{  Copy [points] vectors from project_file to vectors array*/
  array indata;
  int point;

  if (vectors->nr_of_elements!=side_tinfo->nr_of_channels) {
   ERREXIT(side_tinfo->emethods, "project_init: Varying channel numbers in project file!\n");
  }
  if (project_args->nr_of_point>=side_tinfo->nr_of_points) {
   ERREXIT2(side_tinfo->emethods, "project_init: nr_of_point=%d, nr_of_points=%d\n", MSGPARM(project_args->nr_of_point), MSGPARM(side_tinfo->nr_of_points));
  }
  if (project_args->nr_of_item>=side_tinfo->itemsize) {
   ERREXIT2(side_tinfo->emethods, "project_init: nr_of_item=%d, itemsize=%d\n", MSGPARM(project_args->nr_of_item), MSGPARM(side_tinfo->itemsize));
  }

  for (point=0; point<project_args->points; point++) {
   tinfo_array(side_tinfo, &indata);
   array_transpose(&indata);	/* Vector = map */
   if (vectors->nr_of_elements!=indata.nr_of_elements) {
    ERREXIT(side_tinfo->emethods, "project_init: vector size doesn't match\n");
   }
   indata.current_vector=project_args->nr_of_point+point;
   array_setto_vector(&indata);
   array_use_item(&indata, project_args->nr_of_item);
   array_copy(&indata, vectors);
  }
  /*}}}  */
  free_tinfo(side_tinfo);
 } while ((side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo))!=NULL);

 if (vectors->message!=ARRAY_ENDOFSCAN) {
  ERREXIT1(tinfo->emethods, "project_init: Less than %d epochs in project file\n", MSGPARM(vectors->nr_of_vectors));
 }
 
 /*{{{  Set unused channels to zero if requested*/
 if (args[ARGS_ZERO_UNUSEDCOEFFICIENTS].is_set) {
  if (project_args->channel_list!=NULL) {
   do {
    do {
     if (is_in_channellist(vectors->current_element+1, project_args->channel_list)) {
      array_advance(vectors);
     } else {
      array_write(vectors, 0.0);
     }
    } while (vectors->message==ARRAY_CONTINUE);
   } while (vectors->message!=ARRAY_ENDOFSCAN);
  } else {
   ERREXIT(tinfo->emethods, "project_init: Option -z only makes sense in combination with -N!\n");
  }
 }
 /*}}}  */
 /*{{{  De-mean vectors if requested*/
 if (args[ARGS_CORRELATION].is_set) {
  do {
   DATATYPE mean=0.0;
   int nrofaverages=0;
   do {
    if (project_args->channel_list!=NULL && !is_in_channellist(vectors->current_element+1, project_args->channel_list)) {
     array_advance(vectors);
    } else {
     mean+=array_scan(vectors);
     nrofaverages++;
    }
   } while (vectors->message==ARRAY_CONTINUE);
   mean/=nrofaverages;
   array_previousvector(vectors);
   do {
    array_write(vectors, READ_ELEMENT(vectors)-mean);
   } while (vectors->message==ARRAY_CONTINUE);
  } while (vectors->message!=ARRAY_ENDOFSCAN);
 }
 /*}}}  */
 /*{{{  Orthogonalize vectors if requested*/
 if (project_args->orthogonalize_vectors_first) {
  array_make_orthogonal(vectors);
  if (vectors->message==ARRAY_ERROR) {
   ERREXIT(tinfo->emethods, "project_init: Error orthogonalizing projection vectors\n");
  }
 }
 /*}}}  */
 if (!args[ARGS_NONORMALIZATION].is_set) {
 /*{{{  Normalize vectors*/
 do {
  DATATYPE length=0.0;
  do {
   if (project_args->channel_list!=NULL && !is_in_channellist(vectors->current_element+1, project_args->channel_list)) {
    array_advance(vectors);
   } else {
    const DATATYPE hold=array_scan(vectors);
    length+=hold*hold;
   }
  } while (vectors->message==ARRAY_CONTINUE);
  length=sqrt(length);

  array_previousvector(vectors);
  if (length==0.0) {
   ERREXIT1(tinfo->emethods, "project_init: Vector %d has zero length !\n", MSGPARM(vectors->current_vector));
  }
  array_scale(vectors, 1.0/length);
 } while (vectors->message!=ARRAY_ENDOFSCAN);
 /*}}}  */
 }
 /*{{{  Dump vectors if requested*/
 if (args[ARGS_DUMPVECTORS].is_set) {
  FILE * const fp=fopen(args[ARGS_DUMPVECTORS].arg.s, "w");
  if (fp==NULL) {
   ERREXIT1(tinfo->emethods, "project_init: Error opening output file >%s<.\n", MSGPARM(args[ARGS_DUMPVECTORS].arg.s));
  }
  array_transpose(vectors); /* We want one vector to be one column */ 
  array_dump(fp, vectors, ARRAY_MATLAB);
  array_transpose(vectors);
  fclose(fp);
 }
 /*}}}  */

 (*side_tinfo->methods->transform_exit)(side_tinfo);
 free_methodmem(side_tinfo);
 growing_buf_free(&buf);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  project(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
project(transform_info_ptr tinfo) {
 struct project_args_struct *project_args=(struct project_args_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE *retvalue=tinfo->tsdata;
 array indata, scalars, *vectors= &project_args->vectors;
 int itempart, itemparts=tinfo->itemsize-tinfo->leaveright;

 if (project_args->project_mode==PROJECT_MODE_MULTIPLY) {
  if (!(tinfo->nr_of_channels==vectors->nr_of_vectors
      ||(tinfo->nr_of_channels==1 && tinfo->itemsize==vectors->nr_of_vectors))) {
   ERREXIT(tinfo->emethods, "project: Number of scalars and maps doesn't match!\n");
  }
 } else {
 if (vectors->nr_of_elements!=tinfo->nr_of_channels) {
  ERREXIT2(tinfo->emethods, "project: %d channels in epoch, but %d channels in project file.\n", MSGPARM(tinfo->nr_of_channels), MSGPARM(vectors->nr_of_elements));
 }
 }

 array_reset(vectors);

 tinfo_array(tinfo, &indata);
 array_transpose(&indata);	/* Vectors are maps */

 /* Be sure that different output items are adjacent... */
 scalars.nr_of_elements=vectors->nr_of_vectors;
 switch (project_args->project_mode) {
  case PROJECT_MODE_SCALAR:
   scalars.nr_of_vectors=tinfo->nr_of_points;
   scalars.element_skip=itemparts;
   break;
  case PROJECT_MODE_SSP:
  case PROJECT_MODE_SSPS:
   scalars.nr_of_vectors=1;
   scalars.element_skip=1;
   break;
  case PROJECT_MODE_MULTIPLY:
   scalars=indata;
   if (tinfo->nr_of_channels!=vectors->nr_of_vectors) {
    /* Ie, one channel and weights in the items: Convert from 1 element to
     * itemsize elements */
    scalars.element_skip=1;
    scalars.nr_of_elements=tinfo->itemsize;
    itemparts=1;
   }
   indata.element_skip=itemparts;
   indata.nr_of_elements=vectors->nr_of_elements; /* New number of channels */
   indata.nr_of_vectors=scalars.nr_of_vectors; /* New number of points */
   if (array_allocate(&indata)==NULL) {
    ERREXIT(tinfo->emethods, "project: Can't allocate new indata array\n");
   }
   break;
 }

 if (project_args->project_mode==PROJECT_MODE_MULTIPLY) {
  array_transpose(vectors); /* Now the elements are the subspace dimensions */
  for (itempart=0; itempart<itemparts; itempart++) {
   array_use_item(&indata, itempart);
   array_use_item(&scalars, itempart);
   do {
    /*{{{  Build the signal subspace vector*/
    do {
     array_write(&indata, array_multiply(vectors, &scalars, MULT_SAMESIZE));
    } while (indata.message==ARRAY_CONTINUE);
    /*}}}  */
   } while (indata.message!=ARRAY_ENDOFSCAN);
  }
  array_transpose(vectors);
 } else {

 if (array_allocate(&scalars)==NULL) {
  ERREXIT(tinfo->emethods, "project: Can't allocate scalars array\n");
 }

 for (itempart=0; itempart<itemparts; itempart++) {
  array_use_item(&indata, itempart);
  if (project_args->project_mode==PROJECT_MODE_SCALAR) {
   array_use_item(&scalars, itempart);
  }

  do {
   /*{{{  Calculate the projection scalars*/
   do {
    /* With all vectors in turn: */
    DATATYPE product=0.0;
    do {
     if (project_args->channel_list!=NULL && !is_in_channellist(vectors->current_element+1, project_args->channel_list)) {
      array_advance(vectors);
      array_advance(&indata);
     } else {
      product+=array_scan(&indata)*array_scan(vectors);
     }
    } while (vectors->message==ARRAY_CONTINUE);

    array_write(&scalars, product);
    if (scalars.message==ARRAY_CONTINUE) array_previousvector(&indata);
   } while (scalars.message==ARRAY_CONTINUE);
   /*}}}  */
   
   switch (project_args->project_mode) {
    case PROJECT_MODE_SCALAR:
     break;
    case PROJECT_MODE_SSP:
     /*{{{  Build the signal subspace vector*/
     array_transpose(vectors); /* Now the elements are the subspace dimensions */
     array_previousvector(&indata);
     do {
      array_write(&indata, array_multiply(vectors, &scalars, MULT_VECTOR));
     } while (indata.message==ARRAY_CONTINUE);
     array_transpose(vectors);
     /*}}}  */
     break;
    case PROJECT_MODE_SSPS:
     /*{{{  Subtract the signal subspace vector*/
     array_transpose(vectors); /* Now the elements are the subspace dimensions */
     array_previousvector(&indata);
     do {
      array_write(&indata, READ_ELEMENT(&indata)-array_multiply(vectors, &scalars, MULT_VECTOR));
     } while (indata.message==ARRAY_CONTINUE);
     array_transpose(vectors);
     /*}}}  */
     break;
    default:
     /* Can't happen, just to keep the compiler happy... */
     ERREXIT(tinfo->emethods, "project: This cannot happen!\n");
     break;
   }
  } while (indata.message!=ARRAY_ENDOFSCAN);
 }
 }

 /*{{{  Prepare tinfo to reflect the data structure returned*/
 switch (project_args->project_mode) {
  case PROJECT_MODE_SCALAR:
   if (args[ARGS_USE_CHANNELS].is_set) {
    tinfo->nr_of_channels=vectors->nr_of_vectors;
    tinfo->itemsize=itemparts;
   } else {
    tinfo->nr_of_channels=1;
    tinfo->itemsize=itemparts*vectors->nr_of_vectors;
   }
   tinfo->length_of_output_region=scalars.nr_of_elements*scalars.element_skip*scalars.nr_of_vectors;
   tinfo->leaveright=0;
   tinfo->multiplexed=TRUE;
   if (tinfo->channelnames!=NULL) {
    free_pointer((void **)&tinfo->channelnames[0]);
    free_pointer((void **)&tinfo->channelnames);
   }
   free_pointer((void **)&tinfo->probepos);
   create_channelgrid(tinfo);
   retvalue=scalars.start;
   break;
  case PROJECT_MODE_SSP:
  case PROJECT_MODE_SSPS:
   array_free(&scalars);
   retvalue=tinfo->tsdata;
   break;
  case PROJECT_MODE_MULTIPLY:
   /* Note that we don't free `scalars' because it just points to the original tsdata! */
   /* Have to set this correctly so that copy_channelinfo works... */
   tinfo->nr_of_channels=indata.nr_of_elements;
   copy_channelinfo(tinfo, project_args->save_side_tinfo.channelnames, project_args->save_side_tinfo.probepos);
   tinfo->nr_of_points=indata.nr_of_vectors;
   tinfo->itemsize=indata.element_skip;
   tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
   retvalue=indata.start;
   tinfo->multiplexed=TRUE;
   break;
 }
 /*}}}  */

 return retvalue;
}
/*}}}  */

/*{{{  project_exit(transform_info_ptr tinfo)*/
METHODDEF void
project_exit(transform_info_ptr tinfo) {
 struct project_args_struct *project_args=(struct project_args_struct *)tinfo->methods->local_storage;

 array_free(&project_args->vectors);
 if (project_args->project_mode==PROJECT_MODE_MULTIPLY) {
  if (project_args->save_side_tinfo.channelnames!=NULL) {
   free_pointer((void **)&project_args->save_side_tinfo.channelnames[0]);
   free_pointer((void **)&project_args->save_side_tinfo.channelnames);
  }
  free_pointer((void **)&project_args->save_side_tinfo.probepos);
 }
 free_pointer((void **)&project_args->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_project(transform_info_ptr tinfo)*/
GLOBAL void
select_project(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &project_init;
 tinfo->methods->transform= &project;
 tinfo->methods->transform_exit= &project_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="project";
 tinfo->methods->method_description=
  "Transform method to project all incoming epochs onto a subspace spanned by\n"
  " a number of maps read from a project_file\n";
 tinfo->methods->local_storage_size=sizeof(struct project_args_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
