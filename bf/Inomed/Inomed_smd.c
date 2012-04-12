/*
 * Copyright (C) 2008 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Definition of `structure member' arrays for the Inomed format
 *			-- Bernd Feige 24.09.2008 */

#include <stdio.h>
#include <read_struct.h>
#include "Inomed.h"

struct_member_description smd_MULTI_CHANNEL_CONTINUOUS[]={
 {STRUCT_MEMBER_CHAR, "szVersion"},
 {STRUCT_MEMBER_CHAR, "szDate"},
 {STRUCT_MEMBER_CHAR, "szTime"},
 {STRUCT_MEMBER_INT, "sSignaltype"},
 {STRUCT_MEMBER_INT, "sNumberOfChannels"},
 {STRUCT_MEMBER_CHAR, "szComment"},
 {STRUCT_MEMBER_FLOAT, "fSamplingFrequency"},
 {STRUCT_MEMBER_INT, "ulNumberOfSamples"},
 {STRUCT_MEMBER_INT, "Flag"},
 {STRUCT_MEMBER_INT, "BkgndColor"},
 {STRUCT_MEMBER_INT, "GridColor"},
 {STRUCT_MEMBER_CHAR, "szReserve"},
};

struct_member_description smd_CHANNEL[]={
 {STRUCT_MEMBER_INT, "sADInputChannelNumber"},
 {STRUCT_MEMBER_INT, "sFlags"},
 {STRUCT_MEMBER_INT, "sHeadboxTransferChannel[0]"},
 {STRUCT_MEMBER_INT, "sHeadboxTransferChannel[1]"},
 {STRUCT_MEMBER_INT, "sSound"},
 {STRUCT_MEMBER_INT, "iColor"},
 {STRUCT_MEMBER_INT, "iGainHardware"},
 {STRUCT_MEMBER_INT, "iOffsetHardware"},
 {STRUCT_MEMBER_FLOAT, "fGainSoftware"},
 {STRUCT_MEMBER_FLOAT, "fOffsetSoftware"},
 {STRUCT_MEMBER_CHAR, "szChannelSite"},
 {STRUCT_MEMBER_CHAR, "szElectrodePair[0]"},
 {STRUCT_MEMBER_CHAR, "szElectrodePair[1]"},
 {STRUCT_MEMBER_FLOAT, "fHardwareHighpassFrequency"},
 {STRUCT_MEMBER_INT, "Device"},
 {STRUCT_MEMBER_INT, "AmplTrgMode"},
 {STRUCT_MEMBER_INT, "Reseve2"},
 {STRUCT_MEMBER_FLOAT, "fAmplThreshold"},
 {STRUCT_MEMBER_FLOAT, "fTriggerAmplThreshold"},
 {STRUCT_MEMBER_FLOAT, "blankingTime"},
 {STRUCT_MEMBER_FLOAT, "scaling"},
 {STRUCT_MEMBER_CHAR, "szReserve"},
};

struct_member_description smd_DIG_FILTER[]={
 {STRUCT_MEMBER_CHAR, "szName"},
 {STRUCT_MEMBER_FLOAT, "fCutoffFrequency"},
 {STRUCT_MEMBER_CHAR, "szReserve"},
};

// MER
struct_member_description smd_PlotWinInfo[]={
 {STRUCT_MEMBER_CHAR, "PatName"},
 {STRUCT_MEMBER_CHAR, "Position"},
 {STRUCT_MEMBER_INT, "SamplingRate"},
 {STRUCT_MEMBER_INT, "SpikeCount"},
 {STRUCT_MEMBER_INT, "TriggerLinie"},
 {STRUCT_MEMBER_INT, "SiteNr"},
 {STRUCT_MEMBER_INT, "KanalNr"},
 {STRUCT_MEMBER_INT, "op_id"},
 {STRUCT_MEMBER_INT, "SiteID"},
 {STRUCT_MEMBER_INT, "MaxYValue"},
 {STRUCT_MEMBER_INT, "typ"},
 {STRUCT_MEMBER_INT, "Multiplexing"},
 {STRUCT_MEMBER_INT, "EMGChannels"},
 {STRUCT_MEMBER_CHAR, "EMGChannelsDesc[0]"},
 {STRUCT_MEMBER_CHAR, "EMGChannelsDesc[1]"},
 {STRUCT_MEMBER_CHAR, "EMGChannelsDesc[2]"},
 {STRUCT_MEMBER_INT, "BitsPerValue"},
 {STRUCT_MEMBER_CHAR, "filler"},
};
