// Copyright (C) 1997-1999,2001,2005,2008,2010,2011,2013-2015 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
/*{{{}}}*/
/*{{{  Description*/
/*
 * laplacian.cc method to perform the Laplacian operation on the incoming epochs.
 *  The algorithm is roughly constructed after \cite{Le:94a} (EacN 92:433--441):
 *  The eletrode set is triangulated and those points around which a closed
 *  path can be constructed are used for a local planar surface. The Taylor-
 *  expansion parameters (the derivatives up to second order) are then estimated
 *  by solving a set of linear equations by SVD backsubstitution. The second-
 *  order derivatives are finally summed and output as the Laplacian estimation.
 *	-- Bernd Feige 26.12.1996 (draft), 7.04.1997 (first version)
 */
/*}}}  */

/*{{{  #includes*/
#include "TriAn/LinkedObject.hh"
#include "TriAn/Point.hh"
#include "TriAn/Points.hh"
#include "TriAn/TPoints.hh"
#include "TriAn/Triangle.hh"
#include "TriAn/Triangls.hh"
#include "TriAn/ConvexMesh.hh"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "array.h"
#include "bf.h"
#include "growing_buf.h"
}
/*}}}  */

LOCAL const char *method_choice[]={
 "-N",
 "-R",
 "-D",
 NULL
};
enum laplacian_variants_enum {
 METHOD_NORMAL=0,
 METHOD_LOCAL_REFERENCE,
 METHOD_ALL_DERIVATIVES
};
enum ARGS_ENUM {
 ARGS_METHOD_CHOICE=0, 
 ARGS_OUTPUT_MATRIX, 
 ARGS_CHANNELNAMES, 
 NR_OF_ARGUMENTS
};
LOCAL enum laplacian_variants_enum laplacian_variant;
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SELECTION, "Normal operation (-N), just do local reference (-R) or write all derivatives as single output items (-D)", " ", METHOD_NORMAL, method_choice},
 {T_ARGS_TAKES_FILENAME, "outfile: Save the spatial filter matrix in MatLab format", "M", ARGDESC_UNUSED, (char **)"*.mat"},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names (just copy the rest)", "n", ARGDESC_UNUSED, NULL},
};

/* There are 2 first and 2 second derivatives in the planar case */
#define NR_OF_TAYLOR_PARAMETERS 4

/*{{{  Local definitions*/
class LaplacianChannels : public LinkedObject {
public:
 LaplacianChannels();
 LaplacianChannels(transform_info_ptr tinfo, int chan, TPoints* tp, Boundary* surrounding);
 ~LaplacianChannels();
 void calculate_x(void);
 void calculate_x(array* indata);
 void write_laplacian(array* indata, array* outdata);
 void construct_matrix(array* D);
 int channel;
private:
// A pointer into local_arg->tp (not our own copy!):
 TPoints* tp;
// A list of channels in the input data set surrounding this channel
 int n_surrounding;
 int *surrounding_channels;
 array u;	/* Arrays for the SVD solution of Ux=b */
 array v;
 array w;
 array prod;
 array b;
 array x;
};
#define TOL (1e-5)

struct laplacian_storage {
 int *channel_list;
 Bool have_channel_list;
 /* This lists the active channels in the order they are added to tp; used
  * to transform the channel numbers in tp back into input channel numbers:
  * (Can't use channel_list because that could contain duplicates) */
 int *active_channels_list;
 TPoints* tp;
 ConvexMesh* cm;
 int nr_of_laplacian_channels;
 int nr_of_copied_channels;
 LaplacianChannels* laplacian_channels;
};

/*{{{  Implementation of LaplacianChannels*/
LaplacianChannels::LaplacianChannels() {
 channel=n_surrounding=0;
 surrounding_channels=NULL;
}
LaplacianChannels::LaplacianChannels(transform_info_ptr tinfo, int chan, TPoints* tpoint, Boundary* surrounding) {
 /*{{{  Construct all temporary space and perform the SVD on the distance matrix*/
 struct laplacian_storage *local_arg=(struct laplacian_storage *)tinfo->methods->local_storage;
 DATATYPE hold, threshold;
 int i;
 Triangles* const tr=(Triangles*)local_arg->cm->tr->first();
 Triangles* thistr=tr;
 Point normal, ex, ey;

 channel=chan;
 tp=tpoint;
 n_surrounding=surrounding->nr_of_members();
 surrounding_channels=new int[n_surrounding];

 /* What follows is actually the first part of array_svd_solution().
  * We prepare the matrix U of linear and squared dx and dy distances here and
  * do the SVD once; in calculate_x(), we derive the solution for specified
  * inhomogenity (b) by backsubstitution. (equation: U x = b) */

 if (laplacian_variant==METHOD_LOCAL_REFERENCE) {
  b.nr_of_elements=n_surrounding;
  x.nr_of_elements=1;
  x.nr_of_vectors=b.nr_of_vectors=1;
  x.element_skip=b.element_skip=1;
  if (array_allocate(&x)==NULL || array_allocate(&b)==NULL) {
   ERREXIT(tinfo->emethods, "LaplacianChannels: Error allocating arrays\n");
  }
  /* Take care that array_free won't harm when called on the unused arrays: */
  u.start=v.start=w.start=prod.start=NULL;
  surrounding=(Boundary*)surrounding->first();
  i=0;
  while (surrounding!=(Boundary*)Empty) {
   surrounding_channels[i]=local_arg->active_channels_list[((TPoints*)surrounding->point())->before_members()];
   i++;
   surrounding=(Boundary*)surrounding->next();
  }
 } else {
 u.nr_of_elements=b.nr_of_elements=n_surrounding;
 x.nr_of_elements=u.nr_of_vectors=v.nr_of_vectors=v.nr_of_elements=w.nr_of_elements=prod.nr_of_elements=NR_OF_TAYLOR_PARAMETERS;
 x.nr_of_vectors=b.nr_of_vectors=w.nr_of_vectors=prod.nr_of_vectors=1;
 x.element_skip=b.element_skip=u.element_skip=v.element_skip=w.element_skip=prod.element_skip=1;
 if (array_allocate(&x)==NULL || array_allocate(&b)==NULL || array_allocate(&u)==NULL || array_allocate(&v)==NULL || array_allocate(&w)==NULL || array_allocate(&prod)==NULL) {
  ERREXIT(tinfo->emethods, "LaplacianChannels: Error allocating arrays\n");
 }

 /* Construct a 2-dimensional coordinate system normal to the mean of all
  * adjacent triangle normals */
 while ((thistr=thistr->find(tp))!=(Triangles*)Empty) {
  normal=normal+thistr->normal();
  thistr=(Triangles*)thistr->next();
 }
 normal.normalize();
 ex= *tr->p2 - *tp; ex= ex - ex*(ex*normal); ex.normalize();
 ey=normal.cross(ex);

 /* Build the matrix U */
 array_transpose(&u);
 surrounding=(Boundary*)surrounding->first();
 i=0;
 while (surrounding!=(Boundary*)Empty) {
  Point const dp= *surrounding->point()- *tp;
  DATATYPE const dx=ex*dp;
  DATATYPE const dy=ey*dp;
  array_write(&u, dx);
  array_write(&u, dy);
  array_write(&u, dx*dx*0.5);
  array_write(&u, dy*dy*0.5);

  surrounding_channels[i]=local_arg->active_channels_list[((TPoints*)surrounding->point())->before_members()];
  i++;
  surrounding=(Boundary*)surrounding->next();
 }
 array_transpose(&u);

 array_svdcmp(&u, &w, &v);
 array_reset(&u); array_reset(&w); array_reset(&v);
 threshold=TOL*array_max(&w);
 array_transpose(&v);	/* Correct order for the multiplication v*prod in calculate_x() */
 do {
  hold=READ_ELEMENT(&w);
  if (hold<threshold) {
   hold=0.0;
  } else {
   hold=1.0/hold;
  }
  array_write(&w, hold);
 } while (w.message!=ARRAY_ENDOFSCAN);
 array_setto_diagonal(&w);
 }
 /*}}}  */
}
LaplacianChannels::~LaplacianChannels() {
 if (surrounding_channels!=NULL) {
  delete[] surrounding_channels;
  array_free(&x); array_free(&b); array_free(&u); array_free(&v); array_free(&w); array_free(&prod);
 }
}
void LaplacianChannels::calculate_x(void) {
 /*{{{  Calculate the derivatives by backsubstitution given the inhomogenity b*/
 array_reset(&x);
 if (laplacian_variant==METHOD_LOCAL_REFERENCE) {
  array_write(&x, array_mean(&b));
  return;
 }
 /* Calculate U'*b */
 do {
  array_write(&x, array_multiply(&u, &b, MULT_VECTOR));
 } while (u.message!=ARRAY_ENDOFSCAN);
 /* Calculate W*(U'*b) */
 do {
  array_write(&prod, array_multiply(&w, &x, MULT_VECTOR));
 } while (w.message!=ARRAY_ENDOFSCAN);
 /* Calculate x=V*W*(U'*b) */
 do {
  array_write(&x, array_multiply(&v, &prod, MULT_VECTOR));
 } while (v.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
}
void LaplacianChannels::calculate_x(array* indata) {
 /*{{{  Calculate the derivatives by backsubstitution of the actual potentials*/
 /* Note that indata->current_vector is not modified. */
 /* The solution in x is (dP/dx, dP/dy, d^{2}P/dx^2, d^{2}P/dy^2) */
 /* Fill the potential data vector b from indata */
 DATATYPE midpoint_value;
 indata->current_element=channel;
 midpoint_value=READ_ELEMENT(indata);
 array_reset(&b);
 do {
  indata->current_element=surrounding_channels[b.current_element];
  /* A plus towards the central point should be counted positive */
  array_write(&b, midpoint_value-READ_ELEMENT(indata));
 } while (b.message==ARRAY_CONTINUE);
 /* Perform the backsubstitution */
 calculate_x();
 /*}}}  */
}
void LaplacianChannels::write_laplacian(array* indata, array* outdata) {
 /* This method processes ALL LaplacianChannels connected to the one it
  * is called upon, not only THIS one. 
  * Only the current vector of indata is accessed, and one vector of
  * outdata written and advanced. */
 LaplacianChannels* laplacian_channels=(LaplacianChannels*)this->first();
 while (laplacian_channels!=(LaplacianChannels*)Empty) {
  DATATYPE ddx, ddy;
  laplacian_channels->calculate_x(indata);
  switch (laplacian_variant) {
   case METHOD_NORMAL:
    /* Sum up only the second derivatives */
    laplacian_channels->x.current_element=2; // Second derivative in x direction
    ddx=array_scan(&laplacian_channels->x);
    ddy=array_scan(&laplacian_channels->x);
    array_write(outdata, ddx+ddy);
    break;
   case METHOD_LOCAL_REFERENCE:
    array_write(outdata, array_scan(&laplacian_channels->x));
    break;
   case METHOD_ALL_DERIVATIVES:
    /* Write all derivatives as independent items of the output */
    array_reset(&laplacian_channels->x);
    do {
     array_use_item(outdata, laplacian_channels->x.current_element);
     WRITE_ELEMENT(outdata, array_scan(&laplacian_channels->x));
    } while (laplacian_channels->x.message==ARRAY_CONTINUE);
    array_advance(outdata);
    break;
  }
  laplacian_channels=(LaplacianChannels*)laplacian_channels->next();
 }
}
void LaplacianChannels::construct_matrix(array* D) {
 /* This method processes ALL LaplacianChannels connected to the one it
  * is called upon, not only THIS one.
  * What we are doing here is the following: Instead of with the inhomogenity
  * (difference) vectors themselves, we do the backsubstitution with each
  * column of the matrix that would create the difference vector if right-
  * multiplied with the potential values. Done for each LaplacianChannel, we
  * obtain one row for each output channel.
  */
 LaplacianChannels* laplacian_channels=(LaplacianChannels*)this->first();

 while (laplacian_channels!=(LaplacianChannels*)Empty) {
  DATATYPE ddx, ddy;

  do {
   array_reset(&laplacian_channels->b);
   if (D->current_element==laplacian_channels->channel) {
    do {
     array_write(&laplacian_channels->b, 1.0);
    } while (laplacian_channels->b.message==ARRAY_CONTINUE);
   } else {
    do {
     if (D->current_element==laplacian_channels->surrounding_channels[laplacian_channels->b.current_element]) {
      array_write(&laplacian_channels->b, -1.0);
     } else {
      array_write(&laplacian_channels->b, 0.0);
     }
    } while (laplacian_channels->b.message==ARRAY_CONTINUE);
   }
   laplacian_channels->calculate_x();

   switch (laplacian_variant) {
    case METHOD_NORMAL:
    case METHOD_ALL_DERIVATIVES:
     /* Sum up only the second derivatives */
     laplacian_channels->x.current_element=2; // Second derivative in x direction
     ddx=array_scan(&laplacian_channels->x);
     ddy=array_scan(&laplacian_channels->x);
     array_write(D, ddx+ddy);
     break;
    case METHOD_LOCAL_REFERENCE:
     array_write(D, array_scan(&laplacian_channels->x));
     break;
   }
  } while (D->message==ARRAY_CONTINUE);

  laplacian_channels=(LaplacianChannels*)laplacian_channels->next();
 }
}
/*}}}  */
/*}}}  */

/*{{{  laplacian_init(transform_info_ptr tinfo)*/
METHODDEF void
laplacian_init(transform_info_ptr tinfo) {
 struct laplacian_storage *local_arg=(struct laplacian_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int channel, tpoint;
 TPoints* tp;

 if (args[ARGS_CHANNELNAMES].is_set) {
  /* Note that this is NULL if no channel matched, which is why we need have_channel_list as well... */
  local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_CHANNELNAMES].arg.s);
  local_arg->have_channel_list=TRUE;
 } else {
  local_arg->channel_list=NULL;
  local_arg->have_channel_list=FALSE;
 }
 if (args[ARGS_METHOD_CHOICE].is_set) {
  laplacian_variant=(enum laplacian_variants_enum)args[ARGS_METHOD_CHOICE].arg.i;
 } else {
  laplacian_variant=METHOD_NORMAL;
 }

 /*{{{  Count active and `passive' (just copied) channels*/
 tpoint=0;
 local_arg->nr_of_copied_channels=0;
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  if (!local_arg->have_channel_list || is_in_channellist(channel+1, local_arg->channel_list)) {
   tpoint++;
  } else {
   local_arg->nr_of_copied_channels++;
  }
 }
 if (tpoint<5) {
  ERREXIT1(tinfo->emethods, "laplacian_init: Too few channels (%d<5) to derive a laplacian estimation.\n", MSGPARM(tpoint));
 }
 /*}}}  */
 /*{{{  Calculate convex mesh*/
 if ((local_arg->active_channels_list=(int *)malloc(tpoint*sizeof(int)))==NULL) {
  ERREXIT(tinfo->emethods, "laplacian: Error allocating active_channels_list\n");
 }
 local_arg->tp=(TPoints*)Empty;
 tpoint=0;
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  if (!local_arg->have_channel_list || is_in_channellist(channel+1, local_arg->channel_list)) {
   local_arg->tp=(TPoints*)local_arg->tp->addmember(new TPoints(tinfo->probepos[3*channel], tinfo->probepos[3*channel+1], tinfo->probepos[3*channel+2]));
   local_arg->active_channels_list[tpoint]=channel;
   tpoint++;
  }
 }
 local_arg->cm=new ConvexMesh(local_arg->tp);
 /*}}}  */
          
 /*{{{  Construct local surfaces and initialize LaplacianChannels*/
 tp=(TPoints*)local_arg->tp->first();
 local_arg->laplacian_channels=(LaplacianChannels*)Empty;
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  if (!local_arg->have_channel_list || is_in_channellist(channel+1, local_arg->channel_list)) {
   Boundary* surrounding=(Boundary*)Empty;
   Triangles* tr=((Triangles*)local_arg->cm->tr->first())->find(tp);
   if (tr==(Triangles*)Empty) {
    ERREXIT1(tinfo->emethods, "laplacian_init: Can't find a triangle for channel %d.\n", MSGPARM(channel));
   }
   tr->make_firstpoint(tp);
   surrounding=(Boundary*)surrounding->addmember(new Boundary(tr->p2));
   surrounding=(Boundary*)surrounding->addmember(new Boundary(tr->p3));
   while (TRUE) {
    Point* p3=tr->p3;
    Triangles* newtr=((Triangles*)local_arg->cm->tr->first())->find(tp,p3);

    if (newtr!=(Triangles*)Empty && newtr==tr) {
     newtr=((Triangles*)tr->next())->find(tp,p3);
    }
    tr=newtr;
    if (tr==(Triangles*)Empty) break;
    tr->make_firstpoint(tp);
    if (((Boundary*)surrounding->first())->find(tr->p3)!=(Boundary*)Empty) break;
    surrounding=(Boundary*)surrounding->addmember(new Boundary(tr->p3));
   }

   if (tr!=(Triangles*)Empty) {
    local_arg->laplacian_channels=(LaplacianChannels*)local_arg->laplacian_channels->addmember(new LaplacianChannels(tinfo, channel, tp, surrounding));
   }

   delete surrounding;
   tp=(TPoints*)tp->next();
  }
 }
 local_arg->laplacian_channels=(LaplacianChannels*)local_arg->laplacian_channels->first();
 local_arg->nr_of_laplacian_channels=local_arg->laplacian_channels->nr_of_members();
 /*}}}  */

 if (args[ARGS_OUTPUT_MATRIX].is_set) {
  FILE *outfile;

  if (strcmp(args[ARGS_OUTPUT_MATRIX].arg.s, "stdout")==0) {
   outfile=stdout;
  } else {
   if ((outfile=fopen(args[ARGS_OUTPUT_MATRIX].arg.s, "w"))==NULL) {
    ERREXIT1(tinfo->emethods, "laplacian_init: Error creating matrix file %s.\n", MSGPARM(args[ARGS_OUTPUT_MATRIX].arg.s));
   }
  }
  fprintf(outfile, "Original channelnames:\n");
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   fprintf(outfile, " %s", tinfo->channelnames[channel]);
  }
  fprintf(outfile, "\nTransformed channelnames:\n");
  LaplacianChannels* laplacian_channels=local_arg->laplacian_channels;
  do {
   fprintf(outfile, " %s", tinfo->channelnames[laplacian_channels->channel]);
   laplacian_channels=(LaplacianChannels*)laplacian_channels->next();
  } while (laplacian_channels!=(LaplacianChannels*)Empty);
  if (local_arg->have_channel_list) {
   for (channel=0; channel<tinfo->nr_of_channels; channel++) {
    if (!is_in_channellist(channel+1, local_arg->channel_list)) {
     fprintf(outfile, " %s", tinfo->channelnames[channel]);
    }
   }
  }
  fprintf(outfile, "\n");

  array D;
  D.nr_of_vectors=local_arg->nr_of_laplacian_channels+local_arg->nr_of_copied_channels;
  D.nr_of_elements=tinfo->nr_of_channels;
  D.element_skip=1;
  if (array_allocate(&D)==NULL) {
   ERREXIT(tinfo->emethods, "laplacian_init: Error allocating matrix D\n");
  }
  local_arg->laplacian_channels->construct_matrix(&D);
  /* construct_matrix will only fill the LaplacianChannel part of the matrix: */
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   if (!local_arg->have_channel_list && !is_in_channellist(channel+1, local_arg->channel_list)) {
    D.current_element=channel;
    WRITE_ELEMENT(&D, 1.0);
    array_nextvector(&D);
   }
  }
  array_dump(outfile, &D, ARRAY_MATLAB);
  if (outfile!=stdout) fclose(outfile);

  array_free(&D);
 }
 
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  laplacian(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
laplacian(transform_info_ptr tinfo) {
 struct laplacian_storage *local_arg=(struct laplacian_storage *)tinfo->methods->local_storage;
 array indata, outdata;
 LaplacianChannels* laplacian_channels=local_arg->laplacian_channels;
 char **new_channelnames=NULL, *in_buffer=NULL;
 int stringlength=0;
 double *new_probepos=NULL;
 int const output_itemsize=(laplacian_variant==METHOD_ALL_DERIVATIVES ? NR_OF_TAYLOR_PARAMETERS : 1);
 int const nr_of_output_channels=local_arg->nr_of_laplacian_channels+local_arg->nr_of_copied_channels;
 int channel, output_channel;

 /*{{{  Initialize output data*/
 /*{{{  Count the characters in the output channel names*/
 do {
  stringlength+=strlen(tinfo->channelnames[laplacian_channels->channel])+1;
  laplacian_channels=(LaplacianChannels*)laplacian_channels->next();
 } while (laplacian_channels!=(LaplacianChannels*)Empty);
 if (local_arg->have_channel_list) {
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   if (!is_in_channellist(channel+1, local_arg->channel_list)) {
    stringlength+=strlen(tinfo->channelnames[channel])+1;
   }
  }
 }
 /*}}}  */

 outdata.nr_of_elements=nr_of_output_channels;
 outdata.nr_of_vectors=tinfo->nr_of_points;
 outdata.element_skip=output_itemsize;
 if (array_allocate(&outdata)==NULL || 
     (new_channelnames=(char **)malloc(nr_of_output_channels*sizeof(char *)))==NULL ||
     (in_buffer=(char *)malloc(stringlength))==NULL ||
     (new_probepos=(double *)malloc(3*nr_of_output_channels*sizeof(double)))==NULL) {
  ERREXIT(tinfo->emethods, "laplacian: Error allocating output memory\n");
 }
 output_channel=0;
 laplacian_channels=local_arg->laplacian_channels;
 do {
  channel=laplacian_channels->channel;
  new_channelnames[output_channel]=in_buffer;
  strcpy(in_buffer, tinfo->channelnames[channel]);
  in_buffer=in_buffer+strlen(in_buffer)+1;
  new_probepos[3*output_channel  ]=tinfo->probepos[3*channel  ];
  new_probepos[3*output_channel+1]=tinfo->probepos[3*channel+1];
  new_probepos[3*output_channel+2]=tinfo->probepos[3*channel+2];
  output_channel++;
  laplacian_channels=(LaplacianChannels*)laplacian_channels->next();
 } while (laplacian_channels!=(LaplacianChannels*)Empty);

 if (local_arg->have_channel_list) {
  for (channel=0; channel<tinfo->nr_of_channels; channel++) {
   if (!is_in_channellist(channel+1, local_arg->channel_list)) {
    new_channelnames[output_channel]=in_buffer;
    strcpy(in_buffer, tinfo->channelnames[channel]);
    in_buffer=in_buffer+strlen(in_buffer)+1;
    new_probepos[3*output_channel  ]=tinfo->probepos[3*channel  ];
    new_probepos[3*output_channel+1]=tinfo->probepos[3*channel+1];
    new_probepos[3*output_channel+2]=tinfo->probepos[3*channel+2];
    output_channel++;
   }
  }
 }
 /*}}}  */

 tinfo_array(tinfo, &indata);
 array_transpose(&indata);	/* Vectors are maps */
 
 laplacian_channels=local_arg->laplacian_channels;
 do {
  /* Fill the data array by doing backsubstitution with the SVD info */
  laplacian_channels->write_laplacian(&indata, &outdata);
  if (local_arg->have_channel_list) {
   for (channel=0; channel<tinfo->nr_of_channels; channel++) {
    if (!is_in_channellist(channel+1, local_arg->channel_list)) {
     indata.current_element=channel;
     array_use_item(&outdata, 0);
     array_write(&outdata, READ_ELEMENT(&indata));
    }
   }
  }
  array_nextvector(&indata);
 } while (outdata.message!=ARRAY_ENDOFSCAN);

 free_pointer((void **)&tinfo->channelnames[0]);
 free_pointer((void **)&tinfo->channelnames);
 free_pointer((void **)&tinfo->probepos);
 tinfo->nr_of_channels=nr_of_output_channels;
 tinfo->itemsize=output_itemsize;
 tinfo->multiplexed=TRUE;
 tinfo->channelnames=new_channelnames;
 tinfo->probepos=new_probepos;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 return outdata.start;
}
/*}}}  */

/*{{{  laplacian_exit(transform_info_ptr tinfo)*/
METHODDEF void
laplacian_exit(transform_info_ptr tinfo) {
 struct laplacian_storage *local_arg=(struct laplacian_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->channel_list);
 free_pointer((void **)&local_arg->active_channels_list);
 local_arg->laplacian_channels->delall();
 local_arg->laplacian_channels=(LaplacianChannels*)Empty;
 delete local_arg->cm;
 local_arg->cm=(ConvexMesh*)Empty;
 local_arg->tp->delall();
 local_arg->tp=(TPoints*)Empty;

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_laplacian(transform_info_ptr tinfo)*/
GLOBAL void
select_laplacian(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &laplacian_init;
 tinfo->methods->transform= &laplacian;
 tinfo->methods->transform_exit= &laplacian_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="laplacian";
 tinfo->methods->method_description=
  "Transform method to perform the Laplacian operation on the incoming epochs.\n"
  " The algorithm is roughly constructed after \\cite{Le:94a} (EacN 92:433--441):\n"
  " The electrode set is triangulated and those points around which a closed\n"
  " path can be constructed are used for a local planar surface. The Taylor-\n"
  " expansion parameters (the derivatives up to second order) are then estimated\n"
  " by solving a set of linear equations by SVD backsubstitution. The second-\n"
  " order derivatives are finally summed and output as the Laplacian estimation.\n";
 tinfo->methods->local_storage_size=sizeof(struct laplacian_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
