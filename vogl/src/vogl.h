/*
 * VOGL is always defined if a header file is from the 
 * VOGL library. In cases where you do use some VOGL
 * initialisation routines like vinit, just put #ifdef VOGL...
 * around.
 */
#include <stdio.h>

#ifndef VOGL
#define	VOGL
#endif

#ifndef TRUE
#define	TRUE	1
#endif

#ifndef FALSE
#define	FALSE	0
#endif

/*
 * Misc defines...
 */
#define	FLAT	0
#define SMOOTH	1
#define GD_XPMAX 1
#define GD_YPMAX 2

/*
 * standard colour indices
 */
#define	BLACK		0
#define	RED		1
#define	GREEN		2
#define	YELLOW		3
#define	BLUE		4
#define	MAGENTA		5
#define	CYAN		6
#define	WHITE		7

/*
 * when (if ever) we need the precision
 */
#ifdef DOUBLE
#define	float	double
#endif

/*
 * How to convert degrees to radians
 */
#ifndef PI
#define	PI	3.14159265358979
#endif
#define D2R	(PI / 180.0)

/*
 * miscellaneous typedefs and type defines
 */
typedef float	Vector[4];
typedef float	Matrix[4][4];
typedef float	Tensor[4][4][4];
typedef short	Angle;
typedef float	Coord;
typedef long	Icoord;
typedef short	Scoord;
typedef long	Object;
typedef short	Screencoord;
typedef long	Boolean;
typedef unsigned short	Linestyle;

typedef unsigned short	Device;

typedef unsigned short	Colorindex;


/*
 * when register variables get us into trouble
 */
#ifdef NOREGISTER
#define	register
#endif

/*
 * max number of vertices in a ploygon
 */
#define	MAXVERTS	128

/*
 * object definitions
 */
#define MAXENTS		101		/* size of object table */
#define	MAXTOKS		100		/* num. of tokens alloced at once in
					   an object  */

/*
 * Polygon fill modes for "polymode"
 */
#define PYM_POINT	0
#define PYM_LINE	0
#define PYM_FILL	1
#define PYM_HOLLOW	1

/*
 * functions which can appear in objects
 */
#define	ARC		1
#define	CALLOBJ		3
#define	CIRCLE		5
#define	CLEAR		6
#define	COLOR		7
#define	DRAW		8
#define	DRAWSTR		10
#define	VFONT		12
#define	LOADMATRIX	15
#define	MAPCOLOR	16
#define	MOVE		17
#define	MULTMATRIX	18
#define	POLY		19
#define	POPATTRIBUTES	22
#define	POPMATRIX	23
#define	POPVIEWPORT	24
#define	PUSHATTRIBUTES	25
#define	PUSHMATRIX	26
#define	PUSHVIEWPORT	27
#define	RCURVE		28
#define	RPATCH		29
#define	SECTOR		30
#define	VIEWPORT	33
#define	BACKBUFFER	34
#define	FRONTBUFFER	35
#define	SWAPBUFFERS	36
#define	BACKFACING	37
#define	TRANSLATE	38
#define	ROTATE		39
#define	SCALE		40

#define	ARCF		41
#define	CIRCF		42
#define	POLYF		43
#define	RECTF		44
#define	POLYMODE	45
#define	CMOV		46
#define	LINESTYLE	47
#define	LINEWIDTH	48

/*
 * Non standard call...
 */
#define	VFLUSH		70

/*
 * States for bgn* and end* calls
 */
#define	NONE		0	/* Just set current spot */
#define	VPNT		1	/* Draw dots		 */
#define	VLINE		2	/* Draw lines		 */
#define	VCLINE		3	/* Draw closed lines	 */
#define	VPOLY		4	/* Draw a polygon 	 */
#define VTMESH		5       /* Draw a triangular mesh*/
#define VQSTRIP		6       /* Draw a quadralateral mesh*/

/*
 * data types for object tokens
 */
typedef union tk {
	int		i;
	float		f;
} Token;

typedef struct tls {
	int		count;
	Token		*toks;
	struct tls	*next;
} TokList;

/*
 * double buffering modes.
 */
#define	SINGLE		1

/*
 * attributes
 */
typedef struct {
	char		backface,
			mode;			/* Which mode are we in */
	int		color;
	int		fontnum;
	Linestyle	ls;			
	short		lw;			/* Linewidth */
} Attribute;

/*
 * viewport
 */
typedef struct vp {
	float	left;
	float	right;
	float	bottom;
	float	top;
} Viewport; 

/*
 * stacks
 */
typedef	struct	ms {	/* Matrix stack entries	*/
	Matrix		m;
	struct	ms	*back;
} Mstack;

typedef	struct	as {	/* Attribute stack entries */
	Attribute	a;
	struct	as	*back;
} Astack;

typedef	struct	vs {	/* Viewport stack entries */
	Viewport	v;
	struct	vs	*back;
} Vstack;

/*
 * vogle device structures
 */
typedef struct dev {
	char	*devname;		/* name of device */
	char	*large,			/* name of large font */
		*small;			/* name of small font */
	int	(*Vbackb)(void),		/* Set drawing in back buffer */
		(*Vchar)(char),		/* Draw a hardware character */
		(*Vcheckkey)(void),		/* Ckeck if a key was hit */
		(*Vclear)(void),		/* Clear the screen to current color */
		(*Vcolor)(int),		/* Set current color */
		(*Vdraw)(int, int),		/* Draw a line */
		(*Vexit)(void),		/* Exit graphics */
		(*Vfill)(int, int *, int *),		/* Fill a polygon */
		(*Vfont)(char *),		/* Set hardware font */
		(*Vfrontb)(void),		/* Set drawing in front buffer */
		(*Vgetkey)(void),		/* Wait for and get the next key hit */
		(*Vinit)(void),		/* Initialise the device */
		(*Vlocator)(int *, int *),		/* Get mouse/cross hair position */
		(*Vmapcolor)(int, int, int, int),		/* Set color indicies */
		(*Vsetls)(int),		/* Set linestyle */
		(*Vsetlw)(int),		/* Set linewidth */
		(*Vstring)(char *),		/* Draw a hardware string */
		(*Vswapb)(void),		/* Swap front and back buffers */
		(*Vbegin)(void),		/* Begin recording drawing events */
		(*Vsync)(void);		/* Sync display */
} DevEntry;

typedef struct vdev {
	char		initialised,
			clipoff,
			inobject,
			inpolygon,
			fill,			/* polygon filling */
			cpVvalid,		/* is the current device position valid */
			sync,			/* Do we syncronise the display */
			inbackbuffer,		/* are we in the backbuffer */
			clipplanes;		/* active clipping planes */
	TokList		*tokens;		/* ptr to list of tokens for current object */
	Mstack		*transmat;		/* top of transformation stack */
	Astack		*attr;			/* top of attribute stack */
	Vstack		*viewport;		/* top of viewport stack */
	float		hheight, hwidth;	/* hardware character height, width */
	Vector		cpW,			/* current postion in world coords */
			cpWtrans,		/* current world coords transformed */
			upvector;		/* world up */
	int		depth,			/* # bit planes on screen */
			maxVx, minVx,
			maxVy, minVy,
			sizeX, sizeY, 		/* size of square on screen */
			sizeSx, sizeSy,		/* side in x, side in y (# pixels) */
			cpVx, cpVy;
	DevEntry	dev;
	float		savex,			/* Where we started for v*() */
			savey,
			savez;
	char		bgnmode;		/* What to do with v*() calls */
	int		save;			/* Do we save 1st v*() point */

	char		*wintitle;		/* window title */

	char		*devname;		/* pointer to device name */

	Matrix		tbasis, ubasis, *bases; /* Patch stuff */
	
	char		*enabled;		/* pointer to enabled devices mask */
	int		maxfontnum;

	char		alreadyread;		/* queue device stuff */
	char		kbdmode;		/* are we in keyboard mode */
	char		mouseevents;		/* are mouse events enabled */
	char		kbdevents;		/* are kbd events enabled */
	int		devno, data;

	int		concave;		/* concave polygons? */
} VDevice;

extern VDevice	vdevice;		/* device structure */

#define	V_X	0			/* x axis in cpW */
#define	V_Y	1			/* y axis in cpW */
#define	V_Z	2			/* z axis in cpW */
#define	V_W	3			/* w axis in cpW */

/*
 * function definitions
 */

/*
 * arc routines
 */
extern void	arcprecision(int noseg);
extern void	circleprecision(int noseg);
extern void	arc(Coord x, Coord y, Coord radius, Angle sang, Angle eang);
extern void	arcs(Scoord x, Scoord y, Scoord radius, Angle sang, Angle eang);
extern void	arci(Icoord x, Icoord y, Icoord radius, Angle sang, Angle eang);
extern void	arcf(Coord x, Coord y, Coord radius, Angle sang, Angle eang);
extern void	arcfs(Scoord x, Scoord y, Scoord radius, Angle sang, Angle eang);
extern void	arcfi(Icoord x, Icoord y, Icoord radius, Angle sang, Angle eang);
extern void	circ(Coord x, Coord y, Coord radius);
extern void	circs(Scoord x, Scoord y, Scoord radius);
extern void	circi(Icoord x, Icoord y, Icoord radius);
extern void	circf(Coord x, Coord y, Coord radius);
extern void	circfs(Scoord x, Scoord y, Scoord radius);
extern void	circfi(Icoord x, Icoord y, Icoord radius);

/*
 * attr routines
 */
extern void	popattributes(void);
extern void	pushattributes(void);

/*
 * curve routines
 */
extern void	curvebasis(short int id);
extern void	curveprecision(short int nsegments);
extern void	rcrv(Coord (*geom)[4]);
extern void	crv(Coord (*geom)[3]);
extern void	crvn(long int n, Coord (*geom)[3]);
extern void	rcrvn(long int n, Coord (*geom)[4]);
extern void	curveit(short int n);

/*
 * draw routines
 */
extern void	draw(float x, float y, float z);
extern void	draws(Scoord x, Scoord y, Scoord z);
extern void	drawi(Icoord x, Icoord y, Icoord z);
extern void	draw2(float x, float y);
extern void	draw2s(Scoord x, Scoord y);
extern void	draw2i(Icoord x, Icoord y);
extern void	rdr(float dx, float dy, float dz);
extern void	rdrs(Scoord dx, Scoord dy, Scoord dz);
extern void	rdri(Icoord dx, Icoord dy, Icoord dz);
extern void	rdr2(float dx, float dy);
extern void	rdr2s(Scoord dx, Scoord dy);
extern void	rdr2i(Icoord dx, Icoord dy);
extern void	bgnline(void);
extern void	endline(void);
extern void	bgnclosedline(void);
extern void	endclosedline(void);

/*
 * device routines
 */
extern void	qdevice(Device dev);
extern void	unqdevice(Device dev);
extern long	qread(short int *ret);
extern void	qreset(void);
extern long	qtest(void);
extern Boolean	isqueued(Device dev);
extern void	qenter(Device dev, short int val);

extern void	gexit(void);
extern void	gconfig(void);
extern void	shademodel(long int model);
extern long	getgdesc(long int inq);
extern long	winopen(char *title);
extern void	ginit(void);
extern void	gconfig(void);
extern long	getvaluator(Device dev);
extern Boolean	getbutton(Device dev);
extern void	getdev(long int n, Device *devs, short int *vals);
extern void	clear(void);
extern void	colorf(float f);
extern void	color(int i);
extern void	mapcolor(Colorindex i, short int r, short int g, short int b);
extern long	getplanes(void);

extern void	vinit(char *device);
extern void	voutput(char const *path);
extern void	verror(char *str);
extern void	vnewdev(char *device);
extern char	*vgetdev(char *buf);

extern void	deflinestyle(short int n, Linestyle ls);
extern void	setlinestyle(short int n);

/*
 * mapping routines
 */
extern int	WtoVx(float *p);
extern int	WtoVy(float *p);
extern void	CalcW2Vcoeffs(void);

/*
 * general matrix and vector routines
 */
extern void	mult4x4(register float (*a)[4], register float (*b)[4], register float (*c)[4]);
extern void	copymatrix(register float (*a)[4], register float (*b)[4]);
extern void	identmatrix(float (*a)[4]);
extern void	copytranspose(register float (*a)[4], register float (*b)[4]);

extern void	multvector(register float *v, register float *a, register float (*b)[4]);
extern void	copyvector(register float *a, register float *b);
extern void	premultvector(float *v, float *a, float (*b)[4]);

/*
 * matrix stack routines
 */
extern void	getmatrix(float (*m)[4]);
extern void	popmatrix(void);
extern void	loadmatrix(float (*mat)[4]);
extern void	pushmatrix(void);
extern void	multmatrix(float (*mat)[4]);

/*
 * move routines
 */
extern void	move(Coord x, Coord y, Coord z);
extern void	moves(Scoord x, Scoord y, Scoord z);
extern void	movei(Icoord x, Icoord y, Icoord z);
extern void	move2(Coord x, Coord y);
extern void	move2s(Scoord x, Scoord y);
extern void	move2i(Icoord x, Icoord y);
extern void	rmv(Coord dx, Coord dy, Coord dz);
extern void	rmvs(Scoord dx, Scoord dy, Scoord dz);
extern void	rmvi(Icoord dx, Icoord dy, Icoord dz);
extern void	rmv2(float dx, float dy);
extern void	rmv2s(Scoord dx, Scoord dy);
extern void	rmv2i(Icoord dx, Icoord dy);

/*
 * object routines
 */
extern Boolean	isobj(long int ob);
extern long	genobj(void);
extern void	delobj(long int ob);
extern void	makeobj(long int ob);
extern void	callobj(long int ob);
extern void	closeobj(void);
extern long	getopenobj(void);
extern Token	*newtokens(int num);

/*
 * patch routines.
 */
extern void	defbasis(short int id, float (*mat)[4]);
extern void	patchbasis(long int tb, long int ub);
extern void	patchcurves(long int nt, long int nu);
extern void	patchprecision(long int tseg, long int useg);
extern void	patch(float (*geomx)[4], float (*geomy)[4], float (*geomz)[4]);
extern void	rpatch(float (*geomx)[4], float (*geomy)[4], float (*geomz)[4], float (*geomw)[4]);
extern void drpatch(float (*R)[4][4], int ntcurves, int nucurves, int ntsegs, int nusegs, int ntiter, int nuiter);

/*
 * point routines
 */
extern void	pnt(float x, float y, float z);
extern void	pnts(Scoord x, Scoord y, int z);
extern void	pnti(Icoord x, Icoord y, Icoord z);
extern void	pnt2(Coord x, Coord y);
extern void	pnt2s(Scoord x, Scoord y);
extern void	pnt2i(Icoord x, Icoord y);
extern void	bgnpoint(void);
extern void	endpoint(void);

/*
 * v routines
 */
extern void	v4f(float *vec);
extern void	v3f(float *vec);
extern void	v2f(float *vec);
extern void	v4d(double *vec);
extern void	v3d(double *vec);
extern void	v2d(double *vec);
extern void	v4i(long int *vec);
extern void	v3i(long int *vec);
extern void	v2i(long int *vec);
extern void	v4s(short int *vec);
extern void	v3s(short int *vec);
extern void	v2s(short int *vec);

/*
 * polygon routines.
 */
extern void	concave(Boolean yesno);
extern void	backface(int onoff);
extern void	frontface(int onoff);
extern void	polymode(long int mode);
extern void	poly2(long int nv, float (*dp)[2]);
extern void	poly2i(long int nv, Icoord (*dp)[2]);
extern void	poly2s(long int nv, Scoord (*dp)[2]);
extern void	polyi(long int nv, Icoord (*dp)[3]);
extern void	polys(long int nv, Scoord (*dp)[3]);
extern void	polf2(long int nv, float (*dp)[2]);
extern void	polf2i(long int nv, Icoord (*dp)[2]);
extern void	polf2s(long int nv, Scoord (*dp)[2]);
extern void	polfi(long int nv, Icoord (*dp)[3]);
extern void	polfs(long int nv, Scoord (*dp)[3]);
extern void	poly(long int nv, float (*dp)[3]);
extern void	polf(long int nv, float (*dp)[3]);
extern void	pmv(float x, float y, float z);
extern void	pmvi(Icoord x, Icoord y, Icoord z);
extern void	pmv2i(Icoord x, Icoord y);
extern void	pmvs(Scoord x, Scoord y, Scoord z);
extern void	pmv2s(Scoord x, Scoord y);
extern void	pmv2(float x, float y);
extern void	pdr(Coord x, Coord y, Coord z);
extern void	rpdr(Coord dx, Coord dy, Coord dz);
extern void	rpdr2(Coord dx, Coord dy);
extern void	rpdri(Icoord dx, Icoord dy, Icoord dz);
extern void	rpdr2i(Icoord dx, Icoord dy);
extern void	rpdrs(Scoord dx, Scoord dy, Scoord dz);
extern void	rpdr2s(Scoord dx, Scoord dy);
extern void	rpmv(Coord dx, Coord dy, Coord dz);
extern void	rpmv2(Coord dx, Coord dy);
extern void	rpmvi(Icoord dx, Icoord dy, Icoord dz);
extern void	rpmv2i(Icoord dx, Icoord dy);
extern void	rpmvs(Scoord dx, Scoord dy, Scoord dz);
extern void	rpmv2s(Scoord dx, Scoord dy);
extern void	pdri(Icoord x, Icoord y, Icoord z);
extern void	pdr2i(Icoord x, Icoord y);
extern void	pdrs(Scoord x, Scoord y);
extern void	pdr2s(Scoord x, Scoord y);
extern void	pdr2(float x, float y);
extern void	pclos(void);
extern void	bgnpolygon(void);
extern void	endpolygon(void);

/*
 * rectangle routines
 */
extern void	rect(Coord x1, Coord y1, Coord x2, Coord y2);
extern void	recti(Icoord x1, Icoord y1, Icoord x2, Icoord y2);
extern void	rects(Scoord x1, Scoord y1, Scoord x2, Scoord y2);
extern void	rectf(Coord x1, Coord y1, Coord x2, Coord y2);
extern void	rectfi(Icoord x1, Icoord y1, Icoord x2, Icoord y2);
extern void	rectfs(Scoord x1, Scoord y1, Scoord x2, Scoord y2);

/*
 * tensor routines
 */
extern void multtensor(float (*c)[4][4], float (*a)[4], float (*b)[4][4]);
extern void copytensor(float (*b)[4][4], float (*a)[4][4]);
extern void premulttensor(float (*c)[4][4], float (*a)[4], float (*b)[4][4]);
extern void copytensortrans(float (*b)[4][4], float (*a)[4][4]);
extern void transformtensor(float (*S)[4][4], float (*m)[4]);

/*
 * text routines
 */
extern void	font(short int id);
extern void	charstr(char *str);
extern void	cmov(float x, float y, float z);
extern void	cmov2(float x, float y);
extern void	cmovi(Icoord x, Icoord y, Icoord z);
extern void	cmovs(Scoord x, Scoord y, Scoord z);
extern void	cmov2i(Icoord x, Icoord y);
extern void	cmov2s(Scoord x, Scoord y);
extern long	getheight(void);
extern long	getwidth(void);
extern long	strwidth(char *s);
extern void	getcpos(Scoord *cx, Scoord *cy);

/*
 * transformation routines
 */
extern void	scale(float x, float y, float z);
extern void	translate(float x, float y, float z);
extern void	rotate(Angle r, char axis);
extern void	rot(float r, char axis);

/*
 * window definition routines
 */
extern void	ortho(Coord left, Coord right, Coord bottom, Coord top, Coord hither, Coord yon);
extern void	ortho2(Coord left, Coord right, Coord bottom, Coord top);
extern void	lookat(Coord vx, Coord vy, Coord vz, Coord px, Coord py, Coord pz, Angle twist);
extern void	window(Coord left, Coord right, Coord bottom, Coord top, Coord hither, Coord yon);
extern void	polarview(Coord dist, Angle azim, Angle inc, Angle twist);
extern void	perspective(Angle ifov, float aspect, Coord hither, Coord yon);

/*
 * routines for manipulating the viewport
 */
extern void	getviewport (Screencoord *left, Screencoord *right, Screencoord *bottom, Screencoord *top);
extern void	viewport(Screencoord xlow, Screencoord xhigh, Screencoord ylow, Screencoord yhigh);
extern void	popviewport(void);
extern void	pushviewport(void);
extern void	reshapeviewport(void);

/*
 * routines for retrieving the graphics position
 */
extern void	getgp(Coord *x, Coord *y, Coord *z);
extern void	getgpos(Coord *x, Coord *y, Coord *z, Coord *w);

/*
 * routines for handling the buffering
 */
extern void	backbuffer(int yes);
extern void	frontbuffer(int yes);
extern void	swapbuffers(void);
extern void	doublebuffer(void);
extern void	singlebuffer(void);

/*
 * routines for window sizing and positioning
 */
extern void	prefsize(long int x, long int y);
extern void	prefposition(long int x1, long int x2, long int y1, long int y2);

/*
 * Misc control routines
 */
extern void	vsetflush(int yn);
extern void	vflush(void);
extern char * vallocate(unsigned int size);
extern void getprefposandsize(int *x, int *y, int *xs, int *ys);

extern void drcurve(int n, float (*r)[4]);
extern void quickclip(register float *p0, register float *p1);
extern void clip(register float *p0, register float *p1);
extern void polyobj(int n, Token *dp, int fill);
extern FILE *_voutfile(void);
extern void linewidth(short int w);
extern void linewidthf(float w);

/*
 * Hershey functions
 */
void hfont (char *name);
int hnumchars (void);
void hsetpath (char *path);
void hgetcharsize (char c, float *width, float *height);
void hdrawchar (int c);
void htextsize (float width, float height);
float hgetfontwidth (void);
float hgetfontheight (void);
void hgetfontsize (float *cw, float *ch);
float hgetdecender (void);
float hgetascender (void);
void hcharstr (char *string);
float hstrlength (char *s);
void hboxtext (float x, float y, float l, float h, char *s);
void hboxfit (float l, float h, int nchars);
void hcentertext (int onoff);
void hrightjustify (int onoff);
void hleftjustify (int onoff);
void hfixedwidth (int onoff);
void htextang (float ang);

/*
 * Driver entry points
 */
extern void _SUN_devcpy(void);
extern void _PIXRECT_devcpy(void);
extern void _X11_devcpy(void);
extern void _DECX11_devcpy(void);
extern void _NeXT_devcpy(void);
extern void _PS_devcpy(void);
extern void _PSP_devcpy(void);
extern void _CPS_devcpy(void);
extern void _PCPS_devcpy(void);
extern void _HPGL_A1_devcpy(void);
extern void _HPGL_A2_devcpy(void);
extern void _HPGL_A3_devcpy(void);
extern void _HPGL_A4_devcpy(void);
extern void _DXY_devcpy(void);
extern void _TEK_devcpy(void);
extern void _grx_devcpy(void);
extern void _VGUI_devcpy(void);
extern void _hgc_devcpy(void);
extern void _mswin_devcpy(void);
extern void _cga_devcpy(void);
extern void _ega_devcpy(void);
extern void _vga_devcpy(void);
extern void _sigma_devcpy(void);
