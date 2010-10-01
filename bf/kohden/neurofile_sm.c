/*
 * Copyright (C) 1997 Bernd Feige
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
/* Definition of `structure member' arrays for the NeuroFile format
 *			-- Bernd Feige 26.09.1997 */

#include <stdio.h>
#include <read_struct.h>
#include "neurofile.h"

static struct {
 int fastrate;
 float sfreq;
} neurofile_sfreq[]={
 {1, 128.0},
 {2, 256.0},
 {0, 0.0}
};
float neurofile_map_sfreq(int fastrate) {
 int i=0;
 while (neurofile_sfreq[i].sfreq!=0.0 && neurofile_sfreq[i].fastrate!=fastrate) i++;
 return neurofile_sfreq[i].sfreq;
}

struct_member sm_sequence[]={
 {sizeof(struct sequence), 332, 0, 0},
 {(long)&((struct sequence *)NULL)->comment, 0, 40, 0},
 {(long)&((struct sequence *)NULL)->second, 40, 1, 0},
 {(long)&((struct sequence *)NULL)->minute, 41, 1, 0},
 {(long)&((struct sequence *)NULL)->hour, 42, 1, 0},
 {(long)&((struct sequence *)NULL)->day, 43, 1, 0},
 {(long)&((struct sequence *)NULL)->month, 44, 1, 0},
 {(long)&((struct sequence *)NULL)->year, 45, 1, 0},
 {(long)&((struct sequence *)NULL)->length, 46, 2, 1},
 {(long)&((struct sequence *)NULL)->els, 48, 9, 0},
 {(long)&((struct sequence *)NULL)->ref, 57, 1, 0},
 {(long)&((struct sequence *)NULL)->nchan, 58, 2, 1},
 {(long)&((struct sequence *)NULL)->nfast, 60, 2, 1},
 {(long)&((struct sequence *)NULL)->nwil, 62, 2, 1},
 {(long)&((struct sequence *)NULL)->ncoh, 64, 2, 1},
 {(long)&((struct sequence *)NULL)->elnam, 66, 32*4, 0},
 {(long)&((struct sequence *)NULL)->coord, 194, 2*32, 0},
 {(long)&((struct sequence *)NULL)->ewil, 258, 32, 0},
 {(long)&((struct sequence *)NULL)->ecoh, 290, 32, 0},
 {(long)&((struct sequence *)NULL)->gain, 322, 4, 1},
 {(long)&((struct sequence *)NULL)->fastrate, 326, 1, 0},
 {(long)&((struct sequence *)NULL)->slowrate, 327, 1, 0},
 {(long)&((struct sequence *)NULL)->syncode, 328, 2, 1},
 {(long)&((struct sequence *)NULL)->sync0, 330, 2, 1},
 {0,0,0,0}
};
