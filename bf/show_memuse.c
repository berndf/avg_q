/*
 * Copyright (C) 1996-2001,2004,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * show_memuse -- A little diagnostic method to call ps on the calling
 * program, to see whether the program tends to accumulate memory or not
 * 						-- Bernd Feige 21.10.1993
 */
/*}}}  */

#ifdef __linux__
#define USE_PROCFS
#else
#undef  USE_PROCFS
#endif

/*{{{  #includes*/
#ifdef USE_PROCFS
#include <stdlib.h>
#include <string.h>
#else
#ifndef __MINGW32__
#include <sys/time.h>
#include <sys/resource.h>
#endif
#endif
#include <unistd.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  show_memuse_init(transform_info_ptr tinfo) {*/
METHODDEF void
show_memuse_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  show_memuse(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
show_memuse(transform_info_ptr tinfo) {
#define BUFFER_SIZE 1024
 char buffer[BUFFER_SIZE];
#ifdef USE_PROCFS
 char *inbuf=buffer;
 pid_t const mypid=getpid();
 FILE *statusfile;
 snprintf(buffer, BUFFER_SIZE, "/proc/%d/status", mypid);
 /* Here we implement a simple filter appending all the 'Vm' lines of the
  * status file while deleting the alignment whitespace */
 if ((statusfile=fopen(buffer, "r"))!=NULL) {
  while (fgets(inbuf, BUFFER_SIZE, statusfile)!=NULL) {
   if (strncmp(inbuf, "Vm", 2)==0) {
    char *in2buf=inbuf, c;
    while ((c= *inbuf++)!='\0') {
     if (strchr(" \t\r\n", c)==NULL) {
      *in2buf++ = c;
     }
    }
    *in2buf++ = ' ';
    *in2buf='\0';
    inbuf=in2buf;
   }
  }
  fclose(statusfile);
  strcpy(inbuf, "\n");
  TRACEMS1(tinfo->emethods, -1, "show_memuse: %s", MSGPARM(buffer));
 }
#else
#ifdef __MINGW32__
 TRACEMS(tinfo->emethods, -1, "show_memuse: Not yet implemented for mingw32\n");
#else
 struct rusage usage;
 /* Hmmm, this should work but is not (yet?) implemented in the Linux lib... */
 getrusage(RUSAGE_SELF, &usage);
 snprintf(buffer, BUFFER_SIZE, "VmSize:%ldkB VmRSS:%ldkB VmData:%ldkB VmStk:%ldkB", usage.ru_maxrss, usage.ru_ixrss, usage.ru_idrss, usage.ru_isrss);
 TRACEMS3(tinfo->emethods, -1, "show_memuse: Sys time %d.%02ds; %s\n", MSGPARM(usage.ru_stime.tv_sec), MSGPARM(usage.ru_stime.tv_usec/10000), MSGPARM(buffer));
#endif
#endif
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  show_memuse_exit(transform_info_ptr tinfo) {*/
METHODDEF void
show_memuse_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_show_memuse(transform_info_ptr tinfo) {*/
GLOBAL void
select_show_memuse(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &show_memuse_init;
 tinfo->methods->transform= &show_memuse;
 tinfo->methods->transform_exit= &show_memuse_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="show_memuse";
 tinfo->methods->method_description=
  "`Transform method' to call ps to output memory information\n"
  " on this process\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
