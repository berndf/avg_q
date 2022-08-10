/*
 * Copyright (C) 1999,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <stdlib.h>
#include <stdio.h>
#include "bf.h"

int 
main(int argc, char **argv) {
 short int sum=0;
 char const * in_string=get_avg_q_signature();

 if (argc!=1) {
  fprintf(stderr, "Usage: %s\n", argv[0]);
  return(1);
 }

 for (; *in_string!=';'; in_string++) {
  sum+= *in_string;
 }

 printf("%d\n", sum);

 return(0);
}
