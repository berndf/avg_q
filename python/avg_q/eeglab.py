# Copyright (C) 2024 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
EEGLab/matlab utilities.
"""

import h5py

def EEGLab_Epochsource(infile):
 import avg_q.numpy_Script
 from . import avg_q_file
 filename=infile.filename if isinstance(infile,avg_q_file) else infile
 f=h5py.File(filename)
 g=f[list(f.keys())[1]] # First is '#refs#', second is the name of the first dataset
 if 'cfg' not in g:
  print("EEGLab_Epochsource error: %s is not an EEGLab file" % filename)
  return
 # 6 Keys: <KeysViewHDF5 ['cfg', 'fsample', 'label', 'sampleinfo', 'time', 'trial']>
 c=g['cfg']
 # <KeysViewHDF5 ['absdiff', 'baselinewindow', 'boxcar', 'bpfiltdev', 'bpfiltdf', 'bpfiltdir', 'bpfilter', 'bpfiltord',
 # 'bpfilttype', 'bp filtwintype', 'bpinstabilityfix', 'bsfiltdev', 'bsfiltdf', 'bsfiltdir', 'bsfilter', 'bsfiltord',
 # 'bsfilttype', 'bsfiltwintype', 'bsinstabilityf ix', 'callinfo', 'channel', 'chantype', 'checkmaxfilter', 'checkpath',
 # 'coilaccuracy', 'conv', 'coordsys', 'custom', 'dataformat', 'demean', 'd enoise', 'derivative', 'detrend', 'dftfilter',
 # 'dftfreq', 'dftinvert', 'feedback', 'headerformat', 'hilbert', 'hpfiltdev', 'hpfiltdf', 'hpfiltdir', 'hpfilter',
 # 'hpfiltord', 'hpfilttype', 'hpfiltwintype', 'hpinstabilityfix', 'implicitref', 'lpfiltdev', 'lpfiltdf', 'lpfiltdir',
 # 'lpfilter', 'lpfiltord', 'lpfilttype', 'lpfiltwintype', 'lpinstabilityfix', 'medianfilter', 'medianfiltord', 'method',
 # 'montage', 'outputfilepresent', 'padding', 'paddir', 'padtype', 'plotfiltresp', 'polyorder', 'polyremoval', 'precision',
 # 'previous', 'progress', 'rectify', 'refchannel', 'refmethod', 'removeeog', 'removemcg', 'reref', 'resample', 'standardize',
 # 'subspace', 'toolbox', 'trials', 'updatesens', 'usefftfilt', 'version']>

 channelnames=[''.join([chr(x[0]) for x in g[ref]]) for ref in c['channel'][0]]
 sfreq=g['fsample'][0][0]
 beforetrig=c['baselinewindow'][0][0] # There are 3 values, meaning is unclear

 timearr=g['time'] # n_samples HDF5 object references
 ref=timearr[0][0]
 time=g[ref][:,0]

 trialarr=g['trial'] # n_trials HDF5 object references

 epochs=[]
 for ref in trialarr[:,0]:
  data=g[ref] # (nr_of_points, nr_of_channels)

  epoch=avg_q.numpy_Script.numpy_epoch(data)
  epoch.sfreq=sfreq
  epoch.beforetrig=beforetrig
  epoch.xdata=time
  epoch.xchannelname='Time[s]'
  epoch.channelnames=channelnames
  epochs.append(epoch)
 epochsource=avg_q.numpy_Script.numpy_Epochsource(epochs)
 return epochsource
