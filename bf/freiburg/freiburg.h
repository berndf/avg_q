/*
 * Copyright (C) 1995-1997,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#define FREIBURG_HEADER_LENGTH 64

/* This is just one header fragment as seen in some averaged files.
 * Seems that the structure approach is not quite usable for this
 * `soft' format... */
typedef struct {
 short short0;
 short nr_of_averages;
 short RT0;
 short RT1;
 short short4;
} krieger_lis_file;

typedef struct {
 short nr_of_channels;
 short sampling_interval_us;
 short blockluecke;
 short start_hour;
 short start_minute;
 short start_second;
 short start_day;
 short start_month;
 short start_year;
 short stop_hour;
 short stop_minute;
 short stop_second;
 short unused1;
 short unused2;
 short unused3;
 short unused4;
 char text[32];
} BT_file;

struct freiburg_channels_struct {
 int nr_of_channels;
 char **channelnames;
};

extern struct_member sm_krieger_lis[];
extern struct_member sm_BT_file[];
extern struct freiburg_channels_struct freiburg_channels[];
extern struct freiburg_channels_struct sleep_channels[];
extern struct freiburg_channels_struct goeppi_channels[];
