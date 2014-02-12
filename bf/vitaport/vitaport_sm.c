/*
 * Copyright (C) 1996,2000 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the Vitaport format
 *			-- Bernd Feige 17.12.1995 */

#include <stdio.h>
#include <read_struct.h>
#include "vitaport.h"

/* Should I comment my opinion about having a length field in the tag entries
 * and still castrating oneself into using fixed-length marker arrays??? */
struct vitaport_idiotic_multiplemarkertablenames markertable_names[]={
 {"MARKER", 100},
 {"MRK500", 500},
 {NULL, 0}
};

struct_member sm_vitaport_fileheader[]={
 {sizeof(struct vitaport_fileheader), 8, 0, 0},
 {(long)&((struct vitaport_fileheader *)NULL)->Sync, 0, 2, 1},
 {(long)&((struct vitaport_fileheader *)NULL)->hdlen, 2, 2, 1},
 {(long)&((struct vitaport_fileheader *)NULL)->chnoffs, 4, 2, 1},
 {(long)&((struct vitaport_fileheader *)NULL)->hdtyp, 6, 1, 1},
 {(long)&((struct vitaport_fileheader *)NULL)->knum, 7, 1, 1},
 {0,0,0,0}
};
struct_member sm_vitaportI_fileheader[]={
 {sizeof(struct vitaportI_fileheader), 4, 0, 0},
 {(long)&((struct vitaportI_fileheader *)NULL)->blckslh, 0, 2, 1},
 {(long)&((struct vitaportI_fileheader *)NULL)->bytes, 2, 2, 1},
 {0,0,0,0}
};
struct_member sm_vitaportII_fileheader[]={
 {sizeof(struct vitaportII_fileheader), 14, 0, 0},
 {(long)&((struct vitaportII_fileheader *)NULL)->dlen, 0, 4, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Hour, 4, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Minute, 5, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Second, 6, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->rstimeflag, 7, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Day, 8, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Month, 9, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Year, 10, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->Sysvers, 11, 1, 1},
 {(long)&((struct vitaportII_fileheader *)NULL)->scnrate, 12, 2, 1},
 {0,0,0,0}
};
struct_member sm_vitaportII_fileext[]={
 {sizeof(struct vitaportII_fileext), 14, 0, 0},
 {(long)&((struct vitaportII_fileext *)NULL)->chdlen, 0, 2, 1},
 {(long)&((struct vitaportII_fileext *)NULL)->swvers, 2, 2, 1},
 {(long)&((struct vitaportII_fileext *)NULL)->hwvers, 4, 2, 1},
 {(long)&((struct vitaportII_fileext *)NULL)->recorder, 6, 4, 1},
 {(long)&((struct vitaportII_fileext *)NULL)->timer1, 10, 2, 1},
 {(long)&((struct vitaportII_fileext *)NULL)->sysflg12, 12, 2, 1},
 {0,0,0,0}
};

struct_member sm_vitaport_channelheader[]={
 {sizeof(struct vitaport_channelheader), 24, 0, 0},
 {(long)&((struct vitaport_channelheader *)NULL)->kname, 0, 6, 0},
 {(long)&((struct vitaport_channelheader *)NULL)->kunit, 6, 4, 0},
 {(long)&((struct vitaport_channelheader *)NULL)->datype, 10, 1, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->dasize, 11, 1, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->scanfc, 12, 1, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->fltmsk, 13, 1, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->stofac, 14, 1, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->resvd1, 15, 1, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->mulfac, 16, 2, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->offset, 18, 2, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->divfac, 20, 2, 1},
 {(long)&((struct vitaport_channelheader *)NULL)->resvd2, 22, 2, 1},
 {0,0,0,0}
};
struct_member sm_vitaportII_channelheader[]={
 {sizeof(struct vitaportII_channelheader), 16, 0, 0},
 {(long)&((struct vitaportII_channelheader *)NULL)->mvalue, 0, 2, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->cflags, 2, 1, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->hwhp, 3, 1, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->hwampl, 4, 2, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->hwlp, 6, 2, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->nres1, 8, 2, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->nres2, 10, 2, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->nres3, 12, 2, 1},
 {(long)&((struct vitaportII_channelheader *)NULL)->nres4, 14, 2, 1},
 {0,0,0,0}
};
struct_member sm_vitaportIIrchannelheader[]={
 {sizeof(struct vitaportIIrchannelheader), 16, 0, 0},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->doffs, 0, 4, 1},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->dlen, 4, 4, 1},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->mval, 8, 2, 1},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->chflg, 10, 1, 1},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->hpass, 11, 1, 1},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->ampl, 12, 2, 1},
 {(long)&((struct vitaportIIrchannelheader *)NULL)->tpass, 14, 2, 1},
 {0,0,0,0}
};
