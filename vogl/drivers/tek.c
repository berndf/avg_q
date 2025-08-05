
/*
 *	Driver for tekronix 401x or equivalent
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef BSD
#include <sgtty.h>
#else
#include <termio.h>
#endif
#include "vogl.h"

static	FILE	*fp;

#define         MASK            037
#define         BELL            '\007'
#define         FF              '\014'
#define         CAN             '\030'
#define         SUB             '\032'
#define         ESC             '\033'
#define         GS              '\035'
#define         US              '\037'

#define	TEK_X_SIZE		1023
#define	TEK_Y_SIZE		767

#include <signal.h>

static int	LoYold = -1, HiYold = -1, HiXold = -1;
static int	tlstx, tlsty;
static int	click;			/* to emulate a mouse click */

/*
 * TEK_init
 *
 *	set up the graphics mode.
 */
static int
TEK_init(void)
{
	/*
	 *      actually only need to set modes in xhair and pause routines
	 */

	vdevice.depth = 1;

	fp = _voutfile();

	fputc(GS, fp);			/* enter graphics mode */

	vdevice.sizeX = vdevice.sizeY = TEK_Y_SIZE;
	vdevice.sizeSx = TEK_X_SIZE;
	vdevice.sizeSy = TEK_Y_SIZE;
				  
	tlstx = tlsty = -1;

        return(1);
}


/*
 * TEK_exit
 *
 *	cleans up before going back to normal mode
 */
static int
TEK_exit(void)
{
	fputc(US, fp);
	fputc(CAN, fp);

	if (fp != stdout)
		fclose(fp);
	return(0);
}

/*
 * out_bytes
 *
 *	output 2 optomized bytes to the terminal
 */
static void
out_bytes(int x, int y)
{
	int HiY, LoY, HiX, LoX;

	if (x < 0)			/* a bit of last minute checking */
		x = 0;
	if (y < 0)
		y = 0;
	if (x > 1023)
		x = 1023;
	if (y > 1023)
		y = 1023;

	HiY = y / 32  +  32;
	LoY = y % 32  +  96;
	HiX = x / 32  +  32;
	LoX = x % 32  +  64;

	/*
	 * optomize the output stream. Ref: Tektronix Manual.
	 */

	if (HiYold != HiY) {
		fprintf(fp, "%c", HiY);
		HiYold = HiY;
	}

	if ((LoYold != LoY) || (HiXold != HiX)) {
		fprintf(fp, "%c", LoY);
		LoYold = LoY;
		if (HiXold != HiX) {
			fprintf(fp, "%c", HiX);
			HiXold = HiX;
		}
	}

	fprintf(fp, "%c", LoX);
}

/*
 * TEK_draw
 *
 *	draw from the current graphics position to the new one (x, y)
 */
static int
TEK_draw(int x, int y)
{
	if (tlstx != vdevice.cpVx || tlsty != vdevice.cpVy) {
		fputc(GS, fp);
		LoYold = HiYold = HiXold = -1;  /* Force output of all bytes */
		out_bytes(vdevice.cpVx, vdevice.cpVy);
	}

	out_bytes(x, y);
	tlstx = x;
	tlsty = y;

	fflush(fp);
	return(0);
}

/*
 * TEK_getkey
 *
 *	return the next key typed.
 */
static int
TEK_getkey(void)
{
#ifdef BSD
	struct sgttyb	oldtty, newtty;
	char		c;

	ioctl(0, TIOCGETP, &oldtty);

	newtty = oldtty;
	newtty.sg_flags = RAW;

	ioctl(0, TIOCSETP, &newtty);

	read(0, &c, 1);

	ioctl(0, TIOCSETP, &oldtty);
#else
	struct termio   oldtty, newtty;
	char            c;
	  
	ioctl(0, TCGETA, &oldtty);

	newtty = oldtty;
	newtty.c_iflag = BRKINT | IXON | ISTRIP;
	newtty.c_lflag = 0;
	newtty.c_cc[VEOF] = 1;

	ioctl(0, TCSETA, &newtty);

	read(0, &c, 1);

	ioctl(0, TCSETA, &oldtty);
#endif

	return(c);
}

/*
 * TEK_locator
 *
 *	get the position of the crosshairs. This gets a bit sticky since
 * we have no mouse, and the crosshairs do not beahve like a mouse - even a rat!
 * In this case the keys 1 to 9 are used, with each one returning a power of
 * two.
 */
static int
TEK_locator(int *x, int *y)
{
	char		buf[5];
	int		i;
#ifdef BSD
	struct sgttyb   oldtty, newtty;
#else
	struct termio   oldtty, newtty;
#endif

	if (click) {			/* for compatability with other devs */
		click = 0;
		return(0);
	}

	click = 1;

	if (fp == stdout) {
#ifdef BSD
		ioctl(0, TIOCGETP, &oldtty);

		newtty = oldtty;
		newtty.sg_flags = RAW;

		ioctl(0, TIOCSETP, &newtty);
#else
		ioctl(0, TCGETA, &oldtty);

		newtty = oldtty;
		newtty.c_iflag = BRKINT | IXON | ISTRIP;
		newtty.c_lflag = 0;
		newtty.c_cc[VEOF] = 1;

		ioctl(0, TCSETA, &newtty);
#endif
	} else
		return(0);

	fputs("\037\033\032", fp); 
	fflush(fp);

	/* Tek 4010/4014 return 8 bytes upon cross-hair read:
	 *
	 *
	 *		0	character pressed
	 *
	 *		1,2	encoded x position
	 *		3,4	encoded y position
	 *		5,6	CR,LF		- ignored
	 *		7	EOF		- ignored
	 */

	/* first we read in the five meaningfull bytes */

	for (i = 0; i < 5; i++)
		buf[i] = getchar();

	/* just in case we get the newline chars */

#ifdef BSD
	ioctl(0, TIOCFLUSH, (char *)NULL);
#else
	ioctl(0, TCFLSH, (char *)NULL);
#endif

	*x = ((buf[1] & MASK) << 5) | (buf[2] & MASK); 
	*y = ((buf[3] & MASK) << 5) | (buf[4] & MASK);

#ifdef BSD
	ioctl(0, TIOCSETP, &newtty);
#else
	ioctl(0, TCSETA, &oldtty);
#endif

	tlstx = tlsty = -1;

	return(1 << ((int)buf[0] - '1'));
}

/*
 * TEK_clear
 *
 *	clear the screen.
 *
 *	NOTE - You may need to actually output a certain number of
 *	NUL chars here to get the (at least) 1 second delay.
 *	This may occur if running through a communication package
 *	or with something like ethernet. If this is the case then
 *	throw away the sleep(2) above and bung in a loop.
 *	Here's a sample ...
 *
 *	for (i = 0; i < 960; i++) fputc(0, fp);
 *
 *	(for 9600 baud rate)
 *
 */
static int
TEK_clear(void)
{
	fputc(US, fp);
	fputc(ESC, fp);
	fputc(FF, fp);
	fflush(fp);

	tlstx = tlsty = -1;

	if (fp != stdout) {
		int	i;
		for (i = 0; i < 960; i++) 
			fputc(0, fp);
	} else
		sleep(2); /* for tekronix slow erase */
	return(0);
}

/*
 * TEK_font
 *
 *	set for large or small mode.
 */
static int
TEK_font(char *tekfont)
{
	if (strcmp(tekfont, "small") == 0) {
		fprintf(fp, "\033:");
		vdevice.hwidth = 8.0;
		vdevice.hheight = 15.0;
	} else if (strcmp(tekfont, "large") == 0) {
		fprintf(fp, "\0338");
		vdevice.hwidth = 14.0;
		vdevice.hheight = 17.0;
	} else
		return(0);

	tlstx = tlsty = -1;

	return(1);
}

/*
 * TEK_char
 *
 *	outputs one char
 */
static int
TEK_char(char c)
{
	if (tlstx != vdevice.cpVx || tlsty != vdevice.cpVy) {
		fputc(GS, fp);
		LoYold = HiYold = HiXold = -1;  /* Force output of all bytes */
		out_bytes(vdevice.cpVx, vdevice.cpVy);
	}

	fputc(US, fp);
	fputc(c, fp);

	tlstx = tlsty = -1;

	fflush(fp);
	return(0);
}

/*
 * TEK_string
 *
 *	outputs a string
 */
static int
TEK_string(const char *s)
{
	if (tlstx != vdevice.cpVx || tlsty != vdevice.cpVy) {	/* move to start */
		fputc(GS, fp);
		LoYold = HiYold = HiXold = -1;  /* Force output of all bytes */
		out_bytes(vdevice.cpVx, vdevice.cpVy);
	}

	fputc(US, fp);
	fputs(s, fp);

	tlstx = tlsty = -1;

	fflush(fp);
	return(0);
}

/*
 * TEK_fill
 *
 *      "fill" a polygon
 */
static int
TEK_fill(int n, int *x, int *y)
{
	int     i;

	if (tlstx != x[0] || tlsty != y[0]) {
		fputc(GS, fp);
		LoYold = HiYold = HiXold = -1;  /* Force output of all bytes */
		out_bytes(x[0], y[0]);
	}

	for (i = 1; i < n; i++)
		out_bytes(x[i], y[i]);

	out_bytes(x[0], y[0]);

	fflush(fp);

	tlstx = vdevice.cpVx = x[n - 1];
	tlsty = vdevice.cpVy = y[n - 1];
	return(0);
}

/*
 * the device entry
 */
static DevEntry	tekdev = {
	"tek",
	"large",
	"small",
	NULL,
	TEK_char,
	NULL,
	TEK_clear,
	NULL,
	TEK_draw,
	TEK_exit,
	TEK_fill,
	TEK_font,
	NULL,
	TEK_getkey,
	TEK_init,
	TEK_locator,
	NULL,
	NULL,
	NULL,
	TEK_string,
	NULL,
	NULL,
	NULL
};

/*
 * _TEK_devcpy
 *
 *      copy the tektronix device into vdevice.dev.
 */
void
_TEK_devcpy(void)
{
        vdevice.dev = tekdev;
}
