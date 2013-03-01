/*
 * Copyright (C) 1999,2001-2003,2010 Bernd Feige
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
 * echo is a very simple method to output a string.
 * 						-- Bernd Feige 16.08.1999
 */
/*}}}  */

/*{{{  #includes*/
#include <string.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_OFILE, 
 ARGS_STRING, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_FILENAME, "Output file: Write output to this file", "F", ARGDESC_UNUSED, (const char *const *)"stdout"},
 {T_ARGS_TAKES_SENTENCE, "String to output", "", ARGDESC_UNUSED, NULL}
};

/*{{{  Definition of echo_storage*/
struct echo_storage {
 FILE *outfile;
 growing_buf buf;
};
/*}}}  */

/*{{{  echo_init(transform_info_ptr tinfo) {*/
METHODDEF void
echo_init(transform_info_ptr tinfo) {
 struct echo_storage *local_arg=(struct echo_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const *inbuf=args[ARGS_STRING].arg.s;

 growing_buf_init(&local_arg->buf);
 growing_buf_allocate(&local_arg->buf, 0);
 while (*inbuf !='\0') {
  char c=*inbuf++;
  if (c=='\\') {
   char c1;
   switch (c1=*inbuf++) {
    case 'n':
     c='\n';
     break;
    case 't':
     c='\t';
     break;
    case '\0':
     inbuf--;
     break;
    default:
     c=c1;
     break;
   }
   if (c=='\0') break;
  }
  growing_buf_appendchar(&local_arg->buf, c);
 }
 growing_buf_appendchar(&local_arg->buf, '\0');

 if (!args[ARGS_OFILE].is_set) {
  local_arg->outfile=NULL;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stdout")==0) {
  local_arg->outfile=stdout;
 } else if (strcmp(args[ARGS_OFILE].arg.s, "stderr")==0) {
  local_arg->outfile=stderr;
 } else
 if ((local_arg->outfile=fopen(args[ARGS_OFILE].arg.s, "a"))==NULL) {
  ERREXIT1(tinfo->emethods, "echo_init: Can't open file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  echo(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
echo(transform_info_ptr tinfo) {
 struct echo_storage *local_arg=(struct echo_storage *)tinfo->methods->local_storage;

 if (local_arg->outfile==NULL) {
  TRACEMS(tinfo->emethods, -1, local_arg->buf.buffer_start);
 } else {
  fputs(local_arg->buf.buffer_start, local_arg->outfile);
 }
 if (local_arg->outfile!=NULL) fflush(local_arg->outfile);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  echo_exit(transform_info_ptr tinfo) {*/
METHODDEF void
echo_exit(transform_info_ptr tinfo) {
 struct echo_storage *local_arg=(struct echo_storage *)tinfo->methods->local_storage;

 if (local_arg->outfile!=NULL && 
     local_arg->outfile!=stdout && 
     local_arg->outfile!=stderr) {
  fclose(local_arg->outfile);
 }
 growing_buf_free(&local_arg->buf);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_echo(transform_info_ptr tinfo) {*/
GLOBAL void
select_echo(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &echo_init;
 tinfo->methods->transform= &echo;
 tinfo->methods->transform_exit= &echo_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="echo";
 tinfo->methods->method_description=
  "`Transform' method to echo a string to the trace stream or a file.\n"
  " This will be mostly used to indicate progress within a script.\n";
 tinfo->methods->local_storage_size=sizeof(struct echo_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
