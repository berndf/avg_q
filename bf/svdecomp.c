/*
 * Copyright (C) 1996-2001,2004 Bernd Feige
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
 * svdecomp performs a singular value decomposition across channels, yielding
 * a number of items corresponding to different component waveforms; within
 * each item, the waveforms of all channels are equal, only the amplitude is
 * different. This is a somewhat `exploded' but useful view on the SVD result.
 * 						-- Bernd Feige 08.09.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_MAPS=0, 
 ARGS_ONLYMAPS, 
 ARGS_COMPLEX, 
 ARGS_NCOMPONENTS, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "The components are the maps rather than the waveforms", "m", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Output only the maps for later projection", "M", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Treat item pairs as complex entities", "c", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "n_components", "", 3, NULL}
};

/*{{{  svdecomp_init(transform_info_ptr tinfo) {*/
METHODDEF void
svdecomp_init(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 if (args[ARGS_NCOMPONENTS].arg.i<=0) {
  ERREXIT(tinfo->emethods, "svdecomp_init: n_components<=0\n");
 }
 if (args[ARGS_MAPS].is_set && args[ARGS_ONLYMAPS].is_set) {
  ERREXIT(tinfo->emethods, "svdecomp_init: Options -m and -M are exclusive.\n");
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  svdecomp(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
svdecomp(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 int itempart, ncomps=args[ARGS_NCOMPONENTS].arg.i;
 int subitemsize=1;
 array myarray, u, v, w;
 array myarray_decompversion;

 if (args[ARGS_COMPLEX].is_set) {
  if (tinfo->itemsize!=2) {
   ERREXIT(tinfo->emethods, "svdecomp: Complex decomposition only works with itemsize==2\n");
  }
  /* If we want to make a single vector from two adjacent items, which are
   * adjacent in any case, the other points of each vector must be adjacent, too. */
  if (args[ARGS_MAPS].is_set) {
   multiplexed(tinfo);
  } else {
   nonmultiplexed(tinfo);
  }
 }
 tinfo_array(tinfo, &myarray);
 /* By default we decompose taking the full waveforms, ie the channels, 
  * as vectors; with -m, we take the maps as vectors: */
 if (args[ARGS_MAPS].is_set) array_transpose(&myarray);
 v.nr_of_vectors=v.nr_of_elements=w.nr_of_elements=myarray.nr_of_vectors;
 w.nr_of_vectors=1;
 v.element_skip=w.element_skip=1;
 if (args[ARGS_ONLYMAPS].is_set) {
  u.nr_of_vectors=ncomps;
  u.nr_of_elements=tinfo->nr_of_channels;
  u.element_skip=tinfo->itemsize;
 } else {
  u.nr_of_vectors=myarray.nr_of_vectors;
  u.nr_of_elements=myarray.nr_of_elements;
  u.element_skip=ncomps*tinfo->itemsize;
 }
 if (ncomps>w.nr_of_elements) {
  TRACEMS1(tinfo->emethods, 0, "svdecomp: ncomps is larger than available, setting to %d\n", MSGPARM(w.nr_of_elements));
  ncomps=w.nr_of_elements;
 }
 if (array_allocate(&u) == NULL || array_allocate(&v) == NULL || array_allocate(&w) == NULL) {
  ERREXIT(tinfo->emethods, "svdecomp: Error allocating memory\n");
 }

 myarray_decompversion=myarray;
 if (args[ARGS_COMPLEX].is_set) {
  subitemsize=2;
  /* This only changes the length of a component, but not the length of the
   * v or w weight vectors. Access to the `decompversion' version of myarray
   * makes the array look like vectors of interlaced real and complex values,
   * ie of vectors twice as long as before. */
  myarray_decompversion.nr_of_elements*=subitemsize;
  myarray_decompversion.element_skip/=subitemsize;
  array_setreadwrite(&myarray_decompversion);
 }

 for (itempart=0; itempart<tinfo->itemsize; itempart++) {
  int component;
  if (itempart%subitemsize==0) {
   /* For the decomposition, we have only tinfo->itemsize/subitemsize items */
   array_use_item(&myarray_decompversion, itempart/subitemsize);
   array_reset(&myarray_decompversion); array_reset(&w); array_reset(&v);
   array_svdcmp(&myarray_decompversion, &w, &v);
  }
  if (args[ARGS_ONLYMAPS].is_set) {
   array_reset(&v);
   array_use_item(&u, itempart);
   for (component=0; component<ncomps; component++) {
    do {
     array_write(&u, array_scan(&v));
    } while (u.message==ARRAY_CONTINUE);
   }
  } else {
  array_use_item(&myarray, itempart);
  array_reset(&myarray); array_reset(&w); array_reset(&v);
  for (component=0; component<ncomps; component++) {
   /* The role of `waveform' and `channel' is actually reversed if -m is given */
   DATATYPE const waveform_weight=array_scan(&w);
   array_use_item(&u, itempart+component*tinfo->itemsize);
   do {
    DATATYPE const channel_weight=array_scan(&v);
    do {
     array_write(&u, array_scan(&myarray)*waveform_weight*channel_weight);
    } while (u.message==ARRAY_CONTINUE);
    array_previousvector(&myarray);
   } while (u.message==ARRAY_ENDOFVECTOR);
   array_nextvector(&myarray);
  }
  }
 }

 array_free(&v);
 array_free(&w);
 tinfo->itemsize=u.element_skip;
 if (args[ARGS_ONLYMAPS].is_set) {
  tinfo->nr_of_points=ncomps;
  tinfo->multiplexed=TRUE;
  tinfo->beforetrig=0;
  tinfo->sfreq=1;
  free_pointer((void **)&tinfo->xdata);
  tinfo->xchannelname=NULL;
 } else {
  tinfo->multiplexed=args[ARGS_MAPS].is_set;
 }
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 return u.start;
}
/*}}}  */

/*{{{  svdecomp_exit(transform_info_ptr tinfo) {*/
METHODDEF void
svdecomp_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_svdecomp(transform_info_ptr tinfo) {*/
GLOBAL void
select_svdecomp(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &svdecomp_init;
 tinfo->methods->transform= &svdecomp;
 tinfo->methods->transform_exit= &svdecomp_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="svdecomp";
 tinfo->methods->method_description=
  "svdecomp performs a singular value decomposition across channels. By default,\n"
  " it returns a number of items corresponding to different component waveforms;\n"
  " within each item, the waveforms of all channels are equal, only the amplitude\n"
  " is different. This is a somewhat `exploded' but useful view on the SVD result.\n"
  " The argument n_components is the number of SVD components to output.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
