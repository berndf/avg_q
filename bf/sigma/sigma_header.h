/*
 * Copyright (C) 2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Header definition for the sigma PLpro format
 * Helpful information obtained from biosig4c++ http://biosig.sf.net/
 *			-- Bernd Feige 14.08.2020 */

#include "transform.h"

typedef struct {
 char version[16];
 int32_t headlen;
 int32_t pos_channelheader;
 int32_t pos_patinfo;
 int16_t n_channels;
} sigma_file_header;

typedef struct {
 int16_t channelno;
 int16_t notch;
 int8_t sfreq_len;	// 0
 char sfreq[18];
 int8_t LowPass_len;	// 1
 char LowPass[18];
 int8_t HighPass_len;	// 2
 char HighPass[18];
 int8_t gerfac_len;	// 3
 char gerfac[18];
 int16_t Offset;
 int8_t calibration_len;	// 4
 char calibration[18];
 int8_t dimension_len;	// 5
 char dimension[18];
 int8_t Impedance_len;	// 6
 char Impedance[18];
 int8_t Sensitivity_len;	// 7
 char Sensitivity[18];
 int32_t XPos;
 int32_t YPos;
 int8_t Label_len;	// 8
 char Label[8];
 int8_t Reference_len;	// 9
 char Reference[10];
 int8_t Unit_len;	// 10
 char Unit[7];
 int8_t Transducer_len;	// 11
 char Transducer[8];
} sigma_channel_header;

extern struct_member sm_sigma_file[];
extern struct_member sm_sigma_file1[];
extern struct_member sm_sigma_channel[];
extern struct_member_description smd_sigma_file[];
extern struct_member_description smd_sigma_file1[];
extern struct_member_description smd_sigma_channel[];
