/*
 * Copyright (C) 1996 Bernd Feige
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
