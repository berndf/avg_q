/*
 * Copyright (C) 1993-1995 Bernd Feige
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
 * mfxinfo.c - A small mfx application which uses mfx_infoout to print
 * the file header information to stdout.	Bernd Feige 10.02.1993
 */

#include <stdlib.h>
#include <stdio.h>
#include "mfx_file.h"

MFX_FILE *myfile;

int main(int argc, char **argv) {
 if (argc!=2) {
  fprintf(stderr, "This is mfxinfo using the mfx library version\n%s\n\nUsage: %s mfxfile\n", mfx_version, argv[0]);
  return 1;
 }
 if ((myfile=mfx_open(argv[1], "rb", MFX_SHORTS))==NULL) {
  fprintf(stderr, "mfx_open: %s\n", mfx_errors[mfx_lasterr]);
  return 2;
 }
 mfx_infoout(myfile, stdout);
 printf("\nmfx_infoout: %s\n", mfx_errors[mfx_lasterr]);
 mfx_close(myfile);
 return 0;
}
