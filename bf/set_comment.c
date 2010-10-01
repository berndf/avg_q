/*
 * Copyright (C) 1996-2000,2003,2010 Bernd Feige
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
 * set_comment method to set or append to the data set comment
 * 						-- Bernd Feige 15.08.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

LOCAL const char *const append_choice[]={
 "-a",
 "-p",
 NULL
};
enum APPEND_CHOICES {
 APPEND_CHOICE_APPEND=0,
 APPEND_CHOICE_PREPEND
};
enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_COMMENT, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_SELECTION, "Append or prepend to existing comment", " ", 0, append_choice},
 {T_ARGS_TAKES_SENTENCE, "comment", "", ARGDESC_UNUSED, NULL}
};

/*{{{  Definition of set_comment_storage*/
struct set_comment_storage {
 growing_buf buf;
};
/*}}}  */

/*{{{  set_comment_init(transform_info_ptr tinfo) {*/
METHODDEF void
set_comment_init(transform_info_ptr tinfo) {
 struct set_comment_storage *local_arg=(struct set_comment_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 char const *inbuf=args[ARGS_COMMENT].arg.s;

 growing_buf_init(&local_arg->buf);
 growing_buf_allocate(&local_arg->buf, 0);
 while (*inbuf !='\0') {
  char c=*inbuf++;
  if (c=='\\') {
   switch (*inbuf++) {
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
     break;
   }
   if (c=='\0') break;
  }
  growing_buf_appendchar(&local_arg->buf, c);
 }
 growing_buf_appendchar(&local_arg->buf, '\0');

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  set_comment(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
set_comment(transform_info_ptr tinfo) {
 struct set_comment_storage *local_arg=(struct set_comment_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 const char *arg_comment=local_arg->buf.buffer_start;
 char *new_comment;
 int commentlen=strlen(arg_comment)+1;

 if (args[ARGS_APPEND].is_set && tinfo->comment!=NULL) {
  commentlen+=strlen(tinfo->comment);
 }
 if ((new_comment=(char *)malloc(commentlen))==NULL) {
  ERREXIT(tinfo->emethods, "set_comment: Error allocating new comment memory\n");
 }
 if (args[ARGS_APPEND].is_set && tinfo->comment!=NULL) {
  switch ((enum APPEND_CHOICES)args[ARGS_APPEND].arg.i) {
   case APPEND_CHOICE_APPEND:
    strcpy(new_comment, tinfo->comment);
    strcat(new_comment, arg_comment);
    break;
   case APPEND_CHOICE_PREPEND:
    strcpy(new_comment, arg_comment);
    strcat(new_comment, tinfo->comment);
    break;
  }
 } else {
  strcpy(new_comment, arg_comment);
 }
 /* This we cannot do because the general standard says that z_label and 
  * xchannelname live in the same malloc()ed chunk of memory starting at 
  * `comment': free_pointer((void **)&tinfo->comment); */
 tinfo->comment=new_comment;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  set_comment_exit(transform_info_ptr tinfo) {*/
METHODDEF void
set_comment_exit(transform_info_ptr tinfo) {
 struct set_comment_storage *local_arg=(struct set_comment_storage *)tinfo->methods->local_storage;

 growing_buf_free(&local_arg->buf);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_set_comment(transform_info_ptr tinfo) {*/
GLOBAL void
select_set_comment(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &set_comment_init;
 tinfo->methods->transform= &set_comment;
 tinfo->methods->transform_exit= &set_comment_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="set_comment";
 tinfo->methods->method_description=
  "`Transform' method to set or append to the data set comment string\n";
 tinfo->methods->local_storage_size=sizeof(struct set_comment_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
