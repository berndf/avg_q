/*
 * Copyright (C) 2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the CED "cfs" format
 *			-- Bernd Feige 27.02.2016 */

#include <stdio.h>
#include <read_struct.h>
#include "cfs.h"

struct_member sm_TFilChInfo[]={
 {sizeof(TFilChInfo), 48, 0, 0},
 {(long)&((TFilChInfo *)NULL)->chanName, 0, 22, 0},
 {(long)&((TFilChInfo *)NULL)->unitsY, 22, 10, 0},
 {(long)&((TFilChInfo *)NULL)->unitsX, 32, 10, 0},
 {(long)&((TFilChInfo *)NULL)->dType, 42, 1, 0},
 {(long)&((TFilChInfo *)NULL)->dKind, 43, 1, 0},
 {(long)&((TFilChInfo *)NULL)->dSpacing, 44, 2, 0},
 {(long)&((TFilChInfo *)NULL)->otherChan, 46, 2, 0},
 {0,0,0,0}
};

struct_member sm_TDSChInfo[]={
 {sizeof(TDSChInfo), 24, 0, 0},
 {(long)&((TDSChInfo *)NULL)->dataOffset, 0, 4, 0},
 {(long)&((TDSChInfo *)NULL)->dataPoints, 4, 4, 0},
 {(long)&((TDSChInfo *)NULL)->scaleY, 8, 4, 0},
 {(long)&((TDSChInfo *)NULL)->offsetY, 12, 4, 0},
 {(long)&((TDSChInfo *)NULL)->scaleX, 16, 4, 0},
 {(long)&((TDSChInfo *)NULL)->offsetX, 20, 4, 0},
 {0,0,0,0}
};

struct_member sm_TDataHead[]={
 {sizeof(TDataHead), 30, 0, 0},
 {(long)&((TDataHead *)NULL)->lastDS, 0, 4, 0},
 {(long)&((TDataHead *)NULL)->dataSt, 4, 4, 0},
 {(long)&((TDataHead *)NULL)->dataSz, 8, 4, 0},
 {(long)&((TDataHead *)NULL)->flags, 12, 2, 0},
 //{(long)&((TDataHead *)NULL)->dSpace, 14, 16, 0},
 {0,0,0,0}
};

struct_member sm_TFileHead[]={
 {sizeof(TFileHead), 178, 0, 0}, //??
 {(long)&((TFileHead *)NULL)->marker, 0, 8, 0},
 {(long)&((TFileHead *)NULL)->name, 8, 14, 0},
 {(long)&((TFileHead *)NULL)->fileSz, 22, 4, 0},
 {(long)&((TFileHead *)NULL)->timeStr, 26, 8, 0},
 {(long)&((TFileHead *)NULL)->dateStr, 34, 8, 0},
 {(long)&((TFileHead *)NULL)->dataChans, 42, 2, 0},
 {(long)&((TFileHead *)NULL)->filVars, 44, 2, 0},
 {(long)&((TFileHead *)NULL)->datVars, 46, 2, 0},
 {(long)&((TFileHead *)NULL)->fileHeadSz, 48, 2, 0},
 {(long)&((TFileHead *)NULL)->dataHeadSz, 50, 2, 0},
 {(long)&((TFileHead *)NULL)->endPnt, 52, 4, 0},
 {(long)&((TFileHead *)NULL)->dataSecs, 56, 2, 0},
 {(long)&((TFileHead *)NULL)->diskBlkSize, 58, 2, 0},
 {(long)&((TFileHead *)NULL)->commentStr, 60, 74, 0},
 {(long)&((TFileHead *)NULL)->tablePos, 134, 4, 0},
 //{(long)&((TFileHead *)NULL)->fSpace, 138, 40, 0},
 {0,0,0,0}
};

struct_member sm_TVarDesc[]={
 {sizeof(TVarDesc), 36, 0, 0},
 {(long)&((TVarDesc *)NULL)->varDesc, 0, 22, 0},
 {(long)&((TVarDesc *)NULL)->vType, 22, 1, 0},
 {(long)&((TVarDesc *)NULL)->varUnits, 24, 10, 0},
 {(long)&((TVarDesc *)NULL)->vSize, 34, 2, 0},
 {0,0,0,0}
};
