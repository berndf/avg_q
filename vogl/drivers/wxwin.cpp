/*
 * WXWIN driver for VOGL (c) 1996 by Bernd Feige (Feige@ukl.uni-freiburg.de)
 * 
 * To compile:
 * 
 * 1) add WXWIN to device.c and mash-up your makefiles for MsDOS 2) compile with
 * DOBJ=-DPOSTSCRIPT -DHPGL -DWXWIN and MFLAGS=-O2
 * 
 * To run:
 * 
 * set VDEVICE=wxwin
 * 
 */

#ifdef __GNUG__
#pragma implementation
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx_prec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx.h"
#endif

#ifdef _MSC_VER
#pragma warning (disable : 4244)
#endif

#include <stdlib.h>
extern "C" {
#include <stdio.h>
#include <vogl.h>

extern _wxwin_devcpy(void);
}

extern wxFrame *vogl_MainFrame;	// To be set by the main program !

#define MAXCOLOR 256

static struct {
 wxFont *CurrentFont;
 wxFont *SmallFont;
 wxFont *LargeFont;
 wxFrame *subframe;
 wxCanvas *canvas;
 wxPen *pen;
 wxColour *palette[MAXCOLOR];
 wxColour *fg;
 int width;
 int height;
 int line_width;
} wxwin;

static int
wxwin_init(void) {
 wxwin.SmallFont = new wxFont(8, wxROMAN, wxNORMAL, wxNORMAL);
 wxwin.LargeFont = new wxFont(16, wxROMAN, wxNORMAL, wxNORMAL);
 wxwin.CurrentFont=wxwin.SmallFont;
 wxwin.palette[BLACK] = new wxColour(0, 0, 0);
 wxwin.palette[RED] = new wxColour(255, 0, 0);
 wxwin.palette[GREEN] = new wxColour(0, 255, 0);
 wxwin.palette[YELLOW] = new wxColour(255, 255, 0);
 wxwin.palette[BLUE] = new wxColour(0, 0, 255);
 wxwin.palette[MAGENTA] = new wxColour(255, 0, 255);
 wxwin.palette[CYAN] = new wxColour(0, 255, 255);
 wxwin.palette[WHITE] = new wxColour(255, 255, 255);

 wxwin.subframe= new wxFrame(NULL, (vdevice.wintitle!=NULL ? vdevice.wintitle : "Canvas"), 300, 300, 400, 300);
 wxwin.subframe->GetClientSize(&wxwin.width, &wxwin.height);
 wxwin.canvas=new wxCanvas(wxwin.subframe, 0, 0, wxwin.width, wxwin.height, wxRETAINED);
 wxwin.canvas->SetBackground(wxWHITE_BRUSH);
 wxwin.pen=new wxPen(*wxwin.palette[BLACK], 1, wxSOLID);
 wxwin.canvas->SetPen(wxwin.pen);
 wxwin.canvas->SetBrush(wxTRANSPARENT_BRUSH);

 wxwin.line_width=1;

 /* set the VOGL device */
 vdevice.sizeX = wxwin.width;
 vdevice.sizeY = wxwin.height;

 vdevice.sizeSx = wxwin.width;
 vdevice.sizeSy = wxwin.height;
 vdevice.depth = 3;
 printf("vdevice address=%ld, vdevice.sizeSx=%d, vdevice.sizeSy=%d\n", (long)&vdevice,  vdevice.sizeSx, vdevice.sizeSy);

 wxwin.subframe->Show(TRUE);

 return (1);
}

/*
 * wxwin_frontbuffer, wxwin_backbuffer, wxwin_swapbuffers
 * 
 */
static int 
wxwin_frontbuffer(void) {
 printf("wxwin_frontbuffer\n");
 return (0);
}

static int 
wxwin_backbuffer(void) {
 printf("wxwin_backbuffer\n");
 return (0);
}

static int 
wxwin_swapbuffers(void) {
 printf("wxwin_swapbuffers\n");
 return (0);
}

/*
 * wxwin_vclear
 * 
 * Clear the viewport to current colour
 */
static int
wxwin_vclear(void) {
 unsigned int    vw = vdevice.maxVx - vdevice.minVx;
 unsigned int    vh = vdevice.maxVy - vdevice.minVy;
 wxDC *dc = wxwin.canvas->GetDC();

 if ((vdevice.sizeSx == vw) && (vdevice.sizeSy == vh)) {
  dc->Clear(); /* full screen */
  printf("wxwin_vclear Clear\n");
 } else
  printf("wxwin_vclear Rectangle\n");
  dc->DrawRectangle(
       vdevice.minVx,
       vdevice.sizeY-vdevice.minVy,
       vdevice.maxVx,
       vdevice.sizeY-vdevice.maxVy);

 return (1);
}


/*
 * wxwin_exit
 * 
 */
static
wxwin_exit(void) {
 int i;
 printf("wxwin_exit\n");
 delete wxwin.canvas;
 delete wxwin.subframe;
 for (i=0; i<MAXCOLOR; i++) {
  if (wxwin.palette[i]!=NULL) {
   delete wxwin.palette[i];
   wxwin.palette[i]=NULL;
  }
 }
 delete wxwin.SmallFont;
 delete wxwin.LargeFont;
 return (1);
}

static int 
wxwin_sync(void) {
 printf("wxwin_sync\n");
 return (1);
};

static int
noop(void) {
 return (-1);
}

/*
 * wxwin_font : load either of the fonts
 */
static int
wxwin_font(char *name) {
 wxDC *dc = wxwin.canvas->GetDC();
 printf("wxwin_font %s\n", name);
 /*
  * Hacky way to quicky test for small or large font
  * ... see of they are the same pointers.... this
  * assumes that they have been called from the main
  * library routine with *vdevice.Vfont(vdevice.smallfont);
  */
 if (name == vdevice.dev.small) {
  wxwin.CurrentFont=wxwin.SmallFont;
 } else if (name == vdevice.dev.large) {
  wxwin.CurrentFont=wxwin.LargeFont;
 }
 vdevice.hheight = wxwin.CurrentFont->GetPointSize();
 vdevice.hwidth = wxwin.CurrentFont->GetPointSize();
 dc->SetFont(wxwin.CurrentFont);

 return (1);
}

static int 
wxwin_char(int c) {
 wxDC *dc = wxwin.canvas->GetDC();
 char buf[2];
 buf[0]=c;
 buf[1]='\0';

 printf("wxwin_char %c %d %g\n", c, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy - vdevice.hheight);

 dc->DrawText(buf, (float)vdevice.cpVx, (float)(vdevice.sizeSy - vdevice.cpVy - vdevice.hheight));
 vdevice.cpVx += (int)vdevice.hwidth;

 return (1);
};

static int
wxwin_string(char *s) {
 int len = strlen(s);
 wxDC *dc = wxwin.canvas->GetDC();

 //printf("wxwin_string %s %d %g (vdevice.sizeSy=%d, vdevice.cpVy=%d, vdevice.hheight=%g)\n", s, vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy - vdevice.hheight, vdevice.sizeSy, vdevice.cpVy, vdevice.hheight);

 dc->DrawText(s, (float)vdevice.cpVx, (float)(vdevice.sizeSy - vdevice.cpVy - vdevice.hheight));
 vdevice.cpVx += (int)(vdevice.hwidth * len);
 return (1);
}


static int
wxwin_solid(int x, int y) {
 wxDC *dc = wxwin.canvas->GetDC();

 //printf("wxwin_solid %d %d\n", x, y);

 dc->DrawLine(x, vdevice.sizeSy - y,
   vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);

 vdevice.cpVx = x;
 vdevice.cpVy = y;

 return (0);
}

static int
wxwin_pattern(int x, int y) {
 wxDC *dc = wxwin.canvas->GetDC();

 //printf("wxwin_pattern %d %d\n", x, y);

 dc->DrawLine(x, vdevice.sizeSy - y,
   vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy);

 vdevice.cpVx = x;
 vdevice.cpVy = y;

 return (0);
};

static int
wxwin_colour(int i) {
 wxDC *dc = wxwin.canvas->GetDC();

 //printf("wxwin_colour %d\n", i);

 if (i < MAXCOLOR)
  wxwin.fg = wxwin.palette[i]; /* for now */
 else
  wxwin.fg = wxwin.palette[BLACK];
 wxwin.pen->SetColour(*wxwin.fg);
 dc->SetPen(wxwin.pen);

 return (0);
};

/*
 * wxwin_mapcolor
 * 
 * change index i in the color map to the appropriate r, g, b, value.
 */
static int
wxwin_mapcolor(int c, int r, int g, int b) {

 printf("wxwin_mapcolor c=%d (%d %d %d)\n", c, r,g,b);
 if (c >= MAXCOLOR || vdevice.depth == 1) return (-1);

 wxwin.palette[c] = new wxColour(r, g, b);
 return 1;
}


static int
wxwin_fill(int sides, int *x, int *y) {
 wxDC *dc = wxwin.canvas->GetDC();
 int             i, j;

 wxPoint *points=new wxPoint[sides];

 for (i = 0; i < sides; i++) {
  points[i].x = x[i];
  points[i].y = wxwin.height - y[i];
 }

 dc->DrawPolygon(sides, points);

 delete[] points;

 return (0);
};

static int
wxwin_checkkey(void) {
 //printf("wxwin_checkkey\n");
 return c;
}

static int
wxwin_locator(int *x, int *y) {
 *x = *y = 0;
 //printf("wxwin_locator\n");
 return (-1);
}

static int 
wxwin_lwidth(int w) {
 wxwin.line_width = w;
 printf("wxwin_lwidth %d\n", w);
 if (w == 1)
  vdevice.dev.Vdraw = wxwin_solid;
 else
  vdevice.dev.Vdraw = wxwin_pattern;
}

static int 
wxwin_lstyle(int s) {
 printf("wxwin_lstyle %d\n", s);
 return -1;
}

static DevEntry wxwindev = {
 "wxwin",
 "@:pc8x16.fnt",  /* Large font */
 "@:pc8x8.fnt",  /* Small font */
 wxwin_backbuffer,  /* backbuffer */
 wxwin_char,  /* hardware char */
 wxwin_checkkey,  /* keyhit */
 wxwin_vclear,  /* clear viewport to current colour */
 wxwin_colour,  /* set current colour */
 wxwin_solid,  /* draw line */
 wxwin_exit,  /* close graphics & exit */
 wxwin_fill,  /* fill polygon */
 wxwin_font,  /* set hardware font */
 wxwin_frontbuffer, /* front buffer */
 noop,   /* wait for and get key */
 wxwin_init,  /* begin graphics */
 wxwin_locator,  /* get mouse position */
 wxwin_mapcolor,  /* map colour (set indices) */
 wxwin_lstyle,  /* set linestyle */
 wxwin_lwidth,  /* set line width */
 wxwin_string,  /* draw string of chars */
 wxwin_swapbuffers, /* swap buffers */
 wxwin_sync  /* sync display */
};

/*
 * _wxwin_devcpy
 * 
 * copy the pc device into vdevice.dev. (as listed in drivers.c)
 */
int
_wxwin_devcpy(void) {
 vdevice.dev = wxwindev;
 return (0);
}
