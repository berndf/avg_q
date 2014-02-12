/*
 * Copyright (C) 1996,2005 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member description' arrays for the Vitaport format
 *			-- Bernd Feige 17.12.1995 */

#include <stdio.h>
#include <read_struct.h>
#include "vitaport.h"

struct_member_description smd_vitaport_fileheader[]={
 {STRUCT_MEMBER_UINT, "Sync"},
 {STRUCT_MEMBER_UINT, "hdlen"},
 {STRUCT_MEMBER_UINT, "chnoffs"},
 {STRUCT_MEMBER_UINT, "hdtyp"},
 {STRUCT_MEMBER_UINT, "knum"}
};
struct_member_description smd_vitaportI_fileheader[]={
 {STRUCT_MEMBER_UINT, "blckslh"},
 {STRUCT_MEMBER_UINT, "bytes"}
};
struct_member_description smd_vitaportII_fileheader[]={
 {STRUCT_MEMBER_UINT, "dlen"},
 {STRUCT_MEMBER_UINT, "Hour"},
 {STRUCT_MEMBER_UINT, "Minute"},
 {STRUCT_MEMBER_UINT, "Second"},
 {STRUCT_MEMBER_UINT, "rstimeflag"},
 {STRUCT_MEMBER_UINT, "Day"},
 {STRUCT_MEMBER_UINT, "Month"},
 {STRUCT_MEMBER_UINT, "Year"},
 {STRUCT_MEMBER_UINT, "Sysvers"},
 {STRUCT_MEMBER_UINT, "scnrate"}
};
struct_member_description smd_vitaportII_fileext[]={
 {STRUCT_MEMBER_UINT, "chdlen"},
 {STRUCT_MEMBER_UINT, "swvers"},
 {STRUCT_MEMBER_UINT, "hwvers"},
 {STRUCT_MEMBER_UINT, "recorder"},
 {STRUCT_MEMBER_UINT, "timer1"},
 {STRUCT_MEMBER_UINT, "sysflg12"}
};

struct_member_description smd_vitaport_channelheader[]={
 {STRUCT_MEMBER_CHAR, "kname"},
 {STRUCT_MEMBER_CHAR, "kunit"},
 {STRUCT_MEMBER_UINT, "datype"},
 {STRUCT_MEMBER_UINT, "dasize"},
 {STRUCT_MEMBER_UINT, "scanfc"},
 {STRUCT_MEMBER_UINT, "fltmsk"},
 {STRUCT_MEMBER_UINT, "stofac"},
 {STRUCT_MEMBER_UINT, "resvd1"},
 {STRUCT_MEMBER_UINT, "mulfac"},
 {STRUCT_MEMBER_UINT, "offset"},
 {STRUCT_MEMBER_UINT, "divfac"},
 {STRUCT_MEMBER_UINT, "resvd2"}
};
struct_member_description smd_vitaportII_channelheader[]={
 {STRUCT_MEMBER_UINT, "mvalue"},
 {STRUCT_MEMBER_UINT, "cflags"},
 {STRUCT_MEMBER_UINT, "hwhp"},
 {STRUCT_MEMBER_UINT, "hwampl"},
 {STRUCT_MEMBER_UINT, "hwlp"},
 {STRUCT_MEMBER_UINT, "nnres1"},
 {STRUCT_MEMBER_UINT, "nnres2"},
 {STRUCT_MEMBER_UINT, "nnres3"},
 {STRUCT_MEMBER_UINT, "nnres4"}
};
struct_member_description smd_vitaportIIrchannelheader[]={
 {STRUCT_MEMBER_UINT, "doffs"},
 {STRUCT_MEMBER_UINT, "dlen"},
 {STRUCT_MEMBER_UINT, "mval"},
 {STRUCT_MEMBER_UINT, "chflg"},
 {STRUCT_MEMBER_UINT, "hpass"},
 {STRUCT_MEMBER_UINT, "ampl"},
 {STRUCT_MEMBER_UINT, "tpass"}
};
