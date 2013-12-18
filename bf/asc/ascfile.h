/*
 * Copyright (C) 1993-1995,2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#ifndef _ASCFILE_H
#define _ASCFILE_H

#if 0
 {{{  Description
 This header file contains common definitions for the 'asc' file format,
 which is a format to store epochs of data including channel names and
 probe positions, the name and values of the x axis and the epoch values
 themselves.  It is conceived as an intermediate file and result format
 for the transform system.  The epochs do NOT necessarily share channel
 number or names or anything else other than the values defined in the
 variable file header section.  Each epoch can have a 'z value' together
 with a unit assigned to it, eg to hold the time if spectra for different
 time windows are stored.  Each data point or 'item' itself can be a
 tuple of points, its size specified by 'itemsize'.

 There is an actual ascii file version (the default) and a binary file
 version which is written if the binary option is given and recognized
 automatically when read.

 {{{  ASCII file format
 Variable file header section with lines of the type keyword=value, e.g:
 Sfreq=100.00
 THEN, POSSIBLY REPEATED:
 comment line [ZAXIS_DELIMITERz_label'='z_value]
 'channels='nr_of_channels ', points='nr_of_points [', itemsize='itemsize]
 xname channel1_name ... channelN_name
 '-'   channel1_posx ... channelN_posx
 '-'   channel1_posy ... channelN_posy
 '-'   channel1_posz ... channelN_posz
 xval1 channel1_val1 ... channelN_val1
 ...
 xvalM channel1_valM ... channelN_valM

 If itemsize>1, the values in each tupel are enclosed in braces.
 }}}

 {{{  BINARY file format
 Start:
 (unsigned short) ASC_BF_MAGIC
 (short)length_of_variable_header
  Variable file header consisting of lines (terminated by \n to be read
  using fgets) of the form keyword=value\n

 THEN, POSSIBLY REPEATED:

 (int) nr_of_channels, nr_of_points, itemsize, multiplexed
  where multiplexed is the flag taken directly from the tinfo structure
  to describe the value ordering within the epoch: ==TRUE if channels
  are adjacent (and repeated over points), ==FALSE if points are adjacent
  (and repeated over channels).

 (int)length_of_string_section
  String section consisting of zero-terminated strings for:
  comment line [ZAXIS_DELIMITERz_label'='z_value], xname, channel_names

 (double)probepos[nr_of_channels*3]
  where xyz are adjacent and repeated over channels.

 (DATATYPE)xdata[nr_of_points]

 (DATATYPE)data[nr_of_points*nr_of_channels*itemsize]

 (int)length_of_preceeding_dataset
  where the length of this whole epoch data set INCLUDING this int value
  is given to enable programs to skip backwards to the start of the
  previous data set.
 }}}
 }}}
#endif

#define ASC_BF_FLOAT_MAGIC 0xbffb
#define ASC_BF_DOUBLE_MAGIC 0xbddb
#define OLD_ASC_BF_MAGIC 0xbfbf
#define ZAXIS_DELIMITER ";Zaxis:"
#ifdef FLOAT_DATATYPE
#define ASC_BF_MAGIC ASC_BF_FLOAT_MAGIC
#endif
#ifdef DOUBLE_DATATYPE
#define ASC_BF_MAGIC ASC_BF_DOUBLE_MAGIC
#endif

#endif
