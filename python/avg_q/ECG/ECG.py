# Copyright (C) 2008,2010-2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import avg_q
import avg_q.Detector
import avg_q.Channeltypes
import os
import copy

default_sessionaverage_ECGfile='avgECG.asc'

standard_protect_channels=[]
from ..avg_q import channel_list2arg

class ECG(avg_q.Detector.Detector):
 ECG_beforetrig='0.2s'
 ECG_aftertrig='0.5s'
 ECG_reject_bandwidth=20000

 def __init__(self,avg_q_instance,ECGtemplate=None):
  avg_q.Detector.Detector.__init__(self,avg_q_instance)
  self.ECGtemplate=ECGtemplate
  self.get_ECG_script="remove_channel -k ?%s\n" % channel_list2arg(avg_q.Channeltypes.ECGchannels)
  self.mapfile=None
  self.sessionaverage_ECGfile=None
  self.protect_channels=standard_protect_channels
 def set_Epochsource(self,epochsource):
  # This must be used instead of the Script base class 'add_Epochsource' for any file output
  self.Epochsource_list=[epochsource]
  self.base,ext=os.path.splitext(self.Epochsource_list[0].infile.filename)
  self.indir,name=os.path.split(self.base)
  self.ECGtriggers=None
  self.avgECGfile=self.base+'_ECG.asc'
 def detect_ECG(self,outtrigfile=None,maxvalue=None):
  # self.ECGtriggers will be reused if already set
  if self.ECGtriggers is not None: return
  # Save and restore the current list of transforms, since we measure by (temporally)
  # appending to this list
  storetransforms=copy.copy(self.transforms)
  self.add_transform(self.get_ECG_script)
  if self.ECGtemplate:
   self.add_transform('''
set_channelposition -s ECG 0 0 0
fftfilter 0 0 0.5Hz 0.6Hz
convolve %(ECGtemplate)s 1
fftfilter 5Hz 6Hz 1 1
recode -Inf 0 0 0
scale_by invpointquantile 0.9
#write_crossings -E -R 0.5s ECG 1 triggers
#posplot
write_crossings -E -R 0.5s ECG 1 stdout
''' % {'ECGtemplate': self.ECGtemplate})
  else:
   # Note that previous versions filtered "0 0 10Hz 11Hz" here but a systematic
   # optimization between 1Hz and 10Hz shows that "0 0 4Hz 5Hz" yields
   # the best "R peak to other peaks" ratio.
   self.add_transform('''
set_channelposition -s ECG 0 0 0
fftfilter 0 0 4Hz 5Hz
recode -Inf 0 0 0
scale_by invpointquantile 0.98
#write_crossings -E -R 0.5s ECG 1 triggers
#posplot
write_crossings -E -R 0.5s ECG 1 stdout
''')
  self.ECGtriggers=self.detect(outtrigfile, maxvalue)
  self.transforms=storetransforms
 def average_ECG(self):
  '''Average ECG events with rejection.
     Returns the number of accepted ECGs.'''
  if os.path.exists(self.avgECGfile):
   ascfile=avg_q.avg_q_file(self.avgECGfile)
   return self.avg_q_instance.get_description(ascfile,'nrofaverages')
  self.detect_ECG()
  epochsource=avg_q.Epochsource(self.Epochsource_list[0].infile,self.ECG_beforetrig, self.ECG_aftertrig)
  epochsource.set_trigpoints(self.ECGtriggers)
  script=avg_q.Script(self.avg_q_instance)
  script.add_Epochsource(epochsource)
  if self.protect_channels is not None and len(self.protect_channels)>0:
   script.add_transform('''
scale_by -n ?%(protect_channels)s 0
''' % {
    'protect_channels': channel_list2arg(self.protect_channels),
   })
  script.add_transform('''
baseline_subtract
reject_bandwidth %(reject_bandwidth)f
''' % {
   'reject_bandwidth': self.ECG_reject_bandwidth,
  })
  script.set_collect('average')
  script.add_postprocess('''
query nrofaverages stdout
writeasc -b %(avgECGfile)s
''' % {
   'avgECGfile': self.avgECGfile
  })
  rdr=script.runrdr()
  nrofaverages=0
  for line in rdr:
   nrofaverages=int(line)
  return nrofaverages
 def sessionaverage_ECG(self,ECGfiles):
  '''Average the ECG files. The output file self.sessionaverage_ECGfile is default_sessionaverage_ECGfile
  in the current directory, unless set differently by the caller.
  Returns the total number of averages.'''
  # Protect against ECGfiles==[], which results in self.indir being undefined
  if not ECGfiles:
   return 0
  if not self.sessionaverage_ECGfile:
   self.sessionaverage_ECGfile=os.path.join(self.indir,default_sessionaverage_ECGfile)
  if os.path.exists(self.sessionaverage_ECGfile):
   ascfile=avg_q.avg_q_file(self.sessionaverage_ECGfile)
   return self.avg_q_instance.get_description(ascfile,'nrofaverages')
  readasc=''
  for ECGfile in ECGfiles:
   if os.path.exists(ECGfile):
    readasc+='readasc %s\n' % ECGfile
  nrofaverages=0
  if readasc!='':
   self.avg_q_instance.write(readasc+'''
average -W
Post:
query nrofaverages stdout
writeasc -b %s
-
''' % self.sessionaverage_ECGfile)
   rdr=self.avg_q_instance.runrdr()
   for line in rdr:
    nrofaverages=int(line)
  return nrofaverages
 def sessionAverage(self,infiles):
  ecgfiles=[]
  for infile in infiles:
   self.set_Epochsource(avg_q.Epochsource(infile,continuous=True))
   self.average_ECG()
   ecgfiles.append(self.avgECGfile)
  return self.sessionaverage_ECG(ecgfiles)
 def from_trgfile(self,ECGtrgpath,base=None):
  if base is not None:
   self.base=base
  else:
   self.base,ext=os.path.splitext(ECGtrgpath)
   self.base=self.base.replace('_ECG','')
  self.indir,name=os.path.split(self.base)
  trgfile=avg_q.trgfile.trgfile(ECGtrgpath)
  self.ECGtriggers=trgfile.gettuples()
  self.sfreq=float(trgfile.preamble['Sfreq'])
 def get_HRV_asc(self,resample_interval='30s'):
  '''Create {self.base}_HRV.asc with HR and HRV measures.
     By default these are resampled in 30s bins.
     Set resample_interval=None for no resampling (1s steps).
  '''
  import numpy
  import pandas
  import statsmodels.nonparametric.smoothers_lowess
  import scipy.interpolate
  import avg_q.numpy_Script

  s=pandas.Series([x[0] for x in self.ECGtriggers])/self.sfreq
  s.index=s.values

  HR=60/s.diff().iloc[1:]
  HR=HR[HR>20]

  lo=statsmodels.nonparametric.smoothers_lowess.lowess(
   HR,HR.index,
   is_sorted=True,
   frac=0.05)

  ind=HR/lo[:,1]<0.4
  HR[ind]=HR[ind]*3
  ind=HR/lo[:,1]<0.6
  HR[ind]=HR[ind]*2

  ip=scipy.interpolate.interp1d(HR.index,HR,kind='linear',fill_value='extrapolate')
  xvals=numpy.arange(numpy.floor(s.index[1]),numpy.ceil(s.index[-1]))
  rHR=pandas.Series(ip(xvals), index=xvals)

  epoch=avg_q.numpy_Script.numpy_epoch(rHR.values)
  epoch.xdata=rHR.index.values
  epoch.sfreq=1
  epochs=[epoch]

  epochsource=avg_q.numpy_Script.numpy_Epochsource(epochs)
  script=avg_q.Script(self.avg_q_instance)
  script.add_Epochsource(epochsource)
  script.set_collect('append -l')
  # Ranges: LF 0.04–0.15 Hz, HF 0.15– 0.4 Hz (Vandewalle:2007)
  # Order written: HR, LF, HF
  script.add_postprocess('''
push
%(sliding_average)s
writeasc -b -c %(HRVfile)s
# Back to original data and save it again for HRP analysis below
pop
push
detrend -0
push
fftfilter 0 0 0.03Hz 0.04Hz 0.15Hz 0.16Hz 1 1
calc abs
add 1
calc log
%(sliding_average)s
writeasc -a -b -c %(HRVfile)s
pop
fftfilter 0 0 0.14Hz 0.15Hz 0.40Hz 0.41Hz 1 1
calc abs
add 1
calc log
%(sliding_average)s
writeasc -a -b -c %(HRVfile)s

# Now get the original data and convert to heart rate period HRP=1/HR
pop
calc inv
push
%(sliding_average)s
writeasc -a -b -c %(HRVfile)s
pop
detrend -0
push
fftfilter 0 0 0.03Hz 0.04Hz 0.15Hz 0.16Hz 1 1
calc abs
add 1
calc log
%(sliding_average)s
writeasc -a -b -c %(HRVfile)s
pop
fftfilter 0 0 0.14Hz 0.15Hz 0.40Hz 0.41Hz 1 1
calc abs
add 1
calc log
%(sliding_average)s
writeasc -a -b -c %(HRVfile)s
''' % {
   'sliding_average': ' '.join(['sliding_average',str(resample_interval),str(resample_interval)]) if resample_interval is not None else '',
   'HRVfile': self.base+'_HRV.asc',
  })
  script.run()
