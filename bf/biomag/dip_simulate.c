/*
 * Copyright (C) 1996-1999,2001,2003,2010,2013,2014,2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * dip_simulate.c module to simulate epochs using variable--strength dipoles
 *	-- Bernd Feige 2.06.1993
 *
 * dip_simulate is a method of the get_epoch type (ie it sets channel names
 * and allocates the tsdata memory, which it fills with data) with some
 * peculiarities inherent to simulation methods:
 *
 * - The source description must be initialized and a pointer to it
 *   assigned to local_arg->source. In our case, the source description
 *   consists of an array of 'dipole descriptions' of which each has
 *   a position and momentum, some work variables and a 'time' method
 *   to imprint a specified time behavior on the dipole. The 'time' method
 *   may modify the dipole momentum in any way desired; note that in the
 *   current efficient 'lead field' implementation, changes of position
 *   do NOT have any effect, because the actual lead field is calculated
 *   by dip_simulate_init.
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "biomag.h"
#include "growing_buf.h"
/*}}}  */

/*{{{  Defines*/
/* The sensor placement description ... */
#define NRING 3
#define SPAN 63.

#define XCHANNELNAME "Time[s]"

#define CMNTLEN 80
#define CMNTCONST "dip_simulate output"
/*}}}  */

LOCAL const char *const module_names[]={
 "eg_source",
 "var_random_dipoles",
 "ramping_dipoles",
 NULL
};
LOCAL METHOD(struct source_desc *, (dipole_sim_modules[]), (transform_info_ptr tinfo, char **args))={
 &eg_dip_srcmodule,
 &var_random_srcmodule,
 &ramping_srcmodule,
 NULL
};

enum ARGS_ENUM {
 ARGS_SFREQ=0, 
 ARGS_EPOCHS, 
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 ARGS_MODULENAME,
 ARGS_MODULEARGS,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_DOUBLE, "sampling_freq", "", 100, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_epochs", "", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", "", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_SELECTION, "sim_module_name", "", 0, module_names},
 {T_ARGS_TAKES_SENTENCE, "sim_module_args", " ", ARGDESC_UNUSED, (const char *const *)"(  )"}
};

/*{{{  Define the local storage struct*/
struct dip_simulate_storage {
 struct transform_info_struct tinfo;
 long epochs;
 int current_epoch;
 struct source_desc *source;
};
/*}}}  */ 

/*{{{  dip_simulate_init(transform_info_ptr tinfo) {*/
METHODDEF void
dip_simulate_init(transform_info_ptr tinfo) {
 struct dip_simulate_storage *local_arg=(struct dip_simulate_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char **subargs=NULL;
 int ier, dip;
 array r, u;
#ifdef GRADIOMETER_LENGTH
 array a2, r2;
#endif
 struct dipole_desc *dipole;
 double *pos;
 growing_buf buf,tokenbuf,transferbuf;

 memcpy(&local_arg->tinfo, tinfo, sizeof(struct transform_info_struct));
 local_arg->epochs=args[ARGS_EPOCHS].arg.i;
 local_arg->current_epoch=0;
 local_arg->tinfo.sfreq=args[ARGS_SFREQ].arg.d;
 local_arg->tinfo.beforetrig=gettimeslice(&local_arg->tinfo, args[ARGS_BEFORETRIG].arg.s);
 local_arg->tinfo.aftertrig=gettimeslice(&local_arg->tinfo, args[ARGS_AFTERTRIG].arg.s);
 local_arg->tinfo.nr_of_points=local_arg->tinfo.beforetrig+local_arg->tinfo.aftertrig;
 if (local_arg->tinfo.nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "dip_simulate: Invalid nr_of_points %d\n", MSGPARM(local_arg->tinfo.nr_of_points));
 }
 if (args[ARGS_MODULEARGS].is_set) {
  int i, n=0;
  growing_buf_init(&buf);
  growing_buf_takethis(&buf, args[ARGS_MODULEARGS].arg.s);
  growing_buf_init(&tokenbuf);
  growing_buf_allocate(&tokenbuf,0);
  growing_buf_init(&transferbuf);
  growing_buf_allocate(&transferbuf,0);
  if (!growing_buf_get_firsttoken(&buf,&tokenbuf) || strcmp(tokenbuf.buffer_start, "(")!=0) {
   ERREXIT(tinfo->emethods, "dip_simulate_init: Opening bracket expected\n");
  }
  while (growing_buf_get_nexttoken(&buf,&tokenbuf) && strcmp(tokenbuf.buffer_start, ")")!=0) {
   n++;
  }
  if (strcmp(tokenbuf.buffer_start, ")")!=0) {
   ERREXIT(tinfo->emethods, "dip_simulate_init: Closing bracket expected\n");
  }
  if ((subargs=(char **)malloc((n+1)*sizeof(char *)))==NULL) {
   ERREXIT(tinfo->emethods, "dip_simulate_init: Error allocating subargs memory\n");
  }
  growing_buf_get_firsttoken(&buf,&tokenbuf);
  growing_buf_get_nexttoken(&buf,&tokenbuf);
  for (i=0; i<n; i++) {
   /* We only note the offsets here, in case transferbuf gets reallocated */
   subargs[i]=(char *)transferbuf.current_length;
   /* This includes the trailing '\0': */
   growing_buf_append(&transferbuf,tokenbuf.buffer_start,strlen(tokenbuf.buffer_start)+1);
   growing_buf_get_nexttoken(&buf,&tokenbuf);
  }
  /* Fix up the pointers */
  for (i=0; i<n; i++) {
   subargs[i]=transferbuf.buffer_start+(long int)subargs[i];
  }
  subargs[n]=NULL;
 }
 /* subargs are only assessed in this setup call, we free them directly afterwards */
 local_arg->source=(*dipole_sim_modules[args[ARGS_MODULENAME].arg.i])(&local_arg->tinfo, subargs);
 if (args[ARGS_MODULEARGS].is_set) {
  free((void *)subargs);
  growing_buf_free(&transferbuf);
  growing_buf_free(&tokenbuf);
  growing_buf_free(&buf);
 }

 /*{{{  Malloc and settings*/
 btimontage(&local_arg->tinfo, NRING, SPAN, SENSOR_CURVATURE, NULL, NULL);

 /* Allocate leadfield memory */
 for (dip=local_arg->source->nrofsources, dipole=local_arg->source->dipoles; dip>0; dip--, dipole++) {
  dipole->leadfield.nr_of_vectors=local_arg->tinfo.nr_of_channels;
  dipole->leadfield.nr_of_elements=3;
  dipole->leadfield.element_skip=1;
  if (array_allocate(&dipole->leadfield)==NULL) {
   ERREXIT(tinfo->emethods, "dip_simulate_init: Error allocating leadfield memory\n");
  }
 }
 local_arg->tinfo.comment=CMNTCONST;

 /*{{{  Set all sensor positions and gradiometer directions*/
 u.nr_of_vectors=r.nr_of_vectors=local_arg->tinfo.nr_of_channels;
 u.nr_of_elements=r.nr_of_elements=3;
 u.element_skip=r.element_skip=1;
 if (array_allocate(&u)==NULL || array_allocate(&r)==NULL) {
  ERREXIT(tinfo->emethods, "dip_simulate_init: Error allocating array memory\n");
 }

 pos=local_arg->tinfo.probepos;
 do {
  array_write(&r, (*pos)*100);
  array_write(&u, (*pos)*100/SENSOR_CURVATURE);
  pos++;
 } while (r.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
 /*}}}  */

 /*{{{  Initialize the leadfield of each dipole*/
# ifdef GRADIOMETER_LENGTH
 /*{{{  Initialize temp storage a2 (leadfield) and r2 (upper sensor positions)*/
 r2.nr_of_vectors=a2.nr_of_vectors=local_arg->tinfo.nr_of_channels;
 r2.nr_of_elements=a2.nr_of_elements=3;
 r2.element_skip=a2.element_skip=1;

 if (array_allocate(&a2)==NULL || array_allocate(&r2)==NULL) {
  ERREXIT(tinfo->emethods, "dip_simulate_init: Error allocating temp leadfield memory\n");
 }
 array_reset(&a2); array_reset(&r2);
 do {
  array_write(&r2, array_scan(&r)+GRADIOMETER_LENGTH*array_scan(&u));
 } while (r2.message!=ARRAY_ENDOFSCAN);
 /*}}}  */
# endif

 for (dip=local_arg->source->nrofsources, dipole=local_arg->source->dipoles; dip>0; dip--, dipole++) {
  ier = biomag_leadfield_sphere(&dipole->position, NULL, &r, &u, &dipole->leadfield);
  if (ier!=0) {
   ERREXIT1(tinfo->emethods, "dip_simulate_init: biomag_leadfield_sphere error %d\n", MSGPARM(ier));
  }
# ifdef GRADIOMETER_LENGTH
  ier = biomag_leadfield_sphere(&dipole->position, NULL, &r2, &u, &a2);
  if (ier!=0) {
   ERREXIT1(tinfo->emethods, "dip_simulate_init: biomag_leadfield_sphere a2 error %d\n", MSGPARM(ier));
  }
  do {
   array_write(&dipole->leadfield, READ_ELEMENT(&dipole->leadfield)-array_scan(&a2));
  } while (dipole->leadfield.message!=ARRAY_ENDOFSCAN);
# endif
 }

# ifdef GRADIOMETER_LENGTH
 array_free(&a2); array_free(&r2);
# endif
 /*}}}  */
 /*{{{  Free r and u*/
 array_free(&r); array_free(&u);
 /*}}}  */

 /*{{{  Initialize dipole time function(s)*/
 for (dip=local_arg->source->nrofsources, dipole=local_arg->source->dipoles; dip>0; dip--, dipole++) {
  (*dipole->time_function_init)(dipole);
 }
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  dip_simulate(transform_info_ptr tinfo) {*/
/*
 * The method dip_simulate() returns a pointer to the allocated tsdata memory
 */

METHODDEF DATATYPE *
dip_simulate(transform_info_ptr tinfo) {
 struct dip_simulate_storage *local_arg=(struct dip_simulate_storage *)tinfo->methods->local_storage;
 int point, dip;
 double temp;
 struct dipole_desc *dipole;
 array newepoch;

 int const rejected_epochs=tinfo->rejected_epochs;
 int const accepted_epochs=tinfo->accepted_epochs;
 int const failed_assertions=tinfo->failed_assertions;
 if (local_arg->epochs--==0) return NULL;
 deepcopy_tinfo(tinfo, &local_arg->tinfo);
 tinfo->rejected_epochs=rejected_epochs;
 tinfo->accepted_epochs=accepted_epochs;
 tinfo->failed_assertions=failed_assertions;
 tinfo->nrofaverages=1;

 /* The following arrangement corresponds to 'Non-multiplexed' */
 newepoch.nr_of_elements=tinfo->nr_of_points;
 newepoch.nr_of_vectors=tinfo->nr_of_channels;
 newepoch.element_skip=1;
 if (array_allocate(&newepoch)==NULL) {
  ERREXIT(tinfo->emethods, "dip_simulate: Error allocating memory\n");
 }
 /* For the multiplication, we want to access the time points as vectors */
 array_transpose(&newepoch);
 for (point=0; point<tinfo->nr_of_points; point++) {
  for (dip=local_arg->source->nrofsources, dipole=local_arg->source->dipoles; dip>0; dip--, dipole++) {
   newepoch.current_vector=point;
   (*dipole->time_function)(dipole); /* Advance the dipole (one time tick) */
   /*{{{  Add the dipole field at each channel to this time point*/
   array_reset(&dipole->leadfield);
   do {
    temp=array_multiply(&dipole->leadfield, &dipole->dip_moment, MULT_VECTOR);
    array_write(&newepoch, READ_ELEMENT(&newepoch)+temp);
   } while (dipole->leadfield.message!=ARRAY_ENDOFSCAN);
   /*}}}  */
  }
 }

 tinfo->file_start_point=local_arg->current_epoch*tinfo->nr_of_points;
 tinfo->z_label=NULL;
 tinfo->multiplexed=FALSE;
 tinfo->itemsize=1;
 tinfo->leaveright=0;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels;
 tinfo->data_type=TIME_DATA;
 local_arg->current_epoch++;
 
 return newepoch.start;
}
/*}}}  */

/*{{{  dip_simulate_exit(transform_info_ptr tinfo) {*/
METHODDEF void
dip_simulate_exit(transform_info_ptr tinfo) {
 struct dip_simulate_storage *local_arg=(struct dip_simulate_storage *)tinfo->methods->local_storage;
 int dip;
 struct dipole_desc *dipole;

 /*{{{  Exit dipoles, free leadfield memory*/
 for (dip=local_arg->source->nrofsources, dipole=local_arg->source->dipoles; dip>0; dipole++, dip--) {
  (*dipole->time_function_exit)(dipole);
  array_free(&dipole->leadfield);
 }
 /*}}}  */

 if (tinfo->channelnames!=NULL) {
  free_pointer((void **)&local_arg->tinfo.channelnames[0]);
  free_pointer((void **)&local_arg->tinfo.channelnames);
 }
 free_pointer((void **)&local_arg->tinfo.probepos);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_dip_simulate(transform_info_ptr tinfo) {*/
GLOBAL void
select_dip_simulate(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &dip_simulate_init;
 tinfo->methods->transform= &dip_simulate;
 tinfo->methods->transform_exit= &dip_simulate_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="dip_simulate";
 tinfo->methods->method_description=
  "Simulation method to simulate dipolar sources with defined time behavior\n";
 tinfo->methods->local_storage_size=sizeof(struct dip_simulate_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
