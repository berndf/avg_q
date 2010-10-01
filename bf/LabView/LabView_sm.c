/*
 * Copyright (C) 2008 Bernd Feige
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
/* Definition of `structure member' arrays for the LabView format
 *			-- Bernd Feige 26.03.2008 */

#include <read_struct.h>
#include "LabView.h"

struct_member sm_LabView_TD1[]={
 {sizeof(LabView_TD1), 26, 0, 0},
 {(long)&((LabView_TD1 *)NULL)->S, 0, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->DR, 2, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->B, 4, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->P, 6, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->T, 8, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->R, 10, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->Time, 12, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->TriggerOut, 14, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->AUX, 16, 2, 1},
 {(long)&((LabView_TD1 *)NULL)->SecTot, 18, 4, 1},
 {(long)&((LabView_TD1 *)NULL)->SecDR, 22, 4, 1},
 {0,0,0,0}
};

struct_member sm_LabView_TD2[]={
 {sizeof(LabView_TD2), 8, 0, 0},
 {(long)&((LabView_TD2 *)NULL)->nr_of_points, 0, 4, 1},
 {(long)&((LabView_TD2 *)NULL)->nr_of_channels, 4, 4, 1},
 {0,0,0,0}
};
