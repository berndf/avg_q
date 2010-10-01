/*
 * Copyright (C) 1993,1994,1997,1998,2001 Bernd Feige
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
/*
 * tinfo_array.c provides an interface to access the 'tsdata'
 * using the array interface (preferred method !).
 * The array thisarray is initialized to access the data
 * points of the tsdata (itempart=0) as the elements and the
 * channels as the vectors of the array.
 * Use array_transpose if the opposite is intended.
 *					-- Bernd Feige 7.10.1993
 */

#include <transform.h>
#include <array.h>
#include <bf.h>

GLOBAL void
tinfo_array(transform_info_ptr tinfo, array *thisarray) {
 thisarray->start=tinfo->tsdata;
 thisarray->nr_of_elements=(tinfo->data_type==FREQ_DATA ? tinfo->nroffreq : tinfo->nr_of_points);
 thisarray->nr_of_vectors =tinfo->nr_of_channels;
 thisarray->element_skip  =tinfo->itemsize*(tinfo->multiplexed ? tinfo->nr_of_channels : 1);
 thisarray->vector_skip   =tinfo->itemsize*(tinfo->multiplexed ? 1 : thisarray->nr_of_elements);
 array_setreadwrite(thisarray);
 array_reset(thisarray);
}

GLOBAL void
tinfo_array_setshift(transform_info_ptr tinfo, array *thisarray, int shift) {
 /* tinfo_array_setshift(., ., 0) is equivalent to tinfo_array(., .).
  * Otherwise, the accessed shift is set. */
 if ((tinfo->data_type==TIME_DATA && shift>0) || shift<0 || (tinfo->data_type==FREQ_DATA && shift>=tinfo->nrofshifts)) {
  ERREXIT1(tinfo->emethods, "tinfo_array_setshift: Invalid shift %d\n", MSGPARM(shift));
 }
 if (tinfo->data_type==FREQ_DATA) {
  thisarray->start=tinfo->tsdata+shift*tinfo->nroffreq*tinfo->nr_of_channels*tinfo->itemsize;
  thisarray->nr_of_elements=tinfo->nroffreq;
 } else {
  thisarray->start=tinfo->tsdata;
  thisarray->nr_of_elements=tinfo->nr_of_points;
 }
 thisarray->nr_of_vectors =tinfo->nr_of_channels;
 thisarray->element_skip  =tinfo->itemsize*(tinfo->multiplexed ? tinfo->nr_of_channels : 1);
 thisarray->vector_skip   =tinfo->itemsize*(tinfo->multiplexed ? 1 : thisarray->nr_of_elements);
 array_setreadwrite(thisarray);
 array_reset(thisarray);
}

GLOBAL void
tinfo_array_setfreq(transform_info_ptr tinfo, array *thisarray, int freq) {
 /* This method is for accessing tsdata with the shifts being the data points.
  * Note that for TIME_DATA, there will be only 1 element but as many `frequencies'
  * as time points. */
 long const nroffreq=(tinfo->data_type==FREQ_DATA ? tinfo->nroffreq : tinfo->nr_of_points);
 if (freq<0 || freq>=nroffreq) {
  ERREXIT1(tinfo->emethods, "tinfo_array_setfreq: Invalid freq %d\n", MSGPARM(freq));
 }
 thisarray->start=tinfo->tsdata+freq*tinfo->itemsize*(tinfo->multiplexed ? tinfo->nr_of_channels : 1);
 thisarray->nr_of_elements=(tinfo->data_type==FREQ_DATA ? tinfo->nrofshifts : 1);
 if (tinfo->data_type==FREQ_DATA) {
  thisarray->nr_of_elements=tinfo->nrofshifts;
 } else {
  thisarray->nr_of_elements=1;
 }
 thisarray->nr_of_vectors =tinfo->nr_of_channels;
 thisarray->element_skip  =tinfo->itemsize*nroffreq*tinfo->nr_of_channels;
 thisarray->vector_skip   =tinfo->itemsize*(tinfo->multiplexed ? 1 : nroffreq);
 array_setreadwrite(thisarray);
 array_reset(thisarray);
}
