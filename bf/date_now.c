/*
 * Copyright (C) 2002,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int 
main(int argc, char **argv) {
 time_t const time_in_seconds=time(NULL);

 puts(ctime(&time_in_seconds));

 return(0);
}
