/*
 * Copyright (C) 1995,1996 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* This module holds global values that are needed for the kn (Patrick Berg)
 * library.			-- Bernd Feige 4.06.1995
 */

#include "transform.h"
#include "bf.h"
#include "rawlimit.h"
#include "rawdatan.h"

/* maximum sizes of arrays - must be defined in main program */
unsigned int MAXtrials = MAXTRIALS;
unsigned int MAXdata = MAXDATA;
unsigned int MAXdataperchannel = MAXDATAPERCHANNEL;
unsigned int MAXtrarray = MAXTRARRAY;
unsigned int MAXhdstring = MAXHDSTRING;
unsigned int MAXhdarray = MAXHDARRAY; 

struct external_methods_struct *kn_emethods;

#ifdef OBVIOUSLY_OBSOLETE
int        parsize,      /* size of parameter block */
                   adrsize,      /* size of address block */
                   parpos;       /* position of parameters */
#endif OBVIOUSLY_OBSOLETE

struct_member sm_Rheader[]={
/* The first entry, which gives the struct length, cannot be
 * sizeof(struct Rheader) because that would include those values at the end
 * which are defined in the struct but not read from the file... */
 {(long)&((struct Rheader *)NULL)->headarray[6], 40, 0, 0},
 {(long)&((struct Rheader *)NULL)->headsize, 0, 2, 1},
 {(long)&((struct Rheader *)NULL)->headstringsize, 2, 2, 1},
 {(long)&((struct Rheader *)NULL)->trials, 4, 2, 1},
 {(long)&((struct Rheader *)NULL)->maxtrials, 6, 2, 1},
 {(long)&((struct Rheader *)NULL)->conditions, 8, 2, 1},
 {(long)&((struct Rheader *)NULL)->maxchannels, 10, 2, 1},
 {(long)&((struct Rheader *)NULL)->nexttrial, 12, 4, 1},
 {(long)&((struct Rheader *)NULL)->starttime, 16, 4, 1},
 {(long)&((struct Rheader *)NULL)->endtime, 20, 4, 1},
 {(long)&((struct Rheader *)NULL)->subject, 24, 2, 1},
 {(long)&((struct Rheader *)NULL)->group, 26, 2, 1},
 {(long)&((struct Rheader *)NULL)->headarray[0], 28, 2, 1},
 {(long)&((struct Rheader *)NULL)->headarray[1], 30, 2, 1},
 {(long)&((struct Rheader *)NULL)->headarray[2], 32, 2, 1},
 {(long)&((struct Rheader *)NULL)->headarray[3], 34, 2, 1},
 {(long)&((struct Rheader *)NULL)->headarray[4], 36, 2, 1},
 {(long)&((struct Rheader *)NULL)->headarray[5], 38, 2, 1},
 {0,0,0,0}
};

struct_member sm_Rtrial[]={
 {(long)(&((struct Rtrial *)NULL)->nexttrial+1), 62, 0, 0},
 {(long)&((struct Rtrial *)NULL)->trialno, 0, 2, 1},
 {(long)&((struct Rtrial *)NULL)->spare[0], 2, 2, 1},
 {(long)&((struct Rtrial *)NULL)->spare[1], 4, 2, 1},
 {(long)&((struct Rtrial *)NULL)->nkan, 6, 2, 1},
 {(long)&((struct Rtrial *)NULL)->nabt, 8, 2, 1},
 {(long)&((struct Rtrial *)NULL)->ndat, 10, 4, 1},
 {(long)&((struct Rtrial *)NULL)->condition[0], 14, 2, 1},
 {(long)&((struct Rtrial *)NULL)->condition[1], 16, 2, 1},
 {(long)&((struct Rtrial *)NULL)->condition[2], 18, 2, 1},
 {(long)&((struct Rtrial *)NULL)->marker[0], 20, 2, 1},
 {(long)&((struct Rtrial *)NULL)->marker[1], 22, 2, 1},
 {(long)&((struct Rtrial *)NULL)->marker[2], 24, 2, 1},
 {(long)&((struct Rtrial *)NULL)->marker[3], 26, 2, 1},
 {(long)&((struct Rtrial *)NULL)->marker[4], 28, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[0], 30, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[1], 32, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[2], 34, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[3], 36, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[4], 38, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[5], 40, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[6], 42, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialarray[7], 44, 2, 1},
 {(long)&((struct Rtrial *)NULL)->trialsize, 46, 4, 1},
 {(long)&((struct Rtrial *)NULL)->trialhead, 50, 4, 1},
 {(long)&((struct Rtrial *)NULL)->lasttrial, 54, 4, 1},
 {(long)&((struct Rtrial *)NULL)->nexttrial, 58, 4, 1},
 {0,0,0,0}
};
