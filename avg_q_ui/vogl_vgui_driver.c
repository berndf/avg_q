/*
 * Copyright (C) 2008-2014,2016-2018,2020,2025 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * GTK driver for VOGL (c) 1998-2002,2004,2006-2014,2025 by Bernd Feige (Bernd.Feige@uniklinik-freiburg.de)
 * 
 * To compile:
 * 
 * 1) add VGUI to device.c 
 * 2) compile with -DVGUI
 * 
 * To run:
 * 
 * set VDEVICE=VGUI
 * 
 */

#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avg_q_ui.h"
/* This helps to canonify some things like Bool etc.: */
#include <bf.h>
/* Since Object typedef'd in vogl.h clashes with the X11 definition: */
#define Object vogl_Object
#define Boolean vogl_Boolean
#include <vogl.h>
#include <vodevice.h>

#define MAXCOLOR 256
#define FRAME_SIZE_X 600
#define FRAME_SIZE_Y 400
#define POSTSCRIPT_ENVNAME "VPOSTSCRIPT"
#define POSTSCRIPT_COLORDEV "cps"
#define POSTSCRIPT_BWDEV "postscript"

/*-------------------------- Keyboard_Buffer ------------------------------*/

#define KBUFFER_SIZE 64
static struct {
 char buffer[KBUFFER_SIZE];
 char *readptr;
 char *writeptr;
} Keyboard_Buffer;

static void 
Keyboard_Buffer_init(void) {
 Keyboard_Buffer.readptr=Keyboard_Buffer.writeptr=Keyboard_Buffer.buffer;
}
static int 
Keyboard_Buffer_keys_in_buffer(void) {
 int n=Keyboard_Buffer.writeptr-Keyboard_Buffer.readptr;
 if (n<0) n+=KBUFFER_SIZE;
 return n;
}
static char *
Keyboard_Buffer_incptr(char *p) {
 p++;
 if (p>=Keyboard_Buffer.buffer+KBUFFER_SIZE) {
  p=Keyboard_Buffer.buffer;
 }
 return p;
}
static Bool
Keyboard_Buffer_push(char c) {
 char *nextpos=Keyboard_Buffer_incptr(Keyboard_Buffer.writeptr);
 if (nextpos==Keyboard_Buffer.readptr) {
  // Buffer full (writeptr hits readptr from behind...)
  return FALSE;
 } else {
  *Keyboard_Buffer.writeptr=c;
  Keyboard_Buffer.writeptr=nextpos;
  return TRUE;
 }
}
static void
Keyboard_Buffer_pushstring(char *string) {
 while (*string!='\0') {
  if (*string=='\n') {
   Keyboard_Buffer_push(RETKEY);
   string++;
  } else {
   Keyboard_Buffer_push(*string++);
  }
 }
}
static char
Keyboard_Buffer_pop(void) {
 if (Keyboard_Buffer.readptr==Keyboard_Buffer.writeptr) {
  return '\0';
 } else {
  char const c= *Keyboard_Buffer.readptr;
  Keyboard_Buffer.readptr=Keyboard_Buffer_incptr(Keyboard_Buffer.readptr);
  return c;
 }
}

static struct {
 int lastbutton;
 int lastx;
 int lasty;
 int left;
 int top;
 int width;
 int height;
 int line_width;
 int line_style;
 int draw_lastx, draw_lasty;
 GtkWidget *window;
 GtkWidget *canvas;
 int fg;
 GdkRGBA palette[MAXCOLOR];
 cairo_t *cr;
 cairo_surface_t *surface;
 cairo_antialias_t antialias;
 Bool in_frontbuffer;
} VGUI;

// ----------------------- VOGL part ----------------------------

static int VGUI_frontbuffer(void); /* Forward declaration */

static void
my_set_font(cairo_t *cr) {
 cairo_select_font_face(cr, "Sans 20", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
 cairo_set_font_size(cr, 10);
}

static GtkWidget *Notice_window=NULL;
LOCAL void
Notice_window_close(GtkWidget *mwindow) {
 gtk_window_destroy(GTK_WINDOW(mwindow));
 Notice_window=NULL;
}
LOCAL void
Notice_window_close_button(GtkDialog *dialog, gint response_id, GtkWidget *mwindow) {
 Notice_window_close(mwindow);
}
static void
Notice(gchar *message) {
 GtkWidget *box1;
 GtkWidget *label;
 GtkWidget *button;

 if (Notice_window!=NULL) {
  Notice_window_close(Notice_window);
 }

 Notice_window=gtk_window_new();
 gtk_window_set_transient_for(GTK_WINDOW (Notice_window), GTK_WINDOW (VGUI.window));
 g_signal_connect (G_OBJECT (Notice_window), "destroy", G_CALLBACK(Notice_window_close), NULL);
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
static void
button_press_event(GtkGestureClick *gesture, int n_press, double x, double y) {
 int const button=gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
 if (button>0) {
  VGUI.lastbutton=(1<<(button-1));
 }
 VGUI.lastx=x;
 VGUI.lasty=y;
 //printf("Button: %d %d x=%0.3f y=%0.3f\n", VGUI.lastbutton, n_press, x, y);
}
static void
motion_notify_event(GtkEventControllerMotion* self, gdouble x, gdouble y, gpointer user_data) {
 VGUI.lastx = x;
 VGUI.lasty = y;
 //printf("motion_notify_event: %d %d %d\n", VGUI.lastbutton, VGUI.lastx, VGUI.lasty);
}
static Bool
key_press_event(GtkEventControllerKey* self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
 char keysym='\0';
 /* Here, keyval is independent of whether CTRL is held.
  * Enforce the switch below if this is the case. */
 if (keyval<256 && (state&GDK_CONTROL_MASK)==0) {
  //printf("Key pressed: >%c<\n", keyval);
  keysym=(char)keyval;
 } else {
  //printf("Special key: value %d, state %d->shift %d\n", keyval, state, state&GDK_SHIFT_MASK);
  switch(keyval) {
   case GDK_KEY_l:
    keysym=''; /* Ctrl-L */
    break;
   case GDK_KEY_Return:
   case GDK_KEY_KP_Enter:
    keysym=RETKEY;
    break;
   case GDK_KEY_BackSpace:
    keysym=BACKSPACEKEY;
    break;
   case GDK_KEY_Home:
    keysym='c';
    break;
   case GDK_KEY_Left:
    if (state&GDK_CONTROL_MASK) {
     keysym=((state&GDK_SHIFT_MASK) ? 'H' : 'h');
    } else {
     keysym='I';
    }
    break;
   case GDK_KEY_Right:
    if (state&GDK_CONTROL_MASK) {
     keysym=((state&GDK_SHIFT_MASK) ? 'L' : 'l');
    } else {
     keysym='i';
    }
    break;
   case GDK_KEY_Down:
    if (state&GDK_CONTROL_MASK) {
     keysym=((state&GDK_SHIFT_MASK) ? 'J' : 'j');
    } else {
     keysym=')';
    }
    break;
   case GDK_KEY_Up:
    if (state&GDK_CONTROL_MASK) {
     keysym=((state&GDK_SHIFT_MASK) ? 'K' : 'k');
    } else {
     keysym='(';
    }
    break;
   case GDK_KEY_Page_Up:
    keysym='(';
    break;
   case GDK_KEY_Page_Down:
    keysym=')';
    break;
   case GDK_KEY_KP_Left:
    keysym='t';
    break;
   case GDK_KEY_KP_Right:
    keysym='T';
    break;
   case GDK_KEY_KP_Up:
    keysym='+';
    break;
   case GDK_KEY_KP_Down:
    keysym='-';
    break;
   case GDK_KEY_KP_Home:
    keysym='[';
    break;
   case GDK_KEY_KP_End:
    keysym=']';
    break;
   case GDK_KEY_KP_Add:
    keysym='i';
    break;
   case GDK_KEY_KP_Subtract:
    keysym='I';
    break;
   case GDK_KEY_KP_Multiply:
    keysym='m';
    break;
   case GDK_KEY_KP_Page_Up:
    keysym='}';
    break;
   case GDK_KEY_KP_Page_Down:
    keysym='{';
    break;
   default:
    break;
  }
 }
 if (keysym!='\0') Keyboard_Buffer_push(keysym);
 return TRUE;
}
/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static void
draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
 //printf("canvas_draw_event %ld %ld %ld\n", widget, cr, user_data);
 cairo_set_source_surface (cr, VGUI.surface, 0, 0);
 cairo_paint (cr);
}
static Bool
canvas_configure_event(GtkWidget *widget, void *event) {
 //printf("canvas_configure_event\n");
 // Note that in this event, the widget (VGUI.canvas) size is still 0!
 // Have to do all relevant work in canvas_resize_event...
 return TRUE;
}
static Bool
canvas_resize_event(GtkWidget *widget, void *event) {
 cairo_text_extents_t te;
 //printf("canvas_resize_event\n");
 vdevice.sizeX = 1;
 vdevice.sizeY = 1;
 vdevice.minVx = vdevice.minVy = 0;
 vdevice.maxVx = vdevice.sizeSx = gtk_widget_get_width(VGUI.canvas);
 vdevice.maxVy = vdevice.sizeSy = gtk_widget_get_height(VGUI.canvas);
 vdevice.depth = 3;
 if (VGUI.cr!=NULL) {
  cairo_destroy (VGUI.cr);
  cairo_surface_destroy (VGUI.surface);
 }
 //if (gtk_native_get_surface (gtk_widget_get_native (widget)))
 VGUI.surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, vdevice.sizeSx, vdevice.sizeSy);
 VGUI.cr=cairo_create(VGUI.surface);
 cairo_set_antialias(VGUI.cr,VGUI.antialias);
 my_set_font(VGUI.cr);
 cairo_text_extents (VGUI.cr, "A", &te);
 vdevice.hheight = te.height;
 vdevice.hwidth = te.x_advance;
 VGUI.line_width=1;
 //printf("vdevice address=%ld, vdevice.sizeSx=%d, vdevice.sizeSy=%d\n", (long)&vdevice,  vdevice.sizeSx, vdevice.sizeSy);
 Keyboard_Buffer_push('');	// queue a redraw command
 gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(VGUI.canvas), draw_function, NULL, NULL);
 return 1;
}
LOCAL void
window_destroy_event(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 /* We don't want the default handler to be executed: */
 g_signal_stop_emission_by_name(G_OBJECT(VGUI.window), "destroy");
 Keyboard_Buffer_push('X');	/* Queue an exit command */
}
static void
toggle_antialias(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 bool const newstate=!g_variant_get_boolean(g_action_get_state(G_ACTION(menuitem)));
 VGUI.antialias=newstate ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE;
 cairo_set_antialias(VGUI.cr,VGUI.antialias);
 g_action_change_state(G_ACTION(menuitem), g_variant_new_boolean(newstate));
 Keyboard_Buffer_push('');	// queue a redraw command
}
static void
toggle_postscriptcolor(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 bool const newstate=!g_variant_get_boolean(g_action_get_state(G_ACTION(menuitem)));
 static char *envbuffer=NULL;
 if (envbuffer==NULL) {
  /* This assumes that the name of POSTSCRIPT_COLORDEV is longer than POSTSCRIPT_BWDEV */
  envbuffer=malloc((strlen(POSTSCRIPT_ENVNAME)+strlen(POSTSCRIPT_BWDEV)+2)*sizeof(char));
  if (envbuffer==NULL) {
   fprintf(stderr, "vogl_vgui_driver: Error allocating envbuffer!\n");
   exit(1);
  }
 }
 sprintf(envbuffer, "%s=%s", POSTSCRIPT_ENVNAME, newstate ? POSTSCRIPT_COLORDEV : POSTSCRIPT_BWDEV);
 putenv(envbuffer);
 g_action_change_state(G_ACTION(menuitem), g_variant_new_boolean(newstate));
}
static void
pos_modify_mode(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Keyboard_Buffer_pushstring("=change\n_");
}
static void
posplot_about(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Notice(
  "posplot/VOGL GUI driver\n"
  "(c) 1998-2007,2011-2014,2025 by Bernd Feige, Bernd.Feige@gmx.net\n"
  "Using the free GTK library: See http://www.gtk.org/"
 );
}
LOCAL void
process_menu_key(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 const gchar * name=g_action_get_name(G_ACTION(menuitem));
 if (strlen(name)==5) {
  //printf("process_menu_key: %c\n", name[4]);
  Keyboard_Buffer_push(name[4]);
 }
}
static void
menu_key_Leftparen(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Keyboard_Buffer_push('(');
}
static void
menu_key_Rightparen(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Keyboard_Buffer_push(')');
}
static void
menu_key_ENTER(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Keyboard_Buffer_push('\n');
}
static void
menu_key_BACKSPACE(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Keyboard_Buffer_push(BACKSPACEKEY);
}
static void
menu_key_CTRL_L(GSimpleAction *menuitem, GVariant *gv, gpointer data) {
 Keyboard_Buffer_push('');
}
static void
set_palette_entry(int n, gdouble r, gdouble g, gdouble b) {
 VGUI.palette[n].red=r;
 VGUI.palette[n].green=g;
 VGUI.palette[n].blue=b;
 VGUI.palette[n].alpha=1.0;
}
static int
VGUI_init(void) {
 GtkWidget *box1;
 GMenu *menubar, *submenu, *submenu2;
 GSimpleActionGroup *action_group=g_simple_action_group_new();
 GtkEventController *ec_key, *ec_motion;
 GtkGesture *ec_gesture;
 GVariant *variant;
 GSimpleAction *g_action;

 //printf("VGUI_init\n");

 Keyboard_Buffer_init();

 VGUI.window = gtk_window_new();
 gtk_window_set_title (GTK_WINDOW (VGUI.window), "posplot");
 if (VGUI.width==0) {
  /* This means that we're executed for the first time */
  gtk_window_set_default_size(GTK_WINDOW(VGUI.window), FRAME_SIZE_X, FRAME_SIZE_Y);
 } else {
  gtk_window_set_default_size(GTK_WINDOW(VGUI.window), VGUI.width, VGUI.height);
 }
 g_signal_connect(G_OBJECT (VGUI.window), "destroy", G_CALLBACK(window_destroy_event), NULL);

 box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
 gtk_window_set_child(GTK_WINDOW(VGUI.window), box1);

 menubar = g_menu_new();

 /* Dataset menu */
 submenu=g_menu_new();

 g_menu_append(submenu,"quit, accept epoch (q)","app.key_q");
 g_menu_append(submenu,"Quit, reject epoch (Q)","app.key_Q");
 g_menu_append(submenu,"Quit, reject epoch+stop iterated queue (V)","app.key_V");
 g_menu_append(submenu,"Stop (X)","app.key_X");

 /* Separator: */
 submenu2=g_menu_new();

 g_menu_append(submenu2,"About","app.about");
 //g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(posplot_about), NULL);

 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));
 g_menu_append_submenu(menubar,"Quitting",G_MENU_MODEL(submenu));

 /* Command menu */
 submenu=g_menu_new();

 g_menu_append(submenu,"Antialias screen plot lines","app.antialias");
 /* Antialiasing doesn't look nice with lines */
 VGUI.antialias=CAIRO_ANTIALIAS_NONE;

 /* Separator: */
 submenu2=g_menu_new();

 g_menu_append(submenu2,"Start entering an argument (=)","app.key_=");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();

 g_menu_append(submenu2,"Insert trigger (N)","app.key_N");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Record interactions (r)","app.key_r");
 g_menu_append(submenu2,"Replay interactions (R)","app.key_R");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Landscape postscript dump (o)","app.key_o");
 g_menu_append(submenu2,"Portrait postscript dump (O)","app.key_O");
 g_menu_append(submenu2,"Color postscript","app.color_ps");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

#if 0
 { char * const post_dev=getenv(POSTSCRIPT_ENVNAME);
 /* posplot's default is color Postscript */
 if (post_dev!=NULL && strcmp(post_dev, POSTSCRIPT_BWDEV)==0) gtk_widget_activate(menuitem);
 }
#endif

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Dump current point as ARRAY_ASCII (y)","app.key_y");
 g_menu_append(submenu2,"Dump current point full (Y)","app.key_Y");
 g_menu_append(submenu2,"Write datasets (W)","app.key_W");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Grid channels (G)","app.key_G");
 g_menu_append(submenu2,"Enter pos modify mode (=change<RET>__)","app.pos_modify");
 g_menu_append(submenu2,"Leave/switch pos mode (_)","app.key__");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Zero current point ($)","app.key_$");
 g_menu_append(submenu2,"Detrend datasets (D)","app.key_D");
 g_menu_append(submenu2,"Differentiate datasets (%)","app.key_%");
 g_menu_append(submenu2,"Integrate datasets (&)","app.key_&");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 g_menu_append_submenu(menubar,"Commands",G_MENU_MODEL(submenu));

 /* Show menu */
 submenu=g_menu_new();

 g_menu_append(submenu,"Plots (p)","app.key_p");
 g_menu_append(submenu,"Axes (x)","app.key_x");
 g_menu_append(submenu,"Names (n)","app.key_n");
 g_menu_append(submenu,"Marker (m)","app.key_m");
 g_menu_append(submenu,"Triggers (M)","app.key_M");
 g_menu_append(submenu,"Grayscale (d)","app.key_d");
 g_menu_append(submenu,"Info (?)","app.key_?");
 g_menu_append(submenu,"Triangulation (z)","app.key_z");
 g_menu_append_submenu(menubar,"Show",G_MENU_MODEL(submenu));

 /* Select menu */
 submenu=g_menu_new();

 g_menu_append(submenu,"Point by argument (.)","app.key_.");
 g_menu_append(submenu,"Next point (i)","app.key_i");
 g_menu_append(submenu,"Previous point (I)","app.key_I");
 g_menu_append(submenu,"Left interval boundary ([)","app.key_[");
 g_menu_append(submenu,"Right interval boundary (])","app.key_]");

 /* Separator: */
 submenu2=g_menu_new();

 g_menu_append(submenu2,"Dataset by argument (,)","app.key_,");
 g_menu_append(submenu2,"Toggle datasets (*)","app.key_*");
 g_menu_append(submenu2,"Datasets up '('","app.key_Leftparen");
 g_menu_append(submenu2,"Datasets down ')'","app.key_Rightparen");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Next item (})","app.key_}");
 g_menu_append(submenu2,"Previous item ({)","app.key_{");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"One channel (ENTER)","app.key_ENTER");
 g_menu_append(submenu2,"Hide channel (BACKSPACE)","app.key_BACKSPACE");
 g_menu_append(submenu2,"All channels (C)","app.key_C");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 g_menu_append_submenu(menubar,"Select",G_MENU_MODEL(submenu));

 /* Transform menu */
 submenu=g_menu_new();

 g_menu_append(submenu,"Exponential (e)","app.key_e");
 g_menu_append(submenu,"Logarithm (E)","app.key_E");

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Single item mode (')","app.key_'");
 g_menu_append(submenu2,"Absolute values (a)","app.key_a");
 g_menu_append(submenu2,"Power values (P)","app.key_P");
 g_menu_append(submenu2,"Phase values (A)","app.key_A");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 g_menu_append_submenu(menubar,"Transform",G_MENU_MODEL(submenu));

 /* View menu */
 submenu=g_menu_new();

 g_menu_append(submenu,"Redraw (^L)","app.key_CTRL_L");
 g_menu_append(submenu,"Vertical scale lock (v)","app.key_v");
 g_menu_append(submenu,"Set/Toggle subsampling (@)","app.key_@");
 g_menu_append(submenu,"Toggle black/white background (b)","app.key_b");
 g_menu_append(submenu,"Line style mode (u)","app.key_u");
 g_menu_append(submenu,"Set first line style (U)","app.key_U");
 g_menu_append(submenu,"Swap plus/minus (~)","app.key_~");
 g_menu_append(submenu,"Swap x-z (S)","app.key_S");

 /* Separator: */
 submenu2=g_menu_new();
 g_menu_append(submenu2,"Position mode (_)","app.key__");
 g_menu_append(submenu2,"Center channel (Z)","app.key_Z");
 g_menu_append(submenu2,"Nearer (+)","app.key_+");
 g_menu_append(submenu2,"Farther (-)","app.key_-");
 g_menu_append(submenu2,"Turn l (h)","app.key_h");
 g_menu_append(submenu2,"Turn r (l)","app.key_l");
 g_menu_append(submenu2,"Turn u (k)","app.key_k");
 g_menu_append(submenu2,"Turn d (j)","app.key_j");
 g_menu_append(submenu2,"Twist l (t)","app.key_t");
 g_menu_append(submenu2,"Twist r (T)","app.key_T");
 g_menu_append(submenu2,"Enlarge FOV (/)","app.key_/");
 g_menu_append(submenu2,"Decrease FOV (\\)","app.key_\\");
 g_menu_append(submenu2,"Enlarge plots (>)","app.key_>");
 g_menu_append(submenu2,"Shrink plots (<)","app.key_<");
 g_menu_append(submenu2,"Reset parameters (c)","app.key_c");
 g_menu_append_section(submenu,NULL,G_MENU_MODEL(submenu2));

 g_menu_append_submenu(menubar,"View",G_MENU_MODEL(submenu));
 GtkWidget *popover=gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
 gtk_box_append(GTK_BOX(box1), popover);

 const GActionEntry entries[] = {
  {"key_q", process_menu_key },
  {"key_Q", process_menu_key },
  {"key_V", process_menu_key },
  {"key_X", process_menu_key },
  {"about", posplot_about },
  {"key_=", process_menu_key },
  {"key_N", process_menu_key },
  {"key_r", process_menu_key },
  {"key_R", process_menu_key },
  {"key_o", process_menu_key },
  {"key_O", process_menu_key },
  {"key_y", process_menu_key },
  {"key_Y", process_menu_key },
  {"key_W", process_menu_key },
  {"key_G", process_menu_key },
  {"pos_modify", pos_modify_mode },
  {"key__", process_menu_key },
  {"key_$", process_menu_key },
  {"key_D", process_menu_key },
  {"key_%", process_menu_key },
  {"key_&", process_menu_key },
  {"key_p", process_menu_key },
  {"key_x", process_menu_key },
  {"key_n", process_menu_key },
  {"key_m", process_menu_key },
  {"key_M", process_menu_key },
  {"key_d", process_menu_key },
  {"key_?", process_menu_key },
  {"key_z", process_menu_key },
  {"key_.", process_menu_key },
  {"key_i", process_menu_key },
  {"key_I", process_menu_key },
  {"key_[", process_menu_key },
  {"key_]", process_menu_key },
  {"key_,", process_menu_key },
  {"key_*", process_menu_key },
  {"key_Leftparen", menu_key_Leftparen },
  {"key_Rightparen", menu_key_Rightparen },
  {"key_}", process_menu_key },
  {"key_{", process_menu_key },
  {"key_ENTER", menu_key_ENTER },
  {"key_BACKSPACE", menu_key_BACKSPACE },
  {"key_C", process_menu_key },
  {"key_e", process_menu_key },
  {"key_E", process_menu_key },
  {"key_'", process_menu_key },
  {"key_a", process_menu_key },
  {"key_P", process_menu_key },
  {"key_A", process_menu_key },
  {"key_CTRL_L", menu_key_CTRL_L },
  {"key_v", process_menu_key },
  {"key_@", process_menu_key },
  {"key_b", process_menu_key },
  {"key_u", process_menu_key },
  {"key_U", process_menu_key },
  {"key_~", process_menu_key },
  {"key_S", process_menu_key },
  {"key__", process_menu_key },
  {"key_Z", process_menu_key },
  {"key_+", process_menu_key },
  {"key_-", process_menu_key },
  {"key_h", process_menu_key },
  {"key_l", process_menu_key },
  {"key_k", process_menu_key },
  {"key_j", process_menu_key },
  {"key_t", process_menu_key },
  {"key_T", process_menu_key },
  {"key_/", process_menu_key },
  {"key_\\", process_menu_key },
  {"key_>", process_menu_key },
  {"key_<", process_menu_key },
  {"key_c", process_menu_key },
 };
 g_action_map_add_action_entries(G_ACTION_MAP(action_group), entries, G_N_ELEMENTS(entries), NULL);

 variant = g_variant_new_boolean(FALSE);
 g_action = g_simple_action_new_stateful("antialias", NULL, variant);
 g_signal_connect(G_OBJECT(g_action), "activate", G_CALLBACK(toggle_antialias), NULL);
 g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(g_action));
 variant = g_variant_new_boolean(TRUE);
 g_action = g_simple_action_new_stateful("color_ps", NULL, variant);
 g_signal_connect(G_OBJECT(g_action), "activate", G_CALLBACK(toggle_postscriptcolor), NULL);
 g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(g_action));
 gtk_widget_insert_action_group(GTK_WIDGET(VGUI.window), "app", G_ACTION_GROUP(action_group));

 VGUI.cr = NULL; /* Allocated by canvas_configure_event */
 VGUI.surface = NULL;
 VGUI.canvas = gtk_drawing_area_new ();
 gtk_widget_set_name (VGUI.canvas, "posplot");
 gtk_widget_set_hexpand(GTK_WIDGET(VGUI.canvas),TRUE);
 gtk_widget_set_vexpand(GTK_WIDGET(VGUI.canvas),TRUE);

 gtk_box_append(GTK_BOX(box1), VGUI.canvas);
 /* Otherwise, a DrawingArea cannot receive key presses: */
 gtk_widget_set_can_focus(VGUI.canvas, TRUE);
 g_signal_connect (G_OBJECT (VGUI.canvas), "realize", G_CALLBACK (canvas_configure_event), NULL);
 g_signal_connect (G_OBJECT (VGUI.canvas), "resize", G_CALLBACK (canvas_resize_event), NULL);
 ec_key=gtk_event_controller_key_new();
 g_signal_connect (G_OBJECT (ec_key), "key-pressed", G_CALLBACK (key_press_event), NULL);
 // Only works adding to window, not canvas...
 gtk_widget_add_controller (VGUI.window, GTK_EVENT_CONTROLLER(ec_key));

 ec_motion=gtk_event_controller_motion_new();
 g_signal_connect(G_OBJECT(ec_motion), "motion", G_CALLBACK (motion_notify_event), NULL);
 gtk_widget_add_controller (VGUI.canvas, ec_motion);

 ec_gesture=gtk_gesture_click_new();
 gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ec_gesture),0); /* Listen to all buttons */
 g_signal_connect(G_OBJECT(ec_gesture), "pressed", G_CALLBACK (button_press_event), NULL);
 gtk_widget_add_controller (VGUI.canvas, GTK_EVENT_CONTROLLER(ec_gesture));

#ifdef WIN32
 /* Under Windows, if the script window starts iconified, the new window
  * of the application will start iconified as well. "deiconify" doesn't work,
  * "present" deiconifies but places behind other windows. */
 //gtk_window_set_keep_above (GTK_WINDOW(VGUI.window), TRUE);
#endif

 VGUI.lastbutton=0;
 VGUI.in_frontbuffer=FALSE;

 /* Set the colors */
 VGUI.fg = BLACK;
 set_palette_entry(BLACK, 0.0, 0.0, 0.0);
 set_palette_entry(RED, 1.0, 0.0, 0.0);
 set_palette_entry(GREEN, 0.0, 1.0, 0.0);
 set_palette_entry(YELLOW, 1.0, 1.0, 0.0);
 set_palette_entry(BLUE, 0.0, 0.0, 1.0);
 set_palette_entry(MAGENTA, 1.0, 0.0, 1.0);
 set_palette_entry(CYAN, 0.0, 1.0, 1.0);
 set_palette_entry(WHITE, 1.0, 1.0, 1.0);

 gtk_window_present(GTK_WINDOW(VGUI.window));
 //printf("VGUI_init end\n");
 do {
  g_main_context_iteration(NULL,TRUE);
 } while (g_main_context_pending(NULL));

 return (1);
}

/*
 * VGUI_frontbuffer, VGUI_backbuffer, VGUI_swapbuffers
 * 
 */
static void
update_canvas(void) {
 /* Schedule a redraw for the whole area */
 gtk_widget_queue_draw(GTK_WIDGET(VGUI.canvas));
}

static int 
VGUI_frontbuffer(void) {
 //printf("VGUI_frontbuffer\n");
 VGUI.in_frontbuffer=TRUE;
 return (0);
}

static int 
VGUI_backbuffer(void) {
 //printf("VGUI_backbuffer\n");
 VGUI.in_frontbuffer=FALSE;
 return (0);
}

static int 
VGUI_swapbuffers(void) {
 //printf("VGUI_swapbuffers\n");
 update_canvas();
 return (0);
}

/*
 * VGUI_vclear
 * 
 * Clear the viewport to current colour
 */
static int
VGUI_vclear(void) {
 int const vw = vdevice.maxVx - vdevice.minVx + 1;
 int const vh = vdevice.maxVy - vdevice.minVy + 1;
 //printf("VGUI_vclear Clear %d %d - %d %d\n", vdevice.minVx, vdevice.sizeSy-vdevice.maxVy-1, vw, vh);
 gdk_cairo_set_source_rgba (VGUI.cr, &VGUI.palette[VGUI.fg]);
 cairo_rectangle(VGUI.cr, vdevice.minVx, vdevice.sizeSy-vdevice.maxVy-1, vw, vh);
 cairo_fill(VGUI.cr);
 return (1);
}


/*
 * VGUI_exit
 * 
 */
static int
VGUI_exit(void) {
 //printf("VGUI_exit\n");

 if (VGUI.cr!=NULL) {
  cairo_destroy (VGUI.cr);
  cairo_surface_destroy (VGUI.surface);
  VGUI.cr=NULL;
 }
 gtk_window_destroy (GTK_WINDOW(VGUI.window));
 return (1);
}

static int 
VGUI_begin(void) {
 //printf("VGUI_begin\n");
 if (vdevice.bgnmode == VLINE) {
  /* Dash transfer was shamelessly copied from the VOGL X11.c driver... */
  //65535=solid
  if (VGUI.line_style != 0xffff) {
   double dashes[16];
   int     i, n, a, b;

   for (i = 0; i < 16; i++) dashes[i] = 0;

   for (i = 0; i < 16; i++)        /* Over 16 bits */
    if ((VGUI.line_style & (1 << i))) break;

#define ON  1
#define OFF 0

   a = b = OFF;
   if (VGUI.line_style & (1 << 0)) a = b = ON;

   n = 0;
   for (i = 0; i < 16; i++) {      /* Over 16 bits */
    if (VGUI.line_style & (1 << i)) a = ON;
    else               a = OFF;

    if (a != b) {
     b = a;
     n++;
    }
    dashes[n]++;
   }
   n++;

#undef ON
#undef OFF
   cairo_set_dash (VGUI.cr,dashes,n,0);
  } else {
   /* Solid lines are the default anyway */
   cairo_set_dash (VGUI.cr,NULL,0,0);
  }

  gdk_cairo_set_source_rgba(VGUI.cr,&VGUI.palette[VGUI.fg]);
  cairo_set_line_width(VGUI.cr,VGUI.line_width);
  VGUI.draw_lastx=VGUI.draw_lasty= -1;
 }
 return (1);
};

static int 
VGUI_sync(void) {
 //printf("VGUI_sync\n");
 if (vdevice.bgnmode == VLINE) {
  cairo_stroke (VGUI.cr);
 }
 if (VGUI.in_frontbuffer) update_canvas();
 return (1);
};

/*
 * VGUI_font : load either of the fonts
 */
static int VGUI_font(char *name)
{
 //printf("VGUI_font\n");

 return (1);
}

static int VGUI_char(char c)
{
 char const s[2]={c, '\0'};
 //printf("VGUI_char %c %d %d\n", c, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);

 gdk_cairo_set_source_rgba(VGUI.cr,&VGUI.palette[VGUI.fg]);
 my_set_font(VGUI.cr);
 cairo_move_to (VGUI.cr, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);
 cairo_show_text(VGUI.cr, s);

 return (1);
};

static int VGUI_string(const char *s)
{
 //printf("VGUI_string %s %d %g (vdevice.sizeSy=%d, vdevice.cpVy=%d, vdevice.hheight=%g)\n", s, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy - vdevice.hheight, vdevice.sizeSy, vdevice.cpVy, vdevice.hheight);

 gdk_cairo_set_source_rgba(VGUI.cr,&VGUI.palette[VGUI.fg]);
 my_set_font(VGUI.cr);
 cairo_move_to (VGUI.cr, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);
 cairo_show_text(VGUI.cr, s);

 return (1);
}


static int VGUI_draw(int x, int y)
{
 //printf("VGUI_draw %d %d\n", x, y);

 if (vdevice.cpVx!=VGUI.draw_lastx || vdevice.cpVy!=VGUI.draw_lasty) {
  cairo_move_to (VGUI.cr, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);
 }
 cairo_line_to (VGUI.cr, x, vdevice.sizeSy - y);

 VGUI.draw_lastx = vdevice.cpVx = x;
 VGUI.draw_lasty = vdevice.cpVy = y;

 return (0);
}

static int
VGUI_colour(int i)
{
 if (i < MAXCOLOR) {
  VGUI.fg=i;
  //printf("VGUI_colour set to %d %d %d\n", VGUI.palette[i].red, VGUI.palette[i].green, VGUI.palette[i].blue);
 } else {
  VGUI.fg=BLACK;
  //printf("VGUI_colour %d: Setting to BLACK\n", i);
 }
 return (0);
};

/*
 * VGUI_mapcolor
 * 
 * change index i in the color map to the appropriate r, g, b, value.
 */
static int VGUI_mapcolor(int c, int r, int g, int b)
{
 //printf("VGUI_mapcolor c=%d (%d %d %d)\n", c, r,g,b);
 if (c >= MAXCOLOR || vdevice.depth == 1) return (-1);
 set_palette_entry(c, (gdouble)(r/255.0), (gdouble)(g/255.0), (gdouble)(b/255.0));

 return 1;
}

static int VGUI_fill(int sides, int *x, int *y)
{
 //printf("VGUI_fill %d\n", sides);
 return (0);
};

static int need_empty_loops=0;
static int
VGUI_checkkey(void) {
 //printf("VGUI_checkkey\n");
 if (need_empty_loops>0) {
  need_empty_loops--;
  return '\0';
 }
 if (Keyboard_Buffer_keys_in_buffer()==0) {
  do {
   g_main_context_iteration(NULL,TRUE);
  } while (g_main_context_pending(NULL));
 }
 if (Keyboard_Buffer_keys_in_buffer()>0) need_empty_loops+=5;
 return Keyboard_Buffer_pop();
}

static int VGUI_locator(int *x, int *y)
{
 int const button=VGUI.lastbutton;
 VGUI.lastbutton=0;

 /* Note: The VOGL logic waits until the button is RELEASED, ie until
  * button==0, and takes *these* coordinates. If you only set the return values 
  * if button!=0, then no button press will ever be seen by posplot... */
 *x=VGUI.lastx;
 *y=vdevice.sizeSy-VGUI.lasty;

 if (button!=0) {
  need_empty_loops+=2;
 }

 //printf("VGUI_locator: %d %d %d\n", button, *x, *y);

 return button;
}

static int VGUI_lwidth(int w)
{
 //printf("VGUI_lwidth %d\n", w);

 VGUI.line_width=w;
 return 0;
}

static int VGUI_lstyle(int s)
{
 VGUI.line_style=s;
 return 0;
}

static DevEntry VGUIdev = {
 "VGUI",
 "large",  /* Large font */
 "small",  /* Small font */
 VGUI_backbuffer,  /* backbuffer */
 VGUI_char,  /* hardware char */
 VGUI_checkkey,  /* keyhit */
 VGUI_vclear,  /* clear viewport to current colour */
 VGUI_colour,  /* set current colour */
 VGUI_draw,  /* draw line */
 VGUI_exit,  /* close graphics & exit */
 VGUI_fill,  /* fill polygon */
 VGUI_font,  /* set hardware font */
 VGUI_frontbuffer, /* front buffer */
 NULL,   /* wait for and get key */
 VGUI_init,  /* begin graphics */
 VGUI_locator,  /* get mouse position */
 VGUI_mapcolor,  /* map colour (set indices) */
 VGUI_lstyle,  /* set linestyle */
 VGUI_lwidth,  /* set line width */
 VGUI_string,  /* draw string of chars */
 VGUI_swapbuffers, /* swap buffers */
 VGUI_begin, /* begin recording */
 VGUI_sync  /* sync display */
};

/*
 * _VGUI_devcpy
 * 
 * copy the pc device into vdevice.dev. (as listed in drivers.c)
 */
void
_VGUI_devcpy(void) {
 vdevice.dev = VGUIdev;
}
