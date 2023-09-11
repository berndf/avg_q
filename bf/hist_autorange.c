/*
 * Copyright (C) 1993,1994,1997,1998 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * hist_autorange.c provides the global function hist_autorange(tinfo)
 * to automatically allocate the hist_boundaries structure for all
 * selected channels and to set the histogram boundaries to the range
 * of the present tsdata. Additionally, the mean values for each channel
 * are calculated. The program is also useful in its own right to evaluate
 * minimum, maximum and mean values of a given data set.
 *						-- Bernd Feige 17.03.1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "transform.h"
#include "bf.h"

GLOBAL void
hist_autorange(transform_info_ptr tinfo) {
 int channel, point;
 struct hist_boundary *boundaryp;

 if ((tinfo->hist_boundaries=boundaryp=malloc(tinfo->nr_of_channels*sizeof(struct hist_boundary)))==NULL) {
  ERREXIT(tinfo->emethods, "hist_autorange error: malloc for hist_boundaries failed\n");
 }

 multiplexed(tinfo);
 for (channel=0; channel<tinfo->nr_of_channels; channel++) {
  register DATATYPE min=FLT_MAX, max= -FLT_MAX, mean=0.0;
  for (point=0; point<tinfo->nr_of_points; point++) {
   register DATATYPE data=tinfo->tsdata[tinfo->nr_of_channels*point+channel];
   if (data<min) min=data;
   if (data>max) max=data;
   mean+=data;
  }
  boundaryp->hist_min=min;
  boundaryp->hist_max=max;
  boundaryp->hist_mean=mean/tinfo->nr_of_points;
  boundaryp++;
 }
}
