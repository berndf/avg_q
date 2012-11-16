/*
 * Copyright (C) 1996-2012 Bernd Feige
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
/*
 * File:	avg_q_ui.c
 * Purpose:	GTK+ user interface for avg_q
 * Author:	Bernd Feige
 * Created:	7.03.1999
 * Updated:
 * Copyright:	(c) 1999, Bernd Feige
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

#include "transform.h"
#include "bf.h"

#ifdef USE_THREADING
LOCAL GThread *script_tid;
#else
#define gdk_threads_enter()
#define gdk_threads_leave()
#endif

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
CONFIGFILE=0, END_OF_ARGS
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

/* Need some compatibility definitions */
#if GTK_MAJOR_VERSION==2 && GTK_MINOR_VERSION<24
#define GTK_COMBO_BOX_TEXT(x) GTK_COMBO_BOX(x)
#define gtk_combo_box_text_new() gtk_combo_box_new_text()
#define gtk_combo_box_text_append_text(x,y) gtk_combo_box_append_text(x,y)
#define gtk_combo_box_text_get_active_text(x) gtk_combo_box_get_active_text(x)
#endif
#if GTK_MAJOR_VERSION==2
#define gtk_box_new_GTK_ORIENTATION_VERTICAL(spacing) gtk_vbox_new(FALSE,spacing)
#define gtk_box_new_GTK_ORIENTATION_HORIZONTAL(spacing) gtk_hbox_new(FALSE,spacing)
#define gtk_box_new(orientation,spacing) gtk_box_new_##orientation(spacing)
#endif
/*}}}*/

/*{{{ Declarations*/
LOCAL struct transform_info_struct tinfostruc;
LOCAL my_emethod external_method;
LOCAL struct transform_methods_struct mymethod;

LOCAL GtkWidget *Avg_Q_Main_Window;
LOCAL GtkWidget *CurrentScriptPanel, *TracePanel=NULL;
#define CurrentScriptBuffer (gtk_text_view_get_buffer(GTK_TEXT_VIEW(CurrentScriptPanel)))
LOCAL GtkWidget *TraceMenuItems[4];
LOCAL GtkWidget *Avg_q_StatusBar;
LOCAL gint Avg_q_StatusContext, Avg_q_StatusId=0;
LOCAL GtkWidget *Avg_q_StatusRunBar;
LOCAL gint Avg_q_StatusRunContext, Avg_q_StatusRunId=0;
#ifdef STANDALONE
LOCAL gint Avg_q_Run_Script_Now_Tag=0;
#else
LOCAL gint Avg_q_Load_Script_Now_Tag=0;
LOCAL gint Avg_q_Load_Subscript_Tag=0;
#endif
G_LOCK_DEFINE_STATIC (running);
LOCAL Bool running=FALSE, interactive=FALSE, dumponly=FALSE, single_step=FALSE;
G_LOCK_DEFINE_STATIC (stop_request);
LOCAL Bool stop_request=FALSE;
LOCAL FILE *dumpfile;
#define FilenameLength 300
LOCAL char filename[FilenameLength]="avg_q_ui.script";
LOCAL GtkWidget *Run_Entry, *Stop_Entry, *Kill_Entry, *Dump_Entry;
#ifndef STANDALONE
LOCAL GtkWidget *Run_Subscript_Entry;
LOCAL GtkWidget *Dialog_Configuration_Error;
LOCAL gint Avg_q_Run_Keypress_Now_Tag=0;
LOCAL GIOChannel *script_file=NULL;
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
static Bool key_press_for_mainwindow(GtkWidget *widget, GdkEventKey *event);
/*}}}*/

/*{{{  Local functions*/
/*{{{ Setting title and status*/
LOCAL void
set_main_window_title(void) {
 static gchar buffer[FilenameLength+30];
 snprintf(buffer, FilenameLength+30, "avg_q user interface: Script %s", filename);
 gtk_window_set_title (GTK_WINDOW (Avg_Q_Main_Window), buffer);
}
LOCAL void
set_status(gchar *message) {
 gsize thisutflength;
 gchar * const thisutfstring=g_locale_to_utf8(message,-1,NULL,&thisutflength,NULL);
 if (Avg_q_StatusId!=0) {
  gtk_statusbar_remove(GTK_STATUSBAR(Avg_q_StatusBar), Avg_q_StatusContext, Avg_q_StatusId);
 }
 Avg_q_StatusId=gtk_statusbar_push( GTK_STATUSBAR(Avg_q_StatusBar), Avg_q_StatusContext, thisutfstring);
}
LOCAL void
set_runstatus(gchar *message) {
 gsize thisutflength;
 gchar * const thisutfstring=g_locale_to_utf8(message,-1,NULL,&thisutflength,NULL);
 if (Avg_q_StatusRunId!=0) {
  gtk_statusbar_remove(GTK_STATUSBAR(Avg_q_StatusRunBar), Avg_q_StatusRunContext, Avg_q_StatusRunId);
 }
 Avg_q_StatusRunId=gtk_statusbar_push(GTK_STATUSBAR(Avg_q_StatusRunBar), Avg_q_StatusRunContext, thisutfstring);
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
 gtk_text_iter_forward_line(&nextline_iter);
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
 gtk_widget_destroy(GTK_WIDGET(window));
 Notice_window=NULL;
}
LOCAL void
Notice_window_close_button(GtkButton *button, GtkWidget *window) {
 Notice_window_close(window);
}
LOCAL void
Notice(gchar *message) {
 GtkWidget *box1;
 GtkWidget *label;
 GtkWidget *button;

 if (Notice_window!=NULL) {
  gtk_widget_destroy (Notice_window);
  Notice_window=NULL;
 }

 Notice_window=gtk_dialog_new();
 gtk_window_set_position (GTK_WINDOW (Notice_window), GTK_WIN_POS_MOUSE);
 gtk_window_set_wmclass (GTK_WINDOW (Notice_window), "Notice", "avg_q");
 g_signal_connect (G_OBJECT (Notice_window), "destroy", G_CALLBACK (Notice_window_close), NULL);
 gtk_window_set_title (GTK_WINDOW (Notice_window), "Notice");

 box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
 gtk_box_pack_start (GTK_BOX (gtk_dialog_get_action_area(GTK_DIALOG(Notice_window))), box1, FALSE, FALSE, 0);
 gtk_widget_show (box1);

 label = gtk_label_new (message);
 gtk_box_pack_start (GTK_BOX(box1), label, FALSE, FALSE, 0);
 gtk_widget_show (label);

 button = gtk_button_new_with_label ("Okay");
 g_signal_connect_object (G_OBJECT (button), "clicked", G_CALLBACK (Notice_window_close_button), G_OBJECT(Notice_window), G_CONNECT_AFTER);
 gtk_widget_set_can_default (button, TRUE);
 gtk_box_pack_start (GTK_BOX(box1), button, FALSE, FALSE, 0);
 gtk_widget_grab_default (button);
 gtk_widget_show (button);

 gtk_widget_show (Notice_window);
}
/*}}}*/

/*{{{ Handle error and trace messages*/
#define ERROR_STATUS 1
#define ERRORMESSAGE_SIZE 1024
LOCAL char errormessage[ERRORMESSAGE_SIZE];
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
  gtk_widget_destroy(GTK_WIDGET(window));
  TracePanel=NULL;
 }
}
LOCAL void
trace_message(const external_methods_ptr emeth, const int lvl, const char *msgtext) {
 int const llvl=(lvl>=0 ? lvl : -lvl-1);
 if (emeth->trace_level>=llvl) {
  gdk_threads_enter();
  if (TracePanel==NULL) {
   GtkWidget *window;
   GtkWidget *box1;
   GtkWidget *scrolledwindow;

   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);
   gtk_window_set_wmclass (GTK_WINDOW (window), "TracePanel", "avg_q");
   gtk_window_set_title (GTK_WINDOW (window), "Trace messages");
   gtk_window_set_default_size (GTK_WINDOW(window), TRACEFRAME_SIZE_X, TRACEFRAME_SIZE_Y);

   g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (TracePanelDestroy), NULL);
   g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (TracePanelDestroy), NULL);
   /* This disables normal processing of keys for the trace panel, including copying... */
   /*g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (key_press_for_mainwindow), NULL);*/

   box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add (GTK_CONTAINER (window), box1);
   gtk_widget_show (box1);

   scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start (GTK_BOX (box1), scrolledwindow, TRUE, TRUE, 0);
   gtk_widget_show (scrolledwindow);

   TracePanel=gtk_text_view_new();
   gtk_container_add (GTK_CONTAINER (scrolledwindow), TracePanel);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(TracePanel), FALSE);
   gtk_widget_show(TracePanel);

   gtk_widget_show (window);
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
  gdk_threads_leave();
 }
}
LOCAL void
static_execution_callback(const transform_info_ptr tinfo, const execution_callback_place where) {
 if (stop_request) {
  /* Be sure to do ERREXIT only once after `stop_request' has been set
   * to TRUE by the main program; then the queues are exited, and
   * the execution_callback called with E_CALLBACK_BEFORE_EXIT and 
   * E_CALLBACK_AFTER_EXIT for each method, we don't want to hamper this... */
  G_LOCK(stop_request);
  stop_request=FALSE;
  G_UNLOCK(stop_request);
  ERREXIT(tinfostruc.emethods, "Stopped by user.\n");
 }
 if (where==E_CALLBACK_BEFORE_EXEC) {
  gdk_threads_enter();
  if (single_step) {
#if 0
   ItemVal cval;

   TRACEMS3(tinfo->emethods, -1, "QUEUE line %d script %d: %s\n", MSGPARM(tinfo->methods->line_of_script), MSGPARM(tinfo->methods->script_number), MSGPARM(tinfo->methods->method_name));
   script_panel->EditCommand(edLineGoto, tinfo->methods->line_of_script);
   snprintf(errormessage, ERRORMESSAGE_SIZE, "About to execute method %s", tinfo->methods->method_name);
   if (step_dialog->ShowModalDialog(tinfo, errormessage, cval)==M_Cancel) {
    ERREXIT(tinfo->emethods, "Stopped by user.\n");
   }
#endif
  } else {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "Executing script %d line %2d: %s", tinfo->methods->script_number, tinfo->methods->line_of_script, tinfo->methods->method_name);
   set_runstatus(errormessage);
#ifndef USE_THREADING
   while (gtk_events_pending()) gtk_main_iteration();
#endif
  }
  gdk_threads_leave();
 }
}
/*}}}*/

LOCAL void
change_dir(GtkWidget *menuitem) {
 const gchar *uri = NULL;

 /* Note that ACTION_SELECT_FOLDER will create nonexistant folders by default! */
 GtkWidget *filesel=gtk_file_chooser_dialog_new("Select a directory to change to",
  GTK_WINDOW(Avg_Q_Main_Window),
  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
  NULL);
 gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(filesel),TRUE);
 if (g_path_is_absolute(filename)) {
  uri=g_filename_to_uri(filename,NULL,NULL);
 } else {
  char * const path=g_get_current_dir();
  char * const gfilename=g_build_filename(path,filename,NULL);
  uri=g_filename_to_uri(gfilename,NULL,NULL);
  g_free(path);
  g_free(gfilename);
 }
 gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (filesel), uri);

 if (gtk_dialog_run (GTK_DIALOG (filesel)) == GTK_RESPONSE_ACCEPT) {
  gchar *gfilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
  if (chdir(gfilename)==0) {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "Current directory is %s", gfilename);
  } else {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "change_dir: %s: %s", gfilename, strerror(errno));
  }
  set_status(errormessage);
  g_free (gfilename);
 }
 gtk_widget_destroy (filesel);
}
#ifdef USE_THREADING
LOCAL void * run_script_thread(void *thread_args);
LOCAL void
Run_Script(void) {
 script_tid=g_thread_create(run_script_thread, NULL, FALSE, NULL);
}
#if 0
LOCAL void
script_cleanup(void *arg) {
 char * const msg=(char *)arg;
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
 G_LOCK(running);
 running=FALSE;
 G_UNLOCK(running);
 gdk_threads_enter();
 gtk_widget_set_sensitive(Run_Entry, TRUE);
#ifndef STANDALONE
 gtk_widget_set_sensitive(Run_Subscript_Entry, TRUE);
#endif
 gtk_widget_set_sensitive(Dump_Entry, TRUE);
 gtk_widget_set_sensitive(Stop_Entry, FALSE);
 gtk_widget_set_sensitive(Kill_Entry, FALSE);
 set_runstatus(msg);
 gdk_threads_leave();
}
#endif
#else
LOCAL void Run_Script(void);
#endif
LOCAL void
Run_Script_Command(GtkWidget *menuitem) {
 set_status("Running script...");
 save_only_script=only_script=0;
 Run_Script();
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
     argument->arg.s=(char *)argument_descriptor->choices;
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
    argument->is_set=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox));
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
      const gchar * const text=gtk_entry_get_text(GTK_ENTRY(entry));
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
      const gchar * const text=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
      if (text==NULL) {
       /* This should be only a security precaution... */
       TRACEMS(tinfostruc.emethods, 0, "Combo box selection is NULL!\n");
      } else {
       const char * const *choice=argument_descriptor->choices;
       while (*choice!=NULL) {
	if (strcmp(*choice, text)==0) break;
	choice++;
       }
       if (*choice!=NULL) {
	argument->arg.i=(choice-argument_descriptor->choices);
	argument->is_set=TRUE;
       }
      }
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
 growing_buf_firsttoken(linebuf);
 if (!linebuf->have_token) {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "setup_queue: Empty line.");
  set_status(errormessage);
  return FALSE;
 }
 for (m_select=method_selects; *m_select!=NULL; m_select++) {
  (**m_select)(&instance_tinfo);
  if (strcmp(linebuf->current_token, method.method_name)==0) break;
 }
 if (*m_select==NULL) {
  /* The method name was not found - as a convenience, we look for
   * the first method with a name starting like the current name */
  for (m_select=method_selects; *m_select!=NULL; m_select++) {
   (**m_select)(&instance_tinfo);
   if (strncmp(linebuf->current_token, method.method_name, strlen(linebuf->current_token))==0) break;
  }
  if (*m_select==NULL) {
   snprintf(errormessage, ERRORMESSAGE_SIZE, "setup_queue: Unknown method %s", linebuf->current_token);
   set_status(errormessage);
   return FALSE;
  }
 }
 growing_buf_nexttoken(linebuf);
 /* And as another convenience (too convenient? I don't think so), if the line
  * is not accepted by setup_method, configure from defaults: */
 switch (setjmp(instance_emethod.error_jmp)) {
  case 0:
   if (linebuf->have_token) {
    growing_buf myargs;
    growing_buf_init(&myargs);
    /* Protect from a second replacement of delimiters 
     * This would void the backslash-escaping of delimiters
     * from the previous parse */
    myargs.delimiters="";
    myargs.buffer_start=linebuf->current_token;
    myargs.buffer_end=linebuf->buffer_end;
    myargs.current_length=linebuf->current_length-(myargs.buffer_start-linebuf->buffer_start);
    setup_method(&instance_tinfo, &myargs);
   } else {
    setup_method(&instance_tinfo, NULL);
   }
   break;
  case ERROR_STATUS:
   set_status(errormessage);
   /* allocate_methodmem has already been called by setup_method */
   MethodInstance_config_from_defaults();
   gtk_window_deiconify(GTK_WINDOW(Avg_Q_Main_Window));
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
     argument->arg.s=(char *)argument_descriptor->choices;
    }
    break;
   case T_ARGS_TAKES_SELECTION:
    argument->arg.i=(long)argument_descriptor->default_value;
    break;
   default:
    continue;
  }
 }
 return TRUE;
}
LOCAL void
method_dialog_ok(GtkButton *button, GtkWidget *window) {
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

  gtk_widget_destroy(GTK_WIDGET(window));
  window=NULL;
  free_methodmem(&instance_tinfo);
  set_status("Operation accepted.");
 } else {
  gtk_label_set_text(GTK_LABEL(Dialog_Configuration_Error), message);
  gtk_widget_show(Dialog_Configuration_Error);
 }
}
LOCAL void
method_dialog_close(GtkWidget *window) {
 gtk_widget_destroy(GTK_WIDGET(window));
 MethodInstance_window=NULL;
 free_methodmem(&instance_tinfo);
 set_status("Operation canceled.");
}
LOCAL void
method_dialog_close_button(GtkButton *button, GtkWidget *window) {
 method_dialog_close(window);
}
typedef struct {
 GtkWidget **in_dialog_widgets;
 Bool optional;
} method_file_sel_data;
LOCAL void
method_fileselect_all(method_file_sel_data *data) {
 const gchar * const text=gtk_entry_get_text(GTK_ENTRY(*data->in_dialog_widgets));
 const gchar *uri = NULL;

 /* Note that set_current_name only works for ACTION_SAVE or ACTION_CREATE_FOLDER,
  * so we need to use ACTION_SAVE to present any argument... */
 GtkWidget *filesel=gtk_file_chooser_dialog_new("Select a file argument",
  GTK_WINDOW(Avg_Q_Main_Window),
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
  GTK_STOCK_PASTE, GTK_RESPONSE_ACCEPT,
  NULL);
 gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(filesel),TRUE);
 if (text!=NULL) {
  if (strchr(text, '*')!=NULL) {
   GtkFileFilter *filter=gtk_file_filter_new();
   GtkFileFilter *allfiles=gtk_file_filter_new();
   gtk_file_filter_add_pattern(filter,text); gtk_file_filter_set_name(filter,text);
   gtk_file_filter_add_pattern(allfiles,"*"); gtk_file_filter_set_name(allfiles,"All files");
   gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filesel),filter); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filesel),allfiles);
  } else {
   if (g_path_is_absolute(text)) {
    uri=g_filename_to_uri(text,NULL,NULL);
   } else {
    char * const path=g_get_current_dir();
    char * const gfilename=g_build_filename(path,text,NULL);
    uri=g_filename_to_uri(gfilename,NULL,NULL);
    g_free(path);
    g_free(gfilename);
   }
   gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (filesel), uri);
  }
  /* This is necessary to default to a name that does not exist yet... */
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filesel), g_path_get_basename(text));
 }
 if (uri==NULL) {
  /* Set current folder */
  char * const path=g_get_current_dir();
  uri=g_filename_to_uri(path,NULL,NULL);
  gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (filesel), uri);
  g_free(path);
 }
 if (gtk_dialog_run (GTK_DIALOG (filesel)) == GTK_RESPONSE_ACCEPT) {
  char *gfilename;
  gfilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
  gtk_entry_set_text(GTK_ENTRY(*data->in_dialog_widgets), gfilename);
  if (data->optional) {
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->in_dialog_widgets[-1]), TRUE);
  }
  g_free (gfilename);
 }
 gtk_widget_destroy (filesel);
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
LOCAL gint
Run_Keypress_Now(gpointer data) {
 GtkWidget **in_dialog_widgets=(GtkWidget **)data;
 const gchar * const text=gtk_entry_get_text(GTK_ENTRY(*in_dialog_widgets));
 Avg_q_Run_Keypress_Now_Tag=0;

 if (*text=='\0') {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(in_dialog_widgets[-1]), FALSE);
 } else {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(in_dialog_widgets[-1]), TRUE);
 }
 return FALSE;
}
LOCAL Bool
method_normal_entry_keypress_event(GtkWidget *widget, GdkEventKey *event, GtkWidget **in_dialog_widgets) {
 if (event->keyval==GDK_KEY_Return) {
  g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
  g_signal_emit_by_name(G_OBJECT(MethodInstance_Defaultbutton), "clicked", NULL);
  return TRUE;
 }
 return FALSE;
}
LOCAL Bool
method_optional_entry_keypress_event(GtkWidget *widget, GdkEventKey *event, GtkWidget **in_dialog_widgets) {
 if (event->keyval==GDK_KEY_Return) {
  g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
  g_signal_emit_by_name(G_OBJECT(MethodInstance_Defaultbutton), "clicked", NULL);
  return TRUE;
 }
 /* Since we'd like Run_Keypress_Now to be executed AFTER any processing of the
  * key press by the entry editor, we do it the complicated way: */
 Avg_q_Run_Keypress_Now_Tag=g_idle_add(Run_Keypress_Now, (gpointer)in_dialog_widgets);
 return FALSE;
}
LOCAL Bool
method_combo_changed_event(GtkWidget *widget, GtkWidget **in_dialog_widgets) {
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(in_dialog_widgets[-1]), TRUE);
 return FALSE;
}
LOCAL void
MethodInstance_build_dialog(void) {
 GtkWidget *box1, *hbox;
 GtkWidget *button;
 GtkWidget *label;

 old_method_cursorline=CursorLineNumber();
 nr_of_active_elements=0;
 MethodInstance_window=gtk_dialog_new();
 gtk_window_set_position (GTK_WINDOW (MethodInstance_window), GTK_WIN_POS_MOUSE);
 gtk_window_set_wmclass (GTK_WINDOW (MethodInstance_window), "Method instance", "avg_q");
 g_signal_connect (G_OBJECT (MethodInstance_window), "destroy", G_CALLBACK (method_dialog_close), NULL);
 gtk_window_set_title (GTK_WINDOW (MethodInstance_window), method.method_name);

 box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
 gtk_box_pack_start (GTK_BOX (gtk_dialog_get_action_area(GTK_DIALOG(MethodInstance_window))), box1, FALSE, FALSE, 0);
 gtk_widget_show (box1);

 label = gtk_label_new (method.method_description);
 gtk_box_pack_start (GTK_BOX(box1), label, FALSE, FALSE, 0);
 gtk_widget_show (label);

 if (method.nr_of_arguments==0) {
  label = gtk_label_new ("This method has neither options nor arguments.");
  gtk_box_pack_start (GTK_BOX(box1), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
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
    growing_buf arg;
    growing_buf_settothis(&arg, script_variables[variable-1]);
    arg.delimiters="";
    growing_buf_firsttoken(&arg);
#if 0
    printf("Variable %d found: Setting %s to >%s<\n", variable, argument_descriptor->description, script_variables[variable-1]);
#endif
    /* Otherwise, accept_argument will reject the new contents: */
    argument->is_set=FALSE;
    accept_argument(&instance_tinfo, &arg, &method.argument_descriptors[current_argument]);
   }

   if (!variables_only || variable!=0) {
    Bool entry_is_optional=FALSE;
    int entry_index= -1;	/* Where in dialog_widgets is the last entry widget */

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX(box1), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);
    if (*argument_descriptor->option_letters!='\0') {
     GtkWidget * const checkbox=gtk_check_button_new_with_label(argument_descriptor->option_letters);
     entry_is_optional=TRUE;
     gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), argument->is_set);
     gtk_widget_show (checkbox);
     dialog_widgets[nr_of_active_elements++]=checkbox;
    }
    /* Here comes the option description as label: */
    label = gtk_label_new (argument_descriptor->description);
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    switch(argument_descriptor->type) {
     case T_ARGS_TAKES_NOTHING:
      break;
     case T_ARGS_TAKES_LONG: {
#define BUFFER_SIZE 300
      char buffer[BUFFER_SIZE];
      GtkWidget * const entry=gtk_entry_new();

      snprintf(buffer, BUFFER_SIZE, "%ld", argument->arg.i);
      gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);
      gtk_entry_set_text(GTK_ENTRY(entry), buffer);
      entry_index=nr_of_active_elements;
      gtk_widget_show (entry);
      dialog_widgets[nr_of_active_elements++]=entry;
      }
      break;
     case T_ARGS_TAKES_DOUBLE: {
      char buffer[BUFFER_SIZE];
      GtkWidget * const entry=gtk_entry_new();

      snprintf(buffer, BUFFER_SIZE, "%g", argument->arg.d);
      gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);
      gtk_entry_set_text(GTK_ENTRY(entry), buffer);
      entry_index=nr_of_active_elements;
      gtk_widget_show (entry);
      dialog_widgets[nr_of_active_elements++]=entry;
      }
      break;
#undef BUFFER_SIZE
     case T_ARGS_TAKES_STRING_WORD:
     case T_ARGS_TAKES_SENTENCE: {
      GtkWidget * const entry=gtk_entry_new();
      gsize thisutflength;

      gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);
      if (argument->arg.s!=NULL)
       gtk_entry_set_text(GTK_ENTRY(entry), g_locale_to_utf8(argument->arg.s,-1,NULL,&thisutflength,NULL));
      entry_index=nr_of_active_elements;
      gtk_widget_show (entry);
      dialog_widgets[nr_of_active_elements++]=entry;
      }
      break;
     case T_ARGS_TAKES_FILENAME: {
      GtkWidget * const entry=gtk_entry_new();
      GtkWidget * const select_button=gtk_button_new_with_label("Select");
      GCallback const select_callback=G_CALLBACK(entry_is_optional ? method_fileselect_optional : method_fileselect_normal);
      gsize thisutflength;

      gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);
      if (argument->arg.s!=NULL)
       gtk_entry_set_text(GTK_ENTRY(entry), g_locale_to_utf8(argument->arg.s,-1,NULL,&thisutflength,NULL));
      entry_index=nr_of_active_elements;
      gtk_widget_show (entry);
      dialog_widgets[nr_of_active_elements++]=entry;

      /* Note that we do not count the select_button as an active element,
       * since there will be no need to access its state or value */
      gtk_box_pack_start (GTK_BOX(hbox), select_button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (select_button), "clicked", select_callback, (gpointer)&dialog_widgets[nr_of_active_elements-1]);
      gtk_widget_show (select_button);
      }
      break;
     case T_ARGS_TAKES_SELECTION: {
      GtkWidget * const combo=gtk_combo_box_text_new ();
      const gchar *const *choice=argument_descriptor->choices;

      while (*choice!=NULL) {
       gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), *choice);
       choice++;
      }
      gtk_box_pack_start (GTK_BOX(hbox), combo, FALSE, FALSE, 0);
      gtk_combo_box_set_active(GTK_COMBO_BOX(combo),argument->arg.i);
      if (entry_is_optional) {
       g_signal_connect (combo, "changed", G_CALLBACK (method_combo_changed_event), (gpointer)&dialog_widgets[nr_of_active_elements]);
      }
      gtk_widget_show (combo);
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
     GCallback const func=G_CALLBACK(entry_is_optional ?  method_optional_entry_keypress_event : method_normal_entry_keypress_event);
     g_signal_connect (G_OBJECT (dialog_widgets[entry_index]), "key_press_event", func, (gpointer)&dialog_widgets[entry_index]);
    }
   }
  }
 }

 Dialog_Configuration_Error = gtk_label_new ("<Long message to force Configuration Error field width>");
 gtk_box_pack_start (GTK_BOX(box1), Dialog_Configuration_Error, FALSE, FALSE, 0);
 gtk_widget_hide (Dialog_Configuration_Error);

 hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
 gtk_box_pack_start (GTK_BOX(box1), hbox, FALSE, FALSE, 0);
 gtk_widget_show (hbox);

 /* An OK button is now always shown - it could be that our convenient
  * start-of-method-name function has kicked in and the user wants to accept
  * that full method name. Therefore, independently of old_method and any
  * modifiable options, we'll have a default OK button. */
 button = gtk_button_new_with_label ("Okay");
 g_signal_connect_object (G_OBJECT (button), "clicked", G_CALLBACK(method_dialog_ok), G_OBJECT (MethodInstance_window), G_CONNECT_AFTER);
 gtk_widget_set_can_default (button, TRUE);
 gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);
 gtk_widget_grab_default (button);
 gtk_widget_show (button);
 MethodInstance_Defaultbutton=button;

 button = gtk_button_new_with_label ("close");
 g_signal_connect_object (G_OBJECT (button), "clicked", G_CALLBACK(method_dialog_close_button), G_OBJECT (MethodInstance_window), G_CONNECT_AFTER);
 gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);
 gtk_widget_show (button);

 gtk_widget_show (MethodInstance_window);
}

LOCAL void
setup_from_line_requested(void) {
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
}

LOCAL Bool
button_press_event(GtkWidget *widget, GdkEventButton *event) {
 if (event->button==3) {
  setup_from_line_requested();
  return TRUE; /* Event has been handled */
 }
 return FALSE;
}
LOCAL Bool
key_press_in_mainwindow(GtkWidget *widget, GdkEventKey *event) {
 if (event->keyval==GDK_KEY_Tab) {
  setup_from_line_requested();
  return TRUE;
 }
 return FALSE;
}
/*}}}*/

/*{{{ Running scripts*/
#ifdef USE_THREADING
LOCAL void *
run_script_thread(void *thread_args) {
#else
LOCAL void 
Run_Script(void) {
#endif
 /* The `succeeded' variable should not be stack-based because it might
  * be clobbered by longjmp */
 succeeded=FALSE;
 if (running) {
  gdk_threads_enter();
  set_runstatus("Script is already running.");
  gdk_threads_leave();
 } else {
  void (* const * const method_selects)(transform_info_ptr)=get_method_list();
  int laststatus;
  growing_buf script;
  growing_buf_init(&script);
  growing_buf_allocate(&script, 0);

  G_LOCK(running);
  running=TRUE;
  G_UNLOCK(running);
  gdk_threads_enter();
  gtk_widget_set_sensitive(Run_Entry, FALSE);
  gtk_widget_set_sensitive(Run_Subscript_Entry, FALSE);
  gtk_widget_set_sensitive(Dump_Entry, FALSE);
  gtk_widget_set_sensitive(Stop_Entry, TRUE);
  gtk_widget_set_sensitive(Kill_Entry, TRUE);
  gdk_threads_leave();

#ifdef USE_THREADING
#if 0
  /* Note that push and pop must be outside the setjmp switch, otherwise
   * things get mangled by a longjmp */
  pthread_cleanup_push(script_cleanup, (void *)"Script was canceled.");
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
#endif

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
	if (tinfostruc.data_type==FREQ_DATA) {
	 int beforepoints=tinfostruc.beforetrig;
	 int beforeshifts;
	 if (tinfostruc.shiftwidth!=0) beforeshifts=1+(beforepoints-tinfostruc.windowsize)/tinfostruc.shiftwidth;
	 else beforeshifts=0;
	 TRACEMS3(tinfostruc.emethods, -1, "Averages: %d (nrofaverages=%d); Shifts for baseline: %d\n", MSGPARM(tinfostruc.accepted_epochs), MSGPARM(tinfostruc.nrofaverages), MSGPARM(beforeshifts));
	 TRACEMS3(tinfostruc.emethods, -1, "Output is %d frequencies x %d shifts of step %d\n", MSGPARM(tinfostruc.nroffreq), MSGPARM(tinfostruc.nrofshifts), MSGPARM(tinfostruc.shiftwidth));
	} else {
	 TRACEMS2(tinfostruc.emethods, -1, "Averages: %d (nrofaverages=%d)\n", MSGPARM(tinfostruc.accepted_epochs), MSGPARM(tinfostruc.nrofaverages));
	}
	if (tinfostruc.rejected_epochs>0) {
	 snprintf(errormessage, ERRORMESSAGE_SIZE, "Rejection rate: %6.2f%%\n", tinfostruc.rejected_epochs/((float)tinfostruc.rejected_epochs+tinfostruc.accepted_epochs)*100.0);
	 TRACEMS(tinfostruc.emethods, -1, errormessage);
	}
	free_tinfo(&tinfostruc);
       }
      }
      /* This is necessary in order to free the method memory allocated during setup: */
      free_queuemethodmem(&tinfostruc, &iter_queue);
      free_queuemethodmem(&tinfostruc, &post_queue);
     } else {
      gdk_threads_enter();
      set_runstatus("Script is empty.");
      gdk_threads_leave();
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
     gdk_threads_enter();
     set_runstatus("Script was dumped.");
     gdk_threads_leave();
    } else {
     gdk_threads_enter();
     set_runstatus("Script was executed.");
     gdk_threads_leave();
    }
    only_script=save_only_script;
    if (!interactive && script_file==NULL) {
     g_idle_add((GSourceFunc)gtk_main_quit, NULL);
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
    gdk_threads_enter();
    set_runstatus(errormessage);
    goto_line(tinfostruc.methods->line_of_script-1);
    gtk_window_deiconify(GTK_WINDOW(Avg_Q_Main_Window));
    gdk_threads_leave();
    Close_script_file();
    save_only_script=only_script=0;
    interactive=TRUE;
    break;
  }
#ifdef USE_THREADING
#if 0
  pthread_cleanup_pop(FALSE);
#endif
#endif
  G_LOCK(running);
  running=FALSE;
  G_UNLOCK(running);
  gdk_threads_enter();
  gtk_widget_set_sensitive(Run_Entry, TRUE);
  gtk_widget_set_sensitive(Run_Subscript_Entry, TRUE);
  gtk_widget_set_sensitive(Dump_Entry, TRUE);
  gtk_widget_set_sensitive(Stop_Entry, FALSE);
  gtk_widget_set_sensitive(Kill_Entry, FALSE);
  if (script_file!=NULL) {
   Avg_q_Load_Subscript_Tag=g_idle_add(Load_Next_Subscript, NULL);
  }
  gdk_threads_leave();
  growing_buf_free(&script);
 }
#ifdef USE_THREADING
 /* Don't need to call g_thread_exit(), returning is sufficient */
 return(&succeeded);
#else
 return;
#endif
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
Run_CursorSubScript(GtkWidget *menuitem) {
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
 if (script_file!=NULL) {
  if (g_io_channel_unix_get_fd(script_file)!=STDIN_FILENO)  g_io_channel_shutdown(script_file,TRUE,NULL);
  script_file=NULL;
 }
}
LOCAL gint
Load_Next_Subscript(gpointer data) {
 gunichar thischar=0;
 GIOStatus giostatus=G_IO_STATUS_NORMAL;
 struct stat statbuf;
 Bool script_is_FIFO=FALSE;

 fstat(g_io_channel_unix_get_fd(script_file),&statbuf);
 if (S_ISFIFO(statbuf.st_mode)) {
  script_is_FIFO=TRUE;
 }

 growing_buf_clear(&static_linebuf);
 do {
  giostatus=g_io_channel_read_unichar(script_file,&thischar,NULL);
  //printf("%lc",thischar);
  /* Allow MSDOS files by dropping \r characters: */
  if (thischar=='\r') continue;
  if (thischar==0x04) giostatus=G_IO_STATUS_EOF; /* Interpret ^D as EOF */
  if (giostatus!=G_IO_STATUS_EOF) {
   gchar thisgchar[6];
   gsize const thisgcharlength=g_unichar_to_utf8(thischar,(gchar *)&thisgchar);
   growing_buf_append(&static_linebuf, thisgchar, thisgcharlength);
   gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, (gchar *)&thisgchar, thisgcharlength);
  }
  if (giostatus==G_IO_STATUS_EOF || thischar=='\n') {
   growing_buf_appendchar(&static_linebuf, '\0');
   if (!interactive && IS_SEPLINE(static_linebuf.buffer_start)) {
    subscript_loaded++;
    if ((only_script==0 && script_is_FIFO && !dumponly) || only_script==subscript_loaded) break;
   }
   growing_buf_clear(&static_linebuf);
   if (giostatus==G_IO_STATUS_EOF) break;
  }
 } while (1);
 if (giostatus==G_IO_STATUS_EOF) {
  subscript_loaded++;
  Close_script_file();
 }
 if (only_script==subscript_loaded) {
  Close_script_file();
 }
 if (!interactive) {
  /* In dumponly mode, we need to dump all sub-scripts at once, only_script=0 */
  if (only_script==0 && script_is_FIFO && !dumponly) {
   only_script=subscript_loaded;
  }
  Run_Script();
 }
 return FALSE;
}
LOCAL Bool
Load_Script(gchar *name) {
 GError *error=NULL;
 if (strcmp(name, "stdin")==0) {
  script_file=g_io_channel_unix_new(STDIN_FILENO);
 } else {
  script_file=g_io_channel_new_file(name,"r",&error);
 }

 /* We want to always set the current filename, no matter whether the
  * load operation was successful or not.
  * Note that name==filename when initiated by open_file. */
 if (name!=filename) strncpy(filename, name, FilenameLength);
 set_main_window_title();

 if (error==NULL) {
  subscript_loaded=0;
  clear_script();
  Avg_q_Load_Subscript_Tag=g_idle_add(Load_Next_Subscript, NULL);
  snprintf(errormessage, ERRORMESSAGE_SIZE, "Loading script from file %s.", name);
 } else {
  snprintf(errormessage, ERRORMESSAGE_SIZE, "open_file: %s: %s", name, strerror(errno));
  interactive=TRUE;
 }
 set_status(errormessage);
 return (script_file!=NULL);
}
LOCAL void
open_file(GtkWidget *menuitem) {
 const gchar *uri = NULL;

 GtkWidget *filesel=gtk_file_chooser_dialog_new("Select a script file to load",
  GTK_WINDOW(Avg_Q_Main_Window),
  GTK_FILE_CHOOSER_ACTION_OPEN,
  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
  NULL);
 gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(filesel),TRUE);
 /* Note that set_current_name only works for ACTION_SAVE or ACTION_CREATE_FOLDER,
  * so we need to use gtk_file_chooser_set_uri... */
 if (g_path_is_absolute(filename)) {
  uri=g_filename_to_uri(filename,NULL,NULL);
 } else {
  char * const path=g_get_current_dir();
  char * const gfilename=g_build_filename(path,filename,NULL);
  uri=g_filename_to_uri(gfilename,NULL,NULL);
  g_free(path);
  g_free(gfilename);
 }
 gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (filesel), uri);

 if (gtk_dialog_run (GTK_DIALOG (filesel)) == GTK_RESPONSE_ACCEPT) {
  char *gfilename;
  gfilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
  strncpy(filename, gfilename, FilenameLength);
  g_free (gfilename);
  Avg_q_Load_Script_Now_Tag=g_idle_add(Load_Script_Now, filename);
 }
 gtk_widget_destroy (filesel);
}
/*}}}*/
/*{{{ Saving script files*/
LOCAL void
save_file(GtkWidget *menuitem) {
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
LOCAL void
save_file_as(GtkWidget *menuitem) {
 const gchar *uri = NULL;

 GtkWidget *filesel=gtk_file_chooser_dialog_new("Select a script file name to save to",
  GTK_WINDOW(Avg_Q_Main_Window),
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
  GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT,
  NULL);
 gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(filesel),TRUE);
 if (g_path_is_absolute(filename)) {
  uri=g_filename_to_uri(filename,NULL,NULL);
 } else {
  char * const path=g_get_current_dir();
  char * const gfilename=g_build_filename(path,filename,NULL);
  uri=g_filename_to_uri(gfilename,NULL,NULL);
  g_free(path);
  g_free(gfilename);
 }
 gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (filesel), uri);
 /* This is necessary to default to a name that does not exist yet... */
 gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filesel), g_path_get_basename(filename));
 gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (filesel), TRUE);

 if (gtk_dialog_run (GTK_DIALOG (filesel)) == GTK_RESPONSE_ACCEPT) {
  char *gfilename;
  gfilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
  strncpy(filename, gfilename, FilenameLength);
  g_free (gfilename);
  set_main_window_title();
  save_file(NULL);
 }
 gtk_widget_destroy (filesel);
}
/*}}}*/
/*}}}  */
#else
/*{{{ Now follows the STANDALONE implementation of run_script_thread */
#ifdef USE_THREADING
LOCAL void *
run_script_thread(void *thread_args) {
#else
LOCAL void 
Run_Script(void) {
#endif
 /* The `succeeded' variable should not be stack-based because it might
  * be clobbered by longjmp */
 succeeded=FALSE;
 dump_script_pointer=dump_script_pointers;
 if (running) {
  gdk_threads_enter();
  set_runstatus("Script is already running.");
  gdk_threads_leave();
 } else {
 G_LOCK(running);
 running=TRUE;
 G_UNLOCK(running);
 gdk_threads_enter();
 gtk_widget_set_sensitive(Run_Entry, FALSE);
 gtk_widget_set_sensitive(Dump_Entry, FALSE);
 gtk_widget_set_sensitive(Stop_Entry, TRUE);
 gtk_widget_set_sensitive(Kill_Entry, TRUE);
 gdk_threads_leave();

#ifdef USE_THREADING
#if 0
 /* Note that push and pop must be outside the setjmp switch, otherwise
  * things get mangled by a longjmp */
 pthread_cleanup_push(script_cleanup, (void *)"Script was canceled.");
 pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
 pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
#endif
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
      if (tinfostruc.data_type==FREQ_DATA) {
       int beforepoints=tinfostruc.beforetrig;
       int beforeshifts;
       if (tinfostruc.shiftwidth!=0) beforeshifts=1+(beforepoints-tinfostruc.windowsize)/tinfostruc.shiftwidth;
       else beforeshifts=0;
       TRACEMS3(tinfostruc.emethods, -1, "Averages: %d (nrofaverages=%d); Shifts for baseline: %d\n", MSGPARM(tinfostruc.accepted_epochs), MSGPARM(tinfostruc.nrofaverages), MSGPARM(beforeshifts));
       TRACEMS3(tinfostruc.emethods, -1, "Output is %d frequencies x %d shifts of step %d\n", MSGPARM(tinfostruc.nroffreq), MSGPARM(tinfostruc.nrofshifts), MSGPARM(tinfostruc.shiftwidth));
      } else {
       TRACEMS2(tinfostruc.emethods, -1, "Averages: %d (nrofaverages=%d)\n", MSGPARM(tinfostruc.accepted_epochs), MSGPARM(tinfostruc.nrofaverages));
      }
      if (tinfostruc.rejected_epochs>0) {
       snprintf(errormessage, ERRORMESSAGE_SIZE, "Rejection rate: %6.2f%%\n", tinfostruc.rejected_epochs/((float)tinfostruc.rejected_epochs+tinfostruc.accepted_epochs)*100.0);
       TRACEMS(tinfostruc.emethods, -1, errormessage);
      }
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
    gdk_threads_enter();
    set_runstatus("Script was dumped.");
    gdk_threads_leave();
   } else {
    gdk_threads_enter();
    set_runstatus("Script was executed.");
    gdk_threads_leave();
   }
   if (!interactive) {
    gdk_threads_enter();
    g_idle_add((GSourceFunc)gtk_main_quit, NULL);
    gdk_threads_leave();
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
   gdk_threads_enter();
   set_runstatus(errormessage);
   gtk_window_deiconify(GTK_WINDOW(Avg_Q_Main_Window));
   gdk_threads_leave();
   save_only_script=only_script=0;
   interactive=TRUE;
   break;
 }
#ifdef USE_THREADING
#if 0
 pthread_cleanup_pop(FALSE);
#endif
#endif
 G_LOCK(running);
 running=FALSE;
 G_UNLOCK(running);
 gdk_threads_enter();
 gtk_widget_set_sensitive(Run_Entry, TRUE);
 gtk_widget_set_sensitive(Dump_Entry, TRUE);
 gtk_widget_set_sensitive(Stop_Entry, FALSE);
 gtk_widget_set_sensitive(Kill_Entry, FALSE);
 gdk_threads_leave();
 }
#ifdef USE_THREADING
 return(&succeeded);
#else
 return;
#endif
#undef iter_queue
#undef post_queue
}
/*}}}  */
#endif

LOCAL void
Stop_Script(GtkWidget *menuitem) {
 if (running) {
  G_LOCK(stop_request);
  stop_request=TRUE;
  G_UNLOCK(stop_request);
  set_status("Script will be stopped...");
 } else {
  set_status("Nothing to stop.");
 }
}
LOCAL void
Cancel_Script(GtkWidget *menuitem) {
 if (running) {
  set_status("Script will be cancelled...");
#if 0
  pthread_cancel(script_tid);
#endif
 } else {
  set_status("Nothing to cancel.");
 }
}
LOCAL void
Quit_avg_q(GtkWidget *menuitem) {
 /* This is called when the main window is destroyed or from the file
  * menu "Quit" entry - meaning we really want to stop the program... */
 if (running) {
  /* Sadly, Stop_Script isn't sufficient to kill the script, and we also cannot
   * just wait using pthread_join() since we'd block all GTK processing */
  Cancel_Script(menuitem);
 }
 gtk_main_quit();
}

LOCAL void
dump_file_as(GtkWidget *menuitem) {
 const gchar *uri = NULL;
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

 GtkWidget *filesel=gtk_file_chooser_dialog_new("Select an include file name to dump to",
  GTK_WINDOW(Avg_Q_Main_Window),
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
  GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT,
  NULL);
 GtkFileFilter *filter=gtk_file_filter_new();
 GtkFileFilter *allfiles=gtk_file_filter_new();
 gtk_file_filter_add_pattern(filter,"*.h"); gtk_file_filter_set_name(filter,"*.h");
 gtk_file_filter_add_pattern(allfiles,"*"); gtk_file_filter_set_name(allfiles,"All files");
 gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filesel),filter); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filesel),allfiles);
 gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(filesel),TRUE);
 if (g_path_is_absolute(newfilename)) {
  uri=g_filename_to_uri(newfilename,NULL,NULL);
 } else {
  char * const path=g_get_current_dir();
  char * const gfilename=g_build_filename(path,newfilename,NULL);
  uri=g_filename_to_uri(gfilename,NULL,NULL);
  g_free(path);
  g_free(gfilename);
 }
 gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (filesel), uri);
 /* This is necessary to default to a name that does not exist yet... */
 gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filesel), g_path_get_basename(newfilename));
 gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (filesel), TRUE);

 if (gtk_dialog_run (GTK_DIALOG (filesel)) == GTK_RESPONSE_ACCEPT) {
  char *gfilename;
  gfilename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
  if ((dumpfile=fopen(gfilename, "w"))!=NULL) {
   dumponly=TRUE;
   set_status("Dumping script...");
   Run_Script();
  }
  g_free (gfilename);
 }
 gtk_widget_destroy (filesel);
}
LOCAL void
avg_q_about(GtkWidget *menuitem) {
#define BUFFER_SIZE 1024
 char buffer[BUFFER_SIZE];
 snprintf(buffer, BUFFER_SIZE, "%s\nThis user interface was built with the free GTK library (v%d.%d): See http://www.gtk.org/\n", get_avg_q_signature(), GTK_MAJOR_VERSION, GTK_MINOR_VERSION);
 Notice(buffer);
#undef BUFFER_SIZE
}

LOCAL void
set_tracelevel(GtkWidget *menuitem, gpointer data) {
 external_method.emethod.trace_level=(int)data;
}

/* This is used to redirect keypresses to the script panel: */
static Bool
key_press_for_mainwindow(GtkWidget *widget, GdkEventKey *event) {
 static gboolean retarg;
 /* Note that this disables the shortcut setting functions that are otherwise
  * present for menus, when the original key was sent to a menu: */
 g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
 g_signal_emit_by_name(G_OBJECT(Avg_Q_Main_Window), "key_press_event", event, &retarg);
 return TRUE;
}

#ifndef STANDALONE
LOCAL void
method_menu (GtkWidget *menuitem, gpointer data) {
 MethodInstance_init_methodstruc();
 switch (setjmp(instance_emethod.error_jmp)) {
  case 0: {
   if (MethodInstance_window!=NULL) {
    method_dialog_close(GTK_WIDGET(MethodInstance_window));
   }
   MethodInstance_setup_from_method((void (*)(transform_info_ptr))data);
   variables_only=FALSE;
   old_method=FALSE;
   MethodInstance_build_dialog();
   }
   break;
  case ERROR_STATUS:
   set_status(errormessage);
   gtk_window_deiconify(GTK_WINDOW(Avg_Q_Main_Window));
   break;
 }
}
#endif

LOCAL void
create_main_window (void) {
 GtkWidget *box1, *hbox;
 GtkWidget *handle_box;
 GtkWidget *menubar, *filemenu, *tracemenu, *menuitem;
 GtkWidget *topitem;
 GtkWidget *scriptpanel;
 GtkWidget *scrolledwindow;
 GSList *tracegroup;
 GtkAccelGroup *accel_group=gtk_accel_group_new ();
 gchar const *accpath;

 Avg_Q_Main_Window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
#if 0
 gtk_widget_modify_font(GTK_WIDGET(Avg_Q_Main_Window),pango_font_description_from_string(DEFAULT_FONT));
#endif
 gtk_window_set_wmclass (GTK_WINDOW (Avg_Q_Main_Window), "MainWindow", "avg_q");
 gtk_window_set_default_size (GTK_WINDOW(Avg_Q_Main_Window), FRAME_SIZE_X, FRAME_SIZE_Y);
 g_signal_connect (G_OBJECT (Avg_Q_Main_Window), "destroy", G_CALLBACK (Quit_avg_q), NULL);
 g_signal_connect (G_OBJECT (Avg_Q_Main_Window), "delete_event", G_CALLBACK (Quit_avg_q), NULL);

 box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
 gtk_container_add (GTK_CONTAINER (Avg_Q_Main_Window), box1);
 gtk_widget_show (box1);

 handle_box = gtk_handle_box_new ();
 gtk_box_pack_start (GTK_BOX (box1), handle_box, FALSE, TRUE, 0);
 gtk_widget_show (handle_box);

 menubar = gtk_menu_bar_new ();
 gtk_container_add (GTK_CONTAINER (handle_box), menubar);
 gtk_widget_show (menubar);

 /* File menu */
 filemenu=gtk_menu_new();

 menuitem=gtk_tearoff_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Change directory");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(change_dir), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);
 accpath="<AVG_Q_UI>/File";
 gtk_accel_map_add_entry(accpath, GDK_KEY_Z, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(menuitem, accpath, accel_group);

#ifndef STANDALONE
 menuitem=gtk_menu_item_new_with_label("Open");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(open_file), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);
 accpath="<AVG_Q_UI>/File/Open";
 gtk_accel_map_add_entry(accpath, GDK_KEY_L, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(menuitem, accpath, accel_group);

 menuitem=gtk_menu_item_new_with_label("Save");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(save_file), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);
 accpath="<AVG_Q_UI>/File/Save";
 gtk_accel_map_add_entry(accpath, GDK_KEY_S, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(menuitem, accpath, accel_group);

 menuitem=gtk_menu_item_new_with_label("Save as");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(save_file_as), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);
#endif

 Run_Entry=gtk_menu_item_new_with_label("Run");
 g_signal_connect (G_OBJECT (Run_Entry), "activate", G_CALLBACK(Run_Script_Command), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), Run_Entry);
 gtk_widget_show (Run_Entry);
 accpath="<AVG_Q_UI>/File/Run";
 gtk_accel_map_add_entry(accpath, GDK_KEY_R, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(Run_Entry, accpath, accel_group);

#ifndef STANDALONE
 Run_Subscript_Entry=gtk_menu_item_new_with_label("Run sub-script");
 g_signal_connect (G_OBJECT (Run_Subscript_Entry), "activate", G_CALLBACK(Run_CursorSubScript), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), Run_Subscript_Entry);
 gtk_widget_show (Run_Subscript_Entry);
 accpath="<AVG_Q_UI>/File/Run sub-script";
 gtk_accel_map_add_entry(accpath, GDK_KEY_T, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(Run_Subscript_Entry, accpath, accel_group);
#endif

 Stop_Entry=gtk_menu_item_new_with_label("Stop script");
 g_signal_connect (G_OBJECT (Stop_Entry), "activate", G_CALLBACK(Stop_Script), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), Stop_Entry);
 gtk_widget_set_sensitive(Stop_Entry, FALSE);
 gtk_widget_show (Stop_Entry);
 accpath="<AVG_Q_UI>/File/Stop script";
 gtk_accel_map_add_entry(accpath, GDK_KEY_Y, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(Stop_Entry, accpath, accel_group);

 Kill_Entry=gtk_menu_item_new_with_label("Cancel script");
 g_signal_connect (G_OBJECT (Kill_Entry), "activate", G_CALLBACK (Cancel_Script), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), Kill_Entry);
 gtk_widget_set_sensitive(Kill_Entry, FALSE);
 gtk_widget_show (Kill_Entry);

 Dump_Entry=gtk_menu_item_new_with_label("Dump");
 g_signal_connect (G_OBJECT (Dump_Entry), "activate", G_CALLBACK(dump_file_as), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), Dump_Entry);
 gtk_widget_show (Dump_Entry);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("About");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(avg_q_about), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Quit");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(Quit_avg_q), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (filemenu), menuitem);
 gtk_widget_show (menuitem);
 accpath="<AVG_Q_UI>/File/Quit";
 gtk_accel_map_add_entry(accpath, GDK_KEY_Q, GDK_KEY_Control_R);
 gtk_widget_set_accel_path(menuitem, accpath, accel_group);

 g_signal_connect (G_OBJECT (filemenu), "key_press_event", G_CALLBACK (key_press_for_mainwindow), NULL);

 topitem= gtk_menu_item_new_with_label("Script");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), filemenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);

 /* Trace menu */
 tracemenu=gtk_menu_new();

 menuitem=gtk_tearoff_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (tracemenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_radio_menu_item_new_with_label(NULL, "0 (only warnings)");
 tracegroup=gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(set_tracelevel), (gpointer)0);
 gtk_menu_shell_append (GTK_MENU_SHELL (tracemenu), menuitem);
 gtk_widget_show (menuitem);
 TraceMenuItems[0]=menuitem;

 menuitem=gtk_radio_menu_item_new_with_label(tracegroup, "1 (most messages)");
 tracegroup=gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(set_tracelevel), (gpointer)1);
 gtk_menu_shell_append (GTK_MENU_SHELL (tracemenu), menuitem);
 gtk_widget_show (menuitem);
 TraceMenuItems[1]=menuitem;

 menuitem=gtk_radio_menu_item_new_with_label(tracegroup, "2 (more messages)");
 tracegroup=gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(set_tracelevel), (gpointer)2);
 gtk_menu_shell_append (GTK_MENU_SHELL (tracemenu), menuitem);
 gtk_widget_show (menuitem);
 TraceMenuItems[2]=menuitem;

 menuitem=gtk_radio_menu_item_new_with_label(tracegroup, "3 (even more...)");
 tracegroup=gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(set_tracelevel), (gpointer)3);
 gtk_menu_shell_append (GTK_MENU_SHELL (tracemenu), menuitem);
 gtk_widget_show (menuitem);
 TraceMenuItems[3]=menuitem;
 g_signal_connect (G_OBJECT (tracemenu), "key_press_event", G_CALLBACK (key_press_for_mainwindow), NULL);

 topitem= gtk_menu_item_new_with_label("Tracelvl");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), tracemenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);

#ifndef STANDALONE
 {
 void (* const * const method_selects)(transform_info_ptr)=get_method_list();
 void (* const *m_select)(transform_info_ptr);

 int method_type_id;
 for (method_type_id=0; method_type_id<NR_OF_METHOD_TYPES; method_type_id++) {
  enum method_types const method_type=method_types[method_type_id];
  GtkWidget *submenu;

  submenu=gtk_menu_new();
  menuitem=gtk_tearoff_menu_item_new();
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
  gtk_widget_show (menuitem);

  for (m_select=method_selects; *m_select!=NULL; m_select++) {
   (**m_select)(&tinfostruc);
   if (tinfostruc.methods->method_type==method_type) {
    menuitem=gtk_menu_item_new_with_label(tinfostruc.methods->method_name);
    g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(method_menu), (gpointer)(*m_select));
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
    gtk_widget_show (menuitem);
   }
  }

  g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_for_mainwindow), NULL);

  topitem= gtk_menu_item_new_with_label(methodtype_names[method_type]);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
  gtk_widget_show (topitem);

 }
 }
#endif

 scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
 gtk_box_pack_start (GTK_BOX (box1), scrolledwindow, TRUE, TRUE, 0);
 gtk_widget_show (scrolledwindow);

 CurrentScriptPanel=scriptpanel=gtk_text_view_new();
 gtk_container_add (GTK_CONTAINER (scrolledwindow), scriptpanel);
#ifndef STANDALONE
 g_signal_connect_swapped (G_OBJECT (CurrentScriptPanel), "button_press_event", G_CALLBACK (button_press_event), NULL);
 g_signal_connect_swapped (G_OBJECT (CurrentScriptPanel), "key_press_event", G_CALLBACK (key_press_in_mainwindow), NULL);
#endif
 gtk_widget_show(scriptpanel);
 gtk_widget_grab_focus(scriptpanel);

 handle_box = gtk_handle_box_new ();
 gtk_box_pack_start (GTK_BOX (box1), handle_box, FALSE, TRUE, 0);
 gtk_widget_show (handle_box);

 hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
 gtk_widget_show (hbox);
 gtk_container_add (GTK_CONTAINER (handle_box), hbox);

 Avg_q_StatusBar = gtk_statusbar_new();
 /* FIXME The 'natural width' of the status bar remains 0 irrespective of the text
  * pushed to it. This means that if the handle_box is torn off, the whole thing
  * collapses to a mere handle - so we set a minimum width here: */
 gtk_widget_set_size_request(Avg_q_StatusBar, FRAME_SIZE_X/2, -1);
 gtk_box_pack_start (GTK_BOX (hbox), Avg_q_StatusBar, TRUE, TRUE, 0);
 gtk_widget_show (Avg_q_StatusBar);
 Avg_q_StatusContext = gtk_statusbar_get_context_id(GTK_STATUSBAR(Avg_q_StatusBar), "avg_q status");
 set_status("Welcome to avg_q!");

 Avg_q_StatusRunBar = gtk_statusbar_new();
 gtk_widget_set_size_request(Avg_q_StatusRunBar, FRAME_SIZE_X/2, -1);
 gtk_box_pack_start (GTK_BOX (hbox), Avg_q_StatusRunBar, TRUE, TRUE, 0);
 gtk_widget_show (Avg_q_StatusRunBar);
 Avg_q_StatusRunContext = gtk_statusbar_get_context_id(GTK_STATUSBAR(Avg_q_StatusRunBar), "script status");
 set_runstatus("Script processor ready.");

 gtk_window_add_accel_group (GTK_WINDOW(Avg_Q_Main_Window), accel_group);
}

/* passing stream=NULL results in output to trace messages */
LOCAL void
usage(FILE *stream) {
#define BUFFER_SIZE 2048
 char buffer[BUFFER_SIZE];
 growing_buf buf;

#ifndef STANDALONE
 snprintf(buffer, BUFFER_SIZE, "Usage: %s [options] Configfile [script_argument1 ...]\n"
  "Multichannel (EEG/MEG) data processing GUI. Reads a script (Config) file to\n"
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
 growing_buf_firstsingletoken(&buf);
 while (buf.have_token) {
  if (stream==NULL) {
   TRACEMS(tinfostruc.emethods, -1, buf.current_token);
   TRACEMS(tinfostruc.emethods, -1, "\n");
  } else {
   fprintf(stream,"%s\n",buf.current_token);
  }
  growing_buf_nextsingletoken(&buf);
 }
#undef BUFFER_SIZE
}
#ifdef STANDALONE
LOCAL gint
Run_Script_Now(gpointer data) {
 Run_Script_Command(data);
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

/*{{{ main()*/
int
main (int argc, char *argv[]) {
 int errflag=0, c;
 char *vdevice, *envbuffer;
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
 envbuffer=malloc((strlen(vdevice)+9)*sizeof(char));
 if (envbuffer==NULL) {
  fprintf(stderr, "avg_q_ui: Error allocating envbuffer!\n");
  exit(1);
 }
 sprintf(envbuffer, "VDEVICE=%s", vdevice);
 putenv(envbuffer);
 /* Note that the buffer cannot be free()d while the
  * program runs, otherwise `unexpected results' occur... 
  * This is documented behavior: envbuffer becomes part of the environment. */ 

#ifdef USE_THREADING
 /* init threads */
 g_thread_init(NULL);
 gdk_threads_init();
#endif

 gtk_init (&argc, &argv);

#ifdef HAVE_LIBGLE
 gle_init (&argc, &argv);
#endif /* !HAVE_LIBGLE */

 /* These things should be done before the first calls to the BF library
  * (including method selections): */
 set_external_methods(&external_method.emethod, &error_exit, &trace_message, &static_execution_callback);
 external_method.emethod.trace_level=0;
 tinfostruc.methods= &mymethod;
 tinfostruc.emethods= &external_method.emethod;

 create_main_window ();

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
    gtk_window_iconify(GTK_WINDOW(Avg_Q_Main_Window));
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
 gtk_widget_show (Avg_Q_Main_Window);

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
 if (interactive)  {
  gtk_window_deiconify(GTK_WINDOW(Avg_Q_Main_Window));
 }

 iter_queue.start=post_queue.start=NULL;	/* Tell setup_queue that no memory is allocated yet */
#ifdef STANDALONE
 strncpy(filename, DUMP_SCRIPT_NAME, FilenameLength);
 set_main_window_title();
 snprintf(errormessage, ERRORMESSAGE_SIZE,
  "This standalone version of avg_q is\n"
  "provided with a built-in script.\n\n"
  "The file name of the script was:\n%s\n", DUMP_SCRIPT_NAME);
 gtk_text_buffer_insert_at_cursor(CurrentScriptBuffer, errormessage, strlen(errormessage));
 gtk_text_view_set_editable(GTK_TEXT_VIEW(CurrentScriptPanel), FALSE);
#else
 gtk_text_view_set_editable(GTK_TEXT_VIEW(CurrentScriptPanel), TRUE);
#endif

 if (external_method.emethod.trace_level>0 && external_method.emethod.trace_level<4) {
  gtk_widget_activate(TraceMenuItems[external_method.emethod.trace_level]);
 }

#ifndef STANDALONE
 /* Initialize the static line buffer: */
 growing_buf_init(&static_linebuf);
 growing_buf_allocate(&static_linebuf, 0);
#endif

#ifdef STANDALONE
 if (!interactive) {
  Avg_q_Run_Script_Now_Tag=g_idle_add(Run_Script_Now, NULL);
 }
#else
 if (mainargc-optind>=END_OF_ARGS) {
  strncpy(filename, MAINARG(CONFIGFILE), FilenameLength);
  set_main_window_title();
  Avg_q_Load_Script_Now_Tag=g_idle_add(Load_Script_Now, MAINARG(CONFIGFILE));
 } else {
  set_main_window_title();
 }
#endif

 gdk_threads_enter();
 gtk_main ();
 gdk_threads_leave();

 free_queue_storage(&iter_queue);
 free_queue_storage(&post_queue);
#ifndef STANDALONE
 growing_buf_free(&static_linebuf);
#endif

 return 0;
}
/*}}}*/
