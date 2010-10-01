/*
 * Copyright (C) 1993,1995 Bernd Feige
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
#include <math.h>
#include <array.h>

#include "bf.h"
#include "transform.h"

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif
/*}}}  */

/*{{{  Dipole behaviour*/
LOCAL void time_init(struct dipole_desc *dipole) {
 array_copy(&dipole->dip_moment, &dipole->initial_moment);
}
LOCAL void time_tick(struct dipole_desc *dipole) {
 array_copy(&dipole->initial_moment, &dipole->dip_moment);
 array_scale(&dipole->dip_moment, cos(dipole->time[0]));
 dipole->time[0]+=dipole->time[1];	/* time[1] is the frequency */
}
LOCAL void noise_time(struct dipole_desc *dipole) {
 do {
  array_write(&dipole->dip_moment, array_scan(&dipole->initial_moment)*((double)rand())/RAND_MAX);
 } while (dipole->dip_moment.message==ARRAY_CONTINUE);
}
LOCAL void time_exit(struct dipole_desc *dipole) {
 array_copy(&dipole->initial_moment, &dipole->dip_moment);
}
/*}}}  */

/*{{{  Dipole initialization*/
LOCAL DATATYPE dip1pos[3]={1.0, 1.0, 9.0};
LOCAL DATATYPE dip2pos[3]={-3.0, -1.0, 8.0};
LOCAL DATATYPE dip3pos[3]={-3.0, -1.0, 9.0};
LOCAL DATATYPE dip1moment[3]={1.0, 0.0, 0.0};
LOCAL DATATYPE dip2moment[3]={1.0, 0.0, 0.0};
LOCAL DATATYPE dip3moment[3]={1.0, 0.0, 0.0};
LOCAL DATATYPE dip1store[3]={0.0, 0.0, 0.0};
LOCAL DATATYPE dip2store[3]={0.0, 0.0, 0.0};
LOCAL DATATYPE dip3store[3]={0.0, 0.0, 0.0};

LOCAL struct dipole_desc dipdesc[]={
{
 /*{{{  Dipole 1*/
 {dip1pos,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {dip1moment,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {(DATATYPE *)NULL,
  NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {dip1store,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
 &time_init, &time_tick, &time_exit
 /*}}}  */
},
{
 /*{{{  Dipole 2*/
 {dip2pos,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {dip2moment,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {(DATATYPE *)NULL,
  NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {dip2store,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {3.0, 0.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
 &time_init, &time_tick, &time_exit
 /*}}}  */
},
{
 /*{{{  Dipole 3*/
 {dip3pos,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {dip3moment,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {(DATATYPE *)NULL,
  NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {dip3store,
  NULL, NULL, NULL,
  1, 3, 3, 1, 0, 0, 0,
  ARRAY_CONTINUE, NULL, NULL, NULL
 },
 {3.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
 &time_init, &noise_time, &time_exit
 /*}}}  */
}
};

LOCAL struct source_desc eg_dip_srcdesc={
 3, dipdesc
};
/*}}}  */

/*{{{  Dipole module configuration*/
GLOBAL struct source_desc *
eg_dip_srcmodule(transform_info_ptr tinfo, char **args) {
 int i;

 if (args!=(char **)NULL) {
  TRACEMS(tinfo->emethods, 0, "eg_dip_srcmodule: Warning: Ignoring all arguments.\n");
 }
 for (i=0; i<eg_dip_srcdesc.nrofsources; i++) {
  array_setreadwrite(&eg_dip_srcdesc.dipoles[i].position);
  array_setreadwrite(&eg_dip_srcdesc.dipoles[i].dip_moment);
  array_setreadwrite(&eg_dip_srcdesc.dipoles[i].initial_moment);
 }
 return &eg_dip_srcdesc;
}
/*}}}  */
