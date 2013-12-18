/*
 * Copyright (C) 1996-2000 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * File:	avg_q_ui.h
 * Purpose:	"V" user interface for avg_q
 * Author:	Bernd Feige
 * Created:	29.01.1998
 * Updated:	
 * Copyright:	(c) 1998, Bernd Feige
 */

#ifndef myAPP_H
#define myAPP_H

// Include standard V files as needed

#ifdef vDEBUG
#include <v/vdebug.h>
#endif

#include <v/vapp.h>
#include <v/vawinfo.h>

// mycmdwin:
#include <v/vcmdwin.h>	// So we can use vCmdWindow
#include <v/vmenu.h>	// For the menu pane
#include <v/vutil.h>	// For V Utilities
#include <v/vcmdpane.h> // command pane
#include <v/vstatusp.h>	// For the status pane

// mycanvas:
//#include <v/vcanvas.h>
//#include <v/vtextcnv.h>
#include <v/vtexted.h>

// mydialog:
#include <v/vmodald.h>

extern "C" {
#include <setjmp.h>
#include "transform.h"
#include "bf.h"
// #undef Bool
}

// We use this in order to process the error exit differently for
// different instances of my_emethod (namely, for script running and
// configuration errors, which may occur in parallel):
typedef struct {
 struct external_methods_struct emethod;
 jmp_buf error_jmp;
} my_emethod;

class MethodInstance: public vModalDialog {
 public:
  MethodInstance(vBaseWindow* bw);
  virtual ~MethodInstance();
  void setup(void (* const method_select)(transform_info_ptr));
  void setup(growing_buf *linebuf);
  void setup(transform_methods_ptr methodp);
  void build_dialog(void);
  virtual void DialogDisplayed(void);
  virtual void DialogCommand(ItemVal id, ItemVal retval, CmdType ctype);

  void init_methodstruc(void);
  void config_from_defaults(void);
  void config_from_dialog(void);

  my_emethod instance_emethod;
  struct transform_methods_struct method;
  struct transform_info_struct instance_tinfo;

  // The next two commands modify the behavior of build_dialog:
  // This flag (false by default) determines whether the dialog should show
  // only the variable entries
  Bool variables_only;
  // This flag (false by default) causes the dialog box to contain only a `Cancel'
  // button if no user-modifiable entities are present in the box
  Bool old_method;

 private:
  static int const numDialogCommands=200;
  DialogCmd *Dialog_Commands;
};

// This is the dialog displayed when singlestepping
class TraceInfo: public vModalDialog {
 public:
  TraceInfo(vBaseWindow* bw);
  virtual ~TraceInfo();
  virtual void DialogCommand(ItemVal id, ItemVal retval, CmdType ctype);
  ItemVal ShowModalDialog(transform_info_ptr const tinfo, char* msg, ItemVal& retval);
};

class ScriptPanel: public vTextEditor {
 public:		//---------------------------------------- public
  ScriptPanel(vCmdWindow* parent);
  virtual ~ScriptPanel();

  virtual void Redraw(int x, int y, int width, int height);
  virtual void ChangeLoc(long line, int col);

  void type_string(char *s);

  Bool Load_Script(char *filename);
  Bool Save_Script(char *filename);

  Bool Run_Script(void);

  // Run only the sub-script in which the cursor is located
  Bool Run_CursorSubScript(void);

  static const int FilenameLength=300;
  char filename[FilenameLength];
  static const int SaveLineLength=300;
  char saveline[SaveLineLength];
  int fs_filter;

  int current_line;
  int current_col;

  // This tells which script number the cursor is in
  int CursorSubScript();

  // This tells whether a 'Post:' line was seen in the script
  Bool has_Postline();

  Bool running;
  Bool interactive;
  // A flag that tells Run_Script to only dump the script to file dumpfile
  Bool dumponly;
  FILE *dumpfile;
  // The number of the script to execute (0 if all)
  int only_script;

  int nr_of_script_variables;
  char **script_variables;

 protected:	//--------------------------------------- protected
  virtual void TextMouseDown(int row, int col, int button);

 private:		//--------------------------------------- private
  vCmdWindow* _parent;
  queue_desc iter_queue;
  queue_desc post_queue;
  // The following variables could have been local to Run_Script but
  // apparently this is dangerous since longjmp is used there:
  struct queue_desc_struct **dump_script_pointer;
  Bool succeeded;
};

class TracePanel : public vTextEditor {
 public:		//---------------------------------------- public
  TracePanel(vCmdWindow* parent);
  virtual ~TracePanel();
};
class TraceFrame : public vCmdWindow {
 friend int AppMain(int, char**);	// allow AppMain access

 public:		//---------------------------------------- public
  TraceFrame(char*, int, int);
  virtual ~TraceFrame();
  virtual void WindowCommand(ItemVal id, ItemVal val, CmdType cType);
  virtual void KeyIn(vKey keysym, unsigned int shift);
  virtual void CloseWin();
  void ShowWindow();

  TracePanel* trace_panel;
 protected:	//--------------------------------------- protected

 private:		//--------------------------------------- private
  vMenuPane* myMenu;		// For the menu bar
  Bool is_shown;
};


class MyFrame : public vCmdWindow {
 friend int AppMain(int, char**);	// allow AppMain access

 public:		//---------------------------------------- public
  MyFrame(char*, int, int);
  virtual ~MyFrame();
  virtual void WindowCommand(ItemVal id, ItemVal val, CmdType cType);
  virtual void KeyIn(vKey keysym, unsigned int shift);

  void execution_callback(const transform_info_ptr tinfo, const execution_callback_place where);

  TraceFrame* trace_frame;
  ScriptPanel* script_panel;		// For the canvas

 protected:	//--------------------------------------- protected

 private:		//--------------------------------------- private

  // Standard elements
  vMenuPane* myMenu;		// For the menu bar
  vCommandPane* myCmdPane;	// for the command pane
  vStatusPane* myStatus;		// For the status bar

  Bool single_step;
  TraceInfo *step_dialog;

  //wxTextWindow *text_window;
};

// Define a new application
class MyApp: public vApp {
 friend int AppMain(int, char**);	// allow AppMain access
 public:
  MyApp(char* name, int sdi = 0, int h = 0, int w = 0);
  virtual ~MyApp();

  // Routines from vApp that are normally overridden

  virtual vWindow* NewAppWin(vWindow* win, char* name, int w, int h,
	  vAppWinInfo* winInfo);

  virtual void Exit(void);

  virtual int CloseAppWin(vWindow*);

  virtual void AppCommand(vWindow* win, ItemVal id, ItemVal val, CmdType cType);

  virtual void KeyIn(vWindow*, vKey, unsigned int);

  // New routines for this particular app

 protected:	//--------------------------------------- protected

 private:		//--------------------------------------- private

  MyFrame* _myCmdWin;		// Pointer to instance of first window
};

#endif
