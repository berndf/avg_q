/*
 * Copyright (C) 1998 Bernd Feige
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
