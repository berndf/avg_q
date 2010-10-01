/*
 * Copyright (C) 1994,1997,1999,2001,2003 Bernd Feige
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
 * This file defines a dipolar source description that can be used with
 * dip_simulate. Since there are too many possible parameters to be
 * defined by the configure mechanism (and even, time variability
 * programs cannot be given that way), the detailed source description
 * is initialized in this (variable) module.
 *						-- Bernd Feige 3.08.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
 * 3: Counter for envelope
 * 4: Half envelope length
 */

#define MAX_ENVELOPE_CYCLES 10

LOCAL DATATYPE
square(DATATYPE x) {
 return x*x;
}

LOCAL void harmonic_time_init(struct dipole_desc *dipole) {
 array_copy(&dipole->dip_moment, &dipole->initial_moment);
 dipole->time[2]=dipole->time[3]= -1;
}
LOCAL void harmonic_time(struct dipole_desc *dipole) {
 int half_envelope_length=(int)rint((M_PI*MAX_ENVELOPE_CYCLES)/dipole->time[1]);

 if (dipole->time[2]-- >0) return;
 if (dipole->time[3]== -1) {
  /*{{{  Envelope processed: Initiate new wait state*/
  dipole->time[0]=2*M_PI*((double)rand())/RAND_MAX;
  dipole->time[2]=(int)(half_envelope_length*((double)rand())/RAND_MAX);
  dipole->time[4]=(int)(half_envelope_length*((double)rand())/RAND_MAX)+1; /* Avoid zero envelope */
  dipole->time[3]=2*dipole->time[4];
  array_setto_null(&dipole->dip_moment);
  return;
  /*}}}  */
 }
 if (dipole->time[2]== -1) array_setreadwrite(&dipole->dip_moment);
 array_copy(&dipole->initial_moment, &dipole->dip_moment);
 array_scale(&dipole->dip_moment, sin(dipole->time[0])*(1.0-square((dipole->time[3]-dipole->time[4])/dipole->time[4])));
 dipole->time[0]+=dipole->time[1];	/* time[1] is the frequency */
 dipole->time[3]--;
}
LOCAL void harmonic_time_exit(struct dipole_desc *dipole) {
}
/*{{{  Noise dipole behavior*/
LOCAL void noise_time_init(struct dipole_desc *dipole) {
 array_copy(&dipole->dip_moment, &dipole->initial_moment);
}
LOCAL void noise_time(struct dipole_desc *dipole) {
 double scale=((double)rand())/(RAND_MAX/2)-1;
 do {
  array_write(&dipole->dip_moment, array_scan(&dipole->initial_moment)*scale);
 } while (dipole->dip_moment.message==ARRAY_CONTINUE);
}
LOCAL void noise_time_exit(struct dipole_desc *dipole) {
 array_copy(&dipole->initial_moment, &dipole->dip_moment);
}
/*}}}  */
/*}}}  */

/*{{{  Dipole module configuration*/
LOCAL void
sphere_random(float radius, float *x, float *y, float *z) {
 float a, b, c, r;
 do {
  a=((float)rand())/(RAND_MAX/2)-1;
  b=((float)rand())/(RAND_MAX/2)-1;
  c=((float)rand())/(RAND_MAX/2)-1;
  r=a*a+b*b+c*c;
 } while (r>1.0);
 r=radius/sqrt(r);
 *x=a*r; *y=b*r; *z=c*r;
}

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
var_random_srcmodule(transform_info_ptr tinfo, char **args) {
 int n_harmonics, n_noisedips, i, n_dipoles;
 char **inargs=args;
 DATATYPE *memptr;
 float random_pos=0.0, random_moment=0.0;
 double freq_default=0.1;
 struct source_desc *sourcep;
 struct dipole_desc *dipolep;

 if (args==(char **)NULL) {
  ERREXIT(tinfo->emethods, "var_random_srcmodule: Usage: var_random_dipoles ( n_harmonics n_noisedips )\n");
 }
 if (*inargs!=NULL) n_harmonics=atoi(*inargs++);
 if (*inargs!=NULL) n_noisedips=atoi(*inargs++);
 if (inargs-args!=2) {
  ERREXIT(tinfo->emethods, "var_random_srcmodule: Need at least two arguments ( n_harmonics n_noisedips )\n");
 }
 n_dipoles=n_harmonics+n_noisedips;
 if ((sourcep=(struct source_desc *)malloc(sizeof(struct source_desc)+(sizeof(struct dipole_desc)+3*3*sizeof(DATATYPE))*n_dipoles))==NULL) {
  ERREXIT(tinfo->emethods, "var_random_srcmodule: Error allocating memory\n");
 }
 sourcep->nrofsources=n_dipoles;
 dipolep=sourcep->dipoles=(struct dipole_desc *)(sourcep+1);
 memptr=(DATATYPE *)(dipolep+n_dipoles);
 for (i=0; i<n_dipoles; i++, dipolep++, memptr+=9) {
  dipolep->position.start=memptr;
  dipolep->dip_moment.start=memptr+3;
  dipolep->initial_moment.start=memptr+6;
  /*{{{  Read switchable options: FREQ_DEFAULT, RANDOM_POS, RANDOM_MOMENT keyword*/
  if (*inargs!=NULL && strcmp(*inargs, "FREQ_DEFAULT")==0) {
   if (*++inargs!=NULL) {
    freq_default=atof(*inargs);
    inargs++;
   }
  }
  if (*inargs!=NULL && strcmp(*inargs, "RANDOM_POS")==0) {
   if (*++inargs!=NULL) {
    random_pos=atof(*inargs);
    inargs++;
   }
  }
  if (*inargs!=NULL && strcmp(*inargs, "RANDOM_MOMENT")==0) {
   if (*++inargs!=NULL) {
    random_moment=atof(*inargs);
    inargs++;
   }
  }
  /*}}}  */
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
   ERREXIT1(tinfo->emethods, "var_random_srcmodule: Frequency==%d\n", MSGPARM(dipolep->time[1]));
  }
  if (random_pos>0.0) {
   float x, y, z;
   sphere_random(random_pos, &x, &y, &z);
   array_write(&dipolep->position, readval(tinfo, &inargs, x));
   array_write(&dipolep->position, readval(tinfo, &inargs, y));
   array_write(&dipolep->position, readval(tinfo, &inargs, z));
  } else {
   array_write(&dipolep->position, readval(tinfo, &inargs, 1));
   array_write(&dipolep->position, readval(tinfo, &inargs, 1));
   array_write(&dipolep->position, readval(tinfo, &inargs, 7.5));
  }
  if (random_moment>0.0) {
   float x, y, z;
   sphere_random(random_moment, &x, &y, &z);
   array_write(&dipolep->dip_moment, readval(tinfo, &inargs, x));
   array_write(&dipolep->dip_moment, readval(tinfo, &inargs, y));
   array_write(&dipolep->dip_moment, readval(tinfo, &inargs, z));
  } else {
   array_write(&dipolep->dip_moment, readval(tinfo, &inargs, 1));
   array_write(&dipolep->dip_moment, readval(tinfo, &inargs, 0));
   array_write(&dipolep->dip_moment, readval(tinfo, &inargs, 0));
  }
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
