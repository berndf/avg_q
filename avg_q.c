/*
 * Copyright (C) 1996-1999,2001,2005,2009,2010 Bernd Feige
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
/*{{{}}}*/
/*{{{  Description*/
/*
 * avg_q(ueue).c user interface for transform methods -
 * This program obsoletes the single programs mfxaverage and mfxspect.
 *						-- Bernd Feige 28.07.1993
 *
 * avg_q uses setup_queue to read a script (Config) file to setup
 * a processing chain consisting of methods mentioned in the
 * method_selects[] function array.
 * This processing queue is then repeatedly called by the average method
 * and the averaged output postprocessed as determined by the program flags,
 * before it is written to an asc(ii) file.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"

#ifdef FP_EXCEPTION
#include "bfmath.h"
#endif

#ifdef STANDALONE
#include "avg_q_dump.h"
#endif
/*}}}  */

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
#include <getopt.h>
/*}}}  */

/*{{{ argv handling*/
LOCAL char **mainargv;
enum {
#ifdef STANDALONE
END_OF_ARGS=0
#else
CONFIGFILE=0, END_OF_ARGS
#endif
} args;

#define MAINARG(x) mainargv[optind+x]
/*}}}*/

/*{{{ Definitions*/
LOCAL struct transform_info_struct tinfostruc;
LOCAL struct external_methods_struct emethod;
LOCAL struct transform_methods_struct method;

#ifndef DATE
#define DATE " UNKNOWN "
#endif
/*}}}*/

/*{{{  Local functions*/
LOCAL void 
usage(FILE *stream) {
#ifndef STANDALONE
 fprintf(stream, "Usage: %s [options] Configfile [script_argument1 ...]\n"
  "Multichannel (EEG/MEG) data processing tool. Reads a script (Config) file to\n"
  "setup an iterated and a postprocessing queue. The iterated queue between\n"
  "get_epoch method and collect method is executed for all epochs delivered\n"
  "by the get_epoch method. If the collect method outputs a result, this\n"
  "result is fed to the postprocessing queue.\n"
#else
 fprintf(stream, "Usage: %s [options] [script_argument1 ...]\n"
  "Standalone avg_q version with script compiled-in.\n"
  "The file name of the script was: %s\n"
#endif
  "\nSignature:\n%s"
  "\nOptions are:\n"
  "\t-s scriptnumber: Execute only this script (counting from 1)\n"
#ifndef STANDALONE
  "\t-l: List all available methods\n"
  "\t-h methodname: Describe method methodname\n"
  "\t-H: Describe all available methods\n"
  "\t-t level: Set trace level. 0 (default) suppresses activity messages.\n"
  "\t-D: Dump a compilable version of the script.\n"
#else
  "\t-t level: Set trace level. 0 (default) suppresses activity messages.\n"
  "\t-D: Dump the compiled-in script to stdout.\n"
#endif
  "\t--help: This help screen.\n"
  "\t--version: Output the program version (i.e. the signature above).\n"
#ifndef STANDALONE
  , mainargv[0], get_avg_q_signature());
#else
  , mainargv[0], DUMP_SCRIPT_NAME, get_avg_q_signature());
#endif
}
/*}}}  */

/*{{{  int main(int argc, char **argv) {*/
int main(int argc, char **argv) {
 int errflag=0, c;
 int nr_of_script_variables, variables_requested1, variables_requested2, max_var_requested;
 Bool dumponly=FALSE;
 FILE * const dumpfile=stdout;
 int only_script=0;
 char const * const validate_msg=validate(argv[0]);
#ifndef STANDALONE
 void (* const * const method_selects)(transform_info_ptr)=get_method_list();
#endif

 if (validate_msg!=NULL) {
  fputs(get_avg_q_signature(), stdout);
  printf("Validation failed: %s.\n", validate_msg);
  exit(3);
 }

 /* setenv VDEVICE=X11 if it isn't already set */
 if (getenv("VDEVICE")==NULL) {
  char *vdevice, *envbuffer;
  vdevice="X11";
  envbuffer=malloc((strlen(vdevice)+9)*sizeof(char));
  if (envbuffer==NULL) {
   fprintf(stderr, "avg_q: Error allocating envbuffer!\n");
   exit(1);
  }
  sprintf(envbuffer, "VDEVICE=%s", vdevice);
  putenv(envbuffer);
  /* Note that the buffer cannot be free()d while the
   * program runs, otherwise `unexpected results' occur... 
   * This is documented behavior: envbuffer becomes part of the environment. */ 
 }

 /*{{{  MetroWerks specials*/
# ifdef __MWERKS__
 extern int getopt (int argc, char *const *argv, const char *shortopts);
# include <console.h>
 argc = ccommand(&argv);
# endif 
 /*}}}  */

 /*{{{  Preload tinfostruc values*/
 mainargv=argv;
 tinfostruc.emethods= &emethod;
 tinfostruc.methods= &method;
 clear_external_methods(&emethod);
 emethod.trace_level=0;
 /* The following is needed because the global functions
  * printhelp and setup_queue use the external functions
  * given in tinfostruc */
 trafo_std_defaults(&tinfostruc);
 /*}}}  */
 /*{{{  Process command line*/
#ifndef STANDALONE
#define GETOPT_STRING "t:lHh:Ds:"
#else
#define GETOPT_STRING "t:Ds:"
#endif
 const struct option longopts[]={{"help",no_argument,NULL,'?'},{"version",no_argument,NULL,'V'},{NULL,0,NULL,0}};
 while ((c=getopt_long(argc, argv, GETOPT_STRING, longopts, NULL))!=EOF) {
  switch (c) {
   case 't':
    emethod.trace_level=atoi(optarg);
    break;
#ifndef STANDALONE
   case 'l':
    printf("Available methods are:\n");
    printhelp(&tinfostruc, method_selects, "SHORT");
    return 1;
   case 'H':
    printhelp(&tinfostruc, method_selects, NULL);
    return 1;
   case 'h':
    printhelp(&tinfostruc, method_selects, optarg);
    return 1;
#endif
   case 'D':
    dumponly=TRUE;
    break;
   case 's':
    only_script=atoi(optarg);
    break;
   case 'V':
    fprintf(stdout, "%s", get_avg_q_signature());
    exit(0);
    break;
   case '?':
    usage(stdout);
    exit(0);
    break;
   default:
    errflag++;
    continue;
  }
 }
 if (argc-optind<END_OF_ARGS || errflag>0) {
  usage(stderr);
  exit(1);
 }
 nr_of_script_variables=argc-optind-END_OF_ARGS;
 /*}}}  */

 /* Since the trace level might have changed... */
 clear_external_methods(&emethod);
 trafo_std_defaults(&tinfostruc);

 {
#ifndef STANDALONE
 queue_desc iter_queue, post_queue;
 FILE *configfile;
 growing_buf script;
 growing_buf_init(&script);
 growing_buf_allocate(&script, 0);
 if (strcmp(MAINARG(CONFIGFILE), "stdin")==0) {
  configfile=stdin;
 } else {
  if ((configfile=fopen(MAINARG(CONFIGFILE), "r"))==NULL) {
   fprintf(stderr, "Error: Can't open file %s\n", MAINARG(CONFIGFILE));
   exit(2);
  }
 }
 iter_queue.start=post_queue.start=NULL;	/* Tell setup_queue that no memory is allocated yet */
 /* Initialize the method position info: */
 iter_queue.current_input_line=post_queue.current_input_line=0;
 iter_queue.current_input_script=post_queue.current_input_script=0;
 if (dumponly) {
  if (nr_of_script_variables!=0) {
   TRACEMS(tinfostruc.emethods, 0, "Script variables are ignored when dumping!\n");
  }
  fprintf(dumpfile, "#define DUMP_SCRIPT_NAME ");
  /* Use the trafo_stc.c utility function to output the file name as a
   * string that is read as the original string by a C parser */
  fprint_cstring(dumpfile, MAINARG(CONFIGFILE));
  fprintf(dumpfile, "\n\n");
 }

 while (TRUE) { /* Possibly execute multiple sub-scripts */
  enum SETUP_QUEUE_RESULT setup_queue_result;
  growing_buf_clear(&script);
  setup_queue_result=setup_queue(&tinfostruc, method_selects, &iter_queue, &post_queue, configfile, &script);
  if (setup_queue_result==QUEUE_NOT_AVAILABLE) continue;
  if (setup_queue_result==QUEUE_NOT_AVAILABLE_EOF) break;
  if (only_script>0 && iter_queue.current_input_script!=only_script) {
   /* This is necessary in order to free the method memory allocated during setup: */
   free_queuemethodmem(&tinfostruc, &iter_queue);
   free_queuemethodmem(&tinfostruc, &post_queue);
   continue;
  }
  if (dumponly) {
   char prefix_buffer[8];
   sprintf(prefix_buffer, "iter%02d", iter_queue.current_input_script);
   dump_queue(&iter_queue, dumpfile, prefix_buffer);
   sprintf(prefix_buffer, "post%02d", post_queue.current_input_script);
   dump_queue(&post_queue, dumpfile, prefix_buffer);
  } else {
   variables_requested1=set_queuevariables(&tinfostruc, &iter_queue, nr_of_script_variables, argv+optind+END_OF_ARGS);
   variables_requested2=set_queuevariables(&tinfostruc, &post_queue, nr_of_script_variables, argv+optind+END_OF_ARGS);
   max_var_requested=(variables_requested1>variables_requested2 ? variables_requested1 : variables_requested2);
   if (max_var_requested>nr_of_script_variables) {
    ERREXIT2(tinfostruc.emethods, "Arguments up to $%d were requested by the script, but only %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
   }
   if (max_var_requested<nr_of_script_variables) {
    TRACEMS2(tinfostruc.emethods, 0, "%d variables were requested by the script, %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
   }

   /*{{{  Execute the processing pipeline script*/
# ifdef FP_EXCEPTION
   fp_exception_init();	/* This sets up the math exception signal handler */
# endif
   do_queues(&tinfostruc, &iter_queue, &post_queue);
   /*}}}  */

   if (tinfostruc.tsdata!=NULL) {
    free_tinfo(&tinfostruc);
   }
  }
  /* This is necessary in order to free the method memory allocated during setup: */
  free_queuemethodmem(&tinfostruc, &iter_queue);
  free_queuemethodmem(&tinfostruc, &post_queue);
  if (only_script>0 || setup_queue_result==QUEUE_AVAILABLE_EOF) {
   break;
  }
 }
 if (dumponly) {
  int i;
  fprintf(dumpfile, "\nstatic struct queue_desc_struct *dump_script_pointers[]={\n");
  for (i=1; i<=iter_queue.current_input_script; i++) {
   if (only_script==0 || only_script==i) {
    fprintf(dumpfile, "&iter%02d_queue, &post%02d_queue,\n", i, i);
   }
  }
  fprintf(dumpfile, "NULL};\n");
 }
 if (configfile!=stdin) fclose(configfile);
 free_queue_storage(&iter_queue);
 free_queue_storage(&post_queue);
 growing_buf_free(&script);

#else

 struct queue_desc_struct **dump_script_pointer=dump_script_pointers;
 while (*dump_script_pointer!=NULL) { /* Possibly execute multiple sub-scripts */
#define iter_queue (*dump_script_pointer[0])
#define post_queue (*dump_script_pointer[1])
  if (only_script>0 && iter_queue.start[0].script_number!=only_script) {
   dump_script_pointer+=2;
   continue;
  }
  init_queue_from_dump(&tinfostruc, &iter_queue);
  init_queue_from_dump(&tinfostruc, &post_queue);
  if (dumponly) {
   print_script(&iter_queue,&post_queue,dumpfile);
   if (dump_script_pointer[2]!=NULL) {
    fprintf(dumpfile, "-\n");
   }
  } else {
   variables_requested1=set_queuevariables(&tinfostruc, &iter_queue, nr_of_script_variables, argv+optind+END_OF_ARGS);
   variables_requested2=set_queuevariables(&tinfostruc, &post_queue, nr_of_script_variables, argv+optind+END_OF_ARGS);
   max_var_requested=(variables_requested1>variables_requested2 ? variables_requested1 : variables_requested2);
   if (max_var_requested>nr_of_script_variables) {
    ERREXIT2(tinfostruc.emethods, "Arguments up to $%d were requested by the script, but only %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
   }
   if (max_var_requested<nr_of_script_variables) {
    TRACEMS2(tinfostruc.emethods, 0, "%d variables were requested by the script, %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
   }

   /*{{{  Execute the processing pipeline script*/
# ifdef FP_EXCEPTION
   fp_exception_init();	/* This sets up the math exception signal handler */
# endif
   do_queues(&tinfostruc, &iter_queue, &post_queue);
   /*}}}  */

   if (tinfostruc.tsdata!=NULL) {
    free_tinfo(&tinfostruc);
   }
  }
  dump_script_pointer+=2;
  if (only_script>0) {
   break;
  }
 }
#endif
 }

 return 0;
}
/*}}}  */
