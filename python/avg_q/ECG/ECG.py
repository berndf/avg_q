# Copyright (C) 2008,2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import avg_q
import avg_q.Detector
import avg_q.Channeltypes
import os
import copy

default_sessionaverage_ECGfile='avgECG.asc'

standard_protect_channels=[]

class ECG(avg_q.Detector.Detector):
 ECG_beforetrig='0.2s'
 ECG_aftertrig='0.5s'
 ECG_reject_bandwidth=20000

 def __init__(self,avg_q_instance,ECGtemplate=None):
  avg_q.Detector.Detector.__init__(self,avg_q_instance)
  self.ECGtemplate=ECGtemplate
  self.get_ECG_script="remove_channel -k ?%s\n" % ','.join(avg_q.Channeltypes.ECGchannels)
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
scale_by -n %(protect_channels)s 0
''' % {
   'protect_channels': ','.join(self.protect_channels), 
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
  'avgECGfile': self.avgECGfile})
  rdr=script.runrdr()
  nrofaverages=0
  for line in rdr:
   nrofaverages=int(line)
  return nrofaverages
 def sessionaverage_ECG(self,ECGfiles):
  '''Average the ECG files. The output file self.sessionaverage_ECGfile is default_sessionaverage_ECGfile
  in the current directory, unless set differently by the caller.
  Returns the total number of averages.'''
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
  if self.sessionaverage_ECGfile and os.path.exists(self.sessionaverage_ECGfile):
   return
  ecgfiles=[]
  for infile in infiles:
   self.set_Epochsource(avg_q.Epochsource(infile,continuous=True))
   self.average_ECG()
   ecgfiles.append(self.avgECGfile)
  self.sessionaverage_ECG(ecgfiles)
