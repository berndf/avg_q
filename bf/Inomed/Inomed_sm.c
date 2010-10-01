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
