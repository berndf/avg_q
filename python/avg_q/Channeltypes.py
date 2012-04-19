# Copyright (C) 2012 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Utility module defining global default channel sets
"""

# Unipolar EOG channels may qualify as EEG
BipolarEOGchannels=set([
 'Eog',
 'EOG',
 'VEOG',
 'HEOG',
])
ECGchannels=set([
 'Ekg1','Ekg2',
 'Ekg',
 'EKG',
 'ECG',
])
EMGchannels=set([
 'EMG',
 'BEMG1',
 'BEMG2',
])
AUXchannels=set([
 'EDA',
 'EDAShape',
 'GSR_MR_100_EDA',
 'EVT',
])
NonEEGChannels=set.union(BipolarEOGchannels,ECGchannels,EMGchannels,AUXchannels)
