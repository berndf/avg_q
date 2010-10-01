/*
 * Copyright (C) 1993 Bernd Feige
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
 * array_reset.c tiny function to reset the array position counters
 * to the start of the array		-- Bernd Feige 15.09.1993
 *
 */

#include "array.h"

GLOBAL void
array_reset(array *thisarray) {
 thisarray->current_element=thisarray->current_vector=0;
 thisarray->message=ARRAY_CONTINUE;
}
