/*
 * Copyright (C) 1995-1997,2000,2003,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the synamps format
 *			-- Bernd Feige 6.08.1995 */

#include <stdio.h>
#include <read_struct.h>
#include "freiburg.h"

static char *freiburg_channelnames23[23]={
  "vEOG", "hEOG", "Fp1", "Fpz", "Fp2", "F7", "F3", "Fz", "F4",
  "F8", "T3", "C3", "Cz", "C4", "T4", "T5", "P3", "Pz", "P4",
  "T6", "O1", "Oz", "O2"
};

struct freiburg_channels_struct freiburg_channels[]={
 {23, freiburg_channelnames23},
 {0, NULL}
};

/* 7 Channels= `DAISY' */
static char *sleep_channelnames7[7]={
 "vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG"
};
/* 8 Channels= `RK-GOOFY' */
static char *sleep_channelnames8[8]={
 "vEOG", "hEOG", "C3", "C4", "EMG", "EKG", "BEMGl", "BEMGr"
};
/* 10 Channels= `DAGOBERT' */
static char *sleep_channelnames10[10]={
 "vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz", "EMG", "EKG"
};
/* 11 Channels= `DAGOBERT+ALLES' or RKDagobert? */
static char *sleep_channelnames11[11]={
 "vEOG", "hEOG", "C3", "C4", "EMG", "EKG", "BEMGl", "BEMGr",
/* Nasal airflow, thoracal and abdominal strain gauges: */
 "Airflow", "ATMt", "ATMa"
};
/* 12 Channels= `DAG.+ATM+B' but isn't it rather `DAISY' plus something? */
static char *sleep_channelnames12[12]={
 "vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG", "POLY1", "POLY2", "K25", "K26", "ATM"
};
/* 15 Channels= `DAG.+ATM+B' */
static char *sleep_channelnames15[15]={
 "vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz", "EMG", "EKG", "POLY1", "POLY2", "K25", "K26", "ATM"
};
/* 17 Channnels= `DAGOBERT+DAISY' */
static char *sleep_channelnames17[17]={
 "vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz", "EMG", "EKG",
 "vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG"
};
struct freiburg_channels_struct sleep_channels[]={
 {7, sleep_channelnames7},
 {8, sleep_channelnames8},
 {10, sleep_channelnames10},
 {11, sleep_channelnames11},
 {12, sleep_channelnames12},
 {15, sleep_channelnames15},
 {17, sleep_channelnames17},
 {0, NULL}
};

/* One more variation: Data from Goeppingen is still different */
static char *goeppi_channelnames7[7]={
 "vEOG", "C3", "C4", "hEOGl", "hEOGr", "EMG", "EKG"
};
struct freiburg_channels_struct goeppi_channels[]={
 {7, goeppi_channelnames7},
 {0, NULL}
};

struct_member sm_krieger_lis[]={
 {sizeof(krieger_lis_file), 10, 0, 0},
 {(long)&((krieger_lis_file *)NULL)->short0, 0, 2, 1},
 {(long)&((krieger_lis_file *)NULL)->nr_of_averages, 2, 2, 1},
 {(long)&((krieger_lis_file *)NULL)->RT0, 4, 2, 1},
 {(long)&((krieger_lis_file *)NULL)->RT1, 6, 2, 1},
 {(long)&((krieger_lis_file *)NULL)->short4, 8, 2, 1},
 {0,0,0,0}
};

struct_member sm_BT_file[]={
 {sizeof(BT_file), 64, 0, 0},
 {(long)&((BT_file *)NULL)->nr_of_channels, 0, 2, 1},
 {(long)&((BT_file *)NULL)->sampling_interval_us, 2, 2, 1},
 {(long)&((BT_file *)NULL)->blockluecke, 4, 2, 1},
 {(long)&((BT_file *)NULL)->start_hour, 6, 2, 1},
 {(long)&((BT_file *)NULL)->start_minute, 8, 2, 1},
 {(long)&((BT_file *)NULL)->start_second, 10, 2, 1},
 {(long)&((BT_file *)NULL)->start_day, 12, 2, 1},
 {(long)&((BT_file *)NULL)->start_month, 14, 2, 1},
 {(long)&((BT_file *)NULL)->start_year, 16, 2, 1},
 {(long)&((BT_file *)NULL)->stop_hour, 18, 2, 1},
 {(long)&((BT_file *)NULL)->stop_minute, 20, 2, 1},
 {(long)&((BT_file *)NULL)->stop_second, 22, 2, 1},
 {(long)&((BT_file *)NULL)->unused1, 24, 2, 1},
 {(long)&((BT_file *)NULL)->unused2, 26, 2, 1},
 {(long)&((BT_file *)NULL)->unused3, 28, 2, 1},
 {(long)&((BT_file *)NULL)->unused4, 30, 2, 1},
 {(long)&((BT_file *)NULL)->text[0], 32, 32, 0},
 {0,0,0,0}
};
