# Copyright (C) 2012,2013 Bernd Feige
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
 'EOG LOC',
 'EOG ROC',
 'EOGV',
 'EOGl:M2',
 'EOGr:M1',
 'EOGl',
 'EOGr',
])
ECGchannels=set([
 'Ekg1','Ekg2',
 'Ekg',
 'EKG',
 'ECG',
])
EMGchannels=set([
 'EMG',
 'EMG_L',
 'EMG_R',
 'BEMG1',
 'BEMG2',
 'EMG EMGchin',
 'EMG EMGTible',
 'EMG EMGTibri',
 'EMGchin',
])
AUXchannels=set([
 'EDA',
 'EDAShape',
 'GSR_MR_100_EDA',
 'EVT',
 'Airflow',
 'Thorax',
 'Abdomen',
 'Plethysmogram',
 'SpO2',
 'Pulse',
 'Snoring',
 'Lage',
 'Beweg.',
])
NonEEGChannels=set.union(BipolarEOGchannels,ECGchannels,EMGchannels,AUXchannels)
