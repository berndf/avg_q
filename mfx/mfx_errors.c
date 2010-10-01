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
 * mfx_errors.c. This module defines the plain text messages corresponding 
 * to the error numbers defined in mfx_file.h.	Bernd Feige 8.02.1993
 */

#include "mfx_file.h"

char *mfx_errors[]={"No error.", "Error allocating file header memory", 
	"Error allocating channel header memory",
	"Error opening mfx-stream",
	"Error reading file type header", "Error reading file header", 
	"Error reading channel headers",
	"Error reading mfx data", "Error writing mfx data",
	"Error: Unknown header type", "Error: Unknown data type",
	"Error: File length doesn't match header data",
	"Error allocating memory for channel selection list",
	"Error: No channel with that name in file",
	"Error: Use mfx_cselect to select channels first",
	"Error: Seek error", "Error: Negative seek destination",
	"Error: Can't seek past end of file", "Error: inconsistent seek",
	"Error: mfx_alloc: Can't allocate requested memory",
	"Error: Empty epoch", "Error: Expansion list member doesn't end with number",
	"Error: Empty expansion interval", "Error: Expansion interval unterminated",
	"Error flushing mfx headers", "Error: Header access only possible after create"
};
