/*
 * Copyright (C) 1994,1997,1999,2001,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * This file defines a dipolar source description that can be used with
 * dip_simulate. 
 * While var_random_dipoles generates a continuous stream of burst events,
 * this module simulates events that are correlated with epoch latency,
 * ie start and end (with a specified ramp) at defined latencies
 * in each simulated epoch.
 *						-- Bernd Feige 17.11.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <array.h>

#include "bf.h"
#include "transform.h"

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif
/*}}}  */

/*{{{  Dipole behaviour functions*/
/* Variables in the time[] array for harmonic oscillators:
 * 0: momentary phase
 * 1: Frequency
 * 2: Counter for reactivation
 * 3: Counter for duration
 * 4: Duration
 * 5: Rise time
 * 6: Fall time
 * 7: Sleep time
 */

LOCAL void harmonic_time_init(struct dipole_desc *dipole) {
 array_copy(&dipole->dip_moment, &dipole->initial_moment);
 array_setto_null(&dipole->dip_moment);
 /* Starting dipole->time[2] has been set by the configuration */
 dipole->time[3]= dipole->time[4];
}
LOCAL void harmonic_time(struct dipole_desc *dipole) {
 if (dipole->time[2]-- >0) return;
 if (--dipole->time[3]== -1) {
  /*{{{  Envelope processed: Initiate new wait state*/
  dipole->time[0]=2*M_PI*((double)rand())/RAND_MAX;
  /* This value is already the first `sleeping' value: */
  dipole->time[2]=dipole->time[7]-1;
  dipole->time[3]=dipole->time[4];
  array_setto_null(&dipole->dip_moment);
  return;
  /*}}}  */
 }
 if (dipole->time[2]== -1) array_setreadwrite(&dipole->dip_moment);
 array_copy(&dipole->initial_moment, &dipole->dip_moment);
 {double current_factor;
  if (dipole->time[3]<dipole->time[6]) {
   current_factor=dipole->time[3]/dipole->time[6];
  } else if (dipole->time[3]>dipole->time[4]-dipole->time[5]) {
   current_factor=(dipole->time[4]-dipole->time[3])/dipole->time[5];
  } else current_factor=1.0;
  array_scale(&dipole->dip_moment, sin(dipole->time[0])*current_factor);
 }
 dipole->time[0]+=dipole->time[1];	/* time[1] is the frequency */
}
LOCAL void harmonic_time_exit(struct dipole_desc *dipole) {
}

/*{{{  Noise dipoles*/
LOCAL void noise_time_init(struct dipole_desc *dipole) {
 array_copy(&dipole->dip_moment, &dipole->initial_moment);
 array_setto_null(&dipole->dip_moment);
 /* Starting dipole->time[2] has been set by the configuration */
 dipole->time[3]= dipole->time[4];
}
LOCAL void noise_time(struct dipole_desc *dipole) {
 if (dipole->time[2]-- >0) return;
 if (dipole->time[3]== -1) {
  /*{{{  Envelope processed: Initiate new wait state*/
  dipole->time[2]=dipole->time[7];
  dipole->time[3]=dipole->time[4];
  array_setto_null(&dipole->dip_moment);
  return;
  /*}}}  */
 }
 if (dipole->time[2]== -1) array_setreadwrite(&dipole->dip_moment);
 array_copy(&dipole->initial_moment, &dipole->dip_moment);
 {double current_factor;
  if (dipole->time[3]<dipole->time[6]) {
   current_factor=dipole->time[3]/dipole->time[6];
  } else if (dipole->time[3]>dipole->time[4]-dipole->time[5]) {
   current_factor=(dipole->time[4]-dipole->time[3])/dipole->time[5];
  } else current_factor=1.0;
  array_scale(&dipole->dip_moment, (((double)rand())/(RAND_MAX/2)-1)*current_factor);
 }
 dipole->time[3]--;
}
LOCAL void noise_time_exit(struct dipole_desc *dipole) {
}
/*}}}  */
/*}}}  */

/*{{{  Dipole module configuration*/
LOCAL double
readval(transform_info_ptr tinfo, char ***argptr, double defaultval) {
 double val;

 if (**argptr==NULL) {
  val=defaultval;
 } else if (***argptr=='%') {
  /* Placeholder to insert the 'default' */
  val=defaultval; (*argptr)++;
 } else {
  val=gettimefloat(tinfo, *(*argptr)++);
 }
 return val;
}

GLOBAL struct source_desc *
ramping_srcmodule(transform_info_ptr tinfo, char **args) {
 int n_harmonics, n_noisedips, i, n_dipoles;
 char **inargs=args;
 DATATYPE *memptr;
 double freq_default=0.1;
 struct source_desc *sourcep;
 struct dipole_desc *dipolep;

 if (args==(char **)NULL) {
  ERREXIT(tinfo->emethods, "ramping_srcmodule: Usage: ramping_dipoles ( n_harmonics n_noisedips )\n");
 }
 if (*inargs!=NULL) n_harmonics=atoi(*inargs++);
 if (*inargs!=NULL) n_noisedips=atoi(*inargs++);
 if (inargs-args!=2) {
  ERREXIT(tinfo->emethods, "ramping_srcmodule: Need at least two arguments ( n_harmonics n_noisedips )\n");
 }
 n_dipoles=n_harmonics+n_noisedips;
 if ((sourcep=(struct source_desc *)malloc(sizeof(struct source_desc)+(sizeof(struct dipole_desc)+3*3*sizeof(DATATYPE))*n_dipoles))==NULL) {
  ERREXIT(tinfo->emethods, "ramping_srcmodule: Error allocating memory\n");
 }
 sourcep->nrofsources=n_dipoles;
 dipolep=sourcep->dipoles=(struct dipole_desc *)(sourcep+1);
 memptr=(DATATYPE *)(dipolep+n_dipoles);
 for (i=0; i<n_dipoles; i++, dipolep++, memptr+=9) {
  dipolep->position.start=memptr;
  dipolep->dip_moment.start=memptr+3;
  dipolep->initial_moment.start=memptr+6;
  dipolep->position.nr_of_elements=dipolep->dip_moment.nr_of_elements=dipolep->initial_moment.nr_of_elements=3;
  dipolep->position.nr_of_vectors=dipolep->dip_moment.nr_of_vectors=dipolep->initial_moment.nr_of_vectors=1;
  dipolep->position.element_skip=dipolep->dip_moment.element_skip=dipolep->initial_moment.element_skip=1;
  dipolep->position.vector_skip=dipolep->dip_moment.vector_skip=dipolep->initial_moment.vector_skip=3;
  array_setreadwrite(&dipolep->position);
  array_setreadwrite(&dipolep->dip_moment);
  array_setreadwrite(&dipolep->initial_moment);
  array_reset(&dipolep->position);
  array_reset(&dipolep->dip_moment);
  array_reset(&dipolep->initial_moment);
  /* The parameter is the frequency relative to half the sampling frequency: */
  dipolep->time[1]=M_PI*readval(tinfo, &inargs, freq_default);
  if (dipolep->time[1]<=0) {
   ERREXIT1(tinfo->emethods, "ramping_srcmodule: Frequency==%d\n", MSGPARM(dipolep->time[1]));
  }
  dipolep->time[2]=readval(tinfo, &inargs, 0); /* Start latency */
  dipolep->time[4]=readval(tinfo, &inargs, 50); /* Duration */
  dipolep->time[5]=readval(tinfo, &inargs, 10); /* Rise time */
  dipolep->time[6]=readval(tinfo, &inargs, 10); /* Fall time */
  /* Set the time to sleep between the end of the first burst and the
   * start of the next one */
  dipolep->time[7]=tinfo->beforetrig+tinfo->aftertrig-dipolep->time[4];
  array_write(&dipolep->position, readval(tinfo, &inargs, 1));
  array_write(&dipolep->position, readval(tinfo, &inargs, 1));
  array_write(&dipolep->position, readval(tinfo, &inargs, 7.5));
  array_write(&dipolep->dip_moment, readval(tinfo, &inargs, 1));
  array_write(&dipolep->dip_moment, readval(tinfo, &inargs, 0));
  array_write(&dipolep->dip_moment, readval(tinfo, &inargs, 0));
  if (i<n_harmonics) {
   /*{{{  Set harmonic behaviour*/
   dipolep->time_function_init= &harmonic_time_init;
   dipolep->time_function= &harmonic_time;
   dipolep->time_function_exit= &harmonic_time_exit;
   /*}}}  */
  } else {
   /*{{{  Set noise behaviour*/
   dipolep->time_function_init= &noise_time_init;
   dipolep->time_function= &noise_time;
   dipolep->time_function_exit= &noise_time_exit;
   /*}}}  */
  }
 }

 return sourcep;
}
/*}}}  */
