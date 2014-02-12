/*
 * Copyright (C) 2002,2004,2005,2007,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * correlate.c method to correlate the incoming epochs with a number of
 * time courses read from a second file
 *	-- Bernd Feige 13.06.2002
 *
 * This method needs a side_tinfo to be set with a selected get_epoch method;
 * It will use this method to read time courses from epochs when initialized
 * and then do a scalar multiplication between each incoming epoch and the time
 * courses read, after normalizing (but not orthogonalizing !) them. It will
 * only use one vector of scalars per epoch (ie, only the first item).
 * The channel setup of the incoming epoch always remains untouched (ie, there
 * are always just as many output as input channels). When scalars are returned,
 * they are placed in adjacent time points.
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
 ARGS_NONORMALIZATION, 
 ARGS_ORTHOGONALIZE, 
 ARGS_DUMPVECTORS, 
 ARGS_CLOSE, 
 ARGS_FROMEPOCH, 
 ARGS_EPOCHS,
 ARGS_NROFITEM,
 ARGS_CORRELATEFILE,
 ARGS_CHANNELS,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Subspace mode. incoming time courses are projected onto the correlate_file subspace", "s", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Subtract-Subspace mode. projected time courses are subtracted from data", "S", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Multiply mode. The projection phase is skipped, the current epoch contains the weights, time courses*weights is returned.", "m", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Correlation. Subtract the mean from correlate_file time courses first", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "No normalization. Do NOT normalize the time courses from correlate_file", "n", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Orthogonalize the time courses from correlate_file. Recommended for Subspace !", "o", FALSE, NULL},
 {T_ARGS_TAKES_FILENAME, "vectorfile: Dump vectors, after preprocessing, to an ASCII (matlab) file", "D", ARGDESC_UNUSED, (const char *const *)"*.mat"},
 {T_ARGS_TAKES_NOTHING, "Close and reopen the file for each epoch", "C", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Start with epoch number fromepoch (>=1)", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Number of vectors to read from the correlate_file (default: 1)", "e", 1, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: Read vector from this item # (>=0; default: 0)", "i", 0, NULL},
 {T_ARGS_TAKES_FILENAME, "correlate_file", "", ARGDESC_UNUSED, (const char *const *)"*.asc"},
 {T_ARGS_TAKES_STRING_WORD, "channelnames", "", ARGDESC_UNUSED, NULL}
};

/*{{{  Local definitions*/
enum correlate_modes {
 CORRELATE_MODE_SCALAR, CORRELATE_MODE_SSP, CORRELATE_MODE_SSPS, CORRELATE_MODE_MULTIPLY
};

struct correlate_args_struct {
 struct transform_info_struct side_tinfo;
 struct transform_methods_struct methods;
 array vectors;
 int nr_of_correlatefile_channels;
 int nr_of_selected_channels;
 int nr_of_item;
 int orthogonalize_vectors_first;
 enum correlate_modes correlate_mode;
 int epochs;
 int *channel_list;
};
/*}}}  */

/*{{{  correlate_init(transform_info_ptr tinfo)*/
METHODDEF void
correlate_init(transform_info_ptr tinfo) {
 struct correlate_args_struct *correlate_args=(struct correlate_args_struct *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr side_tinfo= &correlate_args->side_tinfo;
 array *vectors= &correlate_args->vectors;
 growing_buf buf;
#define BUFFER_SIZE 80
 char buffer[BUFFER_SIZE];

 side_tinfo->methods= &correlate_args->methods;
 side_tinfo->emethods=tinfo->emethods;
 select_readasc(side_tinfo);
 growing_buf_init(&buf);
 growing_buf_allocate(&buf, 0);
 if (args[ARGS_CLOSE].is_set) {
  growing_buf_appendstring(&buf, "-c ");
 }
 if (args[ARGS_FROMEPOCH].is_set) {
  snprintf(buffer, BUFFER_SIZE, "-f %ld ", args[ARGS_FROMEPOCH].arg.i);
  growing_buf_appendstring(&buf, buffer);
 }
 if (args[ARGS_EPOCHS].is_set) {
  correlate_args->epochs=args[ARGS_EPOCHS].arg.i;
  if (correlate_args->epochs<=0) {
   ERREXIT(tinfo->emethods, "correlate_init: The number of epochs must be positive.\n");
  }
 } else {
  correlate_args->epochs=1;
 }
 snprintf(buffer, BUFFER_SIZE, "-e %d ", correlate_args->epochs);
 growing_buf_appendstring(&buf, buffer);
 growing_buf_appendstring(&buf, args[ARGS_CORRELATEFILE].arg.s);
 if (!buf.can_be_freed || !setup_method(side_tinfo, &buf)) {
  ERREXIT(tinfo->emethods, "correlate_init: Error setting readasc arguments.\n");
 }

 correlate_args->nr_of_item=(args[ARGS_NROFITEM].is_set ? args[ARGS_NROFITEM].arg.i : 0);
 correlate_args->orthogonalize_vectors_first=args[ARGS_ORTHOGONALIZE].is_set;
 if (args[ARGS_SUBSPACE].is_set) {
  correlate_args->correlate_mode=CORRELATE_MODE_SSP;
 } else if (args[ARGS_SUBTRACT_SUBSPACE].is_set) {
  correlate_args->correlate_mode=CORRELATE_MODE_SSPS;
 } else if (args[ARGS_MULTIPLY].is_set) {
  correlate_args->correlate_mode=CORRELATE_MODE_MULTIPLY;
 } else {
  correlate_args->correlate_mode=CORRELATE_MODE_SCALAR;
 }

 (*side_tinfo->methods->transform_init)(side_tinfo);

 /*{{{  Read first correlate_file epoch and allocate vectors array accordingly*/
 if ((side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo))==NULL) {
  ERREXIT(tinfo->emethods, "correlate_init: Can't get the first correlate epoch.\n");
 }
 correlate_args->channel_list=expand_channel_list(side_tinfo, args[ARGS_CHANNELS].arg.s);
 if (correlate_args->channel_list==NULL) {
  ERREXIT(tinfo->emethods, "correlate_init: Zero channels were selected!\n");
 }
 correlate_args->nr_of_correlatefile_channels=side_tinfo->nr_of_channels;
 /* Count the selected channels: */
 for (correlate_args->nr_of_selected_channels=0; correlate_args->channel_list[correlate_args->nr_of_selected_channels]!=0; correlate_args->nr_of_selected_channels++);
 vectors->nr_of_vectors=correlate_args->epochs*correlate_args->nr_of_selected_channels;
 vectors->nr_of_elements=side_tinfo->nr_of_points;
 vectors->element_skip=1;
 if (array_allocate(vectors)==NULL) {
  ERREXIT(tinfo->emethods, "correlate_init: Error allocating vectors memory\n");
 }
 /*}}}  */

 do {
  /*{{{  Copy [channels] vectors from correlate_file to vectors array*/
  array indata;
  int channel;

  if (vectors->nr_of_elements!=side_tinfo->nr_of_points) {
   ERREXIT(side_tinfo->emethods, "correlate_init: Varying epoch lengths in correlate file!\n");
  }
  if (correlate_args->nr_of_correlatefile_channels!=side_tinfo->nr_of_channels) {
   ERREXIT(side_tinfo->emethods, "correlate_init: Varying numbers of channels in correlate file!\n");
  }
  if (correlate_args->nr_of_item>=side_tinfo->itemsize) {
   ERREXIT2(side_tinfo->emethods, "correlate_init: nr_of_item=%d, itemsize=%d\n", MSGPARM(correlate_args->nr_of_item), MSGPARM(side_tinfo->itemsize));
  }

  for (channel=0; correlate_args->channel_list[channel]!=0; channel++) {
   tinfo_array(side_tinfo, &indata);
   /* array_transpose(&indata);	Vector = time course */
   if (vectors->nr_of_elements!=indata.nr_of_elements) {
    ERREXIT(side_tinfo->emethods, "correlate_init: vector size doesn't match\n");
   }
   indata.current_vector=correlate_args->channel_list[channel]-1;
   array_setto_vector(&indata);
   array_use_item(&indata, correlate_args->nr_of_item);
   array_copy(&indata, vectors);
  }
  /*}}}  */
  free_tinfo(side_tinfo);
 } while ((side_tinfo->tsdata=(*side_tinfo->methods->transform)(side_tinfo))!=NULL);

 if (vectors->message!=ARRAY_ENDOFSCAN) {
  ERREXIT1(tinfo->emethods, "correlate_init: Less than %d epochs in correlate file\n", MSGPARM(vectors->nr_of_vectors));
 }
 
 /*{{{  De-mean vectors if requested*/
 if (args[ARGS_CORRELATION].is_set) {
  do {
   DATATYPE const mean=array_mean(vectors);
   array_previousvector(vectors);
   do {
    array_write(vectors, READ_ELEMENT(vectors)-mean);
   } while (vectors->message==ARRAY_CONTINUE);
  } while (vectors->message!=ARRAY_ENDOFSCAN);
 }
 /*}}}  */
 /*{{{  Orthogonalize vectors if requested*/
 if (correlate_args->orthogonalize_vectors_first) {
  array_make_orthogonal(vectors);
  if (vectors->message==ARRAY_ERROR) {
   ERREXIT(tinfo->emethods, "correlate_init: Error orthogonalizing correlation vectors\n");
  }
 }
 /*}}}  */
 if (!args[ARGS_NONORMALIZATION].is_set) {
 /*{{{  Normalize vectors*/
 do {
  DATATYPE const length=array_abs(vectors);
  array_previousvector(vectors);
  if (length==0.0) {
   ERREXIT1(tinfo->emethods, "correlate_init: Vector %d has zero length !\n", MSGPARM(vectors->current_vector));
  }
  array_scale(vectors, 1.0/length);
 } while (vectors->message!=ARRAY_ENDOFSCAN);
 /*}}}  */
 }
 /*{{{  Dump vectors if requested*/
 if (args[ARGS_DUMPVECTORS].is_set) {
  FILE * const fp=fopen(args[ARGS_DUMPVECTORS].arg.s, "w");
  if (fp==NULL) {
   ERREXIT1(tinfo->emethods, "correlate_init: Error opening output file >%s<.\n", MSGPARM(args[ARGS_DUMPVECTORS].arg.s));
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

/*{{{  correlate(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
correlate(transform_info_ptr tinfo) {
 struct correlate_args_struct *correlate_args=(struct correlate_args_struct *)tinfo->methods->local_storage;
 DATATYPE *retvalue=tinfo->tsdata;
 array indata, scalars, *vectors= &correlate_args->vectors;
 int itempart, itemparts=tinfo->itemsize-tinfo->leaveright;

 if (correlate_args->correlate_mode==CORRELATE_MODE_MULTIPLY) {
  if (tinfo->nr_of_points!=vectors->nr_of_vectors) {
   ERREXIT2(tinfo->emethods, "correlate: %d weights but %d input traces!\n", MSGPARM(tinfo->nr_of_points), MSGPARM(vectors->nr_of_vectors));
  }
 } else {
  if (vectors->nr_of_elements!=tinfo->nr_of_points) {
   ERREXIT2(tinfo->emethods, "correlate: %d points in epoch, but %d points in correlate file.\n", MSGPARM(tinfo->nr_of_points), MSGPARM(vectors->nr_of_elements));
  }
 }

 array_reset(vectors);

 tinfo_array(tinfo, &indata);
 /* array_transpose(&indata);	Vectors are time courses */

 /* Be sure that different output items are adjacent... */
 scalars.nr_of_elements=vectors->nr_of_vectors;
 switch (correlate_args->correlate_mode) {
  case CORRELATE_MODE_SCALAR:
   scalars.nr_of_vectors=tinfo->nr_of_points;
   scalars.element_skip=itemparts;
   break;
  case CORRELATE_MODE_SSP:
  case CORRELATE_MODE_SSPS:
   scalars.nr_of_vectors=1;
   scalars.element_skip=1;
   break;
  case CORRELATE_MODE_MULTIPLY:
   scalars=indata;
   indata.element_skip=itemparts;
   indata.nr_of_elements=vectors->nr_of_elements; /* New number of points */
   indata.nr_of_vectors=scalars.nr_of_vectors; /* New number of channels */
   if (array_allocate(&indata)==NULL) {
    ERREXIT(tinfo->emethods, "correlate: Can't allocate new indata array\n");
   }
   break;
 }

 if (correlate_args->correlate_mode==CORRELATE_MODE_MULTIPLY) {
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
  ERREXIT(tinfo->emethods, "correlate: Can't allocate scalars array\n");
 }

 for (itempart=0; itempart<itemparts; itempart++) {
  array_use_item(&indata, itempart);
  if (correlate_args->correlate_mode==CORRELATE_MODE_SCALAR) {
   array_use_item(&scalars, itempart);
  }

  do {
   /*{{{  Calculate the correlation scalars*/
   do {
    /* With all vectors in turn: */
    array_write(&scalars, array_multiply(&indata, vectors, MULT_VECTOR));
    if (scalars.message==ARRAY_CONTINUE) array_previousvector(&indata);
   } while (scalars.message==ARRAY_CONTINUE);
   /*}}}  */
   
   switch (correlate_args->correlate_mode) {
    case CORRELATE_MODE_SCALAR:
     break;
    case CORRELATE_MODE_SSP:
     /*{{{  Build the signal subspace vector*/
     array_transpose(vectors); /* Now the elements are the subspace dimensions */
     array_previousvector(&indata);
     do {
      array_write(&indata, array_multiply(vectors, &scalars, MULT_VECTOR));
     } while (indata.message==ARRAY_CONTINUE);
     array_transpose(vectors);
     /*}}}  */
     break;
    case CORRELATE_MODE_SSPS:
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
     ERREXIT(tinfo->emethods, "correlate: This cannot happen!\n");
     break;
   }
  } while (indata.message!=ARRAY_ENDOFSCAN);
 }
 }

 /*{{{  Prepare tinfo to reflect the data structure returned*/
 switch (correlate_args->correlate_mode) {
  case CORRELATE_MODE_SCALAR:
   tinfo->nr_of_points=vectors->nr_of_vectors;
   tinfo->itemsize=itemparts;
   tinfo->length_of_output_region=scalars.nr_of_elements*scalars.element_skip*scalars.nr_of_vectors;
   tinfo->leaveright=0;
   tinfo->multiplexed=FALSE;
   retvalue=scalars.start;
   break;
  case CORRELATE_MODE_SSP:
  case CORRELATE_MODE_SSPS:
   array_free(&scalars);
   retvalue=tinfo->tsdata;
   break;
  case CORRELATE_MODE_MULTIPLY:
   /* Note that we don't free `scalars' because it just points to the original tsdata! */
   tinfo->nr_of_points=indata.nr_of_elements;
   tinfo->nr_of_channels=indata.nr_of_vectors;
   tinfo->itemsize=indata.element_skip;
   retvalue=indata.start;
   tinfo->multiplexed=FALSE;
   break;
 }
 /*}}}  */

 return retvalue;
}
/*}}}  */

/*{{{  correlate_exit(transform_info_ptr tinfo)*/
METHODDEF void
correlate_exit(transform_info_ptr tinfo) {
 struct correlate_args_struct *correlate_args=(struct correlate_args_struct *)tinfo->methods->local_storage;

 array_free(&correlate_args->vectors);
 free_pointer((void **)&correlate_args->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_correlate(transform_info_ptr tinfo)*/
GLOBAL void
select_correlate(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &correlate_init;
 tinfo->methods->transform= &correlate;
 tinfo->methods->transform_exit= &correlate_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="correlate";
 tinfo->methods->method_description=
  "Transform method to do a scalar multiplication between the time courses of\n"
  " all channels in the incoming epoch with one or more time courses read from\n"
  " correlate_file channels. Multiple scalars are placed in adjacent output points.\n"
  " This method is just like 'project' but for time course vectors.\n";
 tinfo->methods->local_storage_size=sizeof(struct correlate_args_struct);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
