/*
 * Copyright (C) 1998 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */

typedef struct {
 int (*Vbackb)(void);		/* Set drawing in back buffer */
 int (*Vchar)(int c);		/* Draw a hardware character */
 int (*Vcheckkey)(void);		/* Ckeck if a key was hit */
 int (*Vclear)(void);		/* Clear the screen to current color */
 int (*Vcolor)(int i);		/* Set current color */
 int (*Vdraw)(int x, int y);		/* Draw a line */
 int (*Vexit)(void);		/* Exit graphics */
 int (*Vfill)(int sides, int *x, int *y);		/* Fill a polygon */
 int (*Vfont)(char *name);		/* Set hardware font */
 int (*Vfrontb)(void);		/* Set drawing in front buffer */
 int (*Vgetkey)(void);		/* Wait for and get the next key hit */
 int (*Vinit)(void);		/* Initialise the device */
 int (*Vlocator)(int *x, int *y);		/* Get mouse/cross hair position */
 int (*Vmapcolor)(int c, int r, int g, int b);		/* Set color indicies */
 int (*Vsetls)(int s);		/* Set linestyle */
 int (*Vsetlw)(int w);		/* Set linewidth */
 int (*Vstring)(char *s);		/* Draw a hardware string */
 int (*Vswapb)(void);		/* Swap front and back buffers */
 int (*Vsync)(void);		/* Sync display */
} MyDevEntry;

extern MyDevEntry* __get_My_VGUI_devptr(void);
