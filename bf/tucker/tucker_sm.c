/*
 * Copyright (C) 1996 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/* Definition of `structure member' arrays for the tucker format
 *			-- Bernd Feige 16.12.1996 */

#include <stdio.h>
#include <read_struct.h>
#include "tucker.h"

struct_member sm_tucker[]={
 {sizeof(struct tucker_header), 36, 0, 0},
 {(long)&((struct tucker_header *)NULL)->Version, 0, 4, 1},
 {(long)&((struct tucker_header *)NULL)->Year, 4, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Month, 6, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Day, 8, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Hour, 10, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Minute, 12, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Sec, 14, 2, 1},
 {(long)&((struct tucker_header *)NULL)->MSec, 16, 4, 1},
 {(long)&((struct tucker_header *)NULL)->SampRate, 20, 2, 1},
 {(long)&((struct tucker_header *)NULL)->NChan, 22, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Gain, 24, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Bits, 26, 2, 1},
 {(long)&((struct tucker_header *)NULL)->Range, 28, 2, 1},
 {(long)&((struct tucker_header *)NULL)->NPoints, 30, 4, 1},
 {(long)&((struct tucker_header *)NULL)->NEvents, 34, 2, 1},
 {0,0,0,0}
};
