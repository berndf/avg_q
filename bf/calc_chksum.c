/*
 * Copyright (C) 1999 Bernd Feige
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
