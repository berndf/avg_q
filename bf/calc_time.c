/*
 * Copyright (C) 1999 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int 
main(int argc, char **argv) {
 struct tm timestruct;
 time_t time_in_seconds;

 if (argc!=7) {
  fprintf(stderr, "Usage: %s day month year  hour minute second\n", argv[0]);
  return(1);
 }

 timestruct.tm_sec=timestruct.tm_min=timestruct.tm_hour=0;
 timestruct.tm_mday=atoi(argv[1]);
 timestruct.tm_mon= atoi(argv[2])-1;
 timestruct.tm_year=atoi(argv[3])-1900;

 timestruct.tm_hour=atoi(argv[4]);
 timestruct.tm_min= atoi(argv[5]);
 timestruct.tm_sec= atoi(argv[6]);

 time_in_seconds=mktime(&timestruct);
 if (time_in_seconds== (time_t) -1) {
  putenv("TZ=MET");
  tzset();
  time_in_seconds=mktime(&timestruct);
 }
 if (time_in_seconds== (time_t) -1) return 1;

 printf("%ldL\n", (unsigned long)time_in_seconds);

 return(0);
}
