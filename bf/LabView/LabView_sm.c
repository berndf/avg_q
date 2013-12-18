/*
 * Copyright (C) 2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
