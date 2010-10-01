/*
 * Copyright (C) 1994,1995,1997,2003 Bernd Feige
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
 * array_dump package to dump an array to a file / read it from a file
 *					-- Bernd Feige 25.02.1994
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"
/*}}}  */

LOCAL int mathlab_count=0;	/* To assign different 'variable names' */
LOCAL char *format_tags[]={
 "# ARRAY_DUMP ASCII",	/* ARRAY_ASCII */
 "# name: "		/* ARRAY_MATLAB */
};
/* This may be small because it's only used for the identifier line: */
#define LINEBUF_SIZE 80

/*{{{  array_dump(FILE *fp, array *thisarray, array_dump_format format)*/
GLOBAL void
array_dump(FILE *fp, array *thisarray, array_dump_format format) {
 array_reset(thisarray);
 fprintf(fp, "%s", format_tags[format]);
 switch (format) {
  case ARRAY_ASCII:
  case ARRAY_MATLAB:
   if (format==ARRAY_ASCII) {
    fprintf(fp, "\n# %d %d\n", thisarray->nr_of_elements, thisarray->nr_of_vectors);
   } else {
    fprintf(fp, "xx%d\n# type: matrix\n# rows: %d\n# columns: %d\n", mathlab_count++, thisarray->nr_of_vectors, thisarray->nr_of_elements);
   }
   do {
    while (TRUE) {
     DATATYPE hold=array_scan(thisarray);
     if (thisarray->message==ARRAY_CONTINUE) {
      fprintf(fp, "%g\t", hold);
     } else {
      fprintf(fp, "%g\n", hold);
      break;
     }
    }
   } while (thisarray->message!=ARRAY_ENDOFSCAN);
   break;
  case ARRAY_DUMPFORMATS:
  default:
   thisarray->message=ARRAY_ERROR;
   break;
 }
}
/*}}}  */

/*{{{  array_undump(FILE *fp, array *thisarray)*/
GLOBAL void
array_undump(FILE *fp, array *thisarray) {
 array_dump_format format;
 char linebuf[LINEBUF_SIZE];
 float inval;

 /*{{{  Verify magic string and set format*/
 errno=0;
 while (fgets(linebuf, LINEBUF_SIZE, fp) && errno==0 && *linebuf=='\n');
 if (errno!=0) {
  /* Can't get the first non-zero line */
  thisarray->message=ARRAY_ERROR;
  return;
 }
 for (format=(array_dump_format)0; format<ARRAY_DUMPFORMATS &&
      strncmp(linebuf, format_tags[format], strlen(format_tags[format]))!=0;
      format=(array_dump_format)(format+1));
 /*}}}  */

 switch (format) {
  case ARRAY_ASCII:
  case ARRAY_MATLAB:
   if (format==ARRAY_ASCII) {
    fscanf(fp, "# %d %d\n", &thisarray->nr_of_elements, &thisarray->nr_of_vectors);
   } else {
    fscanf(fp, "# type: matrix\n# rows: %d\n# columns: %d\n", &thisarray->nr_of_vectors, &thisarray->nr_of_elements);
   }
   thisarray->element_skip=1;
   if (thisarray->nr_of_elements<=0 || thisarray->nr_of_vectors<=0 ||
       array_allocate(thisarray)==NULL) {
    thisarray->message=ARRAY_ERROR;
    return;
   }
   do {
    fscanf(fp, "%g", &inval);
    array_write(thisarray, inval);
    if (thisarray->message!=ARRAY_CONTINUE) {
     fscanf(fp, "%*[^\n]");	/* Gobble up anything up to \n */
    }
   } while (thisarray->message!=ARRAY_ENDOFSCAN);
   break;
  case ARRAY_DUMPFORMATS:
  default:
   thisarray->message=ARRAY_ERROR;
   break;
 }
}
/*}}}  */
