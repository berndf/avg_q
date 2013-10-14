/*
 * Copyright (C) 1998-2002,2004,2006-2009,2011-2012 Bernd Feige
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
 * GTK driver for VOGL (c) 1998-2002,2004,2006-2009,2011-2012 by Bernd Feige (Feige@ukl.uni-freiburg.de)
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

#ifdef USE_THREADING
static GCond *block_thread=NULL;
static GStaticMutex block_thread_mutex=G_STATIC_MUTEX_INIT;
#else
#define gdk_threads_enter()
#define gdk_threads_leave()
#endif

/* Need some compatibility definitions */
#if GTK_MAJOR_VERSION==2
#define gtk_box_new(orientation,spacing) gtk_vbox_new(FALSE,spacing)
#endif

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
#if GTK_MAJOR_VERSION==2
#else
 cairo_region_t *crect;
#endif
 int fg;
 GdkRGBA palette[MAXCOLOR];
 cairo_t *cr;
 cairo_antialias_t antialias;
 Bool backbuffer;
 Bool paint_started;
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
 gtk_widget_destroy(GTK_WIDGET(mwindow));
 Notice_window=NULL;
}
LOCAL void
Notice_window_close_button(GtkButton *button, GtkWidget *mwindow) {
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

 Notice_window=gtk_dialog_new();
 g_signal_connect (G_OBJECT (Notice_window), "destroy", G_CALLBACK(Notice_window_close), NULL);
 gtk_window_set_title (GTK_WINDOW (Notice_window), "Notice");

 box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
 gtk_box_pack_start (GTK_BOX (gtk_dialog_get_action_area(GTK_DIALOG(Notice_window))), box1, FALSE, FALSE, 0);
 gtk_widget_show (box1);

 label = gtk_label_new (message);
 gtk_box_pack_start (GTK_BOX(box1), label, FALSE, FALSE, 0);
 gtk_widget_show (label);

 button = gtk_button_new_with_label ("Okay");
 g_signal_connect_object (G_OBJECT (button), "clicked", G_CALLBACK(Notice_window_close_button), G_OBJECT (Notice_window), G_CONNECT_AFTER);
 gtk_widget_set_can_default (button, TRUE);
 gtk_box_pack_start (GTK_BOX(box1), button, FALSE, FALSE, 0);
 gtk_widget_grab_default (button);
 gtk_widget_show (button);

 gtk_widget_show (Notice_window);
}
#ifdef USE_THREADING
static void
notify_input(void) {
 /* This is called whenever we need to check our input */
 g_static_mutex_lock(&block_thread_mutex);
 g_cond_broadcast(block_thread);
 g_static_mutex_unlock(&block_thread_mutex);
}
#else
#define notify_input()
#endif
static Bool
button_press_event(GtkWidget *widget, GdkEventButton *event) {
 if (event->button>0) {
  VGUI.lastbutton=(1<<(event->button-1));
 }
 notify_input();
 //printf("Button: %d\n", VGUI.lastbutton);
 return TRUE;
}
static Bool
motion_notify_event(GtkWidget *widget, GdkEventMotion *event) {
 GdkModifierType state;

 if (event->is_hint) {
  gdk_window_get_device_position (event->window, event->device, &VGUI.lastx, &VGUI.lasty, &state);
 } else {
  VGUI.lastx = event->x;
  VGUI.lasty = event->y;
  state = event->state;
 }

 notify_input();
 //printf("motion_notify_event: %d %d %d\n", VGUI.lastbutton, VGUI.lastx, VGUI.lasty);

 return TRUE;
}
static Bool
key_press_event(GtkWidget *widget, GdkEventKey *event) {
 if (event->length==1) {
  //printf("Key pressed: >%s<\n", event->string);
  Keyboard_Buffer_push(*event->string);
 } else {
  char keysym='\0';
  //printf("Key without string: value %d, state %d->shift %d\n", event->keyval, event->state, (event->state&GDK_SHIFT_MASK));
  switch(event->keyval) {
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
    if (event->state&GDK_CONTROL_MASK) {
     keysym=((event->state&GDK_SHIFT_MASK) ? 'H' : 'h');
    } else {
     keysym='I';
    }
    break;
   case GDK_KEY_Right:
    if (event->state&GDK_CONTROL_MASK) {
     keysym=((event->state&GDK_SHIFT_MASK) ? 'L' : 'l');
    } else {
     keysym='i';
    }
    break;
   case GDK_KEY_Down:
    if (event->state&GDK_CONTROL_MASK) {
     keysym=((event->state&GDK_SHIFT_MASK) ? 'J' : 'j');
    } else {
     keysym=')';
    }
    break;
   case GDK_KEY_Up:
    if (event->state&GDK_CONTROL_MASK) {
     keysym=((event->state&GDK_SHIFT_MASK) ? 'K' : 'k');
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
  if (keysym!='\0') Keyboard_Buffer_push(keysym);
 }
 notify_input();
 return TRUE;
}
static Bool
window_configure_event(GtkWidget *widget, GdkEventConfigure *event) {
 //printf("window_configure_event: %d %d %d %d\n", event->x, event->y, event->width, event->height);
 /* Store position and size so we can open the next window with the same params */
 VGUI.left=event->x;
 VGUI.top=event->y;
 VGUI.width=event->width;
 VGUI.height=event->height;
 /* If we return TRUE here, in GTK-2.0 the sub-widgets won't be resized!! */ 
 return FALSE;
}
static Bool
canvas_configure_event(GtkWidget *widget, GdkEventConfigure *event) {
 GtkAllocation allocation;
 //printf("canvas_configure_event\n");
 gtk_widget_get_allocation(widget,&allocation);

 vdevice.sizeSx = vdevice.sizeX = allocation.width;
 vdevice.sizeSy = vdevice.sizeY = allocation.height;
 vdevice.depth = 3;
 //printf("vdevice address=%ld, vdevice.sizeSx=%d, vdevice.sizeSy=%d\n", (long)&vdevice,  vdevice.sizeSx, vdevice.sizeSy);
 if (VGUI.paint_started) {
#if GTK_MAJOR_VERSION==2
  GdkRectangle const crect={0, 0, vdevice.sizeSx, vdevice.sizeSy};
  gdk_window_end_paint(gtk_widget_get_window(VGUI.canvas));
  gdk_window_begin_paint_region(gtk_widget_get_window(VGUI.canvas),gdk_region_rectangle(&crect));
#else
  cairo_rectangle_int_t const crect={0, 0, vdevice.sizeSx, vdevice.sizeSy};
  gdk_window_end_paint(gtk_widget_get_window(VGUI.canvas));
  if (VGUI.crect!=NULL) {
   cairo_region_destroy(VGUI.crect);
  }
  VGUI.crect=cairo_region_create_rectangle(&crect);
  gdk_window_begin_paint_region(gtk_widget_get_window(VGUI.canvas),VGUI.crect);
#endif
  VGUI.paint_started=TRUE;
 }

 Keyboard_Buffer_push('');	// queue a redraw command
 notify_input();
 return TRUE;
}
static Bool
canvas_expose_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
 return canvas_configure_event(widget,NULL);
}
static Bool
window_destroy_event(GtkWidget *windowp, gpointer data, GtkWidget *widget) {
 /* We don't want the default handler to be executed: */
 g_signal_stop_emission_by_name(G_OBJECT(widget), "delete_event");
 Keyboard_Buffer_push('X');	/* Queue an exit command */
 notify_input();
 return TRUE;
}
static void
toggle_backbuffer(GtkWidget *menuitem, gpointer data) {
 VGUI_frontbuffer(); /* Be sure to finish buffered drawing */
 VGUI.backbuffer=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
 Keyboard_Buffer_push('');	// queue a redraw command
 notify_input();
}
static void
toggle_antialias(GtkWidget *menuitem, gpointer data) {
 VGUI.antialias=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE;
 Keyboard_Buffer_push('');	// queue a redraw command
 notify_input();
}
static void
set_postscriptcolor(GtkWidget *menuitem, gpointer data) {
 Bool const usecolor=(Bool)data;
 static char *envbuffer=NULL;
 if (envbuffer==NULL) {
  /* This assumes that the name of POSTSCRIPT_BWDEV is longer than POSTSCRIPT_COLORDEV */
  envbuffer=malloc((strlen(POSTSCRIPT_ENVNAME)+strlen(POSTSCRIPT_BWDEV)+2)*sizeof(char));
  if (envbuffer==NULL) {
   fprintf(stderr, "vogl_vgui_driver: Error allocating envbuffer!\n");
   exit(1);
  }
 }
 sprintf(envbuffer, "%s=%s", POSTSCRIPT_ENVNAME, usecolor ? POSTSCRIPT_COLORDEV : POSTSCRIPT_BWDEV);
 putenv(envbuffer);
}
static void
posplot_about (GtkWidget *menuitem) {
 Notice(
  "posplot/VOGL GUI driver\n"
  "(c) 2002-2007,2011-2012 by Bernd Feige, Bernd.Feige@gmx.net\n"
  "Using the free GTK library: See http://www.gtk.org/"
 );
}
static void
insert_key(GtkWidget *menuitem, gpointer data) {
 char *string=(char *)data;
 while (*string!='\0') {
  if (*string=='\n') {
   Keyboard_Buffer_push(RETKEY);
   string++;
  } else {
   Keyboard_Buffer_push(*string++);
  }
 }
 notify_input();
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
 GtkWidget *menubar, *submenu, *menuitem;
 GtkWidget *topitem;
 GSList *colorgroup;
 GtkAllocation allocation;
 cairo_t *cr;
 cairo_text_extents_t te;

#ifdef USE_THREADING
 block_thread=g_cond_new();
#endif

 //printf("VGUI_init\n");
 gdk_threads_enter();

 Keyboard_Buffer_init();

 VGUI.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 gtk_window_set_wmclass (GTK_WINDOW (VGUI.window), "posplot", "avg_q");
 gtk_window_set_title (GTK_WINDOW (VGUI.window), "posplot");
 if (VGUI.width==0) {
  /* This means that we're executed for the first time */
  gtk_window_set_default_size(GTK_WINDOW(VGUI.window), FRAME_SIZE_X, FRAME_SIZE_Y);
 } else {
  gtk_window_set_default_size(GTK_WINDOW(VGUI.window), VGUI.width, VGUI.height);
 }
 g_signal_connect_object (G_OBJECT (VGUI.window), "delete_event", G_CALLBACK(window_destroy_event), G_OBJECT(VGUI.window), G_CONNECT_SWAPPED);
 g_signal_connect (G_OBJECT (VGUI.window), "configure_event", G_CALLBACK (window_configure_event), NULL);

 box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
 gtk_container_add (GTK_CONTAINER (VGUI.window), box1);
 gtk_widget_show (box1);

 menubar = gtk_menu_bar_new ();
 gtk_container_add (GTK_CONTAINER (box1), menubar);
 gtk_widget_show (menubar);

 /* Dataset menu */
 submenu=gtk_menu_new();

 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("quit, accept epoch (q)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"q");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Quit, reject epoch (Q)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"Q");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Quit, reject epoch+stop iterated queue (V)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"V");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Stop (X)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"X");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("About");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(posplot_about), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 topitem= gtk_menu_item_new_with_label("Quitting");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);
 /* Catch Keypress events also when the mouse points to the open menu: */
 g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_event), NULL);

 /* Command menu */
 submenu=gtk_menu_new();

 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_check_menu_item_new_with_label("Use screen backbuffer");
 gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
 g_signal_connect (G_OBJECT (menuitem), "toggled", G_CALLBACK(toggle_backbuffer), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_check_menu_item_new_with_label("Antialias screen plot lines");
 gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);
 g_signal_connect (G_OBJECT (menuitem), "toggled", G_CALLBACK(toggle_antialias), NULL);
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Start entering an argument (=)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"=");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Insert trigger (N)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"N");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Record interactions (r)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"r");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Replay interactions (R)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"R");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Landscape postscript dump (o)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"o");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Portrait postscript dump (O)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"O");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_radio_menu_item_new_with_label(NULL, "Color postscript");
 colorgroup=gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(set_postscriptcolor), (gpointer)TRUE);
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_radio_menu_item_new_with_label(colorgroup, "B/W postscript");
 colorgroup=gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(set_postscriptcolor), (gpointer)FALSE);
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 { char * const post_dev=getenv(POSTSCRIPT_ENVNAME);
 /* posplot's default is color Postscript */
 if (post_dev!=NULL && strcmp(post_dev, POSTSCRIPT_BWDEV)==0) gtk_widget_activate(menuitem);
 }

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Dump current point as ARRAY_ASCII (y)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"y");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Dump current point full (Y)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"Y");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Write datasets (W)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"W");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Grid channels (G)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"G");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Enter pos modify mode");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"=change\n_");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Leave/switch pos mode (_)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"_");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Zero current point ($)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"$");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Detrend datasets (D)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"D");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Differentiate datasets (%)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"%");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Integrate datasets (&)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"&");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 topitem= gtk_menu_item_new_with_label("Commands");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);
 /* Catch Keypress events also when the mouse points to the open menu: */
 g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_event), NULL);

 /* Show menu */
 submenu=gtk_menu_new();

 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Plots (p)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"p");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Axes (x)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"x");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Names (n)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"n");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Marker (m)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"m");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Triggers (M)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"M");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Grayscale (d)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"d");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Info (?)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"?");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Triangulation (z)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"z");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 topitem= gtk_menu_item_new_with_label("Show");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);
 /* Catch Keypress events also when the mouse points to the open menu: */
 g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_event), NULL);

 /* Select menu */
 submenu=gtk_menu_new();

 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Point by argument (.)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)".");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Next point (i)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"i");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Previous point (I)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"I");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Left interval boundary ([)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"[");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Right interval boundary (])");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"]");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Dataset by argument (,)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)",");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Toggle datasets (*)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"*");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Datasets up '('");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"(");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Datasets down ')'");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)")");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Next item (})");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"}");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Previous item ({)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"{");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("One channel (ENTER)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"\n");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Hide channel (BACKSPACE)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("All channels (C)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"C");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 topitem= gtk_menu_item_new_with_label("Select");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);
 /* Catch Keypress events also when the mouse points to the open menu: */
 g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_event), NULL);

 /* Transform menu */
 submenu=gtk_menu_new();

 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);


 menuitem=gtk_menu_item_new_with_label("Exponential (e)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"e");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Logarithm (E)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"E");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Single item mode (')");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"'");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Absolute values (a)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"a");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Power values (P)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"P");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Phase values (A)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"A");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 topitem= gtk_menu_item_new_with_label("Transform");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);
 /* Catch Keypress events also when the mouse points to the open menu: */
 g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_event), NULL);

 /* View menu */
 submenu=gtk_menu_new();

 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Redraw (^L)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Vertical scale lock (v)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"v");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Set/Toggle subsampling (@)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"@");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Toggle black/white background (b)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"b");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Line style mode (u)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"u");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Set first line style (U)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"U");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Swap plus/minus (~)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"~");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Swap x-z (S)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"S");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 /* Separator: */
 menuitem=gtk_menu_item_new();
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Position mode (_)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"_");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Center channel (Z)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"Z");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Nearer (+)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"+");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Farther (-)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"-");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Turn l (h)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"h");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Turn r (l)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"l");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Turn u (k)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"k");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Turn d (j)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"j");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Twist l (t)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"t");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Twist r (T)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"T");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Enlarge FOV (/)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"/");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Decrease FOV (\\)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"\\");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Enlarge plots (>)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)">");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Shrink plots (<)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"<");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 menuitem=gtk_menu_item_new_with_label("Reset parameters (c)");
 g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK(insert_key), (gpointer)"c");
 gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
 gtk_widget_show (menuitem);

 topitem= gtk_menu_item_new_with_label("View");
 gtk_menu_item_set_submenu (GTK_MENU_ITEM (topitem), submenu);
 gtk_menu_shell_append (GTK_MENU_SHELL (menubar), topitem);
 gtk_widget_show (topitem);
 /* Catch Keypress events also when the mouse points to the open menu: */
 g_signal_connect (G_OBJECT (submenu), "key_press_event", G_CALLBACK (key_press_event), NULL);


 VGUI.canvas = gtk_drawing_area_new ();
 gtk_widget_set_name (VGUI.canvas, "posplot");
 gtk_widget_set_double_buffered (VGUI.canvas, FALSE);

 gtk_box_pack_start (GTK_BOX (box1), VGUI.canvas, TRUE, TRUE, 0);
 /* Otherwise, a DrawingArea cannot receive key presses: */
 gtk_widget_set_can_focus(VGUI.canvas, TRUE);
 gtk_widget_set_events(VGUI.canvas, GDK_EXPOSURE_MASK
		      | GDK_BUTTON_PRESS_MASK
		      | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK
		      | GDK_KEY_PRESS_MASK
		      | GDK_PROPERTY_CHANGE_MASK);
 g_signal_connect (G_OBJECT (VGUI.canvas), "configure_event", G_CALLBACK (canvas_configure_event), NULL);
#if GTK_MAJOR_VERSION==2
 g_signal_connect (G_OBJECT (VGUI.canvas), "expose_event", G_CALLBACK (canvas_expose_event), NULL);
#else
 g_signal_connect (G_OBJECT (VGUI.canvas), "draw", G_CALLBACK (canvas_expose_event), NULL);
#endif
 g_signal_connect (G_OBJECT (VGUI.canvas), "key_press_event", G_CALLBACK (key_press_event), NULL);
 g_signal_connect (G_OBJECT (VGUI.canvas), "button_press_event", G_CALLBACK (button_press_event), NULL);
 g_signal_connect (G_OBJECT (VGUI.canvas), "motion_notify_event", G_CALLBACK (motion_notify_event), NULL);

 gtk_widget_show (VGUI.canvas);
 gtk_widget_grab_focus(VGUI.canvas);

 gtk_widget_show (VGUI.window);

 VGUI.lastbutton=0;

 /* This is here for security, since a configure_event should have already
  * occurred at this point (actually at the creation of the canvas) */
 gtk_widget_get_allocation(VGUI.canvas, &allocation);
 vdevice.sizeSx = vdevice.sizeX = allocation.width;
 vdevice.sizeSy = vdevice.sizeY = allocation.height;
 vdevice.depth = 3;
 //printf("vdevice address=%ld, vdevice.sizeSx=%d, vdevice.sizeSy=%d\n", (long)&vdevice,  vdevice.sizeSx, vdevice.sizeSy);
 VGUI.cr=NULL;
#if GTK_MAJOR_VERSION==2
#else
 VGUI.crect=NULL;
#endif
 /* Antialiasing doesn't look nice with lines */
 VGUI.antialias=CAIRO_ANTIALIAS_NONE;
 VGUI.backbuffer=TRUE;

 cr = gdk_cairo_create (gtk_widget_get_window(VGUI.canvas));
 my_set_font(cr);
 cairo_text_extents (cr, "A", &te);
 vdevice.hheight = te.height;
 vdevice.hwidth = te.x_advance;
 VGUI.line_width=1;
 cairo_destroy (cr);
 gdk_threads_leave();

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

 VGUI.paint_started=FALSE;

 //printf("VGUI_init end\n");

 return (1);
}

/*
 * VGUI_frontbuffer, VGUI_backbuffer, VGUI_swapbuffers
 * 
 */
static int 
VGUI_frontbuffer(void) {
 //printf("VGUI_frontbuffer\n");
 if (VGUI.paint_started) gdk_window_end_paint(gtk_widget_get_window(VGUI.canvas));
 VGUI.paint_started=FALSE;
 return (0);
}

static int 
VGUI_backbuffer(void) {
 //printf("VGUI_backbuffer\n");
 if (VGUI.paint_started) gdk_window_end_paint(gtk_widget_get_window(VGUI.canvas));
 if (VGUI.backbuffer) {
#if GTK_MAJOR_VERSION==2
  GdkRectangle const crect={0, 0, vdevice.sizeSx, vdevice.sizeSy};
  gdk_window_begin_paint_region(gtk_widget_get_window(VGUI.canvas),gdk_region_rectangle(&crect)),
#else
  cairo_rectangle_int_t const crect={0, 0, vdevice.sizeSx, vdevice.sizeSy};
  if (VGUI.crect!=NULL) {
   cairo_region_destroy(VGUI.crect);
  }
  VGUI.crect=cairo_region_create_rectangle(&crect);
  gdk_window_begin_paint_region(gtk_widget_get_window(VGUI.canvas),VGUI.crect);
#endif
  VGUI.paint_started=TRUE;
 }
 return (0);
}

static int 
VGUI_swapbuffers(void) {
 if (vdevice.inbackbuffer) {
  //printf("VGUI_swapbuffers\n");
  if (VGUI.paint_started) gdk_window_end_paint(gtk_widget_get_window(VGUI.canvas));
  VGUI.paint_started=FALSE;
 }
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
 gdk_threads_enter();
 cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(VGUI.canvas));

 //printf("VGUI_vclear Clear %d %d - %d %d\n", vw, vh, vdevice.sizeSx, vdevice.sizeSy);
 gdk_cairo_set_source_rgba(cr,&VGUI.palette[VGUI.fg]);
 cairo_rectangle(cr, vdevice.minVx, vdevice.sizeSy-vdevice.maxVy-1, vw, vh);
 cairo_fill(cr);
 cairo_destroy (cr);
 gdk_threads_leave();
 return (1);
}


/*
 * VGUI_exit
 * 
 */
static int
VGUI_exit(void) {
 //printf("VGUI_exit\n");

 gdk_threads_enter();
 if (VGUI.paint_started) {
  gdk_window_end_paint(gtk_widget_get_window(VGUI.canvas));
  VGUI.paint_started=FALSE;
 }
#if GTK_MAJOR_VERSION==2
#else
 if (VGUI.crect!=NULL) {
  cairo_region_destroy(VGUI.crect);
 }
#endif
 gtk_widget_destroy (VGUI.window);
 gdk_threads_leave();
#ifdef USE_THREADING
 g_cond_free(block_thread);
 block_thread=NULL;
#endif
 return (1);
}

static int 
VGUI_begin(void) {
 //printf("VGUI_begin\n");
 if (vdevice.bgnmode == VLINE) {
  VGUI.cr = gdk_cairo_create (gtk_widget_get_window(VGUI.canvas));
  cairo_set_antialias(VGUI.cr,VGUI.antialias);
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
#if 0
  } else {
   /* Solid lines are the default anyway */
   gdk_threads_enter();
   cairo_set_dash (VGUI.cr,NULL,0,0);
   gdk_threads_leave();
#endif
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
  cairo_destroy (VGUI.cr);
  VGUI.cr=NULL;
 }
 return (1);
};

static int
noop(void) {
 //printf("VGUI_noop\n");
 return (-1);
}

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

 gdk_threads_enter();
 cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(VGUI.canvas));
 gdk_cairo_set_source_rgba(cr,&VGUI.palette[VGUI.fg]);
 my_set_font(cr);
 cairo_move_to (cr, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);
 cairo_show_text(cr, s);
 cairo_destroy (cr);
 gdk_threads_leave();

 return (1);
};

static int VGUI_string(char *s)
{
 //printf("VGUI_string %s %d %g (vdevice.sizeSy=%d, vdevice.cpVy=%d, vdevice.hheight=%g)\n", s, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy - vdevice.hheight, vdevice.sizeSy, vdevice.cpVy, vdevice.hheight);

 gdk_threads_enter();
 cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(VGUI.canvas));
 gdk_cairo_set_source_rgba(cr,&VGUI.palette[VGUI.fg]);
 my_set_font(cr);
 cairo_move_to (cr, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);
 cairo_show_text(cr, s);
 cairo_destroy (cr);
 gdk_threads_leave();

 return (1);
}


static int VGUI_draw(int x, int y)
{
 //printf("VGUI_solid %d %d\n", x, y);

 gdk_threads_enter();
 if (VGUI.cr!=NULL) {
  if (vdevice.cpVx!=VGUI.draw_lastx || vdevice.cpVy!=VGUI.draw_lasty) {
   cairo_move_to (VGUI.cr, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);
  }
  cairo_line_to (VGUI.cr, x, vdevice.sizeSy - y);
 }
 gdk_threads_leave();

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

#ifdef USE_THREADING
#if 0
static void
posplot_cleanup(void *arg) {
 /* The least we need to do when the thread is cancelled while we wait for
  * being notified about input is to unlock that mutex again... */
 g_static_mutex_unlock(&block_thread_mutex);
}
#endif
#endif
static int need_empty_loops=0;
static int
VGUI_checkkey(void) {
 //printf("VGUI_checkkey\n");
 if (need_empty_loops>0) {
  need_empty_loops--;
  return '\0';
 }
 if (Keyboard_Buffer_keys_in_buffer()==0) {
#ifdef USE_THREADING
  /* Block until something interesting happens */
#if 0
  pthread_cleanup_push(posplot_cleanup, NULL);
#endif
  g_static_mutex_lock(&block_thread_mutex);
  g_cond_wait(block_thread, g_static_mutex_get_mutex(&block_thread_mutex));
  g_static_mutex_unlock(&block_thread_mutex);
#if 0
  pthread_cleanup_pop(FALSE);
#endif
#else
  do {
   gtk_main_iteration_do(TRUE);
  } while (gtk_events_pending());
#endif
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
 noop,   /* wait for and get key */
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
