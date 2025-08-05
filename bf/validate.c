/*
 * Copyright (C) 1999-2018,2020,2024,2025 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <bf.h>
#include <avg_q_config.h>

#ifndef VERSION
#define VERSION "6.x.x"
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
 "(c) 1992-2025 by Bernd Feige, Bernd.Feige@gmx.net\n"
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
  sign=(char *)malloc(strlen(sig)+7+9+4);
 }
#ifdef TIMEOUT
 sprintf(sign, "%s; %02dbit; %09ld\n", sig, POINTER_SIZE, (long)(TIMEOUT-time(NULL)));
#else
 sprintf(sign, "%s; %02dbit\n", sig, POINTER_SIZE);
#endif
 return sign;
}
