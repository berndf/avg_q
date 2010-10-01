/*
 * Copyright (C) 1999-2010 Bernd Feige
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <bf.h>

#ifndef VERSION
#define VERSION "4.x.x"
#endif
#ifndef DATE
#define DATE " UNKNOWN "
#endif

#ifdef FLOAT_DATATYPE
#define FLOAT_SIZE "single precision"
#endif
#ifdef DOUBLE_DATATYPE
#define FLOAT_SIZE "double precision"
#endif

LOCAL char const * const sig=
 "avg_q multichannel (EEG/MEG) data processing tool\n"
 "(c) 1992-2010 by Bernd Feige, Bernd.Feige@gmx.net\n"
 "This version (" VERSION ", " FLOAT_SIZE ") was compiled on " DATE ".\n"
 SIGNATURE;

GLOBAL char const *
validate(char * const program) {
#ifdef CHKSUM
 short int sum=0;
 char const *in_string=get_avg_q_signature();

 for (; *in_string!=';'; in_string++) {
  sum+= *in_string;
 }

 if (sum!=CHKSUM) return "Chksum";
#endif
#ifdef TIMEOUT
 if (time(NULL)>TIMEOUT) return "Timeout";
#endif
 return NULL;
}

GLOBAL char const * 
get_avg_q_signature(void) {
 static char *sign=NULL;
 if (sign==NULL) {
  sign=(char *)malloc(strlen(sig)+9+4);
 }
#ifdef TIMEOUT
 sprintf(sign, "%s; %9ld\n", sig, TIMEOUT-time(NULL));
#else
 sprintf(sign, "%s;\n", sig);
#endif
 return sign;
}
