/*
 * Copyright (C) 1996-1998,2000,2003,2004 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * swap_xz reads data sets from a file using the readasc method
 * and reorganizes them into a version with swapped `x' and `z' axes
 * 					Bernd Feige 16.11.1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"

#define MAX_METHODARG_LENGTH 1024

/*{{{  Global variables*/
struct transform_info_struct firsttinfo;
transform_info_ptr tinfo;
transform_info_ptr tinfo_next;
struct transform_methods_struct methods[2];
struct external_methods_struct emethod;
/*}}}  */

int main(int argc, char *argv[]) {
 int fromepoch=1, epochs= -1, errors=0, binary_output=FALSE;
 growing_buf args;
 char writeasc_args[MAX_METHODARG_LENGTH];

 /*{{{}}}*/
 /*{{{  Process the command line*/
 while (argc>1 && *argv[1]=='-') {
  switch (argv[1][1]) {
   case 'B':
    binary_output=TRUE;
    break;
   case 'f':
    argv++; argc--;
    fromepoch=atoi(argv[1]);
    break;
   case 'e':
    argv++; argc--;
    epochs=atoi(argv[1]);
    break;
   default:
    errors++;
    break;
  }
  argv++; argc--;
 }
 if (errors>0 || argc<2) {
  fprintf(stderr, "Usage: %s ascfile1 ascfile2 ...\n"
  " -B: Write output in binary format\n"
  " -f fromepoch: Specify start epoch beginning with 1\n"
  " -e epochs: Specify maximum number of epochs to get\n"
  , argv[0]);
  return 1;
 }
 /*}}}  */

 while (argc>=2) {
  clear_external_methods(&emethod);
  firsttinfo.methods=methods;
  firsttinfo.emethods= &emethod;
  tinfo= &firsttinfo;
  printf("swap_xz: %s->%s.zx\n", argv[1], argv[1]);
 /*{{{  Load all specified epochs into memory*/
 select_readasc(tinfo);
 trafo_std_defaults(tinfo);
 snprintf(writeasc_args, MAX_METHODARG_LENGTH, "-f %d -e %d %s", fromepoch, epochs, argv[1]);
 growing_buf_settothis(&args, writeasc_args);
 setup_method(tinfo, &args);
 (*tinfo->methods->transform_init)(tinfo);
 while ((*tinfo->methods->transform)(tinfo)!=NULL) {
  if ((tinfo_next=calloc(1,sizeof(struct transform_info_struct)))==NULL) {
   ERREXIT(&emethod, "swap_xz: Error malloc'ing tinfo struct\n");
  }
  tinfo_next->methods=methods;
  tinfo_next->emethods= &emethod;
  tinfo_next->previous=tinfo;
  tinfo_next->next=NULL;
  tinfo->next=tinfo_next;
  tinfo=tinfo_next;
 }
 (*tinfo->methods->transform_exit)(&firsttinfo);
 /* The last tinfo structure was allocated in vein, throw away */
 tinfo=tinfo->previous;
 free(tinfo->next);
 tinfo->next=NULL;
 /*}}}  */

 /*{{{  Call the swap_xz global function and write the result*/
 {
 transform_info_ptr outtinfo=swap_xz(&firsttinfo);
  
 outtinfo->methods=methods+1;
 select_writeasc(outtinfo);
 trafo_std_defaults(outtinfo);
 snprintf(writeasc_args, MAX_METHODARG_LENGTH, "%s %s.zx", (binary_output ? "-b" : ""), argv[1]);
 growing_buf_settothis(&args, writeasc_args);
 setup_method(outtinfo, &args);
 (*outtinfo->methods->transform_init)(outtinfo);
 for (tinfo=outtinfo; tinfo!=NULL; tinfo=tinfo->next) {
  tinfo->methods=methods+1;
  (*tinfo->methods->transform)(tinfo);
 }
 (*outtinfo->methods->transform_exit)(outtinfo);
 free_methodmem(&firsttinfo);
 free(outtinfo->tsdata); /* swap_xz allocates everything in one chunk (`ydata') */
 free(outtinfo->xdata);
 free(outtinfo); outtinfo=NULL;
 }
 /*}}}  */

  firsttinfo.methods=methods;
  free_methodmem(&firsttinfo);
  free_tinfo(&firsttinfo);

  argv++; argc--;
 }

 return 0;
}
