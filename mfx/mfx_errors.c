/*
 * Copyright (C) 1993,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
