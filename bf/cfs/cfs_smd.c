/*
 * Copyright (C) 2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member description' arrays for the NeuroFile format
 *			-- Bernd Feige 26.09.1997 */

#include <stdio.h>
#include <read_struct.h>
#include "cfs.h"

struct_member_description smd_TFilChInfo[]={
 {STRUCT_MEMBER_CHAR, "chanName"},
 {STRUCT_MEMBER_CHAR, "unitsY"},
 {STRUCT_MEMBER_CHAR, "unitsX"},
 {STRUCT_MEMBER_INT, "dType"},
 {STRUCT_MEMBER_INT, "dKind"},
 {STRUCT_MEMBER_INT, "dSpacing"},
 {STRUCT_MEMBER_INT, "otherChan"},
};

struct_member_description smd_TDSChInfo[]={
 {STRUCT_MEMBER_INT, "dataOffset"},
 {STRUCT_MEMBER_INT, "dataPoints"},
 {STRUCT_MEMBER_FLOAT, "scaleY"},
 {STRUCT_MEMBER_FLOAT, "offsetY"},
 {STRUCT_MEMBER_FLOAT, "scaleX"},
 {STRUCT_MEMBER_FLOAT, "offsetX"},
};

struct_member_description smd_TDataHead[]={
 {STRUCT_MEMBER_INT, "lastDS"},
 {STRUCT_MEMBER_INT, "dataSt"},
 {STRUCT_MEMBER_INT, "dataSz"},
 {STRUCT_MEMBER_INT, "flags"},
};

struct_member_description smd_TFileHead[]={
 {STRUCT_MEMBER_CHAR, "marker"},
 {STRUCT_MEMBER_CHAR, "name"},
 {STRUCT_MEMBER_INT, "fileSz"},
 {STRUCT_MEMBER_CHAR, "timeStr"},
 {STRUCT_MEMBER_CHAR, "dateStr"},
 {STRUCT_MEMBER_INT, "dataChans"},
 {STRUCT_MEMBER_INT, "filVars"},
 {STRUCT_MEMBER_INT, "datVars"},
 {STRUCT_MEMBER_INT, "fileHeadSz"},
 {STRUCT_MEMBER_INT, "dataHeadSz"},
 {STRUCT_MEMBER_INT, "endPnt"},
 {STRUCT_MEMBER_INT, "dataSecs"},
 {STRUCT_MEMBER_INT, "diskBlkSize"},
 {STRUCT_MEMBER_CHAR, "commentStr"},
 {STRUCT_MEMBER_INT, "tablePos"},
};

struct_member_description smd_TVarDesc[]={
 {STRUCT_MEMBER_CHAR, "varDesc"},
 {STRUCT_MEMBER_INT, "vType"},
 {STRUCT_MEMBER_CHAR, "varUnits"},
 {STRUCT_MEMBER_INT, "vSize"},
};
