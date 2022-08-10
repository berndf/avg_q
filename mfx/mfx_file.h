/*
 * Copyright (C) 1992-1994,1997,2003,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * mfx_file.h definitions for implementation and use of mfx_file.c
 * Bernd Feige 23.11.1992
 */

#ifndef MFX_FILE_H
#define MFX_FILE_H

#include <stdio.h>
#include <stdint.h>
#include "mfx_header_format.h"
#include "mfx_data_format.h"

/*{{{}}}*/
/*{{{  TRUE, FALSE*/
#ifndef Bool
#define Bool int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/*}}}  */

/* Compatibility: On SUNs, bcopy is good for overlapping copies */
#ifdef NO_MEMMOVE
#define memmove(to,from,N) bcopy(from,to,N)
#endif

/*
 * The possible data types on the user side of the module
 */

enum mfx_datatypes {MFX_SHORTS, MFX_FLOATS, MFX_DOUBLES};

/*
 * The known subversive mfx format changes
 */

enum mfx_subversions {SUBVERSION_TILL1192, SUBVERSION_PAST1192};

/*
 * This is central file structure. Pointers on instances of this structure
 * are used as file handles (exactly as in stdio)
 */

typedef struct MFX_FILE_STRUCT {
 FILE*	mfxstream;
 long	data_offset;
 long	mfxpos;
 long	mfxlen;
 struct mfx_header_format_data 	typeheader;
 struct mfx_header_data001	fileheader;
 struct mfx_channel_data001*	channelheaders;
 int	was_just_created;	/* Flag: 1 if header has yet to be written */
 int	nr_of_channels_selected;/* Length of the following array */
 int*	selected_channels;	/* List of selected channel numbers (starting with 1) */
 int16_t* input_buffer;		/* Holds 1 row of data (1 data point, all channels) */
 enum	mfx_datatypes datatype;
 enum	mfx_subversions subversion;
 char	filename[256];
 char	filemode[32];
} MFX_FILE;

/*
 * The mfx error numbers. The array mfx_errors[] of corresponding plain 
 * text messages is defined in mfx_errors.c.
 * The number of the last error gets stored in mfx_lasterr, which is a
 * global variable defined by the mfx_file module
 */

typedef enum mfx_errnums {MFX_NOERR, MFX_NOMEM1, MFX_NOMEM2, 
	MFX_NOFILE,
	MFX_READERR1, MFX_READERR2, MFX_READERR3, 
	MFX_READERR4, MFX_WRITEERR4,
	MFX_UNKNOWNHTYPE, MFX_UNKNOWNDTYPE, MFX_FILELEN,
	MFX_CHANSELNOMEM, MFX_WRONGCHANNELNAME, MFX_NOCHANNEL, 
	MFX_SEEKERROR, MFX_NEGSEEK, MFX_PLUSSEEK,
	MFX_SEEKINCONSISTENT, MFX_ALLOCERR, 
	MFX_EMPTYEPOCH, MFX_EXPANDNONUM, MFX_EXPANDINTERVAL,
	MFX_EXPANDUNTERMINATED, MFX_HEADERFLUSHERR, MFX_HEADERACCESS
	} mfx_errtype;

extern mfx_errtype mfx_lasterr;
extern char *mfx_errors[];
extern char *mfx_version;	/* Defined by the mfx_file main module */

/*
 * Function declarations
 */

MFX_FILE*   mfx_open(char const *filename, char const *filemode, enum mfx_datatypes datatype);
MFX_FILE*   mfx_create(char const *filename, int nr_of_channels, enum mfx_datatypes datatype);
mfx_errtype mfx_describefile(MFX_FILE* mfxfile, char const *file_descriptor, float sample_period, int pts_per_epoch, float epoch_begin_latency);
mfx_errtype mfx_describechannel(MFX_FILE* mfxfile, int channel_nr, char const *channelname, char const *yaxis_label, unsigned short data_type, double ymin, double ymax);
mfx_errtype mfx_close(MFX_FILE* mfxfile);
int	    mfx_get_channel_number(MFX_FILE* mfxfile, char const * channelname);
int	    mfx_cselect(MFX_FILE* mfxfile, char const * channelnames);
mfx_errtype mfx_read(void *tobuffer, long count, MFX_FILE *mfxfile);
mfx_errtype mfx_write(void *frombuffer, long count, MFX_FILE *mfxfile);
long	    mfx_tell(MFX_FILE* mfxfile);
mfx_errtype mfx_seek(MFX_FILE* mfxfile, long offset, int whence);
int	    mfx_eof(MFX_FILE* mfxfile);
char*	    mfx_getchannelname(MFX_FILE* mfxfile, int number);
void*	    mfx_alloc(MFX_FILE* mfxfile, long count);
int	    mfx_seektrigger(MFX_FILE* mfxfile, char const * triggerchannel, int triggercode);
void*	    mfx_getepoch(MFX_FILE* mfxfile, long* epochlenp, long beforetrig, 
	    long aftertrig, char const * triggerchannel, int triggercode);
mfx_errtype mfx_infoout(MFX_FILE* mfxfile, FILE* outfile);

#endif	/* ifndef MFX_FILE_H */
