/*
 * Copyright (C) 1996-2000,2004 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*
 * do_posplot reads data sets from a file using the readasc method
 * and plots all or the given range of data sets using the posplot
 * method.				Bernd Feige 10.02.1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"

#ifdef FP_EXCEPTION
#include "bfmath.h"
#endif

struct transform_info_struct firsttinfo;
transform_info_ptr tinfo;
transform_info_ptr tinfo_next;
struct transform_methods_struct method;
struct external_methods_struct emethod;
#define POSPLOT_ARGS_SIZE 1024
char posplot_args[POSPLOT_ARGS_SIZE];

int main(int argc, char *argv[]) {
 int start= -1, end= -1, R_opt=0, err=0, newargc=argc;
 int epochs_loaded=0;
 char **newargv=argv;
 char *r_opt_arg=NULL;
 growing_buf args;

 /*{{{}}}*/
 /*{{{  Process the command line*/
 tinfo= &firsttinfo;
 while (newargc>1 && *newargv[1]=='-') {
  switch (newargv[1][1]) {
   case 'R':
    R_opt++;
    break;
   case 'r':
    r_opt_arg=newargv[2];
    newargv++; newargc--;
    break;
   default:
    fprintf(stderr, "%s: Unknown option -%c\n", argv[0], newargv[1][1]);
    err++;
    break;
  }
  newargv++; newargc--;
 }
 if (err>0 || newargc<2 || newargc>3) {
  fprintf(stderr, "Usage: %s [-R] [-r runfile] ascdatafile [start-end]\n"
	" -R: Replay from file `posplot_rec'\n"
	" -r runfile: Replay from file runfile\n"
	, argv[0]);
  return 1;
 }

 if (newargc==3) sscanf(newargv[2], "%d-%d", &start, &end);
 if (start<0) start=0;
 /*}}}  */

# ifdef FP_EXCEPTION
 fp_exception_init();	/* This sets up the math exception signal handler */
# endif

 clear_external_methods(&emethod);
 firsttinfo.methods= &method;
 firsttinfo.emethods= &emethod;

 /*{{{  Load all specified epochs into memory*/
 select_readasc(tinfo);
 trafo_std_defaults(tinfo);
 snprintf(posplot_args, POSPLOT_ARGS_SIZE, "-f %d -e %d %s", start+1, ((end>=0 && end>=start) ? end-start+1 : -1), newargv[1]);
 growing_buf_settothis(&args, posplot_args);
 setup_method(tinfo, &args);
 (*method.transform_init)(tinfo);
 while ((*method.transform)(tinfo)!=NULL) {
  epochs_loaded++;
  if ((tinfo_next=calloc(1,sizeof(struct transform_info_struct)))==NULL) {
   ERREXIT(&emethod, "do_posplot: Error malloc'ing tinfo struct\n");
  }
  tinfo_next->methods= &method;
  tinfo_next->emethods= &emethod;
  tinfo_next->previous=tinfo;
  tinfo_next->next=NULL;
  tinfo->next=tinfo_next;
  tinfo=tinfo_next;
 }
 (*method.transform_exit)(&firsttinfo);
 if (epochs_loaded==0) {
  ERREXIT(&emethod, "do_posplot: No epoch could be loaded.\n");
 }
 /* The last tinfo structure was allocated in vein, throw away */
 tinfo=tinfo->previous;
 free(tinfo->next);
 tinfo->next=NULL;
 /*}}}  */

 select_posplot(&firsttinfo);
 trafo_std_defaults(&firsttinfo);
 *posplot_args='\0';
 if (R_opt) strcat(posplot_args, "-R");
 if (r_opt_arg!=NULL) {
  strcat(posplot_args, " -r ");
  strcat(posplot_args, r_opt_arg);
 }
 growing_buf_settothis(&args, posplot_args);
 setup_method(&firsttinfo, &args);
 (*method.transform_init)(&firsttinfo);
 (*method.transform)(&firsttinfo);
 (*method.transform_exit)(&firsttinfo);
 return 0;
}
