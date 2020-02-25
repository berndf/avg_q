#include <stdio.h>
#include <string.h>

#include "vogl.h"

static	Vector	cpos;	/* Current character position */
static	int	sync, sc_x, sc_y;

/*
 * font
 * 	assigns a font.
 */
void
font(short int id)
{
	Token	*tok;

	if (!vdevice.initialised)
		verror("font: vogl not initialised");

	if (id < 0 || id >= vdevice.maxfontnum)
		verror("font: font number is out of range");
	
	if (vdevice.inobject) {
		tok = newtokens(2);
		tok[0].i = VFONT;
		tok[1].i = id;
		return;
	}

	if (id == 1) {
		if (!(*vdevice.dev.Vfont)(vdevice.dev.large)) 
			verror("font: unable to open large font");
	} else if (id == 0) {
		if (!(*vdevice.dev.Vfont)(vdevice.dev.small))
			verror("font: unable to open small font");
	}

	vdevice.attr->a.fontnum = id;
}

/*
 * charstr
 *
 * Draw a string from the current character position.
 *
 */
void
charstr(const char *str)
{
	int	cx, cy;
	char	c;
	Token	*tok;
	int	oldclipoff = vdevice.clipoff;
	int	sl = strlen(str);

	if(!vdevice.initialised) 
		verror("charstr: vogl not initialized");

	if (vdevice.inobject) {
		tok = newtokens(2 + strlen(str) / sizeof(Token));

		tok[0].i = DRAWSTR;
		strcpy((char *)&tok[1], str);

		return;
	}

	if ((sync = vdevice.sync)!=0)
		vdevice.sync = 0;

	cx = vdevice.cpVx;
	cy = vdevice.cpVy;

	vdevice.cpVx = sc_x;
	vdevice.cpVy = sc_y;

	/*   If not clipping then simply display text and return  */

	if (oldclipoff) {
		(*vdevice.dev.Vstring)(str);
	} else { /* Check if string is within viewport */
		 /* Could clip in Z maybe? */
		int	left_s = vdevice.cpVx;
		int	bottom_s = vdevice.cpVy - (int)vdevice.hheight;
		int	top_s = bottom_s + (int)vdevice.hheight;
		int	right_s = left_s + (int)((sl + 1) * vdevice.hwidth);

		if (left_s > vdevice.minVx &&
		    bottom_s < vdevice.maxVy &&
		    top_s > vdevice.minVy &&
		    right_s < vdevice.maxVx) {
			(*vdevice.dev.Vstring)(str);
		} else {
			while ((c = *str++)) {
				if (vdevice.cpVx > vdevice.minVx &&
				    vdevice.cpVx < vdevice.maxVx - (int)vdevice.hwidth) {
					(*vdevice.dev.Vchar)(c);
				}
				vdevice.cpVx += (int)vdevice.hwidth;
			}
		}
	}

	sc_x += sl * (int)vdevice.hwidth;

	/* Put our original device graphics position back... */
	vdevice.cpVx = cx;
	vdevice.cpVy = cy;

	if (sync) {
		vdevice.sync = 1;
		(*vdevice.dev.Vsync)();
	}

}

/*
 * cmov
 *
 *	Sets the current character position.
 */
void
cmov(float x, float y, float z)
{
	Vector	res;
	Token	*tok;

	if (vdevice.inobject) {
		tok = newtokens(4);

		tok[0].i = CMOV;
		tok[1].f = x;
		tok[2].f = y;
		tok[3].f = z;

		return;
	}

	cpos[V_X] = x;
	cpos[V_Y] = y;
	cpos[V_Z] = z;
	cpos[V_W] = 1.0;

	multvector(res, cpos, vdevice.transmat->m);

	sc_x = WtoVx(res);
	sc_y = WtoVy(res);
}

 
/*
 * cmov2
 *
 *	Sets the current character position. Ignores the Z coord.
 *	
 *
 */
void
cmov2(float x, float y)
{
	cmov(x, y, 0.0);
}

/*
 * cmovi
 *
 *	Same as cmov but with integer arguments....
 */
void
cmovi(Icoord x, Icoord y, Icoord z)
{
	cmov((Coord)x, (Coord)y, (Coord)z);
}

/*
 * cmovs
 *
 *	Same as cmov but with short integer arguments....
 */
void
cmovs(Scoord x, Scoord y, Scoord z)
{
	cmov((Coord)x, (Coord)y, (Coord)z);
}

/*
 * cmov2i
 *
 *	Same as cmov2 but with integer arguments....
 */
void
cmov2i(Icoord x, Icoord y)
{
	cmov((Coord)x, (Coord)y, 0.0);
}

/*
 * cmov2s
 *
 *	Same as cmov but with short integer arguments....
 */
void
cmov2s(Scoord x, Scoord y)
{
	cmov((Coord)x, (Coord)y, 0.0);
}


/*
 * strwidth
 *
 * Return the length of a string in pixels
 *
 */
long
strwidth(char *s)
{
#ifdef SUN_CC
	/*
	 * SUN's ANSI CC compiler bitches here sating to use an explicit
	 * cast for strlen... it's only a warning, but this fixes it...
    	 */
	return((long)((size_t)strlen(s) * vdevice.hwidth));
#else
	return((long)(strlen(s) * vdevice.hwidth));
#endif
}

/* 
 * getheight
 *
 * Return the maximum Height of the current font
 */
long 
getheight(void)
{
	if (!vdevice.initialised)
		verror("getheight: vogl not initialized");

	return((long)vdevice.hheight);
}

/*
 * getwidth
 *
 * Return the maximum Width of the current font.
 *
 */
long
getwidth(void)
{
	if (!vdevice.initialised)
		verror("getwidth: vogl not initialised");


	return((long)vdevice.hwidth);
}

/*
 * getcpos
 *
 * Return the current character position in screen coords 
 */
void
getcpos(Scoord *cx, Scoord *cy)
{
	*cx = sc_x;
	*cy = sc_y;
}
