/*
 * Copyright (C) 1996-1999,2001,2003 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * normalize_channelbox 'transform method' setting the main plane of the
 * channel positions to the x-y plane, with the topmost (max z) channel highest
 * in y and the frontmost (max x) highest in x. This is done by three planar
 * rotations about the unit axes after centering on the midpoint.
 * First, the topmost channel is aligned with the y axis by two orthogonal
 * rotations; then, the frontmost channel is rotated about the y axis
 * as to maximize its x component.
 * The method name refers to the fact that for this to succeed, the channel
 * arrangement should fit in a flat box.
 * 						-- Bernd Feige 30.06.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_DECIDE_LEFTRIGHT=0, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Frontmost channel will be lowest in x if y of center of box is positive, ie if the dewar was on the left side of the head", "d", FALSE, NULL}
};

/* posplot accesses this struct to see from which side the view is, so
 * `Bool view_from_left' should remain the first entry */
struct normalize_channelbox_storage {
 Bool view_from_left;
};

/*{{{  normalize_channelbox_init(transform_info_ptr tinfo) {*/
METHODDEF void
normalize_channelbox_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  normalize_channelbox(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
normalize_channelbox(transform_info_ptr tinfo) {
 struct normalize_channelbox_storage *local_arg=(struct normalize_channelbox_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 transform_info_ptr const tinfoptr=tinfo;
 int channel, xmaxchan, xminchan, ymaxchan, yminchan, zmaxchan, zminchan;
 DATATYPE value, value2, xmax, ymax, zmax, xmin, ymin, zmin, xmid, ymid, zmid;
 DATATYPE tval[9];
 double phi;
 array myarray, t;
 local_arg->view_from_left=FALSE;

  /*{{{  Construct array myarray*/
  myarray.start=(DATATYPE *)tinfoptr->probepos;
  myarray.nr_of_elements=3;
  myarray.nr_of_vectors=tinfoptr->nr_of_channels;
  myarray.vector_skip=3;
  myarray.element_skip=1;
  array_setreadwrite_double(&myarray);
  /*}}}  */

  /*{{{  Swap x and y if data set is 'Roma' (x=left->right)*/
  if (strncmp(tinfo->comment, "Roma ", 5)==0) {
   /* This is the trace that vp2mfx leaves in Roma files. Though generality
    * lacks, this seems unavoidable in this module */
   array_reset(&myarray);
   do {
    value=array_scan(&myarray);
    value2=array_scan(&myarray);
    myarray.current_element=0;
    array_write(&myarray, value2);
    array_write(&myarray, -value);
    array_advance(&myarray);
   } while (myarray.message!=ARRAY_ENDOFSCAN);
  }
  /*}}}  */

  /*{{{  Find min and max values of the box*/
  array_reset(&myarray);
  xmax=xmin=array_scan(&myarray);
  ymax=ymin=array_scan(&myarray);
  zmax=zmin=array_scan(&myarray);
  xmaxchan=xminchan=ymaxchan=yminchan=zmaxchan=zminchan=0;
  channel=1;
  do {
   value=array_scan(&myarray);
   if (value>xmax) { xmax=value; xmaxchan=channel; }
   if (value<xmin) { xmin=value; xminchan=channel; }
   value=array_scan(&myarray);
   if (value>ymax) { ymax=value; ymaxchan=channel; }
   if (value<ymin) { ymin=value; yminchan=channel; }
   value=array_scan(&myarray);
   if (value>zmax) { zmax=value; zmaxchan=channel; }
   if (value<zmin) { zmin=value; zminchan=channel; }
   channel++;
  } while (myarray.message!=ARRAY_ENDOFSCAN);
#  ifdef DEBUG
  printf("zmaxchan=%s, xmaxchan=%s\n", tinfoptr->channelnames[zmaxchan], tinfoptr->channelnames[xmaxchan]);
#  endif
  /*}}}  */
  
  /*{{{  Subtract midpoint*/
  xmid=(xmax+xmin)/2; ymid=(ymax+ymin)/2; zmid=(zmax+zmin)/2;
#  ifdef DEBUG
  printf("xmid=%g, ymid=%g, zmid=%g\n", xmid, ymid, zmid);
#  endif
  array_reset(&myarray);
  do {
   array_write(&myarray, READ_ELEMENT(&myarray)-xmid);
   array_write(&myarray, READ_ELEMENT(&myarray)-ymid);
   array_write(&myarray, READ_ELEMENT(&myarray)-zmid);
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  /*}}}  */
  
  /*{{{  Construct array t to be used for rotations*/
  t.start=tval;
  t.nr_of_elements=3;
  t.nr_of_vectors=3;
  t.element_skip=1;
  t.vector_skip=3;
  array_setreadwrite(&t);
  /*}}}  */
  
  /*{{{  rotate zmaxchan about the x axis by its angle to the y axis (maximize y)*/
  /* By this rotation, the z component of zmaxchan is removed. */
  array_reset(&t);
  myarray.current_vector=zmaxchan;
  myarray.current_element=1;
  value=array_scan(&myarray);	/* y value */
  value2=READ_ELEMENT(&myarray);	/* z value */
  value/=sqrt(value*value+value2*value2);
  phi=(value2>0 ? -acos(value) : acos(value));
#  ifdef DEBUG
  printf("z=%g, cos phi=%g=%g, phi=%g\n", value2, value, cos(phi), phi);
#  endif
  array_write(&t, 1.0);
  array_write(&t, 0.0);
  array_write(&t, 0.0);

  array_write(&t, 0.0);
  array_write(&t, cos(phi));
  array_write(&t, sin(phi));

  array_write(&t, 0.0);
  array_write(&t, -sin(phi));
  array_write(&t, cos(phi));

  array_transpose(&t);
  array_reset(&myarray);
  do {
   DATATYPE x, y, z;
   x=array_multiply(&t, &myarray, MULT_SAMESIZE);
   y=array_multiply(&t, &myarray, MULT_SAMESIZE);
   z=array_multiply(&t, &myarray, MULT_SAMESIZE);
   array_previousvector(&myarray);
   array_write(&myarray, x);
   array_write(&myarray, y);
   array_write(&myarray, z);
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  /*}}}  */
  
  /*{{{  rotate zmaxchan about the z axis by its angle to the y axis (maximize y)*/
  /* Now remove the x component of zmaxchan. Coordinates should be 0, z, 0 */
  array_reset(&t);
  myarray.current_element=0;
  myarray.current_vector=zmaxchan;
  value2=array_scan(&myarray);	/* x value */
  value=READ_ELEMENT(&myarray); /* y value */
  value/=sqrt(value*value+value2*value2);
  phi=(value2>0 ? -acos(value) : acos(value));
#  ifdef DEBUG
  printf("z=%g, cos phi=%g=%g, phi=%g\n", value2, value, cos(phi), phi);
#  endif
  array_write(&t, cos(phi));
  array_write(&t, sin(phi));
  array_write(&t, 0.0);

  array_write(&t, -sin(phi));
  array_write(&t, cos(phi));
  array_write(&t, 0.0);

  array_write(&t, 0.0);
  array_write(&t, 0.0);
  array_write(&t, 1.0);

  if (value2<0.0) array_transpose(&t);
  array_reset(&myarray);
  do {
   DATATYPE x, y, z;
   x=array_multiply(&t, &myarray, MULT_SAMESIZE);
   y=array_multiply(&t, &myarray, MULT_SAMESIZE);
   z=array_multiply(&t, &myarray, MULT_SAMESIZE);
   array_previousvector(&myarray);
   array_write(&myarray, x);
   array_write(&myarray, y);
   array_write(&myarray, z);
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  /*}}}  */

  /*{{{  rotate xmaxchan about the y axis by its angle to the x axis (maximize x)*/
  /* This is the only rotation that will leave zmaxchan at its position */
  array_reset(&t);
  myarray.current_element=0;
  myarray.current_vector=(args[ARGS_DECIDE_LEFTRIGHT].is_set && ymid>0.0 ? local_arg->view_from_left=TRUE,xminchan : xmaxchan);
  value=array_scan(&myarray);	/* x value */
  array_advance(&myarray);
  value2=READ_ELEMENT(&myarray); /* z value */
  value/=sqrt(value*value+value2*value2);
  phi=(value2>0 ? -acos(value) : acos(value));
#  ifdef DEBUG
  printf("z=%g, cos phi=%g=%g, phi=%g\n", value2, value, cos(phi), phi);
#  endif
  array_write(&t, cos(phi));
  array_write(&t, 0.0);
  array_write(&t, -sin(phi));

  array_write(&t, 0.0);
  array_write(&t, 1.0);
  array_write(&t, 0.0);

  array_write(&t, sin(phi));
  array_write(&t, 0.0);
  array_write(&t, cos(phi));

  if (value2<0.0) array_transpose(&t);
  array_reset(&myarray);
  do {
   DATATYPE x, y, z;
   x=array_multiply(&t, &myarray, MULT_SAMESIZE);
   y=array_multiply(&t, &myarray, MULT_SAMESIZE);
   z=array_multiply(&t, &myarray, MULT_SAMESIZE);
   array_previousvector(&myarray);
   array_write(&myarray, x);
   array_write(&myarray, y);
   array_write(&myarray, z);
  } while (myarray.message!=ARRAY_ENDOFSCAN);
  /*}}}  */

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  normalize_channelbox_exit(transform_info_ptr tinfo) {*/
METHODDEF void
normalize_channelbox_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_normalize_channelbox(transform_info_ptr tinfo) {*/
GLOBAL void
select_normalize_channelbox(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &normalize_channelbox_init;
 tinfo->methods->transform= &normalize_channelbox;
 tinfo->methods->transform_exit= &normalize_channelbox_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="normalize_channelbox";
 tinfo->methods->method_description=
  "`Transform method' setting the main plane of the channel positions\n"
  "to the x-y plane, with the topmost channel highest in y and the\n"
  "frontmost highest in x\n";
 tinfo->methods->local_storage_size=sizeof(struct normalize_channelbox_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
