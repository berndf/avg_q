/*
 * Copyright (C) 1998,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <vogl.h>
#include "vguidev.h"

static MyDevEntry* mydev;

static int VGUI_backbuffer() {
 return (*mydev->Vbackb)();
}
static int VGUI_char(c) 
 int c;
{
 return (*mydev->Vchar)(c);
}
static int VGUI_checkkey() {
 return (*mydev->Vcheckkey)();
}
static int VGUI_vclear() {
 return (*mydev->Vclear)();
}
static int VGUI_colour(i)
 int i;
{
 return (*mydev->Vcolor)(i);
}
static int VGUI_solid(x,y)
 int x, y;
{
 return (*mydev->Vdraw)(x,y);
}
static int VGUI_exit() {
 return (*mydev->Vexit)();
}
static int VGUI_fill(sides,x,y)
 int sides, *x, *y;
{
 return (*mydev->Vfill)(sides,x,y);
}
static int VGUI_font(name)
 char *name;
{
 return (*mydev->Vfont)(name);
}
static int VGUI_frontbuffer() {
 return (*mydev->Vfrontb)();
}
static int VGUI_getkey() {
 return (*mydev->Vgetkey)();
}
static int VGUI_init() {
 return (*mydev->Vinit)();
}
static int VGUI_locator(x,y)
 int *x, *y;
{
 return (*mydev->Vlocator)(x,y);
}
static int VGUI_mapcolor(c,r,g,b) 
 int c, r, g, b;
{
 return (*mydev->Vmapcolor)(c,r,g,b);
}
static int VGUI_lstyle(s)
 int s;
{
 return (*mydev->Vsetls)(s);
}
static int VGUI_lwidth(w)
 int w;
{
 return (*mydev->Vsetlw)(w);
}
static int VGUI_string(s)
 char *s;
{
 return (*mydev->Vstring)(s);
}
static int VGUI_swapbuffers() {
 return (*mydev->Vswapb)();
}
static int VGUI_sync() {
 return (*mydev->Vsync)();
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
 VGUI_solid,  /* draw line */
 VGUI_exit,  /* close graphics & exit */
 VGUI_fill,  /* fill polygon */
 VGUI_font,  /* set hardware font */
 VGUI_frontbuffer, /* front buffer */
 VGUI_getkey,   /* wait for and get key */
 VGUI_init,  /* begin graphics */
 VGUI_locator,  /* get mouse position */
 VGUI_mapcolor,  /* map colour (set indices) */
 VGUI_lstyle,  /* set linestyle */
 VGUI_lwidth,  /* set line width */
 VGUI_string,  /* draw string of chars */
 VGUI_swapbuffers, /* swap buffers */
 VGUI_sync  /* sync display */
};

/*
 * _VGUI_devcpy
 * 
 * copy the pc device into vdevice.dev. (as listed in drivers.c)
 */
int
_VGUI_devcpy() {
 mydev=__get_My_VGUI_devptr();
 vdevice.dev = VGUIdev;
 return (0);
}
