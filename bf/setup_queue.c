/*
 * Copyright (C) 1996-2004,2006-2009,2011,2013,2016,2023 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * setup_queue.c -- Central module for dealing with queues or pipelines
 * of processing methods starting with a list of method select function
 * pointers.
 * This includes outputting the method description contained in all or a single
 * of these methods (printhelp), the configuration of queues of these methods
 * from text scripts (setup_queue), the single execution and exiting of a
 * single queue (do_queue, exit_queue) and the complete processing of a
 * text file and execution of all steps defined by a script file (do_script).
 * do_script and printhelp may be the only functions needed to call from
 * outside.
 *						-- Bernd Feige 22.10.1993
 */

/*{{{}}}*/
/*{{{  Queue execution: Remarks*/
/*
 * A queue or pipeline of methods is executed sequentially, the epoch
 * memory handed over from one method to the next.
 * A method can either return a pointer to the old epoch memory, which it
 * may have modified or only analyzed in some way, or return a pointer
 * to a newly allocated epoch memory, in which case the old memory will
 * automatically be free()'d. A method may also return the NULL pointer
 * to indicate an error. In this case, the previous epoch memory is free()'d
 * and the execution of this queue terminated.
 *
 * There are two queues now, generally: iter_queue and post_queue.
 * iter_queue is meant to be a queue starting with a get_epoch method,
 * repeated for every epoch delivered by that method and probably ending
 * with some result-forming method like average. The iteration is done until
 * the get_epoch method returns NULL. All other methods may return NULL, which
 * will only end this pass through the queue and increment the rejected_epochs
 * counter. CAUTION: The last method in iter_queue has to care for properly
 * free()ing the incoming epochs in case it doesn't need them. This is done
 * by 'collect' methods.
 * post_queue is executed on the result obtained after exiting iter_queue
 * (post-processing).
 *
 * The methods are initialized just before they are called for the first
 * time, ie with valid 'production' data available. The exit calls of
 * each initialized method in a queue are called after the last run through
 * that queue, ie the exit calls have a chance to modify the final
 * result.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  Allocate/free methodmem*/
GLOBAL Bool 
allocate_methodmem(transform_info_ptr tinfo) {
 long const length=tinfo->methods->nr_of_arguments*sizeof(transform_argument)+tinfo->methods->local_storage_size;
 transform_argument *space=NULL;
 if (length>0) {
  if ((space=(transform_argument *)calloc(length,1))==NULL) return FALSE;
 }
 tinfo->methods->arguments=(tinfo->methods->nr_of_arguments>0 ? space : NULL);
 tinfo->methods->local_storage=(tinfo->methods->local_storage_size>0 ? (void *)(space+tinfo->methods->nr_of_arguments) : NULL);
 return TRUE;
}
GLOBAL void
free_methodmem(transform_info_ptr tinfo) {
 if (tinfo->methods->arguments!=NULL) {
  int optno;
  for (optno=0; optno<tinfo->methods->nr_of_arguments; optno++) {
   switch (tinfo->methods->argument_descriptors[optno].type) {
    case T_ARGS_TAKES_FILENAME:
    case T_ARGS_TAKES_STRING_WORD:
    case T_ARGS_TAKES_SENTENCE:
     free_pointer((void **)&tinfo->methods->arguments[optno].arg.s);
     break;
    default:
     break;
   }
  }
  free(tinfo->methods->arguments);
 } else {
  if (tinfo->methods->local_storage!=NULL) free(tinfo->methods->local_storage);
 }
 tinfo->methods->arguments=NULL;
 tinfo->methods->local_storage=NULL;
}
/*}}}  */

/*{{{  setup_method and friends*/
/* This function returns an error message if it sees a problem with the
 * method configuration, NULL otherwise. */
GLOBAL char * 
check_method_args(transform_info_ptr tinfo) {
 int argno;
 transform_argument_descriptor *argument_descriptor=tinfo->methods->argument_descriptors;
 transform_argument *arg=tinfo->methods->arguments;

 if (tinfo->methods->nr_of_arguments>0 && argument_descriptor==NULL)
  return "Argument descriptor is NULL";
 if (tinfo->methods->local_storage_size>0 && tinfo->methods->local_storage==NULL)
  return "Local storage is NULL";

 for (argno=0; argno<tinfo->methods->nr_of_arguments; argno++) {
  switch (argument_descriptor->type) {
   case T_ARGS_TAKES_LONG:
   case T_ARGS_TAKES_DOUBLE:
   case T_ARGS_TAKES_SELECTION:
    if (argument_descriptor->option_letters[0]=='\0' && !arg->is_set)
     return "Required argument is missing";
    break;
   case T_ARGS_TAKES_FILENAME:
   case T_ARGS_TAKES_STRING_WORD:
   case T_ARGS_TAKES_SENTENCE:
    if (argument_descriptor->option_letters[0]=='\0') {
     if (!arg->is_set || (arg->variable==0 && arg->arg.s==NULL))
      return "Required argument is missing";
    } else {
     if (arg->is_set && arg->variable==0 && arg->arg.s==NULL)
      return "Option argument string is NULL";
    }
    break;
   default:
    break;
  }
  argument_descriptor++; arg++;
 }
 return NULL;
}

GLOBAL Bool
accept_argument(transform_info_ptr tinfo, growing_buf *args, growing_buf *tokenbuf, transform_argument_descriptor *argument_descriptor) {
 transform_argument_writeable *arg=(transform_argument_writeable *)tinfo->methods->arguments+(argument_descriptor-tinfo->methods->argument_descriptors);
 char *const save_current_token=args->current_token;
 char *endptr;

 if (arg->is_set) {
  return FALSE;
 }
 if (argument_descriptor->type==T_ARGS_TAKES_NOTHING) {
  arg->is_set=TRUE;
  return TRUE;
 }
 if (!growing_buf_get_nexttoken(args,tokenbuf)) {
  /* At end of arguments */
  if (argument_descriptor->type==T_ARGS_TAKES_SELECTION
      && argument_descriptor->option_letters[0]==' ') {
   /* Allow optional selectors to be satisfied by empty strings -
    * this is for variables: Optional selections take a variable 
    * but perhaps using one of the choices isn't desired at runtime. */
   return TRUE;
  } else {
   return FALSE;
  }
 }
 if (tokenbuf->buffer_start[0]=='$') {
  arg->variable=strtol(tokenbuf->buffer_start+1, &endptr, 10);
  if (*endptr!='\0' || arg->variable<=0) {
   ERREXIT1(tinfo->emethods, "accept_argument: >%s< is not a proper variable identifier.\n", MSGPARM(tokenbuf->buffer_start));
  }
 } else
 switch (argument_descriptor->type) {
  case T_ARGS_TAKES_LONG:
   arg->arg.i=strtol(tokenbuf->buffer_start, &endptr, 10);
   if (*endptr!='\0') return FALSE;
   break;
  case T_ARGS_TAKES_DOUBLE:
   arg->arg.d=get_value(tokenbuf->buffer_start, &endptr);
   if (*endptr!='\0') return FALSE;
   break;
  case T_ARGS_TAKES_FILENAME:
  case T_ARGS_TAKES_STRING_WORD:
   arg->arg.s=strdup(tokenbuf->buffer_start);
   break;
  case T_ARGS_TAKES_SENTENCE: {
   char * const save_delimiters=args->delimiters;
   /* Ensure that the whole remaining line is taken */
   args->current_token=save_current_token;
   args->delimiters="";
   growing_buf_get_nexttoken(args,tokenbuf);
   arg->arg.s=strdup(tokenbuf->buffer_start);
   args->delimiters=save_delimiters;
   }
   break;
  case T_ARGS_TAKES_SELECTION: {
   const char *const *inchoices=argument_descriptor->choices;
   while (*inchoices!=NULL && strcmp(*inchoices, tokenbuf->buffer_start)!=0) inchoices++;
   if (*inchoices==NULL) {
    /* No choice fitted, don't consume this token */
    args->current_token=save_current_token;
    return FALSE;
   }
   arg->arg.i=inchoices-argument_descriptor->choices;
   }
   break;
  default:
   break;
 }
 arg->is_set=TRUE;
 return TRUE;
}

GLOBAL Bool 
setup_method(transform_info_ptr tinfo, growing_buf *args) {
 int optno=0;
 char *message;
 
 if (!allocate_methodmem(tinfo)) {
  ERREXIT(tinfo->emethods, "setup_method: Error allocating memory\n");
 }

 /* Allow args==NULL for methods without required arguments */
 if (args!=NULL) {
  transform_argument_descriptor *argument_descriptor;
  Bool argument_found;
  growing_buf tokenbuf;
  growing_buf_init(&tokenbuf);
  growing_buf_allocate(&tokenbuf,0);
  tokenbuf.delim_protector='\\';
  /* Reset token pointer, just as growing_buf_get_firsttoken does */
  args->current_token=args->buffer_start;
  /* Scan all the optional parameters that may be issued at the start */
  do {
   argument_descriptor=tinfo->methods->argument_descriptors;
   argument_found=FALSE;
   for (optno=0; optno<tinfo->methods->nr_of_arguments && argument_descriptor->option_letters[0]!='\0'; optno++) {
    /* Take care: this loop also skips over the optional arguments, so the last repetition of do {} must not be
     * terminated prematurely */
    if (args->current_token>=args->buffer_start+args->current_length) continue;
    if (argument_descriptor->option_letters[0]==' ') {
     /* Optional argument without option switch */
     int const skip_args=atoi(argument_descriptor->option_letters+1);
     argument_found=accept_argument(tinfo, args, &tokenbuf, argument_descriptor);
     if (argument_found) {
      int skipno;
      for (skipno=0; skipno<skip_args; skipno++) {
       argument_descriptor++; optno++;
       if (!accept_argument(tinfo, args, &tokenbuf, argument_descriptor)) {
        ERREXIT2(tinfo->emethods, "setup_method %s: Required argument to option >%s< is missing\n", MSGPARM(tinfo->methods->method_name), MSGPARM((argument_descriptor-skipno-1)->description));
       }
      }
      break;
     }
     argument_descriptor+=skip_args; optno+=skip_args;
    } else {
     /* Check for option with option switch */
     char *const save_current_token=args->current_token;
     if (growing_buf_get_nexttoken(args,&tokenbuf) && tokenbuf.buffer_start[0]=='-' && strcmp(tokenbuf.buffer_start+1, argument_descriptor->option_letters)==0) {
      if (!accept_argument(tinfo, args, &tokenbuf, argument_descriptor)) {
       ERREXIT2(tinfo->emethods, "setup_method %s: Error setting up option -%s\n", MSGPARM(tinfo->methods->method_name), MSGPARM(argument_descriptor->option_letters));
      }
      argument_found=TRUE; break;
     } else {
      args->current_token=save_current_token;
     }
    }
    argument_descriptor++;
   }
  } while (argument_found);
  /* Now process the required arguments */
  argument_descriptor=tinfo->methods->argument_descriptors+optno;
  for (; optno<tinfo->methods->nr_of_arguments; optno++) {
   argument_found=accept_argument(tinfo, args, &tokenbuf, argument_descriptor);
   if (!argument_found && argument_descriptor->option_letters[0]=='\0') {
    ERREXIT2(tinfo->emethods, "setup_method %s: No argument found for `%s'\n", MSGPARM(tinfo->methods->method_name), MSGPARM(argument_descriptor->description));
   }
   argument_descriptor++;
  }
  if (growing_buf_get_nexttoken(args,NULL)) {
   ERREXIT1(tinfo->emethods, "setup_method %s: Too many arguments.\n", MSGPARM(tinfo->methods->method_name));
  }
  growing_buf_free(&tokenbuf);
 }
 if ((message=check_method_args(tinfo))!=NULL) {
  ERREXIT2(tinfo->emethods, "setup_method %s: %s\n", MSGPARM(tinfo->methods->method_name), MSGPARM(message));
 }
 return TRUE;
}
/*}}}  */

/*{{{  setup_queue(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), queue_desc *iter_queue, queue_desc *post_queue, FILE *scriptfile, growing_buf *scriptp) {*/
/*
 * setup_queue is a global function reading text lines from the FILE *
 * scriptfile and setting up tinfo values
 * as well as the two queues iter_queue and post_queue accordingly.
 * The arrays of transform_method_structs for iter_queue and post_queue are
 * automatically allocated and/or extended. To indicate that there is no
 * pre-allocated memory for a queue, queue->start should be NULL.
 * For each input line, the methods in the `available methods' list m_selects
 * are polled by selecting them and comparing their name with the given
 * command. If the method is found, the configure part of the method is called
 * with config_args containing pointers to the rest of the words.
 */

/* This is how many method descriptors setup_queue will allocate at a time */
#define QUEUE_ALLOC_STEP 10

LOCAL void
init_queue_storage(queue_desc *queue) {
 /* queue->start must be NULL if the queue space was not used before */
 if (queue->start==NULL) {
  queue->allocated_methods=0;
 }
 queue->nr_of_methods=0;
 queue->nr_of_get_epoch_methods=queue->current_get_epoch_method=0;
}

/* A pointer to the growing_buf to be used must be passed - it must already
 * be initialized and allocated. This is because the buffer memory must remain
 * allocated until the queues exit. It can be freed afterwards by the caller. */
GLOBAL enum SETUP_QUEUE_RESULT
setup_queue(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), queue_desc *iter_queue, queue_desc *post_queue, FILE *scriptfile, growing_buf *scriptp) {
 /*{{{  Read the (next script within) config file*/
 Bool linestart=TRUE;
 int c;
 while (1) {
  c=fgetc(scriptfile);
  /* Allow MSDOS files by dropping \r characters: */
  if (c=='\r') continue;
  if (c==0x04) c=EOF; /* Interpret ^D as EOF */
  if (linestart && c=='-') {
   /* '-' at linestart: End of script. Skip the rest of the line. */
   do {
    c=fgetc(scriptfile);
    if (c==0x04) c=EOF; /* Interpret ^D as EOF */
   } while (c!=EOF && c!='\n');
   break;
  }
  if (c==EOF) {
   /* If there was no EOL at EOF, no separator would be present
    * at the end of the script. So we add one in this case. */
   if (scriptp->current_length==0 || scriptp->buffer_start[scriptp->current_length-1]!='\n') growing_buf_appendchar(scriptp, '\n');
   break;
  }
  growing_buf_appendchar(scriptp, c);
  linestart=(c=='\n');
 }
 /*}}}  */

 //fprintf(stderr,"%s",scriptp->buffer_start);
 if (setup_queue_from_buffer(tinfo, m_selects, iter_queue, post_queue, scriptp)) {
  return (c==EOF ? QUEUE_AVAILABLE_EOF : QUEUE_AVAILABLE);
 } else {
  return (c==EOF ? QUEUE_NOT_AVAILABLE_EOF : QUEUE_NOT_AVAILABLE);
 }
}
GLOBAL Bool
setup_queue_from_buffer(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), queue_desc *iter_queue, queue_desc *post_queue, growing_buf *scriptp) {
 char *inbuf;
 void (* const *m_select)(transform_info_ptr);
 transform_methods_ptr newstart;
 const transform_methods_ptr storemethods=tinfo->methods;
 queue_desc *queue=iter_queue;
 int last_collect_position= -1;
 Bool have_line;
 growing_buf script= *scriptp, linebuf, tokenbuf;
 script.delimiters="\n";	/* High-level parsing is by lines */

 init_queue_storage(iter_queue);
 init_queue_storage(post_queue);
 tinfo->methods=queue->start;

 /* Both current_input_script and current_input_line should be initialized to 0 by the caller and start with 1 on return: */
 iter_queue->current_input_script++; post_queue->current_input_script++;
 growing_buf_init(&linebuf);
 growing_buf_allocate(&linebuf,0);
 linebuf.delim_protector='\\';
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);
 tokenbuf.delim_protector='\\';

 have_line=growing_buf_get_firstsingletoken(&script,&linebuf);
 while (have_line) {
  Bool within_branch=FALSE;
  Bool get_epoch_override=FALSE;

  iter_queue->current_input_line++; post_queue->current_input_line++;

  /* Remove comment if it exists, unless escaped */
  if ((inbuf=strchr(linebuf.buffer_start, '#'))!=NULL) {
   if (inbuf==linebuf.buffer_start || *(inbuf-1)!='\\') {
    while (*inbuf!='\0') *inbuf++ = '\0';
   }
  }

  if (*linebuf.buffer_start=='>') {
   within_branch=TRUE;
   *linebuf.buffer_start=' ';
  }
  if (*linebuf.buffer_start=='!') {
   get_epoch_override=TRUE;
   *linebuf.buffer_start=' ';
  }
  
  linebuf.delimiters=" \t\r\n"; /* Get only the first word */
  growing_buf_get_firsttoken(&linebuf,&tokenbuf);

  if (tokenbuf.current_length>0) {	/* Non-empty line ? */
   /*{{{  'Post:' keyword ? Switch to post processing queue*/
   if (strcmp(tokenbuf.buffer_start, "Post:")==0) {
    if (queue==post_queue) {
     ERREXIT(tinfo->emethods, "setup_queue: Multiple 'Post:' lines not allowed.\n");
    }
    queue=post_queue;
    init_queue_storage(queue);
    tinfo->methods=queue->start;
    queue->current_get_epoch_method= -1; /* No more get_epoch methods allowed */
    have_line=growing_buf_get_nextsingletoken(&script,&linebuf);
    continue;
   }
   /*}}}  */
   /*{{{  queuesize exceeded ? Allocate or Re-allocate queue memory*/
   if (queue->nr_of_methods>=queue->allocated_methods) {
    queue->allocated_methods+=QUEUE_ALLOC_STEP;
    if (queue->start==NULL) {
     if ((queue->start=(struct transform_methods_struct *)malloc(queue->allocated_methods*sizeof(struct transform_methods_struct)))==NULL) {
      ERREXIT1(tinfo->emethods, "setup_queue: Error allocating queue of %d entries\n", MSGPARM(queue->allocated_methods));
     }
    } else {
     if ((newstart=(struct transform_methods_struct *)realloc(queue->start, queue->allocated_methods*sizeof(struct transform_methods_struct)))==NULL) {
      ERREXIT1(tinfo->emethods, "setup_queue: Error reallocating queue to %d entries\n", MSGPARM(queue->allocated_methods));
     }
     queue->start=newstart;
    }
    tinfo->methods=queue->start+queue->nr_of_methods;
   }
   /*}}}  */

   /* Set the line number information as early as possible, 
    * since the error handler might use it! */
   tinfo->methods->line_of_script=queue->current_input_line;
   tinfo->methods->script_number=queue->current_input_script;

   /*{{{  Locate the method and configure it*/
   for (m_select=m_selects; *m_select!=NULL; m_select++) {
    (**m_select)(tinfo);
    if (strcmp(tokenbuf.buffer_start, tinfo->methods->method_name)==0) break;
   }
   if (*m_select==NULL) {
    ERREXIT1(tinfo->emethods, "setup_queue: Unknown method %s\n", MSGPARM(tokenbuf.buffer_start));
   }

   if (get_epoch_override && tinfo->methods->method_type!=TRANSFORM_METHOD) {
    ERREXIT(tinfo->emethods, "setup_queue: Only transform methods may be overridden as get_epoch methods.\n");
   }
   if (tinfo->methods->method_type==GET_EPOCH_METHOD || get_epoch_override) {
    if (within_branch) {
     ERREXIT(tinfo->emethods, "setup_queue: Branch-marking a get_epoch method makes no sense.\n");
    }
    if (queue!=iter_queue || queue->current_get_epoch_method<0) {
     ERREXIT(tinfo->emethods, "setup_queue: A get_epoch method is only allowed as one of the first methods\n");
    }
    queue->nr_of_get_epoch_methods++;
    queue->current_get_epoch_method++;
   } else {
    if (within_branch) {
     if (queue->current_get_epoch_method<0) {
      ERREXIT(tinfo->emethods, "setup_queue: A branch must start after a get_epoch method.\n");
     }
     queue->nr_of_get_epoch_methods++;
     queue->current_get_epoch_method++;
    } else {
     queue->current_get_epoch_method= -1; /* No more get_epoch methods allowed */
    }
    if (queue==iter_queue && queue->nr_of_methods==0) {
     ERREXIT(tinfo->emethods, "setup_queue: The first method in the iterated queue must be a get_epoch method\n");
    }
    if (tinfo->methods->method_type==COLLECT_METHOD) {
     if (within_branch) {
      ERREXIT(tinfo->emethods, "setup_queue: Collect methods cannot be branch-marked.\n");
     }
     if (queue==post_queue || last_collect_position>=0) {
      ERREXIT(tinfo->emethods, "setup_queue: Multiple collect methods or collect method in post processing queue\n");
     } else {
      last_collect_position=queue->nr_of_methods;
     }
    }
   }
   tinfo->methods->within_branch=within_branch;
   tinfo->methods->get_epoch_override=get_epoch_override;

   linebuf.delimiters=""; /* Get the rest of the line */
   if (growing_buf_get_nexttoken(&linebuf,&tokenbuf)) {
    setup_method(tinfo, &tokenbuf);
   } else {
    setup_method(tinfo, NULL);
   }
   tinfo->methods->init_done=FALSE;
   tinfo->methods++; queue->nr_of_methods++;
   /*}}}  */
  }
  have_line=growing_buf_get_nextsingletoken(&script,&linebuf);
 }
 growing_buf_free(&tokenbuf);
 growing_buf_free(&linebuf);

 /*{{{  Perform final checks on queue consistency*/
 if (iter_queue->nr_of_methods>0 && iter_queue->start[iter_queue->nr_of_methods-1].method_type!=COLLECT_METHOD) {
  ERREXIT1(tinfo->emethods, "setup_queue: The iterated queue must be closed with a collect method\n%s", MSGPARM(last_collect_position>0 ? " - Did you forget the Post: keyword?\n" : ""));
 }
 /*}}}  */

 /* This is to compensate for the fact that one separator line is skipped: */
 iter_queue->current_input_line++; post_queue->current_input_line++;

 tinfo->methods=storemethods;
 return (iter_queue->nr_of_methods>0);
}
/*}}}  */

/*{{{  dump_queue(queue_desc *queue, FILE *dumpfile, char *prefix) {*/
GLOBAL void
dump_queue(queue_desc *queue, FILE *dumpfile, char *prefix) {
 if (queue->nr_of_methods==0) {
  fprintf(dumpfile,
   "static struct queue_desc_struct %s_queue={\n"
   " NULL,\n"
   " 0,"
   " 0,\n"
   " 0,"
   " 0\n"
   "};\n",
  prefix
  );
 } else {
  int i;
  for (i=0; i<queue->nr_of_methods; i++) {
   if (queue->start[i].arguments!=NULL) {
    int argno;
    fprintf(dumpfile, "/* Args for method %s (script %d line %d): */\nstatic transform_argument %s%d_arguments[%d]={\n", queue->start[i].method_name, queue->start[i].script_number, queue->start[i].line_of_script, prefix, i, queue->start[i].nr_of_arguments);
    for (argno=0; argno<queue->start[i].nr_of_arguments; argno++) {
     if (queue->start[i].arguments[argno].is_set) {
      fprintf(dumpfile, " { TRUE, %d, ", queue->start[i].arguments[argno].variable);
      switch (queue->start[i].argument_descriptors[argno].type) {
       case T_ARGS_TAKES_NOTHING:
       case T_ARGS_TAKES_SELECTION:
       case T_ARGS_TAKES_LONG:
	fprintf(dumpfile, "{.i=%ldL}", queue->start[i].arguments[argno].arg.i);
	break;
       case T_ARGS_TAKES_DOUBLE:
	fprintf(dumpfile, "{.d=%g}", queue->start[i].arguments[argno].arg.d);
	break;
       case T_ARGS_TAKES_STRING_WORD:
       case T_ARGS_TAKES_SENTENCE:
       case T_ARGS_TAKES_FILENAME:
	if (queue->start[i].arguments[argno].arg.s==NULL) {
	 fprintf(dumpfile, "{.s=NULL}");
	} else {
	 fprintf(dumpfile, "{.s=");
	 /* Use the trafo_stc.c utility function to output the file name as a
	  * string that is read as the original string by a C parser */
	 fprint_cstring(dumpfile, queue->start[i].arguments[argno].arg.s);
	 fprintf(dumpfile, "}");
	}
	break;
       default:
	continue;
      }
     } else {
      fprintf(dumpfile, " { FALSE, 0, {.i=0L}");
     }
     fprintf(dumpfile, " }, /* -%s */\n", queue->start[i].argument_descriptors[argno].option_letters);
    }
    fprintf(dumpfile, "};\n");
   }
  }
  fprintf(dumpfile, "static struct transform_methods_struct %s_methods[%d]={\n",
   prefix, queue->nr_of_methods);
  for (i=0; i<queue->nr_of_methods; i++) {
#define ARGS_STRUCT_NAME_SIZE 80
   char args_struct_name[ARGS_STRUCT_NAME_SIZE];
   if (queue->start[i].arguments==NULL) {
    strcpy(args_struct_name, "NULL");
   } else {
    snprintf(args_struct_name, ARGS_STRUCT_NAME_SIZE, "%s%d_arguments", prefix, i);
   }
   fprintf(dumpfile, 
    "{select_%s, NULL, NULL,\n"
    " NULL, NULL, NULL, NULL,\n"
    " \"%s\", NULL, %d, %d, NULL, %s, %d, NULL, FALSE, %d, %d,\n"
    " %d, %d"
    "},\n",
    queue->start[i].method_name,
    queue->start[i].method_name,
    queue->start[i].method_type, queue->start[i].nr_of_arguments, args_struct_name,
    queue->start[i].local_storage_size,
    queue->start[i].within_branch,
    queue->start[i].get_epoch_override,
    queue->start[i].line_of_script, queue->start[i].script_number
   );
  }
  fprintf(dumpfile, "};\n");
  fprintf(dumpfile,
   "static struct queue_desc_struct %s_queue={\n"
   " %s_methods,\n"
   " %d,"
   " %d,\n"
   " %d,"
   " %d\n"
   "};\n",
   prefix,
   prefix,
   queue->nr_of_methods,
   queue->nr_of_methods, /* queue->allocated_methods, */
   queue->nr_of_get_epoch_methods, 
   queue->current_get_epoch_method);
 }
}

GLOBAL void
init_queue_from_dump(transform_info_ptr tinfo, queue_desc *queue) {
 int i;
 for (i=0; i<queue->nr_of_methods; i++) {
  /* The dump initializes the 'init' pointer to the select call... */
  void (* const method_select)(transform_info_ptr)=queue->start[i].transform_init;
  tinfo->methods=queue->start+i;
  (*method_select)(tinfo);
  tinfo->methods->local_storage=NULL;
  if (tinfo->methods->local_storage_size>0) {
   if ((tinfo->methods->local_storage=(void *)calloc(tinfo->methods->local_storage_size,1))==NULL) {
    ERREXIT(tinfo->emethods, "init_queue_from_dump: Error allocating memory.\n");
   }
  }
 }
}

GLOBAL int
set_queuevariables(transform_info_ptr tinfo, queue_desc *queue, int argc, char **argv) {
 int i;
 int max_variable_accessed=0;
 growing_buf arg, tokenbuf;

 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);

 for (i=0; i<queue->nr_of_methods; i++) {
  int argno;
  tinfo->methods=queue->start+i;
  for (argno=0; argno<tinfo->methods->nr_of_arguments; argno++) {
   int const variable=tinfo->methods->arguments[argno].variable;
   if (variable>max_variable_accessed) max_variable_accessed=variable;
   if (tinfo->methods->arguments[argno].is_set && variable!=0) {
    if (variable>argc || argv[variable-1]==NULL) continue;
    growing_buf_settothis(&arg, argv[variable-1]);
    arg.delimiters=""; /* Ensure that the whole argument becomes the first token */
    /* Otherwise, accept_argument will reject the new contents: */
    ((transform_argument_writeable *)tinfo->methods->arguments)[argno].is_set=FALSE;
    /* Reset token pointer, just as growing_buf_get_firsttoken does */
    arg.current_token=arg.buffer_start;
    if (!accept_argument(tinfo, &arg, &tokenbuf, &tinfo->methods->argument_descriptors[argno])) {
     ERREXIT1(tinfo->emethods, "set_queuevariables: Argument >%s< not accepted\n", MSGPARM(argv[variable-1]));
    }
   }
  }
 }
 growing_buf_free(&tokenbuf);
 return max_variable_accessed;
}
/*}}}  */

/*{{{  print_script(transform_info_ptr tinfo, queue_desc *queue, FILE *printfile) {*/
GLOBAL void
sprint_argument_value(growing_buf *outbuf, transform_methods_ptr methodp, int argno, Bool with_escapes) {
#define BUFFER_SIZE 1024
 char buffer[BUFFER_SIZE];
 switch (methodp->argument_descriptors[argno].type) {
  case T_ARGS_TAKES_NOTHING:
   break;
  case T_ARGS_TAKES_LONG:
   snprintf(buffer, BUFFER_SIZE, " %ld", methodp->arguments[argno].arg.i);
   growing_buf_appendstring(outbuf, buffer);
   break;
  case T_ARGS_TAKES_DOUBLE:
   snprintf(buffer, BUFFER_SIZE, " %g", methodp->arguments[argno].arg.d);
   growing_buf_appendstring(outbuf, buffer);
   break;
  case T_ARGS_TAKES_STRING_WORD:
  case T_ARGS_TAKES_FILENAME: 
   if (with_escapes) {
    /* Print with escapes: */
    char const *in_arg=methodp->arguments[argno].arg.s;
    char const * const delimiters=" \t\r\n";
    char const delim_protector='\\';
    /* With strings, current_length includes the zero at the end; we want to
     * overwrite this zero */
    if (outbuf->current_length>0) outbuf->current_length--;
    growing_buf_appendchar(outbuf, ' ');
    while (*in_arg!='\0') {
     if (strchr(delimiters, *in_arg)!=NULL || *in_arg==delim_protector) {
      growing_buf_appendchar(outbuf, delim_protector);
     }
     growing_buf_appendchar(outbuf, *in_arg++);
    }
    growing_buf_appendchar(outbuf, '\0');
    break;
   }
  case T_ARGS_TAKES_SENTENCE:
   snprintf(buffer, BUFFER_SIZE, " %s", methodp->arguments[argno].arg.s);
   growing_buf_appendstring(outbuf, buffer);
   break;
  case T_ARGS_TAKES_SELECTION:
   snprintf(buffer, BUFFER_SIZE, " %s", methodp->argument_descriptors[argno].choices[methodp->arguments[argno].arg.i]);
   growing_buf_appendstring(outbuf, buffer);
   break;
  default:
   break;
 }
#undef BUFFER_SIZE
}
GLOBAL void
sprint_argument(growing_buf *outbuf, transform_methods_ptr methodp, int argno) {
#define BUFFER_SIZE 128
 char buffer[BUFFER_SIZE];
 if (methodp->arguments[argno].is_set) {
  if (methodp->argument_descriptors[argno].option_letters[0]!='\0' &&
      methodp->argument_descriptors[argno].option_letters[0]!=' ') {
   snprintf(buffer, BUFFER_SIZE, " -%s", methodp->argument_descriptors[argno].option_letters);
   growing_buf_appendstring(outbuf, buffer);
  }
  if (methodp->arguments[argno].variable!=0) {
   snprintf(buffer, BUFFER_SIZE, " $%d", methodp->arguments[argno].variable);
   growing_buf_appendstring(outbuf, buffer);
  } else {
   sprint_argument_value(outbuf, methodp, argno, TRUE);
  }
 }
#undef BUFFER_SIZE
}
/* This script allows to print a version of a `compiled' queue in script form 
 * that can be read back in using setup_queue */
GLOBAL void
sprint_method(growing_buf *outbuf, transform_methods_ptr methodp) {
 growing_buf_clear(outbuf);	/* Reset contents to empty string */
 growing_buf_appendstring(outbuf, methodp->method_name);
 if (methodp->arguments!=NULL) {
  int argno;
  for (argno=0; argno<methodp->nr_of_arguments; argno++) {
   sprint_argument(outbuf, methodp, argno);
  }
 }
}
LOCAL void
print_queue(queue_desc *queue, FILE *printfile) {
 if (queue->nr_of_methods!=0) {
  int i;
  growing_buf buf;
  growing_buf_init(&buf);
  growing_buf_allocate(&buf, 0);
  for (i=0; i<queue->nr_of_methods; i++) {
   sprint_method(&buf, &queue->start[i]);
   fprintf(printfile, "%s%s%s\n", (queue->start[i].within_branch ? ">" : ""), (queue->start[i].get_epoch_override ? "!" : ""), buf.buffer_start);
  }
  growing_buf_free(&buf);
 }
}
GLOBAL void
print_script(queue_desc *iter_queue, queue_desc *post_queue, FILE *printfile) {
 if (iter_queue->nr_of_methods!=0) {
  print_queue(iter_queue, printfile);
  if (post_queue->nr_of_methods!=0) {
   fprintf(printfile, "Post:\n");
   print_queue(post_queue, printfile);
  }
 }
}
/*}}}  */

/*{{{  printhelp(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr)...*/
/*
 * The global function printhelp prints names, types and descriptions of those
 * methods in the m_selects array; if the method_name argument is not NULL,
 * it only prints information for the method with this name.
 * If method_name eq "SHORT", then only a list of method names is given.
 */

GLOBAL char const *methodtype_names[]={
 "GET_EPOCH_METHOD",
 "PUT_EPOCH_METHOD",
 "COLLECT_METHOD",
 "TRANSFORM_METHOD",
 "REJECT_METHOD"
};
GLOBAL char const *transform_argument_type_names[T_ARGS_NR_OF_TYPES]={
 "nothing",
 "integer",
 "floating point",
 "string word",
 "sentence",
 "filename",
 "selection"
};

GLOBAL void
printhelp(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), char *method_name) {
 void (* const *m_select)(transform_info_ptr);
 transform_methods_ptr methods_start=tinfo->methods;
 struct transform_methods_struct mymethod;
 int output_count=0, shortflag=FALSE;

 if (method_name!=NULL && strcmp(method_name, "SHORT")==0) {
  method_name=NULL;
  shortflag=TRUE;
 }
 tinfo->methods= &mymethod;
 for (m_select=m_selects; *m_select!=NULL; m_select++) {
  (**m_select)(tinfo);
  if (method_name==NULL || strcmp(mymethod.method_name, method_name)==0) {
   if (shortflag) {
    TRACEMS2(tinfo->emethods, -1, " %-20s: %s\n", MSGPARM(tinfo->methods->method_name), MSGPARM(methodtype_names[mymethod.method_type]));
   } else {
    transform_argument_descriptor *argument_descriptor=tinfo->methods->argument_descriptors;
    int argno=0;
    TRACEMS3(tinfo->emethods, -1, "Method %s: %s\n\n%s", MSGPARM(tinfo->methods->method_name), MSGPARM(methodtype_names[mymethod.method_type]), MSGPARM(tinfo->methods->method_description));
    while (argno<tinfo->methods->nr_of_arguments && argument_descriptor->option_letters[0]!='\0') {
     argument_descriptor++; argno++;
    }
    if (argno<tinfo->methods->nr_of_arguments) {
     TRACEMS(tinfo->emethods, -1, "ARGUMENTS:\n");
     while (argno<tinfo->methods->nr_of_arguments) {
      if (argument_descriptor->option_letters[0]==' ') {
       TRACEMS1(tinfo->emethods, -1, " [%s]", MSGPARM(argument_descriptor->description));
      } else {
       TRACEMS1(tinfo->emethods, -1, " %s", MSGPARM(argument_descriptor->description));
      }
      if (argument_descriptor->type==T_ARGS_TAKES_SELECTION) {
       const char * const *inchoices=argument_descriptor->choices;
       TRACEMS(tinfo->emethods, -1, " {");
       while (*inchoices!=NULL) {
	TRACEMS1(tinfo->emethods, -1, " %s", MSGPARM(*inchoices));
	inchoices++;
       }
       TRACEMS(tinfo->emethods, -1, " }");
      }
      argument_descriptor++; argno++;
     }
     TRACEMS(tinfo->emethods, -1, "\n");
    }
    argument_descriptor=tinfo->methods->argument_descriptors; argno=0;
    if (argno<tinfo->methods->nr_of_arguments && argument_descriptor->option_letters[0]!='\0') {
     TRACEMS(tinfo->emethods, -1, "OPTIONS:\n");
     while (argno<tinfo->methods->nr_of_arguments && argument_descriptor->option_letters[0]!='\0') {
      if (argument_descriptor->option_letters[0]==' ') {
       if (argument_descriptor->type==T_ARGS_TAKES_SELECTION) {
	const char * const *inchoices=argument_descriptor->choices;
        TRACEMS(tinfo->emethods, -1, " {");
	while (*inchoices!=NULL) {
         TRACEMS1(tinfo->emethods, -1, " %s", MSGPARM(*inchoices));
	 inchoices++;
	}
        TRACEMS1(tinfo->emethods, -1, " } %s\n", MSGPARM(argument_descriptor->description));

       } else {
        TRACEMS1(tinfo->emethods, -1, " Optional: %s\n", MSGPARM(argument_descriptor->description));
       }
      } else {
       TRACEMS2(tinfo->emethods, -1, " -%s %s\n", MSGPARM(argument_descriptor->option_letters), MSGPARM(argument_descriptor->description));
      }
      argument_descriptor++; argno++;
     }
    }
    TRACEMS(tinfo->emethods, -1, "\n\n");
   }
   output_count++;
  }
 }
 if (output_count==0 && method_name!=NULL) {
  TRACEMS1(tinfo->emethods, 0, "Sorry, method %s is unknown.\n", MSGPARM(method_name));
 }
 tinfo->methods=methods_start;
}
/*}}}  */

/*{{{  do_queue(transform_info_ptr tinfo, queue_desc *queue, int *last_method_nrp) {*/
GLOBAL DATATYPE *
do_queue(transform_info_ptr tinfo, queue_desc *queue, int *last_method_nrp) {
 const transform_methods_ptr storemethods=tinfo->methods;
 const DATATYPE *storetsdata=tinfo->tsdata;
 DATATYPE *newtsdata=NULL;
 int method_nr=queue->current_get_epoch_method;

 /* Execute the process chain with possible rejection. */
 while (method_nr<queue->nr_of_methods) {
  tinfo->methods=queue->start+method_nr;
  /* Save an index to the last executed method, so that the caller may
   * determine which method caused the exit if the return value is NULL */
  *last_method_nrp=method_nr;
  /* Right now, additional GET_EPOCH_METHODs are initialized after the previous
   * are exhausted. This saves some memory but makes the program exit, maybe
   * after substantial work, if the file for the next method is not available.*/
  if (tinfo->methods->method_type==GET_EPOCH_METHOD || tinfo->methods->get_epoch_override) {
   /* Clear the tinfo memory. Otherwise, there are just too many values and 
    * pointers which could be erroneously interpreted when tinfo is reused... */
   transform_methods_ptr const methods=tinfo->methods;
   external_methods_ptr const emethods=tinfo->emethods;
   long const points_in_file=tinfo->points_in_file;
   int const rejected_epochs=tinfo->rejected_epochs;
   int const accepted_epochs=tinfo->accepted_epochs;
   int const nrofaverages=tinfo->nrofaverages;
   int const failed_assertions=tinfo->failed_assertions;
   memset((void *)tinfo, 0, sizeof(struct transform_info_struct));
   tinfo->methods=methods;
   tinfo->emethods=emethods;
   tinfo->points_in_file=points_in_file;
   tinfo->rejected_epochs=rejected_epochs;
   tinfo->accepted_epochs=accepted_epochs;
   tinfo->nrofaverages=nrofaverages;
   tinfo->failed_assertions=failed_assertions;
   growing_buf_init(&tinfo->triggers);
  }
  if (tinfo->methods->init_done==FALSE) {
   trafo_std_defaults(tinfo);
   if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_BEFORE_INIT);
   (*tinfo->methods->transform_init)(tinfo);
   if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_AFTER_INIT);
  }
  if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_BEFORE_EXEC);
  newtsdata=(*tinfo->methods->transform)(tinfo);
  if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_AFTER_EXEC);
  /* Free everything from the old tinfo if it was rejected */
  if (newtsdata==NULL) free_tinfo(tinfo);
  /* Free the old tsdata if a new data set was allocated */
  if (newtsdata!=tinfo->tsdata && tinfo->tsdata!=NULL) free(tinfo->tsdata);
  tinfo->tsdata=newtsdata;
  if (newtsdata==NULL) {
   if (tinfo->methods->method_type==GET_EPOCH_METHOD || tinfo->methods->get_epoch_override) {
    /* A GET_EPOCH_METHOD was tried but didn't yield an epoch: Try the next */
    /* First exit this branch... */
    do {
     if (tinfo->methods->init_done) {
      if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_BEFORE_EXIT);
      (*tinfo->methods->transform_exit)(tinfo);
      if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_AFTER_EXIT);
     }
     method_nr++;
     tinfo->methods=queue->start+method_nr;
    } while (method_nr<queue->nr_of_methods && tinfo->methods->within_branch);
    queue->current_get_epoch_method=method_nr;
    if (queue->current_get_epoch_method<queue->nr_of_get_epoch_methods) {
     continue;
    } 
   }
   /* Stop this run through the queue */
   break;
  }
  /* Skip the next GET_EPOCH_METHODs and their branches */
  method_nr++; 
  while (method_nr<queue->nr_of_methods && 
   (queue->start[method_nr].method_type==GET_EPOCH_METHOD || queue->start[method_nr].get_epoch_override)) {
   method_nr++; 
   while (method_nr<queue->nr_of_methods && queue->start[method_nr].within_branch) {
    method_nr++; 
   }
  }
 }

 tinfo->tsdata=(DATATYPE *)storetsdata;
 tinfo->methods=storemethods;
 return newtsdata;
}
/*}}}  */

/*{{{  exit_queue(transform_info_ptr tinfo, queue_desc *queue) {*/
GLOBAL void
exit_queue(transform_info_ptr tinfo, queue_desc *queue) {
 const transform_methods_ptr storemethods=tinfo->methods;
 int i;

 tinfo->methods=queue->start;
 
 /* Call transform_exit method of any initialized method */
 for (i=0; i<queue->nr_of_methods; i++) {
  if (tinfo->methods->init_done) {
   if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_BEFORE_EXIT);
   (*tinfo->methods->transform_exit)(tinfo);
   if (tinfo->emethods->execution_callback!=NULL) (*tinfo->emethods->execution_callback)(tinfo, E_CALLBACK_AFTER_EXIT);
  }
  tinfo->methods++;
 }

 tinfo->methods=storemethods;
}
/*}}}  */

/*{{{  free_queuemethodmem(transform_info_ptr tinfo, queue_desc *queue) {*/
GLOBAL void
free_queuemethodmem(transform_info_ptr tinfo, queue_desc *queue) {
 const transform_methods_ptr storemethods=tinfo->methods;
 int i;

 tinfo->methods=queue->start;
 
 for (i=0; i<queue->nr_of_methods; i++) {
  free_methodmem(tinfo);
  tinfo->methods++;
 }

 tinfo->methods=storemethods;
}
/*}}}  */

/*{{{  free_queue_storage(queue_desc *queue)*/
GLOBAL void
free_queue_storage(queue_desc *queue) {
 free_pointer((void **)&queue->start);
}
/*}}}  */

/*{{{  do_queues(transform_info_ptr tinfo, queue_desc *iter_queue, queue_desc *post_queue) {*/
/* This function executes a script when the queues are already set up.
 * Sets (and returns) tinfo->tsdata, which will be NULL if all data was rejected. */
GLOBAL DATATYPE *
do_queues(transform_info_ptr tinfo, queue_desc *iter_queue, queue_desc *post_queue) {
 int last_method_nr, rejected_epochs=0, accepted_epochs=0;
 DATATYPE *newtsdata;
 struct transform_info_struct last_successful;

 iter_queue->current_get_epoch_method=post_queue->current_get_epoch_method=0;
 last_successful.tsdata=NULL;
 /* Execute iter_queue until the first (get_epoch) method returns an error */
 for (;;) {
  tinfo->accepted_epochs=accepted_epochs;
  tinfo->rejected_epochs=rejected_epochs;
  newtsdata=do_queue(tinfo, iter_queue, &last_method_nr);
  if (newtsdata==NULL) {
   /* Terminate the iter_queue if a GET_EPOCH_METHOD returned NULL: */
   if (iter_queue->start[last_method_nr].method_type==GET_EPOCH_METHOD || iter_queue->start[last_method_nr].get_epoch_override) break;
   /* Terminate the iter_queue if stopsignal was set: */
   if (tinfo->stopsignal) break;
   /* It is not clear what a `rejection' by a collect method should mean.
    * We decide not to count NULL returned by a collect method (just like
    * always done by null_sink) as rejection: */
   if (iter_queue->start[last_method_nr].method_type==COLLECT_METHOD) {
    accepted_epochs++;
   } else {
    rejected_epochs++;
   }
  } else {
   last_successful= *tinfo;
   last_successful.tsdata=newtsdata;
   tinfo->tsdata=NULL;	/* Prevent do_queue from freeing the last data */
   accepted_epochs++;
  }
 }

 if (last_successful.tsdata!=NULL) {
  /* Only use last_successful if there was at least one valid epoch, else use
   * tinfo itself to present to the exit calls */
  *tinfo=last_successful;
 }
 exit_queue(tinfo, iter_queue);
 tinfo->accepted_epochs=accepted_epochs;
 tinfo->rejected_epochs=rejected_epochs;
 /* exit_queue is given a chance to recover some valid data, but if not: */
 if (tinfo->tsdata==NULL) {
  if (post_queue->nr_of_methods!=0) {
   TRACEMS(tinfo->emethods, 0, "do_script: No valid epoch resulted from the iterated queue.\n");
  }
  return tinfo->tsdata;
 }

 if (post_queue->nr_of_methods!=0) {
  /*{{{  */
  newtsdata=do_queue(tinfo, post_queue, &last_method_nr);
  exit_queue(tinfo, post_queue);
  if (newtsdata==NULL) {
   TRACEMS(tinfo->emethods, 0, "do_script: Postprocessing queue returned NULL\n");
  }
  tinfo->tsdata=newtsdata;
  /*}}}  */
 } else {
  TRACEMS(tinfo->emethods, 0, "do_script: An end result is available but there's no Post: queue!\n");
 }

 return tinfo->tsdata;
}
/*}}}  */
