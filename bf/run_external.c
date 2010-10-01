/*
 * Copyright (C) 1996-2002 Bernd Feige
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
 * run_external -- A little method to call an external program
 * 						-- Bernd Feige 29.06.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_COMMAND=0, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SENTENCE, "program_path args...", "", ARGDESC_UNUSED, NULL}
};

/*{{{  run_external_init(transform_info_ptr tinfo) {*/
METHODDEF void
run_external_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  run_external(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
run_external(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 /* Special treatment for the chdir command. It would be an overkill to invent a new method 'chdir'... */
 if (strncmp(args[ARGS_COMMAND].arg.s, "chdir ", 6)==0) {
  TRACEMS1(tinfo->emethods, 1, "run: Changing directory to >%s<\n", MSGPARM(args[ARGS_COMMAND].arg.s+6));
  if (chdir(args[ARGS_COMMAND].arg.s+6)!=0) {
   ERREXIT2(tinfo->emethods, "run: %s trying to chdir to >%s<\n", MSGPARM(strerror(errno)), MSGPARM(args[ARGS_COMMAND].arg.s+6));
  }
 } else {
  TRACEMS1(tinfo->emethods, 1, "run: Executing >%s<\n", MSGPARM(args[ARGS_COMMAND].arg.s));
  system(args[ARGS_COMMAND].arg.s);
 }
 return tinfo->tsdata;
}
/*}}}  */

/*{{{  run_external_exit(transform_info_ptr tinfo) {*/
METHODDEF void
run_external_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_run(transform_info_ptr tinfo) {*/
/* Oops, note that our scheme of dumping and compiling dumped
 * scripts depends upon the condition that the select call for a method
 * is called select_METHODNAME where METHODNAME is the name as seen by
 * the user. This could only be overridden by having the name of the
 * select call explicitly set as a method property in each and every 
 * select code piece... */
GLOBAL void
select_run(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &run_external_init;
 tinfo->methods->transform= &run_external;
 tinfo->methods->transform_exit= &run_external_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="run";
 tinfo->methods->method_description=
  "`Transform method' to run an external program\n"
  " The argument is executed using the `system()' call, except\n"
  " `chdir dir', which changes the current working directory to `dir'.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
