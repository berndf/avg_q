/*
 * Copyright (C) 2008,2010,2012-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* Definition of `structure member' arrays for the Inomed format
 *			-- Bernd Feige 24.09.2008 */

#include <stdio.h>
#include <read_struct.h>
#include "Inomed.h"

struct_member sm_MULTI_CHANNEL_CONTINUOUS[]={
 {sizeof(MULTI_CHANNEL_CONTINUOUS), 1108, 0, 0},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->szVersion, 0, 20, 0},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->szDate, 20, 11, 0},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->szTime, 31, 9, 0},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->sSignaltype, 40, 2, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->sNumberOfChannels, 42, 2, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->szComment, 44, 30, 0},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->fSamplingFrequency, 76, 4, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->ulNumberOfSamples, 80, 4, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->Flag, 84, 4, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->BkgndColor, 88, 4, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->GridColor, 92, 4, 1},
 {(long)&((MULTI_CHANNEL_CONTINUOUS *)NULL)->szReserve, 96, 1024-12, 0},
 {0,0,0,0}
};

struct_member sm_CHANNEL[]={
 {sizeof(CHANNEL), 380, 0, 0},
 {(long)&((CHANNEL *)NULL)->sADInputChannelNumber, 0, 2, 1},
 {(long)&((CHANNEL *)NULL)->sFlags, 2, 2, 1},
 {(long)&((CHANNEL *)NULL)->sHeadboxTransferChannel[0], 4, 2, 1},
 {(long)&((CHANNEL *)NULL)->sHeadboxTransferChannel[1], 6, 2, 1},
 {(long)&((CHANNEL *)NULL)->sSound, 8, 2, 1},
 {(long)&((CHANNEL *)NULL)->iColor, 12, 4, 1},
 {(long)&((CHANNEL *)NULL)->iGainHardware, 16, 4, 1},
 {(long)&((CHANNEL *)NULL)->iOffsetHardware, 20, 4, 1},
 {(long)&((CHANNEL *)NULL)->fGainSoftware, 24, 4, 1},
 {(long)&((CHANNEL *)NULL)->fOffsetSoftware, 28, 4, 1},
 {(long)&((CHANNEL *)NULL)->szChannelSite, 32, 30, 0},
 {(long)&((CHANNEL *)NULL)->szElectrodePair[0], 62, 30, 0},
 {(long)&((CHANNEL *)NULL)->szElectrodePair[1], 92, 30, 0},
 {(long)&((CHANNEL *)NULL)->fHardwareHighpassFrequency, 124, 4, 1},
 {(long)&((CHANNEL *)NULL)->Device, 128, 2, 1},
 {(long)&((CHANNEL *)NULL)->AmplTrgMode, 130, 1, 0},
 {(long)&((CHANNEL *)NULL)->Reseve2, 131, 1, 0},
 {(long)&((CHANNEL *)NULL)->fAmplThreshold, 132, 4, 1},
 {(long)&((CHANNEL *)NULL)->fTriggerAmplThreshold, 136, 4, 1},
 {(long)&((CHANNEL *)NULL)->blankingTime, 140, 4, 1},
 {(long)&((CHANNEL *)NULL)->scaling, 144, 8, 1},
 {(long)&((CHANNEL *)NULL)->szReserve, 152, 228, 0},
 {0,0,0,0}
};

struct_member sm_DIG_FILTER[]={
 {sizeof(DIG_FILTER), 100, 0, 0},
 {(long)&((DIG_FILTER *)NULL)->szName, 0, 30, 0},
 {(long)&((DIG_FILTER *)NULL)->fCutoffFrequency, 32, 4, 1},
 {(long)&((DIG_FILTER *)NULL)->szReserve, 36, 64, 0},
 {0,0,0,0}
};

// MER
struct_member sm_PlotWinInfo[]={
 {sizeof(PlotWinInfo), 602, 0, 0},
 {(long)&((PlotWinInfo *)NULL)->PatName, 0, 40, 0},
 {(long)&((PlotWinInfo *)NULL)->Position, 40, 40, 0},
 {(long)&((PlotWinInfo *)NULL)->SamplingRate, 80, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->SpikeCount, 84, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->TriggerLinie, 88, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->SiteNr, 92, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->KanalNr, 96, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->op_id, 100, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->SiteID, 104, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->MaxYValue, 108, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->typ, 112, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->Multiplexing, 116, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->EMGChannels, 120, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->EMGChannelsDesc[0], 124, 40, 0},
 {(long)&((PlotWinInfo *)NULL)->EMGChannelsDesc[1], 164, 40, 0},
 {(long)&((PlotWinInfo *)NULL)->EMGChannelsDesc[2], 204, 40, 0},
 {(long)&((PlotWinInfo *)NULL)->BitsPerValue, 244, 4, 1},
 {(long)&((PlotWinInfo *)NULL)->filler, 248, 354, 0},
 {0,0,0,0}
};
