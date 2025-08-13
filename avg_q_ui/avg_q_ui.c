/*
 * Copyright (C) 2008-2014,2016-2020,2023-2025 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * File:	avg_q_ui.c
 * Purpose:	GTK+ user interface for avg_q
 * Author:	Bernd Feige
 * Created:	7.03.1999
 * Updated:
 * Copyright:	(c) 1999-2025, Bernd Feige
 */

/*{{{ Includes*/
#include "glib.h"

#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#include <setjmp.h>
#include <errno.h>
#include <signal.h>

/* For stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "avg_q_ui.h"
#include "transform.h"
#include "bf.h"

/* We use this in order to process the error exit differently for
 * different instances of my_emethod (namely, for script running and
 * configuration errors, which may occur in parallel): */
typedef struct {
 struct external_methods_struct emethod;
 jmp_buf error_jmp;
} my_emethod;

/* For strtol: */
#include <stdlib.h>
/* For the string functions: */
#include <string.h>
#ifdef __GNUC__
/* For getopt: */
#include <unistd.h>
#endif
#ifdef FP_EXCEPTION
#include "bfmath.h"
#endif
#ifdef STANDALONE
#include "avg_q_dump.h"
#define DUMPFILENAME DUMP_SCRIPT_NAME
#else
#define DUMPFILENAME "avg_q_dump.h"
#endif
#define AVG_Q_UIRC ".avg_q_uirc"
#define DEFAULT_FONT "Sans 14"
/*}}}*/

/*{{{  External declarations*/
extern char *optarg;
extern int optind, opterr;
#include <getopt.h>
/*}}}  */

/*{{{ argv handling*/
LOCAL int mainargc;
LOCAL char **mainargv;
enum {
#ifdef STANDALONE
END_OF_ARGS=0
#else
SCRIPTFILE=0, END_OF_ARGS
#endif
} args;

#define MAINARG(x) mainargv[optind+x]
/*}}}*/

/*{{{ Definitions*/
#ifndef STANDALONE
/* This defines the order in which we want the methods to appear. Everything else
 * is enumerated by the method_type numbers as given by transform.h. */
LOCAL enum method_types method_types[]={
 GET_EPOCH_METHOD,
 REJECT_METHOD,
 TRANSFORM_METHOD,
 PUT_EPOCH_METHOD,
 COLLECT_METHOD
};
#endif

#define IS_POSTLINE(string) (strncmp(string, "Post:", 5)==0)
#define IS_SEPLINE(string) (string[0]=='-')

#define FRAME_SIZE_X 800
#define FRAME_SIZE_Y 500
#define TRACEFRAME_SIZE_X 400
#define TRACEFRAME_SIZE_Y 200
/*}}}*/

/*{{{ Declarations*/
LOCAL struct transform_info_struct tinfostruc;
LOCAL my_emethod external_method;
LOCAL struct transform_methods_struct mymethod;

GMainLoop *mainloop;
LOCAL GtkWidget *Avg_Q_Main_Window;
LOCAL GtkWidget *Avg_Q_Scriptpanel, *TracePanel=NULL;
#define CurrentScriptBuffer (gtk_text_view_get_buffer(GTK_TEXT_VIEW(Avg_Q_Scriptpanel)))
LOCAL GtkWidget *TraceMenuItems[4];
LOCAL GtkWidget *Avg_q_StatusBar;
LOCAL GtkWidget *Avg_q_StatusRunBar;
#ifdef STANDALONE
LOCAL gint Avg_q_Run_Script_Now_Tag=0;
#else
LOCAL gint Avg_q_Load_Script_Now_Tag=0;
LOCAL gint Avg_q_Load_Subscript_Tag=0;
#endif
LOCAL Bool running=FALSE, iconified=FALSE, interactive=FALSE, dumponly=FALSE;
LOCAL Bool stop_request=FALSE;
LOCAL FILE *dumpfile;
#define FilenameLength 300
LOCAL char filename[FilenameLength+1]="avg_q_ui.script";
LOCAL GAction *Run_Action, *Stop_Action, *Kill_Action, *Dump_Action;
#ifndef STANDALONE
LOCAL GAction *Run_Subscript_Action;
LOCAL GtkWidget *Dialog_Configuration_Error;
LOCAL FILE *scriptfile=NULL;
LOCAL int subscript_loaded=0;
#endif
LOCAL int only_script=0, nr_of_script_variables=0;
LOCAL int save_only_script=0;
LOCAL char **script_variables=NULL;
LOCAL queue_desc iter_queue, post_queue;
/* The following variables could have been local to Run_Script but
 * apparently this is dangerous since longjmp is used there: */
LOCAL Bool succeeded=FALSE;
struct queue_desc_struct **dump_script_pointer;
/*}}}*/

/*{{{ Forward declarations */
#ifndef STANDALONE
LOCAL gint Load_Next_Subscript(gpointer data);
LOCAL gint Load_Script_Now(gpointer data);
LOCAL void Close_script_file(void);
#endif
/*}}}*/

#define gtk_entry_get_text(x) gtk_entry_buffer_get_text(gtk_entry_get_buffer(x))

/*{{{  Local functions*/
/*{{{ Setting title and status*/
LOCAL void
set_main_window_title(void) {
 static gchar buffer[FilenameLength+31];
 snprintf(buffer, FilenameLength+30, "avg_q user interface: Script %s", filename);
 gtk_window_set_title (GTK_WINDOW (Avg_Q_Main_Window), buffer);
}
LOCAL void
set_status(gchar *message) {
 gsize thisutflength;
 gchar * const thisutfstring=g_locale_to_utf8(message,-1,NULL,&thisutflength,NULL);
 gtk_label_set_text(GTK_LABEL(Avg_q_StatusBar), thisutfstring);
}
LOCAL void
set_runstatus(gchar *message) {
 gsize thisutflength;
 gchar * const thisutfstring=g_locale_to_utf8(message,-1,NULL,&thisutflength,NULL);
 gtk_label_set_text(GTK_LABEL(Avg_q_StatusRunBar), thisutfstring);
}
LOCAL void
set_running(Bool run) {
 running=run;
 if (running) {
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Run_Action), FALSE);
#ifndef STANDALONE
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Run_Subscript_Action), FALSE);
#endif
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Dump_Action), FALSE);
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Stop_Action), TRUE);
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Kill_Action), TRUE);
 } else {
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Run_Action), TRUE);
#ifndef STANDALONE
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Run_Subscript_Action), TRUE);
#endif
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Dump_Action), TRUE);
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Stop_Action), FALSE);
  g_simple_action_set_enabled(G_SIMPLE_ACTION(Kill_Action), FALSE);
 }
}
/*}}}*/

#ifndef STANDALONE
/*{{{  Interactive mode functions*/
LOCAL GtkTextIter current_iter;
LOCAL growing_buf static_linebuf;
LOCAL int
getNextLine(void) {
 growing_buf_clear(&static_linebuf);
 if (gtk_text_iter_is_end(&current_iter)) return -1;
 while (1) {
  gunichar const thischar=gtk_text_iter_get_char(&current_iter);
  gchar thisgchar[6],*thislocalechar=NULL;
  gsize thisgcharlength,thislocalecharlength;
  gtk_text_iter_forward_char(&current_iter);
  if (thischar=='\n' || thischar==0) {
   break;
  }
  thisgcharlength=g_unichar_to_utf8(thischar,(gchar *)&thisgchar);
  thislocalechar=g_locale_from_utf8((gchar *)&thisgchar,thisgcharlength,NULL,&thislocalecharlength,NULL);
  growing_buf_append(&static_linebuf, thislocalechar, thislocalecharlength);
  free_pointer((void **)&thislocalechar);
 }
 growing_buf_appendchar(&static_linebuf, '\0');
 return 1;
}
LOCAL int
getFirstLine(void) {
 gtk_text_buffer_get_start_iter(CurrentScriptBuffer, &current_iter);
 return getNextLine();
}
LOCAL void
delete_to_EOL(void) {
 GtkTextIter nextline_iter=current_iter;
 /* Only backward_char if there was a next line to move to */
 if (gtk_text_iter_forward_line(&nextline_iter)) gtk_text_iter_backward_char(&nextline_iter);
 gtk_text_buffer_delete(CurrentScriptBuffer, &current_iter, &nextline_iter);
}
LOCAL int
CursorLineNumber(void) {
 gtk_text_buffer_get_iter_at_mark(CurrentScriptBuffer, &current_iter, gtk_text_buffer_get_insert(CurrentScriptBuffer));
 return gtk_text_iter_get_line(&current_iter);
}
LOCAL Bool 
has_Postline(void) {
 /* This one tries to determine whether the current sub-script (the one where the 
  * cursor is currently in) already contains a 'Post:' keyword. */
 int const cursor_line=CursorLineNumber();
 int line=0;
 Bool seen=FALSE;

 getFirstLine();
 do {
  if (IS_SEPLINE(static_linebuf.buffer_start)) {
   /* Sub-script separator */
   if (line<=cursor_line) {
    seen=FALSE;
   } else {
    break;
   }
  } else if (IS_POSTLINE(static_linebuf.buffer_start)) {
   seen=TRUE;
  }
  line++;
 } while (getNextLine()!= -1);

 return seen;
}
LOCAL void 
goto_line(int line) {
 gtk_text_buffer_get_iter_at_line(CurrentScriptBuffer, &current_iter, line);
 gtk_text_buffer_place_cursor(CurrentScriptBuffer, &current_iter);
}
/*}}}  */
#endif

/*{{{ Notice window*/
LOCAL GtkWidget *Notice_window=NULL;
LOCAL void
Notice_window_close(GtkWidget *window) {
 gtk_window_destroy(GTK_WINDOW(window));
 Notice_window=NULL;
}
LOCAL void
Notice_window_close_button(GtkDialog *dialog, gint response_id, GtkWidget *window) {
 Notice_window_close(Notice_window);
}
LOCAL void
Notice(gchar *message) {
 GtkWidget *box1;
 GtkWidget *label;
 GtkWidget *button;

 if (Notice_window!=NULL) {
  gtk_window_destroy(GTK_WINDOW(Notice_window));
  Notice_window=NULL;
 }

 Notice_window=gtk_window_new();
 gtk_window_set_transient_for(GTK_WINDOW (Notice_window), GTK_WINDOW (Avg_Q_Main_Window));
 //gtk_window_set_position (GTK_WINDOW (Notice_window), GTK_WIN_POS_MOUSE);
 g_signal_connect (G_OBJECT (Notice_window), "destroy", G_CALLBACK (Notice_window_close), NULL);
 gtk_window_set_title (GTK_WINDOW (Notice_window), "Notice");

 box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
 gtk_window_set_child(GTK_WINDOW(Notice_window), box1);

 label = gtk_label_new (message);
 gtk_box_append(GTK_BOX(box1), label);

 button=gtk_button_new_with_label("Okay");
 g_signal_connect_after(G_OBJECT(button), "clicked", G_CALLBACK(Notice_window_close_button), G_OBJECT (Notice_window));
 gtk_box_append(GTK_BOX(box1), button);

 gtk_window_present (GTK_WINDOW(Notice_window));
}
/*}}}*/

/*{{{ Handle error and trace messages*/
#define ERROR_STATUS 1
#define ERRORMESSAGE_SIZE 1024
LOCAL char errormessage[ERRORMESSAGE_SIZE];
LOCAL void
error_exit(const external_methods_ptr emeth, const char *msgtext) __attribute__ ((__noreturn__));
LOCAL void
error_exit(const external_methods_ptr emeth, const char *msgtext) {
 char * newlinepos;
 if (strstr(msgtext, "by user")!=NULL) {
  *errormessage= '\0';
 } else {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "ERROR - ");
 }
 snprintf(errormessage+strlen(errormessage), ERRORMESSAGE_SIZE-strlen(errormessage), msgtext, emeth->message_parm[0], emeth->message_parm[1],
	emeth->message_parm[2], emeth->message_parm[3],
	emeth->message_parm[4], emeth->message_parm[5],
	emeth->message_parm[6], emeth->message_parm[7]);

 /* The newline makes an ugly black spot at least on Windows: erase it. */
 newlinepos=strchr(errormessage, '\n');
 if (newlinepos!=NULL) *newlinepos='\0';

 longjmp(((my_emethod *)emeth)->error_jmp, ERROR_STATUS);
}
LOCAL void
TracePanelDestroy(GtkWidget *window) {
 if (TracePanel!=NULL) {
  gtk_window_destroy(GTK_WINDOW(window));
  TracePanel=NULL;
 }
}
LOCAL void
trace_message(const external_methods_ptr emeth, const int lvl, const char *msgtext) {
 int const llvl=(lvl>=0 ? lvl : -lvl-1);
 if (emeth->trace_level>=llvl) {
  if (TracePanel==NULL) {
   GtkWidget *window;
   GtkWidget *box1;
   GtkWidget *scrolledwindow;

   window = gtk_window_new();
   //gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);
   gtk_window_set_title (GTK_WINDOW (window), "Trace messages");
   gtk_window_set_default_size (GTK_WINDOW(window), TRACEFRAME_SIZE_X, TRACEFRAME_SIZE_Y);

   g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (TracePanelDestroy), NULL);

   box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_window_set_child(GTK_WINDOW(window), box1);

   scrolledwindow = gtk_scrolled_window_new();
   gtk_widget_set_hexpand(GTK_WIDGET(scrolledwindow),TRUE);
   gtk_widget_set_vexpand(GTK_WIDGET(scrolledwindow),TRUE);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_append(GTK_BOX(box1), scrolledwindow);

   TracePanel=gtk_text_view_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), TracePanel);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(TracePanel), FALSE);
   gtk_window_present(GTK_WINDOW(window));
  }
  *errormessage='\0';
  if (lvl==0) {
   strcpy(errormessage, "WARNING - ");
  } else if (lvl>0) {
   snprintf(errormessage, ERRORMESSAGE_SIZE, " T%d - ", lvl);
  }
  snprintf(errormessage+strlen(errormessage), ERRORMESSAGE_SIZE-strlen(errormessage), msgtext, emeth->message_parm[0], emeth->message_parm[1],
	 emeth->message_parm[2], emeth->message_parm[3],
	 emeth->message_parm[4], emeth->message_parm[5],
	 emeth->message_parm[6], emeth->message_parm[7]);

  /* The following procedure is necessary to always insert at the end, because
   * the cursor will be positioned within the buffer even if we set
   * cursor_visible to FALSE, and insertion occurs there if we use the simple
   * insert_at_cursor... */
  {GtkTextBuffer * const textbuffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(TracePanel));
   GtkTextIter enditer;
   gsize thisutflength;
   gchar * const thisutfstring=g_locale_to_utf8(errormessage,-1,NULL,&thisutflength,NULL);
   gtk_text_buffer_get_end_iter(textbuffer, &enditer);
   gtk_text_buffer_insert(textbuffer, &enditer, thisutfstring, thisutflength);
   free_pointer((void **)&thisutfstring);
  }
 }
}
LOCAL void
static_execution_callback(const transform_info_ptr tinfo, const execution_callback_place where) {
 if (stop_request) {
  /* Be sure to do ERREXIT only once after `stop_request' has been set
   * to TRUE by the main program; then the queues are exited, and
   * the execution_callback called with E_CALLBACK_BEFORE_EXIT and 
   * E_CALLBACK_AFTER_EXIT for each method, we don't want to hamper this... */
  stop_request=FALSE;
  ERREXIT(tinfostruc.emethods, "Stopped by user.\n");
 }
 if (where==E_CALLBACK_BEFORE_EXEC) {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "Executing script %d line %2d: %s", tinfo->methods->script_number, tinfo->methods->line_of_script, tinfo->methods->method_name);
  set_runstatus(errormessage);
  while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);
 }
}
/*}}}*/

LOCAL void
change_dir_finished(GObject *source, GAsyncResult *result, void *data) {
 GFile *gfile;
 GError *error = NULL;
 gfile = gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (source), result, &error);
 if (gfile!=NULL) {
  char *gfilename=g_file_get_path(gfile);
  if (chdir(gfilename)==0) {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "Current directory is %s", gfilename);
  } else {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "change_dir: %s: %s", gfilename, strerror(errno));
  }
  set_status(errormessage);
  g_object_unref(gfile);
 }
}
LOCAL void
change_dir(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 char * const path=g_get_current_dir();
 GFile * const gfile=g_file_new_for_path(path);
 GtkFileDialog *filedialog = gtk_file_dialog_new();
 gtk_file_dialog_set_initial_folder(filedialog, gfile);
 g_object_unref(gfile);
 g_free (path);
 gtk_file_dialog_select_folder(filedialog,GTK_WINDOW(Avg_Q_Main_Window),NULL,&change_dir_finished,NULL);
}
LOCAL int
change_dir_cb(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 change_dir(NULL,NULL,NULL);
 return 1;
}
LOCAL void Run_Script(void);
LOCAL void
Run_Script_Command(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 set_status("Running script...");
 save_only_script=only_script=0;
 Run_Script();
}
LOCAL int
Run_Script_Command_cb(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 Run_Script_Command(NULL,NULL,NULL);
 return 1;
}
/*}}} */

#ifndef STANDALONE
/*{{{ The whole MethodInstance implementation is skipped when compiling STANDALONE */
/*{{{ MethodInstance: GUI for method configuration*/
LOCAL my_emethod instance_emethod;
LOCAL struct transform_methods_struct method;
LOCAL struct transform_info_struct instance_tinfo;
LOCAL GtkWidget *dialog_widgets[1000];
LOCAL GtkWidget *MethodInstance_window=NULL;
LOCAL GtkWidget *MethodInstance_Defaultbutton;
LOCAL Bool variables_only=FALSE, old_method=FALSE;
LOCAL int old_method_cursorline=0;
LOCAL int nr_of_active_elements=0;

LOCAL void
MethodInstance_init_methodstruc(void) {
 /* This method just initializes the struct storage for the first time */
 method.nr_of_arguments=0;
 method.arguments=NULL;
 method.local_storage_size=0;
 method.local_storage=NULL;
 method.within_branch=FALSE;
 method.get_epoch_override=FALSE;
 method.init_done=FALSE;

 set_external_methods(&instance_emethod.emethod, &error_exit, &trace_message, NULL);
 instance_emethod.emethod.trace_level=0;
 instance_tinfo.methods= &method;
 instance_tinfo.emethods= &instance_emethod.emethod;
}
LOCAL void
MethodInstance_config_from_defaults(void) {
 int current_argument;
 for (current_argument=0; current_argument<method.nr_of_arguments; current_argument++) {
  transform_argument_descriptor * const argument_descriptor=method.argument_descriptors+current_argument;
  transform_argument_writeable * const argument=(transform_argument_writeable *)method.arguments+current_argument;
  argument->is_set=FALSE;
  switch(argument_descriptor->type) {
   case T_ARGS_TAKES_NOTHING:
    argument->is_set=(Bool)argument_descriptor->default_value;
    break;
   case T_ARGS_TAKES_LONG:
    argument->arg.i=(long)argument_descriptor->default_value;
    break;
   case T_ARGS_TAKES_DOUBLE:
    argument->arg.d=(double)argument_descriptor->default_value;
    break;
   case T_ARGS_TAKES_STRING_WORD:
   case T_ARGS_TAKES_SENTENCE:
   case T_ARGS_TAKES_FILENAME:
    if (argument_descriptor->choices==NULL) {
     argument->arg.s=NULL;
    } else {
     argument->arg.s=strdup((char *)argument_descriptor->choices);
    }
    break;
   case T_ARGS_TAKES_SELECTION:
    argument->arg.i=(long)argument_descriptor->default_value;
    break;
   default:
    continue;
  }
 }
}
LOCAL void
MethodInstance_setup_from_method(void (* const method_select)(transform_info_ptr)) {
 (*method_select)(&instance_tinfo);
 allocate_methodmem(&instance_tinfo);

 MethodInstance_config_from_defaults();
}
LOCAL void
MethodInstance_config_from_dialog(void) {
 int current_argument, active_widget=0;
 char *endptr;
 for (current_argument=0; current_argument<method.nr_of_arguments; current_argument++) {
  transform_argument_descriptor * const argument_descriptor=method.argument_descriptors+current_argument;
  transform_argument_writeable * const argument=(transform_argument_writeable *)method.arguments+current_argument;
  int const variable=argument->variable;

  if (!variables_only || variable!=0) {
   if (*argument_descriptor->option_letters!='\0') {
    GtkWidget * const checkbox=dialog_widgets[active_widget++];
    argument->is_set=gtk_check_button_get_active(GTK_CHECK_BUTTON(checkbox));
   } else {
    argument->is_set=FALSE;
   }
   /* Check the value only for required arguments and checked options */
   if (*argument_descriptor->option_letters=='\0' || argument->is_set) {
    switch(argument_descriptor->type) {
     case T_ARGS_TAKES_NOTHING:
      break;
     case T_ARGS_TAKES_LONG: {
      GtkWidget * const entry=dialog_widgets[active_widget++];
      const gchar * const text=gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(entry)));
      if (*text!='\0') {
       argument->arg.i=strtol(text, &endptr, 10);
       if (*endptr=='\0') argument->is_set=TRUE;
      }
      }
      break;
     case T_ARGS_TAKES_DOUBLE: {
      GtkWidget * const entry=dialog_widgets[active_widget++];
      const gchar * const text=gtk_entry_get_text(GTK_ENTRY(entry));
      if (*text!='\0') {
       argument->arg.d=get_value(text, &endptr);
       if (*endptr=='\0') argument->is_set=TRUE;
      }
      }
      break;
     case T_ARGS_TAKES_STRING_WORD:
     case T_ARGS_TAKES_SENTENCE:
     case T_ARGS_TAKES_FILENAME: {
      GtkWidget * const entry=dialog_widgets[active_widget++];
      const gchar * const text=gtk_entry_get_text(GTK_ENTRY(entry));
      gsize thislocalelength;

      if (*text!='\0') {
       argument->arg.s=g_locale_from_utf8(text,-1,NULL,&thislocalelength,NULL);
       argument->is_set=TRUE;
      }
      }
      break;
     case T_ARGS_TAKES_SELECTION: {
      GtkWidget * const combo=dialog_widgets[active_widget++];
      argument->arg.i=gtk_drop_down_get_selected(GTK_DROP_DOWN(combo));
      argument->is_set=TRUE;
      }
      break;
     default:
      continue;
    }
   } else {
    /* Keep the active_widget index up to date */
    switch(argument_descriptor->type) {
     case T_ARGS_TAKES_NOTHING:
      break;
     case T_ARGS_TAKES_LONG:
     case T_ARGS_TAKES_DOUBLE:
     case T_ARGS_TAKES_STRING_WORD:
     case T_ARGS_TAKES_SENTENCE:
     case T_ARGS_TAKES_FILENAME:
     case T_ARGS_TAKES_SELECTION:
      active_widget++;
      break;
     default:
      continue;
    }
   }
  }
  if (variable!=0) {
   char * const delimiters=" \t\r\n";
   char *inbuf;
   growing_buf_clear(&static_linebuf);

   /* Convert argument to string without escaping of delimiters: */
   sprint_argument_value(&static_linebuf, &method, current_argument, FALSE);
   inbuf=static_linebuf.buffer_start;
   while (*inbuf!='\0' && strchr(delimiters, *inbuf)!=NULL) inbuf++;
   if (*inbuf!='\0') {
    if (variable>nr_of_script_variables) {
     /* We have to reallocate and copy the old array */
     char **new_vars=malloc(variable*sizeof(char *));
     int i;
     for (i=0; i<variable-1; i++) {
      if (i>=nr_of_script_variables) {
       new_vars[i]=NULL;
      } else {
       new_vars[i]=script_variables[i];
      }
     }
     script_variables=new_vars;
     nr_of_script_variables=variable;
    }
    script_variables[variable-1]=malloc((strlen(inbuf)+1)*sizeof(char));
    strcpy(script_variables[variable-1], inbuf);
#if 0
    printf("Variable %d set: >%s<\n", variable, script_variables[variable-1]);
#endif
   }
  }
 }
}
LOCAL Bool
MethodInstance_setup_from_line(growing_buf *linebuf) {
 void (* const *m_select)(transform_info_ptr);
 char *inbuf;
 int current_argument;
 void (* const * const method_selects)(transform_info_ptr)=get_method_list();

 growing_buf tokenbuf;
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);
 tokenbuf.delim_protector='\\';

 /* Remove comment if it exists */
 if ((inbuf=strchr(linebuf->buffer_start, '#'))!=NULL) {
  while (*inbuf!='\0') *inbuf++ = '\0';
 }
 if (*linebuf->buffer_start=='>') {
  method.within_branch=TRUE;
  *linebuf->buffer_start=' ';
 }
 if (*linebuf->buffer_start=='!') {
  method.get_epoch_override=TRUE;
  *linebuf->buffer_start=' ';
 }
 linebuf->delimiters=" \t\r\n"; /* Get only the first word */
 if (!growing_buf_get_firsttoken(linebuf,&tokenbuf)) {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "setup_queue: Empty line.");
  set_status(errormessage);
  return FALSE;
 }
 for (m_select=method_selects; *m_select!=NULL; m_select++) {
  (**m_select)(&instance_tinfo);
  if (strcmp(tokenbuf.buffer_start, method.method_name)==0) break;
 }
 if (*m_select==NULL) {
  /* The method name was not found - as a convenience, we look for
   * the first method with a name starting like the current name */
  for (m_select=method_selects; *m_select!=NULL; m_select++) {
   (**m_select)(&instance_tinfo);
   if (strncmp(tokenbuf.buffer_start, method.method_name, strlen(tokenbuf.buffer_start))==0) break;
  }
  if (*m_select==NULL) {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "setup_queue: Unknown method %s", tokenbuf.buffer_start);
   set_status(errormessage);
   return FALSE;
  }
 }
 /* And as another convenience (too convenient? I don't think so), if the line
  * is not accepted by setup_method, configure from defaults: */
 switch (setjmp(instance_emethod.error_jmp)) {
  case 0:
   linebuf->delimiters=""; /* Get the rest of the line */
   if (growing_buf_get_nexttoken(linebuf,&tokenbuf)) {
    setup_method(&instance_tinfo, &tokenbuf);
   } else {
    setup_method(&instance_tinfo, NULL);
   }
   break;
  case ERROR_STATUS:
   set_status(errormessage);
   /* allocate_methodmem has already been called by setup_method */
   MethodInstance_config_from_defaults();
   gtk_window_unminimize(GTK_WINDOW(Avg_Q_Main_Window));
   return TRUE;
 }

 /* Now fill in the default values for the unset options so that they show
  * up correctly in the dialog: */
 for (current_argument=0; current_argument<method.nr_of_arguments; current_argument++) {
  transform_argument_descriptor * const argument_descriptor=method.argument_descriptors+current_argument;
  transform_argument_writeable * const argument=(transform_argument_writeable *)method.arguments+current_argument;
  if (*argument_descriptor->option_letters=='\0' ||
      argument->is_set) continue;
  switch(argument_descriptor->type) {
   case T_ARGS_TAKES_LONG:
    argument->arg.i=(long)argument_descriptor->default_value;
    break;
   case T_ARGS_TAKES_DOUBLE:
    argument->arg.d=(double)argument_descriptor->default_value;
    break;
   case T_ARGS_TAKES_STRING_WORD:
   case T_ARGS_TAKES_SENTENCE:
   case T_ARGS_TAKES_FILENAME:
    if (argument_descriptor->choices==NULL) {
     argument->arg.s=NULL;
    } else {
     argument->arg.s=strdup((char *)argument_descriptor->choices);
    }
    break;
   case T_ARGS_TAKES_SELECTION:
    argument->arg.i=(long)argument_descriptor->default_value;
    break;
   default:
    continue;
  }
 }
 growing_buf_free(&tokenbuf);
 return TRUE;
}
LOCAL void
method_dialog_close(GtkWidget *window) {
 if (MethodInstance_window!=NULL) {
  gtk_window_destroy(GTK_WINDOW(MethodInstance_window));
  MethodInstance_window=NULL;
  free_methodmem(&instance_tinfo);
  set_status("Operation canceled.");
 }
}
LOCAL void
method_dialog_ok(GtkWidget *window) {
 char *message;

 MethodInstance_config_from_dialog();

 message=check_method_args(&instance_tinfo);
 if (message==NULL) {
  gchar *thisutfstring;
  gsize thisutflength;
  enum {CL_NOTEMPTY, CL_EMPTY, CL_EMPTYBRANCH, CL_EMPTYOVERRIDE} cursor_line_empty=CL_NOTEMPTY;
  growing_buf buf;
  growing_buf_init(&buf);
  growing_buf_allocate(&buf, 0);
  sprint_method(&buf, &method);

  /* Note that old_method_cursorline is set also for old_method==FALSE.
   * It is always the line where the cursor was on when the dialog was built
   * (set at the start of MethodInstance_build_dialog()). We want to return
   * there in any case, as the user might e.g. want to copy something from
   * the script to paste it to the dialog, which will change the current line. */
  goto_line(old_method_cursorline);
  if (old_method) {
   delete_to_EOL();
  }

  {/* Test whether the current line is empty */
  if (getNextLine()<=0 || *static_linebuf.buffer_start=='\0') {
   cursor_line_empty=CL_EMPTY;
  } else if (strcmp(static_linebuf.buffer_start, ">")==0) {
   cursor_line_empty=CL_EMPTYBRANCH;
  } else if (strcmp(static_linebuf.buffer_start, "!")==0) {
   cursor_line_empty=CL_EMPTYOVERRIDE;
  }
  }
  switch (cursor_line_empty) {
   case CL_NOTEMPTY:
    break;
   case CL_EMPTY:
    break;
   case CL_EMPTYBRANCH:
   case CL_EMPTYOVERRIDE:
    break;
  }

  if (method.within_branch) {
   gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, ">", 1);
  }
  if (method.get_epoch_override) {
   gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, "!", 1);
  }
  thisutfstring=g_locale_to_utf8(buf.buffer_start,-1,NULL,&thisutflength,NULL);
  gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, thisutfstring, thisutflength);
  free_pointer((void **)&thisutfstring);

  if (method.method_type==COLLECT_METHOD &&
      strcmp(method.method_name, "null_sink")!=0 &&
      !has_Postline()) {
   gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, "\nPost:", 6);
  }

  switch (cursor_line_empty) {
   case CL_NOTEMPTY:
    /* Move to the start of the next line, which is the original cursor line */
    gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, "\n", 1);
    break;
   case CL_EMPTY:
   case CL_EMPTYBRANCH:
   case CL_EMPTYOVERRIDE:
    /* On the last line, we should add a carriage return */
    gtk_text_buffer_get_iter_at_mark(CurrentScriptBuffer, &current_iter, gtk_text_buffer_get_insert(CurrentScriptBuffer));
    if (gtk_text_iter_is_end(&current_iter)) {
     gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, "\n", 1);
    }
    break;
  }
  growing_buf_free(&buf);

  /* This is necessary because method_dialog_close() is also called on destroy, so MethodInstance_window
   * must be NULL before it is destroyed... */
  GtkWidget *tmpwin=MethodInstance_window;
  MethodInstance_window=NULL;
  free_methodmem(&instance_tinfo);
  set_status("Operation accepted.");
  gtk_window_destroy(GTK_WINDOW(tmpwin));
  gtk_widget_grab_focus(GTK_WIDGET(Avg_Q_Scriptpanel));
 } else {
  gtk_label_set_text(GTK_LABEL(Dialog_Configuration_Error), message);
  gtk_widget_set_visible (Dialog_Configuration_Error,TRUE);
 }
}
typedef struct {
 GtkWidget **in_dialog_widgets;
 Bool optional;
} method_file_sel_data;
LOCAL void
method_fileselect_finished(GObject *source, GAsyncResult *result, void *gdata) {
 GFile *gfile;
 GError *error = NULL;
 method_file_sel_data *data=(method_file_sel_data *)gdata;
 gfile = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);
 if (gfile!=NULL) {
  char *gfilename=g_file_get_path(gfile);
  GtkEntryBuffer * const entrybuffer=gtk_entry_buffer_new(gfilename,strlen(gfilename));
  gtk_entry_set_buffer(GTK_ENTRY(*data->in_dialog_widgets), entrybuffer);
  if (data->optional) {
   gtk_check_button_set_active(GTK_CHECK_BUTTON(data->in_dialog_widgets[-1]), TRUE);
  }
  g_free (gfilename);
  g_object_unref(gfile);
 }
}
LOCAL void
method_fileselect_all(method_file_sel_data *data) {
 const gchar * const text=gtk_entry_get_text(GTK_ENTRY(*data->in_dialog_widgets));
 GFile *gfile;
 GtkFileDialog *filedialog = gtk_file_dialog_new();

 if (text!=NULL) {
  if (strchr(text, '*')!=NULL) {
   GtkFileFilter *filter=gtk_file_filter_new();
   GtkFileFilter *allfiles=gtk_file_filter_new();
   gtk_file_filter_add_pattern(filter,text); gtk_file_filter_set_name(filter,text);
   gtk_file_filter_add_pattern(allfiles,"*"); gtk_file_filter_set_name(allfiles,"All files");
   GListStore* liststore = g_list_store_new (GTK_TYPE_FILE_FILTER);
   g_list_store_append(liststore, filter);
   g_list_store_append(liststore, allfiles);
   gtk_file_dialog_set_filters(filedialog,G_LIST_MODEL(liststore));
   char * const path=g_get_current_dir();
   gfile=g_file_new_for_path(path);
   gtk_file_dialog_set_initial_folder(filedialog, gfile);
   g_object_unref(gfile);
   g_free (path);
  } else {
   if (g_path_is_absolute(text)) {
    gfile=g_file_new_for_path(text);
   } else {
    char * const path=g_get_current_dir();
    gfile=g_file_new_build_filename(path,text,NULL);
    g_free(path);
   }
   gtk_file_dialog_set_initial_file(filedialog, gfile);
   g_object_unref(gfile);
  }
 }
 gtk_file_dialog_set_title(filedialog, "Select a file argument");
 gtk_file_dialog_open(filedialog,GTK_WINDOW(Avg_Q_Main_Window),NULL,&method_fileselect_finished,data);
}
LOCAL void
method_fileselect_normal(GtkWidget *select_button, GtkWidget **in_dialog_widgets) {
 static method_file_sel_data data;

 data.in_dialog_widgets=in_dialog_widgets;
 data.optional=FALSE;

 method_fileselect_all(&data);
}
LOCAL void
method_fileselect_optional(GtkWidget *select_button, GtkWidget **in_dialog_widgets) {
 static method_file_sel_data data;

 data.in_dialog_widgets=in_dialog_widgets;
 data.optional=TRUE;

 method_fileselect_all(&data);
}
LOCAL Bool
method_combo_changed_event(GtkWidget *widget, gpointer data, gpointer checkbutton) {
 //printf("method_combo_changed_event\n");
 gtk_check_button_set_active(GTK_CHECK_BUTTON(checkbutton), TRUE);
 return FALSE;
}
LOCAL void
MethodInstance_build_dialog(void) {
 GtkWidget *box1, *hbox;
 GtkWidget *label;
 GtkWidget *button;

 old_method_cursorline=CursorLineNumber();
 nr_of_active_elements=0;
 MethodInstance_window=gtk_window_new();
 gtk_window_set_transient_for(GTK_WINDOW (MethodInstance_window), GTK_WINDOW (Avg_Q_Main_Window));
 //gtk_window_set_position (GTK_WINDOW (MethodInstance_window), GTK_WIN_POS_MOUSE);
 g_signal_connect (G_OBJECT (MethodInstance_window), "destroy", G_CALLBACK (method_dialog_close), NULL);
 gtk_window_set_title (GTK_WINDOW (MethodInstance_window), method.method_name);

 box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
 gtk_window_set_child(GTK_WINDOW(MethodInstance_window), box1);

 label = gtk_label_new (method.method_description);
 gtk_box_append(GTK_BOX(box1), label);

 if (method.nr_of_arguments==0) {
  label = gtk_label_new ("This method has neither options nor arguments.");
  gtk_box_append(GTK_BOX(box1), label);
 } else {
  int current_argument;
  for (current_argument=0; current_argument<method.nr_of_arguments; current_argument++) {
   transform_argument_descriptor * const argument_descriptor=method.argument_descriptors+current_argument;
   transform_argument_writeable * const argument=(transform_argument_writeable *)method.arguments+current_argument;
   int const variable=argument->variable;

   if (variable!=0 &&
       variable<=nr_of_script_variables &&
       script_variables[variable-1]!=NULL &&
       argument->is_set) {
    growing_buf arg, tokenbuf;
    growing_buf_settothis(&arg, script_variables[variable-1]);
    arg.delimiters="";
    growing_buf_init(&tokenbuf);
    growing_buf_allocate(&tokenbuf,0);
#if 0
    printf("Variable %d found: Setting %s to >%s<\n", variable, argument_descriptor->description, script_variables[variable-1]);
#endif
    /* Otherwise, accept_argument will reject the new contents: */
    argument->is_set=FALSE;
    /* Reset token pointer, just as growing_buf_get_firsttoken does */
    arg.current_token=arg.buffer_start;
    accept_argument(&instance_tinfo, &arg, &tokenbuf, &method.argument_descriptors[current_argument]);
    growing_buf_free(&tokenbuf);
   }

   if (!variables_only || variable!=0) {
    Bool entry_is_optional=FALSE;
    int entry_index= -1;	/* Where in dialog_widgets is the last entry widget */

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(box1), hbox);
    if (*argument_descriptor->option_letters!='\0') {
     GtkWidget * const checkbox=gtk_check_button_new_with_label(argument_descriptor->option_letters);
     entry_is_optional=TRUE;
     gtk_box_append (GTK_BOX (hbox), checkbox);
     gtk_check_button_set_active(GTK_CHECK_BUTTON(checkbox), argument->is_set);
     dialog_widgets[nr_of_active_elements++]=checkbox;
    }
    /* Here comes the option description as label: */
    label = gtk_label_new (argument_descriptor->description);
    gtk_box_append (GTK_BOX(hbox), label);

    switch(argument_descriptor->type) {
     case T_ARGS_TAKES_NOTHING:
      break;
     case T_ARGS_TAKES_LONG: {
#define BUFFER_SIZE 300
      char buffer[BUFFER_SIZE];
      snprintf(buffer, BUFFER_SIZE, "%ld", argument->arg.i);
      GtkEntryBuffer * const entrybuffer=gtk_entry_buffer_new(buffer,strlen(buffer));
      GtkWidget * const entry=gtk_entry_new_with_buffer(GTK_ENTRY_BUFFER(entrybuffer));
      gtk_box_append (GTK_BOX(hbox), entry);
      entry_index=nr_of_active_elements;
      dialog_widgets[nr_of_active_elements++]=entry;
      }
      break;
     case T_ARGS_TAKES_DOUBLE: {
      char buffer[BUFFER_SIZE];
      snprintf(buffer, BUFFER_SIZE, "%g", argument->arg.d);
      GtkEntryBuffer * const entrybuffer=gtk_entry_buffer_new(buffer,strlen(buffer));
      GtkWidget * const entry=gtk_entry_new_with_buffer(GTK_ENTRY_BUFFER(entrybuffer));
      gtk_box_append (GTK_BOX(hbox), entry);
      entry_index=nr_of_active_elements;
      dialog_widgets[nr_of_active_elements++]=entry;
      }
      break;
#undef BUFFER_SIZE
     case T_ARGS_TAKES_STRING_WORD:
     case T_ARGS_TAKES_SENTENCE: {
      GtkWidget * const entry=gtk_entry_new();
      gtk_box_append (GTK_BOX(hbox), entry);
      if (argument->arg.s!=NULL) {
       gsize thisutflength;
       gchar *const buffer=g_locale_to_utf8(argument->arg.s,-1,NULL,&thisutflength,NULL);
       GtkEntryBuffer * const entrybuffer=gtk_entry_buffer_new(buffer,strlen(buffer));
       gtk_entry_set_buffer(GTK_ENTRY(entry), entrybuffer);
      }
      entry_index=nr_of_active_elements;
      dialog_widgets[nr_of_active_elements++]=entry;
      }
      break;
     case T_ARGS_TAKES_FILENAME: {
      GtkWidget * const entry=gtk_entry_new();
      GtkWidget * const select_button=gtk_button_new_with_label("Select");
      GCallback const select_callback=G_CALLBACK(entry_is_optional ? method_fileselect_optional : method_fileselect_normal);
      gtk_box_append (GTK_BOX(hbox), entry);
      if (argument->arg.s!=NULL) {
       gsize thisutflength;
       gchar *const buffer=g_locale_to_utf8(argument->arg.s,-1,NULL,&thisutflength,NULL);
       GtkEntryBuffer * const entrybuffer=gtk_entry_buffer_new(buffer,strlen(buffer));
       gtk_entry_set_buffer(GTK_ENTRY(entry), entrybuffer);
      }
      entry_index=nr_of_active_elements;
      dialog_widgets[nr_of_active_elements++]=entry;

      /* Note that we do not count the select_button as an active element,
       * since there will be no need to access its state or value */
      gtk_box_append (GTK_BOX(hbox), select_button);
      g_signal_connect (G_OBJECT (select_button), "clicked", select_callback, (gpointer)&dialog_widgets[nr_of_active_elements-1]);
      }
      break;
     case T_ARGS_TAKES_SELECTION: {
      GtkWidget * const combo=gtk_drop_down_new_from_strings(argument_descriptor->choices);

      gtk_box_append (GTK_BOX(hbox), combo);
      gtk_drop_down_set_selected(GTK_DROP_DOWN(combo),argument->arg.i);
      if (entry_is_optional) {
       g_signal_connect_after(combo, "notify::selected-item", G_CALLBACK(method_combo_changed_event), (gpointer)dialog_widgets[nr_of_active_elements-1]);
      }
      dialog_widgets[nr_of_active_elements++]=combo;
      }
      break;
     default:
      continue;
    }
    if (entry_index>=0) {
     /* Could be that there is a better way, but we want the `Return' key to
      * select the default button also within an entry widget.
      * We achieve this (for now?) by intercepting the key_press_event. */
     //GCallback const func=G_CALLBACK(entry_is_optional ?  method_optional_entry_keypress_event : method_normal_entry_keypress_event);
     //g_signal_connect (G_OBJECT (dialog_widgets[entry_index]), "key_press_event", func, (gpointer)&dialog_widgets[entry_index]);
    }
   }
  }
 }

 Dialog_Configuration_Error = gtk_label_new ("<Long message to force Configuration Error field width>");
 gtk_box_append(GTK_BOX(box1), Dialog_Configuration_Error);
 gtk_widget_set_visible (Dialog_Configuration_Error,FALSE);

 /* An OK button is now always shown - it could be that our convenient
  * start-of-method-name function has kicked in and the user wants to accept
  * that full method name. Therefore, independently of old_method and any
  * modifiable options, we'll have a default OK button. */
 hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
 gtk_box_append(GTK_BOX(box1), hbox);
 MethodInstance_Defaultbutton=gtk_button_new_with_label("Okay");
 g_signal_connect_after(G_OBJECT(MethodInstance_Defaultbutton), "clicked", G_CALLBACK(method_dialog_ok), G_OBJECT (MethodInstance_window));
 gtk_box_append (GTK_BOX(hbox), MethodInstance_Defaultbutton);
 button=gtk_button_new_with_label("Close");
 g_signal_connect_after(G_OBJECT(button), "clicked", G_CALLBACK(method_dialog_close), G_OBJECT (MethodInstance_window));
 gtk_box_append (GTK_BOX(hbox), button);
 gtk_window_present(GTK_WINDOW(MethodInstance_window));
}

LOCAL int
setup_from_line_requested(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 Bool have_something=TRUE;
 goto_line(CursorLineNumber());
 if (getNextLine()>0) {
  /* Special help messages... */
  if (IS_SEPLINE(static_linebuf.buffer_start)) {
   Notice(
    "This is a script separator.\n"
    "Such separators divide the whole text in multiple scripts\n"
    "which are independently processed from top to bottom.\n"
   );
  } else if (*static_linebuf.buffer_start=='#') {
   Notice(
    "Hash signs ('#') mark a line as comment.\n"
    "This can be used to annotate the script or to disable methods\n"
    "without deleting the line from the script.\n"
   );
  } else if (IS_POSTLINE(static_linebuf.buffer_start)) {
   Notice(
    "This is the `Post:' keyword.\n"
    "It marks the beginning of the post-processing queue.\n"
    "The collect method creates the end result of the iterated queue\n"
    "(an average, for example) which the post-processing queue will\n"
    "usually either save or display.\n"
   );
  } else {
   /* Now try to decode this into a method instance: */
   static_linebuf.delim_protector='\\';

   MethodInstance_init_methodstruc();
   if (MethodInstance_window!=NULL) {
    method_dialog_close(GTK_WIDGET(MethodInstance_window));
   }
   if (MethodInstance_setup_from_line(&static_linebuf)) {
    old_method=TRUE;
    MethodInstance_build_dialog();
   } else {
    have_something=FALSE;
   }
  }
 } else {
  have_something=FALSE;
 }
 if (!have_something) {
  Notice(
   "This line neither contains a known method name\n"
   "(not even the beginning of one!), nor the `Post:'\n"
   "keyword, nor a comment (`#').\n"
  );
 }
 return 1;
}
/*}}}*/

/*{{{ Running scripts*/
LOCAL void 
Run_Script(void) {
 /* The `succeeded' variable should not be stack-based because it might
  * be clobbered by longjmp */
 succeeded=FALSE;
 if (running) {
  set_runstatus("Script is already running.");
 } else {
  void (* const * const method_selects)(transform_info_ptr)=get_method_list();
  int laststatus;
  growing_buf script;
  growing_buf_init(&script);
  growing_buf_allocate(&script, 0);

  set_running(TRUE);

  switch (setjmp(external_method.error_jmp)) {
   case 0:
    /* Initialize the method position info */
    iter_queue.current_input_line=post_queue.current_input_line=0;
    iter_queue.current_input_script=post_queue.current_input_script=0;
    if (dumponly) {
     if (nr_of_script_variables!=0) {
      TRACEMS(tinfostruc.emethods, 0, "Script variables are ignored when dumping!\n");
     }
     fprintf(dumpfile, "#define DUMP_SCRIPT_NAME ");
     /* Use the trafo_stc.c utility function to output the file name as a
      * string that is read as the original string by a C parser */
     fprint_cstring(dumpfile, filename);
     fprintf(dumpfile, "\n\n");
    }
    laststatus=getFirstLine();
    while (laststatus!= -1) {
     succeeded=FALSE;	/* Only succeed if all sub-scripts succeed */
     /* Possibly execute multiple sub-scripts */
     growing_buf_clear(&script);
     while (laststatus!= -1) {
      if (IS_SEPLINE(static_linebuf.buffer_start)) {
       laststatus=getNextLine();
       break;
      }
      growing_buf_appendstring(&script, static_linebuf.buffer_start);
      growing_buf_appendstring(&script, "\n");
      laststatus=getNextLine();
     }
     /* Remove the last \0 kept by growing_buf_appendstring - it would get counted as an empty line.
      * Note that '\n' will be replaced by \0 during parsing, so the line tokens are terminated. */
     if (script.current_length>0) script.current_length--;
     if (setup_queue_from_buffer(&tinfostruc, method_selects, &iter_queue, &post_queue, &script)) {
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
       int const variables_requested1=set_queuevariables(&tinfostruc, &iter_queue, nr_of_script_variables, script_variables);
       int const variables_requested2=set_queuevariables(&tinfostruc, &post_queue, nr_of_script_variables, script_variables);
       int const max_var_requested=(variables_requested1>variables_requested2 ? variables_requested1 : variables_requested2);
       if (max_var_requested>nr_of_script_variables) {
	ERREXIT2(tinfostruc.emethods, "Arguments up to $%d were requested by the script, but only %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
       }
       if (max_var_requested<nr_of_script_variables) {
	TRACEMS2(tinfostruc.emethods, 0, "%d variables were requested by the script, %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
       }
#ifdef FP_EXCEPTION
       fp_exception_init();	/* This sets up the math exception signal handler */
#endif
       do_queues(&tinfostruc, &iter_queue, &post_queue);
       if (tinfostruc.tsdata!=NULL) {
	free_tinfo(&tinfostruc);
       }
      }
      /* This is necessary in order to free the method memory allocated during setup: */
      free_queuemethodmem(&tinfostruc, &iter_queue);
      free_queuemethodmem(&tinfostruc, &post_queue);
     } else {
      set_runstatus("Script is empty.");
     }
     succeeded=TRUE;
     if (only_script>0 && iter_queue.current_input_script==only_script) break;
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
     dumponly=FALSE;
     if (dumpfile!=stdout) {
      fclose(dumpfile);
      dumpfile=stdout;
     }
     set_runstatus("Script was dumped.");
    } else {
     set_runstatus("Script was executed.");
    }
    only_script=save_only_script;
    if (!interactive && scriptfile==NULL) {
     g_idle_add((GSourceFunc)g_main_loop_quit, mainloop);
    }
    break;
   case ERROR_STATUS:
    /* Catch errors in exiting the queue (should be none!) */
    switch (setjmp(external_method.error_jmp)) {
     case 0:
      free_tinfo(&tinfostruc);
      if (iter_queue.start!=NULL) exit_queue(&tinfostruc, &iter_queue);
      if (post_queue.start!=NULL) exit_queue(&tinfostruc, &post_queue);
      break;
     case ERROR_STATUS:
      break;
    }
    set_runstatus(errormessage);
    goto_line(tinfostruc.methods->line_of_script-1);
    gtk_window_unminimize(GTK_WINDOW(Avg_Q_Main_Window));
    Close_script_file();
    save_only_script=only_script=0;
    interactive=TRUE;
    break;
  }
  set_running(FALSE);
  if (scriptfile!=NULL) {
   Avg_q_Load_Subscript_Tag=g_idle_add(Load_Next_Subscript, NULL);
  }
  growing_buf_free(&script);
 }
 return;
}
LOCAL int
CursorSubScript(void) {
 /* Small utility method to return the number of the script to which
  * the cursor line belongs (starting with 1; a separator line counts as
  * belonging to the following script) */
 int script=1;
 int const cursor_line=CursorLineNumber();
 int line=0;

 getFirstLine();
 do {
  if (IS_SEPLINE(static_linebuf.buffer_start)) {
   /* Sub-script separator */
   script++;
  }
  line++;
 } while (line<cursor_line && getNextLine()!= -1);

 return script;
}
LOCAL void
Run_CursorSubScript(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 only_script=CursorSubScript();
 set_status("Running sub-script at cursor...");
 Run_Script();
}
/*}}}*/
/*{{{ Loading script files*/
LOCAL void
clear_script(void) {
 GtkTextIter start, end;
 gtk_text_buffer_get_bounds(CurrentScriptBuffer, &start, &end);
 gtk_text_buffer_delete(CurrentScriptBuffer, &start, &end);
}
LOCAL void
Close_script_file(void) {
 if (scriptfile!=NULL) {
  if (scriptfile!=stdin) fclose(scriptfile);
  scriptfile=NULL;
 }
}
LOCAL gint
Load_Next_Subscript(gpointer data) {
 int c;
 /* We remember this here because it is tested also one time after scriptfile is already
  * closed and therefore scriptfile==NULL */
 Bool const script_is_FIFO=(scriptfile==stdin);
 growing_buf_clear(&static_linebuf);
 while (1) {
  c=fgetc(scriptfile);
  /* Allow MSDOS files by dropping \r characters: */
  if (c=='\r') continue;
  if (c==0x04) c=EOF; /* Interpret ^D as EOF */
  if (c==EOF) {
   /* If there was no EOL at EOF, no separator would be present
    * at the end of the script. So we add one in this case. */
   if (static_linebuf.current_length>0 && static_linebuf.buffer_start[static_linebuf.current_length-1]!='\n') growing_buf_appendchar(&static_linebuf, '\n');
  } else {
   growing_buf_appendchar(&static_linebuf, c);
  }
  if (c=='\n' || c==EOF) {
   gsize thisutflength;
   gchar * const thisutfstring=g_locale_to_utf8(static_linebuf.buffer_start,static_linebuf.current_length,NULL,&thisutflength,NULL);
   gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, thisutfstring, thisutflength);
   free_pointer((void **)&thisutfstring);
   growing_buf_appendchar(&static_linebuf, '\0');
   if (!interactive && (c==EOF || IS_SEPLINE(static_linebuf.buffer_start))) {
    /* If reading from stdin, we must execute the sub-script just read now;
     * otherwise, all sub-scripts are read and executed in one go by Run_Script */
    subscript_loaded++;
    if ((only_script==0 && script_is_FIFO && !dumponly) || only_script==subscript_loaded) break;
   }
   if (c==EOF) break;
   growing_buf_clear(&static_linebuf);
  }
 }
 if (c==EOF || only_script==subscript_loaded) {
  Close_script_file();
 }
 if (!interactive) {
  /* In dumponly mode, we need to dump all sub-scripts at once, only_script=0 */
  save_only_script=only_script;
  if (only_script==0 && script_is_FIFO && !dumponly) {
   only_script=subscript_loaded;
  }
  Run_Script();
  only_script=save_only_script;
 }
 return FALSE;
}
LOCAL Bool
Load_Script(gchar *name) {
 if (strcmp(name, "stdin")==0) {
  scriptfile=stdin;
 } else {
  scriptfile=fopen(name,"r");
 }

 /* We want to always set the current filename, no matter whether the
  * load operation was successful or not.
  * Note that name==filename when initiated by open_file. */
 if (name!=filename) strncpy(filename, name, FilenameLength);
 set_main_window_title();

 if (scriptfile!=NULL) {
  subscript_loaded=0;
  clear_script();
  Avg_q_Load_Subscript_Tag=g_idle_add(Load_Next_Subscript, NULL);
  snprintf(errormessage, ERRORMESSAGE_SIZE, "Loading script from file %s.", name);
 } else {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "open_file: %s: %s", name, strerror(errno));
  interactive=TRUE;
 }
 set_status(errormessage);
 return (scriptfile!=NULL);
}
LOCAL void
open_file_finished(GObject *source, GAsyncResult *result, void *gdata) {
 GFile *gfile;
 GError *error = NULL;
 gfile = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);
 if (gfile!=NULL) {
  char *gfilename=g_file_get_path(gfile);
  strncpy(filename, gfilename, FilenameLength);
  Avg_q_Load_Script_Now_Tag=g_idle_add(Load_Script_Now, filename);
  g_free (gfilename);
  g_object_unref(gfile);
 }
}
LOCAL void
open_file(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 GFile *gfile;
 GtkFileDialog *filedialog = gtk_file_dialog_new();
 GtkFileFilter *filter=gtk_file_filter_new();
 GtkFileFilter *allfiles=gtk_file_filter_new();
 gtk_file_filter_add_pattern(filter,"*.script"); gtk_file_filter_set_name(filter,"*.script");
 gtk_file_filter_add_pattern(allfiles,"*"); gtk_file_filter_set_name(allfiles,"All files");
 GListStore* liststore = g_list_store_new (GTK_TYPE_FILE_FILTER);
 g_list_store_append(liststore, filter);
 g_list_store_append(liststore, allfiles);
 gtk_file_dialog_set_filters(filedialog,G_LIST_MODEL(liststore));
 gtk_file_dialog_set_title(filedialog, "Select a script file to load");
 if (g_path_is_absolute(filename)) {
  gfile=g_file_new_for_path(filename);
 } else {
  char * const path=g_get_current_dir();
  gfile=g_file_new_build_filename(path,filename,NULL);
  g_free(path);
 }
 gtk_file_dialog_set_initial_file(filedialog, gfile);
 gtk_file_dialog_open(filedialog,GTK_WINDOW(Avg_Q_Main_Window),NULL,&open_file_finished,NULL);
 g_object_unref(gfile);
}
LOCAL int
open_file_cb(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 open_file(NULL,NULL,NULL);
 return 1;
}
/*}}}*/
/*{{{ Saving script files*/
LOCAL void
save_file(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 FILE *outfile = fopen(filename, "w");
 if (outfile!=NULL) {
  gunichar lastchar=0;
  gtk_text_buffer_get_start_iter(CurrentScriptBuffer, &current_iter);
  while (1) {
   gchar thisgchar[6];
   gsize thisgcharlength;
   gunichar const thischar= gtk_text_iter_get_char(&current_iter);
   if (thischar==0) {
    break;
   }
   thisgcharlength=g_unichar_to_utf8(thischar,(gchar *)&thisgchar);
   fwrite((void *)&thisgchar, thisgcharlength, 1, outfile);
   lastchar=thischar;
   gtk_text_iter_forward_char(&current_iter);
  }
  /* Ensure that the last line is terminated... */
  if (lastchar!='\n') fwrite("\n", sizeof(char), 1, outfile);
  fclose(outfile);
  snprintf(errormessage, ERRORMESSAGE_SIZE, "Script saved to file %s.", filename);
 } else {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "save_file: %s: %s", filename, strerror(errno));
 }
 set_status(errormessage);
}
LOCAL int
save_file_cb(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 save_file(NULL,NULL,NULL);
 return 1;
}
LOCAL void
save_file_as_finished(GObject *source, GAsyncResult *result, void *gdata) {
 GFile *gfile;
 GError *error = NULL;
 gfile = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, &error);
 if (gfile!=NULL) {
  char *gfilename=g_file_get_path(gfile);
  strncpy(filename, gfilename, FilenameLength);
  g_free (gfilename);
  set_main_window_title();
  save_file(NULL,NULL,NULL);
  g_object_unref(gfile);
 }
}
LOCAL void
save_file_as(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 GFile *gfile;
 GtkFileDialog *filedialog = gtk_file_dialog_new();
 GtkFileFilter *filter=gtk_file_filter_new();
 GtkFileFilter *allfiles=gtk_file_filter_new();
 gtk_file_filter_add_pattern(filter,"*.script"); gtk_file_filter_set_name(filter,"*.script");
 gtk_file_filter_add_pattern(allfiles,"*"); gtk_file_filter_set_name(allfiles,"All files");
 GListStore* liststore = g_list_store_new (GTK_TYPE_FILE_FILTER);
 g_list_store_append(liststore, filter);
 g_list_store_append(liststore, allfiles);
 gtk_file_dialog_set_filters(filedialog,G_LIST_MODEL(liststore));
 gtk_file_dialog_set_title(filedialog, "Select a script file name to save to");
 if (g_path_is_absolute(filename)) {
  gfile=g_file_new_for_path(filename);
 } else {
  char * const path=g_get_current_dir();
  gfile=g_file_new_build_filename(path,filename,NULL);
  g_free(path);
 }
 gtk_file_dialog_set_initial_file(filedialog, gfile);
 gtk_file_dialog_save(filedialog,GTK_WINDOW(Avg_Q_Main_Window),NULL,&save_file_as_finished,NULL);
 g_object_unref(gfile);
}
/*}}}*/
/*}}}  */
#else
/*{{{ Now follows the STANDALONE implementation of run_script */
LOCAL void 
Run_Script(void) {
 /* The `succeeded' variable should not be stack-based because it might
  * be clobbered by longjmp */
 succeeded=FALSE;
 dump_script_pointer=dump_script_pointers;
 if (running) {
  set_runstatus("Script is already running.");
 } else {
 set_running(TRUE);

 switch (setjmp(external_method.error_jmp)) {
  case 0:
   if (dumponly) {
    if (nr_of_script_variables!=0) {
     TRACEMS(tinfostruc.emethods, 0, "Script variables are ignored when dumping!\n");
    }
   }
   while (*dump_script_pointer!=NULL) {
    succeeded=FALSE;	/* Only succeed if all sub-scripts succeed */
    /* Possibly execute multiple sub-scripts */
#define iter_queue (*dump_script_pointer[0])
#define post_queue (*dump_script_pointer[1])
    if (only_script>0 && iter_queue.start[0].script_number!=only_script) {
     dump_script_pointer+=2;
     continue;
    }
    if (iter_queue.start->transform==NULL) {
     /* The transform call of the first method in the queue is NULL:
      * This means that init_queue_from_dump still needs to be executed
      * (the select call will fill in the missing information) */
     init_queue_from_dump(&tinfostruc, &iter_queue);
     init_queue_from_dump(&tinfostruc, &post_queue);
    }
     if (dumponly) {
      print_script(&iter_queue,&post_queue,dumpfile);
      if (dump_script_pointer[2]!=NULL) {
       fprintf(dumpfile, "-\n");
      }
     } else {
      int const variables_requested1=set_queuevariables(&tinfostruc, &iter_queue, nr_of_script_variables, script_variables);
      int const variables_requested2=set_queuevariables(&tinfostruc, &post_queue, nr_of_script_variables, script_variables);
      int const max_var_requested=(variables_requested1>variables_requested2 ? variables_requested1 : variables_requested2);
      if (max_var_requested>nr_of_script_variables) {
       ERREXIT2(tinfostruc.emethods, "Arguments up to $%d were requested by the script, but only %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
      }
      if (max_var_requested<nr_of_script_variables) {
       TRACEMS2(tinfostruc.emethods, 0, "%d variables were requested by the script, %d were given!\n", MSGPARM(max_var_requested), MSGPARM(nr_of_script_variables));
      }
# ifdef FP_EXCEPTION
     fp_exception_init();	/* This sets up the math exception signal handler */
# endif
     do_queues(&tinfostruc, &iter_queue, &post_queue);
     if (tinfostruc.tsdata!=NULL) {
      free_tinfo(&tinfostruc);
     }
     }
    succeeded=TRUE;
    dump_script_pointer+=2;
    if (only_script>0) {
     break;
    }
   }
   if (dumponly) {
    dumponly=FALSE;
    if (dumpfile!=stdout) {
     fclose(dumpfile);
     dumpfile=stdout;
    }
    set_runstatus("Script was dumped.");
   } else {
    set_runstatus("Script was executed.");
   }
   if (!interactive) {
    g_idle_add((GSourceFunc)g_main_loop_quit, mainloop);
   }
   break;
  case ERROR_STATUS:
   /* Catch errors in exiting the queue (should be none!) */
   switch (setjmp(external_method.error_jmp)) {
    case 0:
     free_tinfo(&tinfostruc);
     if (iter_queue.start!=NULL) exit_queue(&tinfostruc, &iter_queue);
     if (post_queue.start!=NULL) exit_queue(&tinfostruc, &post_queue);
     break;
    case ERROR_STATUS:
     break;
   }
   set_runstatus(errormessage);
   gtk_window_unminimize(GTK_WINDOW(Avg_Q_Main_Window));
   save_only_script=only_script=0;
   interactive=TRUE;
   break;
 }
 set_running(FALSE);
 }
 return;
#undef iter_queue
#undef post_queue
}
/*}}}  */
#endif

LOCAL void
Stop_Script(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 if (running) {
  stop_request=TRUE;
  set_status("Script will be stopped...");
 } else {
  set_status("Nothing to stop.");
 }
}
/* Callback signature */
LOCAL int
Stop_Script_cb(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 Stop_Script(NULL,NULL,NULL);
 return 1;
}
LOCAL void
Cancel_Script(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 if (running) {
  set_status("Script will be cancelled...");
 } else {
  set_status("Nothing to cancel.");
 }
}
LOCAL void
Quit_avg_q(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 /* This is called when the main window is destroyed or from the file
  * menu "Quit" entry - meaning we really want to stop the program... */
 if (running) {
  /* Sadly, Stop_Script isn't sufficient to kill the script */
  Cancel_Script(NULL,NULL,NULL);
 }
 g_idle_add((GSourceFunc)g_main_loop_quit, mainloop);
}
/* Callback signature */
LOCAL int
Quit_avg_q_cb(GtkWidget *menuitem, GVariant *gv, gpointer data) {
 Quit_avg_q(NULL,NULL,NULL);
 return 1;
}

LOCAL void
dump_file_as_finished(GObject *source, GAsyncResult *result, void *gdata) {
 GFile *gfile;
 GError *error = NULL;
 gfile = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, &error);
 if (gfile!=NULL) {
  char *gfilename=g_file_get_path(gfile);
  if ((dumpfile=fopen(gfilename, "w"))!=NULL) {
   dumponly=TRUE;
   set_status("Dumping script...");
   Run_Script();
  }
  g_free (gfilename);
  g_object_unref(gfile);
 }
}
LOCAL void
dump_file_as(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 GFile *gfile;
 GtkFileDialog *filedialog = gtk_file_dialog_new();
 GtkFileFilter *filter=gtk_file_filter_new();
 GtkFileFilter *allfiles=gtk_file_filter_new();
#ifndef STANDALONE
 gtk_file_filter_add_pattern(filter,"*.h"); gtk_file_filter_set_name(filter,"*.h");
#else
 gtk_file_filter_add_pattern(filter,"*.script"); gtk_file_filter_set_name(filter,"*.script");
#endif
 gtk_file_filter_add_pattern(allfiles,"*"); gtk_file_filter_set_name(allfiles,"All files");
 GListStore* liststore = g_list_store_new (GTK_TYPE_FILE_FILTER);
 g_list_store_append(liststore, filter);
 g_list_store_append(liststore, allfiles);
 gtk_file_dialog_set_filters(filedialog,G_LIST_MODEL(liststore));

#ifndef STANDALONE
 const gchar *newfilename;
 const gchar *extension;
 /* Add extension ".h" */
 extension=g_strrstr(filename,".");
 if (extension) {
  gchar * filename2=g_strndup(filename,extension-filename);
  newfilename=g_strconcat(filename2,".h",NULL);
  g_free(filename2);
 } else {
  newfilename=g_strconcat(filename,".h",NULL);
 }
 if (g_path_is_absolute(newfilename)) {
  gfile=g_file_new_for_path(newfilename);
 } else {
  char * const path=g_get_current_dir();
  gfile=g_file_new_build_filename(path,newfilename,NULL);
  g_free(path);
 }
 g_free((gpointer)newfilename);
#else
 GFile *gfile2=g_file_new_for_path(DUMPFILENAME);
 char * const path=g_get_current_dir();
 char * const basename=g_file_get_basename(gfile2);
 gfile=g_file_new_build_filename(path,basename,NULL);;
 g_free(path);
 g_object_unref(gfile2);
#endif
 gtk_file_dialog_set_initial_file(filedialog, gfile);
 g_object_unref(gfile);
 gtk_file_dialog_set_title(filedialog, "Select an include file name to dump to");
 gtk_file_dialog_save(filedialog,GTK_WINDOW(Avg_Q_Main_Window),NULL,&dump_file_as_finished,NULL);
}
LOCAL void
avg_q_about(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
#define BUFFER_SIZE 1024
 char buffer[BUFFER_SIZE];
 snprintf(buffer, BUFFER_SIZE, "%s\nThis user interface was built with the free GTK library (v%d.%d): See http://www.gtk.org/\n", get_avg_q_signature(), GTK_MAJOR_VERSION, GTK_MINOR_VERSION);
 Notice(buffer);
#undef BUFFER_SIZE
}

LOCAL void
set_tracelevel(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 gchar const *const name=g_action_get_name(G_ACTION(menuitem));
 external_method.emethod.trace_level=(int)(name[strlen(name)-1]-'0');
 printf("Tracelevel set to %d\n", external_method.emethod.trace_level);
}

#ifndef STANDALONE
LOCAL void
method_menu(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 gchar const *const name=g_action_get_name(G_ACTION(menuitem));
 gchar const *const methodname=name+7;
 void (* const * const method_selects)(transform_info_ptr)=get_method_list();
 void (* const *m_select)(transform_info_ptr);
 for (m_select=method_selects; *m_select!=NULL; m_select++) {
  (**m_select)(&tinfostruc);
  if (strcmp(tinfostruc.methods->method_name,methodname)==0) {
   break;
  }
 }
 MethodInstance_init_methodstruc();
 switch (setjmp(instance_emethod.error_jmp)) {
  case 0: {
   if (MethodInstance_window!=NULL) {
    method_dialog_close(GTK_WIDGET(MethodInstance_window));
   }
   MethodInstance_setup_from_method(*m_select);
   variables_only=FALSE;
   old_method=FALSE;
   MethodInstance_build_dialog();
   }
   break;
  case ERROR_STATUS:
   set_status(errormessage);
   gtk_window_unminimize(GTK_WINDOW(Avg_Q_Main_Window));
   break;
 }
}
#endif

LOCAL void
create_main_window (void) {
 GtkWidget *box1, *hbox;
 GMenu *menubar, *filemenu, *filemenu2, *tracemenu;
 GtkWidget *scrolledwindow;
 GSimpleActionGroup *action_group=g_simple_action_group_new();
 GtkShortcut *shortcut;
 GtkEventController *ec;

 Avg_Q_Main_Window = gtk_window_new();
#if 0
 gtk_widget_modify_font(GTK_WIDGET(Avg_Q_Main_Window),pango_font_description_from_string(DEFAULT_FONT));
#endif
 gtk_window_set_default_size (GTK_WINDOW(Avg_Q_Main_Window), FRAME_SIZE_X, FRAME_SIZE_Y);
 g_signal_connect (G_OBJECT (Avg_Q_Main_Window), "destroy", G_CALLBACK (Quit_avg_q), NULL);

 box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
 gtk_window_set_child(GTK_WINDOW(Avg_Q_Main_Window), box1);

 menubar = g_menu_new();

 /* File menu */
 filemenu=g_menu_new();

 g_menu_append(filemenu,"Change directory","app.change_dir");
#ifndef STANDALONE
 g_menu_append(filemenu,"Open","app.open");
 g_menu_append(filemenu,"Save","app.save");
 g_menu_append(filemenu,"Save as","app.save_as");
#endif
 g_menu_append(filemenu,"Run","app.run");
#ifndef STANDALONE
 g_menu_append(filemenu,"Run sub-script","app.run_subscript");
#endif
 g_menu_append(filemenu,"Stop script","app.stop");
 g_menu_append(filemenu,"Cancel script","app.cancel");
 g_menu_append(filemenu,"Dump script","app.dump");

 /* Separator: */
 filemenu2=g_menu_new();

 g_menu_append(filemenu2,"About","app.about");
 g_menu_append(filemenu2,"Quit","app.quit");
 g_menu_append_section(filemenu,NULL,G_MENU_MODEL(filemenu2));
 g_menu_append_submenu(menubar,"Script",G_MENU_MODEL(filemenu));

 /* Trace menu */
 tracemenu=g_menu_new();

 g_menu_append(tracemenu,"0 (only warnings)","app.tracelevel_0");
 g_menu_append(tracemenu,"1 (most messages)","app.tracelevel_1");
 g_menu_append(tracemenu,"2 (more messages)","app.tracelevel_2");
 g_menu_append(tracemenu,"3 (even more ...)","app.tracelevel_3");
 g_menu_append_submenu(menubar,"Tracelvl",G_MENU_MODEL(tracemenu));

 const GActionEntry entries[] = {
  {"change_dir", change_dir },
  {"run", Run_Script_Command },
  {"stop", Stop_Script },
  {"cancel", Cancel_Script },
  {"dump", dump_file_as },
  {"about", avg_q_about },
  {"quit",  Quit_avg_q  },
  {"tracelevel_0", set_tracelevel},
  {"tracelevel_1", set_tracelevel},
  {"tracelevel_2", set_tracelevel},
  {"tracelevel_3", set_tracelevel},
 };
 g_action_map_add_action_entries(G_ACTION_MAP(action_group), entries, G_N_ELEMENTS(entries), NULL);

#ifndef STANDALONE
 const GActionEntry entries2[] = {
  {"open", open_file },
  {"save", save_file },
  {"save_as", save_file_as },
  {"run_subscript", Run_CursorSubScript },
 };
 g_action_map_add_action_entries(G_ACTION_MAP(action_group), entries2, G_N_ELEMENTS(entries), NULL);
 Run_Subscript_Action=g_action_map_lookup_action(G_ACTION_MAP(action_group),"run_subscript");

 {
 void (* const * const method_selects)(transform_info_ptr)=get_method_list();
 void (* const *m_select)(transform_info_ptr);

 int method_type_id;
 for (method_type_id=0; method_type_id<NR_OF_METHOD_TYPES; method_type_id++) {
  enum method_types const method_type=method_types[method_type_id];
  GMenu *submenu=g_menu_new();
  GString *submenuname=g_string_new(methodtype_names[method_type]);
  g_string_replace(submenuname,"_","__",0);

  for (m_select=method_selects; *m_select!=NULL; m_select++) {
   (**m_select)(&tinfostruc);
   if (tinfostruc.methods->method_type==method_type) {
    gchar *actionname=g_strjoin("","app.method_",tinfostruc.methods->method_name,NULL);
    GString *methodname=g_string_new(tinfostruc.methods->method_name);
    const GActionEntry tmpentries[] = {{actionname+4, method_menu}};
    g_action_map_add_action_entries(G_ACTION_MAP(action_group), tmpentries, 1, NULL);
    g_string_replace(methodname,"_","__",0);
    g_menu_append(submenu,methodname->str,actionname);
    g_free(actionname);
    g_free(methodname);
   }
  }

  g_menu_append_submenu(menubar,submenuname->str,G_MENU_MODEL(submenu));
 }
 }
#endif

 /* Looking up the remaining actions must be done here, after the action group is complete */
 Run_Action=g_action_map_lookup_action(G_ACTION_MAP(action_group),"run");
 Stop_Action=g_action_map_lookup_action(G_ACTION_MAP(action_group),"stop");
 Kill_Action=g_action_map_lookup_action(G_ACTION_MAP(action_group),"cancel");
 Dump_Action=g_action_map_lookup_action(G_ACTION_MAP(action_group),"dump");

 GtkWidget *popover=gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
 gtk_box_append(GTK_BOX(box1), popover);

 scrolledwindow = gtk_scrolled_window_new();
 gtk_widget_set_hexpand(GTK_WIDGET(scrolledwindow),TRUE);
 gtk_widget_set_vexpand(GTK_WIDGET(scrolledwindow),TRUE);
 gtk_box_append(GTK_BOX(box1), scrolledwindow);

 Avg_Q_Scriptpanel=gtk_text_view_new();
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow),GTK_WIDGET(Avg_Q_Scriptpanel));

 hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
 Avg_q_StatusBar = gtk_label_new(NULL);
 gtk_widget_set_hexpand(GTK_WIDGET(Avg_q_StatusBar),TRUE);
 gtk_box_append(GTK_BOX(hbox), Avg_q_StatusBar);
 set_status("Welcome to avg_q!");

 Avg_q_StatusRunBar = gtk_label_new(NULL);
 gtk_box_append(GTK_BOX(hbox), Avg_q_StatusRunBar);
 gtk_widget_set_hexpand(GTK_WIDGET(Avg_q_StatusRunBar),TRUE);
 gtk_box_append(GTK_BOX(box1), hbox);
 set_runstatus("Script processor ready.");

 gtk_widget_insert_action_group(GTK_WIDGET(Avg_Q_Main_Window), "app", G_ACTION_GROUP(action_group));
 //gtk_window_add_accel_group (GTK_WINDOW(Avg_Q_Main_Window), accel_group);

 ec=gtk_shortcut_controller_new();
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("<Control>d"),
  gtk_callback_action_new(&change_dir_cb,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("<Control>q"),
  gtk_callback_action_new(&Quit_avg_q_cb,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("<Control>r"),
  gtk_callback_action_new(&Run_Script_Command_cb,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("<Control>k"),
  gtk_callback_action_new(&Stop_Script_cb,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
#ifndef STANDALONE
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("<Control>o"),
  gtk_callback_action_new(&open_file_cb,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("<Control>s"),
  gtk_callback_action_new(&save_file_cb,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
#endif
 gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(ec),GTK_SHORTCUT_SCOPE_GLOBAL);
 gtk_widget_add_controller(GTK_WIDGET(Avg_Q_Main_Window),ec);

#ifndef STANDALONE
 ec=gtk_shortcut_controller_new();
 shortcut=gtk_shortcut_new(
  gtk_shortcut_trigger_parse_string("Tab"),
  gtk_callback_action_new(&setup_from_line_requested,NULL,NULL));
 gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ec),shortcut);
 gtk_widget_add_controller(GTK_WIDGET(Avg_Q_Scriptpanel),ec);
#endif

 set_running(FALSE);
}

/* passing stream=NULL results in output to trace messages */
LOCAL void
usage(FILE *stream) {
#define BUFFER_SIZE 2048
 char buffer[BUFFER_SIZE];
 growing_buf buf, tokenbuf;
 Bool have_token;

#ifndef STANDALONE
 snprintf(buffer, BUFFER_SIZE, "Usage: %s [options] scriptfile [script_argument1 ...]\n"
  "Multichannel (EEG/MEG) data processing GUI. Reads a script file to\n"
  "setup an iterated and a postprocessing queue. The iterated queue between\n"
  "get_epoch method and collect method is executed for all epochs delivered\n"
  "by the get_epoch method. If the collect method outputs a result, this\n"
  "result is fed to the postprocessing queue.\n"
#else
 snprintf(buffer, BUFFER_SIZE, "Usage: %s [options] [script_argument1 ...]\n"
  "Standalone avg_q version with script compiled-in.\n"
  "The file name of the script was: %s\n"
#endif
  "\nSignature:\n%s"
  "\nOptions are:\n"
  "\t-i: Iconify. Start with the main window iconified.\n"
  "\t-I: Interactive. Don't automatically run the script.\n"
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

 growing_buf_settothis(&buf, buffer);
 buf.delimiters="\n";
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);
 have_token=growing_buf_get_firstsingletoken(&buf,&tokenbuf);
 while (have_token) {
  if (stream==NULL) {
   TRACEMS(tinfostruc.emethods, -1, tokenbuf.buffer_start);
   TRACEMS(tinfostruc.emethods, -1, "\n");
  } else {
   fprintf(stream,"%s\n",tokenbuf.buffer_start);
  }
  have_token=growing_buf_get_nextsingletoken(&buf,&tokenbuf);
 }
 growing_buf_free(&tokenbuf);
#undef BUFFER_SIZE
}
#ifdef STANDALONE
LOCAL gint
Run_Script_Now(gpointer data) {
 Run_Script_Command(NULL,NULL,NULL);
 return FALSE;
}
#else
LOCAL gint
Load_Script_Now(gpointer data) {
 Load_Script(data);
 return FALSE;
}
#endif
/*}}}  */

#ifdef __MINGW32__
LOCAL int 
setenv(const char *name, const char *value, int overwrite) {
 char *envbuffer=malloc((strlen(name)+strlen(value)+2)*sizeof(char));
 if (envbuffer==NULL) {
  fprintf(stderr, "avg_q_ui: Error allocating envbuffer!\n");
  exit(1);
 }
 sprintf(envbuffer, "%s=%s", name, value);
 putenv(envbuffer);
 /* Note that the buffer cannot be free()d while the
  * program runs, otherwise `unexpected results' occur... 
  * This is documented behavior: envbuffer becomes part of the environment. */ 
 return 0;
}
#endif

/*{{{ main()*/
int
main (int argc, char *argv[]) {
 int errflag=0, c;
 char *vdevice;
 char const * const validate_msg=validate(argv[0]);
#ifndef STANDALONE
 void (* const * const method_selects)(transform_info_ptr)=get_method_list();
#endif

 if (validate_msg!=NULL) {
  fputs(get_avg_q_signature(), stdout);
  printf("Validation failed: %s.\n", validate_msg);
  exit(3);
 }

 /* Here we have the problem that we might have set VDEVICE for non-GUI versions
  * of avg_q and do_posplot; We usually want to set it to "VGUI" so that posplot
  * uses the vogl_vgui_driver integrated with avg_q_ui but there should be a way
  * to specify otherwise as well; So, we poll a new environment variable 
  * "VGUIDEVICE" to enable us to use (eg) native X11 if we want to */
 if ((vdevice=getenv("VGUIDEVICE"))==NULL) {
  vdevice="VGUI";
 }

 setenv("VDEVICE",vdevice,TRUE);

 gtk_init();

#ifdef HAVE_LIBGLE
 gle_init (&argc, &argv);
#endif /* !HAVE_LIBGLE */

 /* These things should be done before the first calls to the BF library
  * (including method selections): */
 set_external_methods(&external_method.emethod, &error_exit, &trace_message, &static_execution_callback);
 external_method.emethod.trace_level=0;
 tinfostruc.methods= &mymethod;
 tinfostruc.emethods= &external_method.emethod;

 create_main_window();

 /* Now we come to processing the options we were given: */
 mainargc=argc; mainargv=argv;
 opterr=0;	/* We don't want getopt() to print error messages to stderr */
 /*{{{  Process command line*/
#ifndef STANDALONE
#define GETOPT_STRING "t:lHh:DiIs:"
#else
#define GETOPT_STRING "t:DiIs:"
#endif
 const struct option longopts[]={{"help",no_argument,NULL,'?'},{"version",no_argument,NULL,'V'},{NULL,0,NULL,0}};
 while ((c=getopt_long(argc, argv, GETOPT_STRING, longopts, NULL))!=EOF) {
  switch (c) {
   case 't':
    external_method.emethod.trace_level=atoi(optarg);
    break;
#ifndef STANDALONE
   case 'l':
    TRACEMS(tinfostruc.emethods, -1, "Available methods are:\n");
    printhelp(&tinfostruc, method_selects, "SHORT");
    break;
   case 'H':
    printhelp(&tinfostruc, method_selects, NULL);
    break;
   case 'h':
    printhelp(&tinfostruc, method_selects, optarg);
    break;
#endif
   case 'D':
    dumponly=TRUE;
    dumpfile=stdout;
    break;
   case 'i':
    iconified=TRUE;
    break;
   case 'I':
    interactive=TRUE;
    break;
   case 's':
    save_only_script=only_script=atoi(optarg);
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
 gtk_window_present(GTK_WINDOW(Avg_Q_Main_Window));
 /* This allows things like mimimization to work */
 while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);

 if (errflag>0) {
  usage(NULL);
  interactive=TRUE;
  dumponly=FALSE;
 } else if (mainargc-optind>=END_OF_ARGS) {
  nr_of_script_variables=mainargc-optind-END_OF_ARGS;
  if (nr_of_script_variables>0) {
   script_variables=mainargv+optind+END_OF_ARGS;
  }
#ifndef STANDALONE
 } else {
  interactive=TRUE;
  dumponly=FALSE;
  save_only_script=only_script=0;
#endif
 }
 /*}}}  */

 iter_queue.start=post_queue.start=NULL;	/* Tell setup_queue that no memory is allocated yet */
#ifdef STANDALONE
 strncpy(filename, DUMP_SCRIPT_NAME, FilenameLength);
 set_main_window_title();
 snprintf(errormessage, ERRORMESSAGE_SIZE,
  "This standalone version of avg_q is\n"
  "provided with a built-in script.\n\n"
  "The file name of the script was:\n%s\n", DUMP_SCRIPT_NAME);
 gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, errormessage, strlen(errormessage));
 gtk_text_view_set_editable(GTK_TEXT_VIEW(Avg_Q_Scriptpanel), FALSE);
#else
 gtk_text_view_set_editable(GTK_TEXT_VIEW(Avg_Q_Scriptpanel), TRUE);
#endif

 if (external_method.emethod.trace_level>0 && external_method.emethod.trace_level<4) {
  gtk_widget_activate(TraceMenuItems[external_method.emethod.trace_level]);
 }

#ifdef STANDALONE
 if (!interactive) {
  Avg_q_Run_Script_Now_Tag=g_idle_add(Run_Script_Now, NULL);
 }
#else
 /* Initialize the static line buffer: */
 growing_buf_init(&static_linebuf);
 growing_buf_allocate(&static_linebuf, 0);
 if (mainargc-optind>=END_OF_ARGS) {
  strncpy(filename, MAINARG(SCRIPTFILE), FilenameLength);
  set_main_window_title();
  Avg_q_Load_Script_Now_Tag=g_idle_add(Load_Script_Now, MAINARG(SCRIPTFILE));
 } else {
  set_main_window_title();
 }
 if (iconified & !interactive) {
  gtk_window_minimize(GTK_WINDOW(Avg_Q_Main_Window));
 }
#endif

 mainloop=g_main_loop_new(NULL, FALSE);
 g_main_loop_run(mainloop);
 g_main_loop_unref(mainloop);

 free_queue_storage(&iter_queue);
 free_queue_storage(&post_queue);
#ifndef STANDALONE
 growing_buf_free(&static_linebuf);
#endif

 return 0;
}
/*}}}*/
