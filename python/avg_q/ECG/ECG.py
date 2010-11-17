# Copyright (C) 2008,2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import avg_q.trgfile
import avg_q.Detector
import os

default_sessionaverage_ECGfile='avgECG.asc'

standard_protect_channels=[]

class ECG(avg_q.Detector.Detector):
 ECG_beforetrig='0.2s'
 ECG_aftertrig='0.5s'
 ECG_reject_bandwidth=20000

 def __init__(self,avg_q_object,ECGtemplate=None):
  avg_q.Detector.Detector.__init__(self,avg_q_object)
  self.ECGtemplate=ECGtemplate
  self.get_ECG_script="remove_channel -k ECG\n"
  self.mapfile=None
  self.sessionaverage_ECGfile=None
 def set_infile(self,infile,protect_channels=standard_protect_channels):
  avg_q.Detector.Detector.set_infile(self,infile)
  self.ECGtriggers=None
  self.avgECGfile=self.base+'_ECG.asc'
  self.channelnames=self.avg_q_object.get_description(self.infile,'channelnames')
  self.protect_channels=[x for x in protect_channels if x in self.channelnames]
 def detect_ECG(self):
  # self.ECGtriggers will be reused if already set
  if self.ECGtriggers is not None:
   return
  if self.ECGtemplate:
   self.ECGtriggers=self.detect(self.get_ECG_script+'''
set_channelposition -s ECG 0 0 0
fftfilter 0 0 0.5Hz 0.6Hz
convolve %(ECGtemplate)s 1
fftfilter 5Hz 6Hz 1 1
recode -Inf 0 0 0
scale_by invpointquantile 0.9
#write_crossings -E -R 0.5s ECG 1 triggers
#posplot
write_crossings -E -R 0.5s ECG 1 stdout
null_sink
-
''' % {'ECGtemplate': self.ECGtemplate})
  else:
   self.ECGtriggers=self.detect(self.get_ECG_script+'''
set_channelposition -s ECG 0 0 0
fftfilter 0 0 10Hz 11Hz
recode -Inf 0 0 0
scale_by invpointquantile 0.99
#write_crossings -E -R 0.5s ECG 1 triggers
#posplot
write_crossings -E -R 0.5s ECG 1 stdout
null_sink
-
''')
 def average_ECG(self):
  '''Average ECG events with rejection.
     Returns the number of accepted ECGs.'''
  if os.path.exists(self.avgECGfile):
   ascfile=avg_q.avg_q_file(self.avgECGfile)
   return self.avg_q_object.get_description(ascfile,'nrofaverages')
  self.detect_ECG()
  self.avg_q_object.get_epoch_around_add(self.infile, self.ECGtriggers, self.ECG_beforetrig, self.ECG_aftertrig)
  if len(self.protect_channels)>0:
   postprocess='''
scale_by -n %(protect_channels)s 0
''' % {
   'protect_channels': ','.join(self.protect_channels), 
   }
  else:
   postprocess=''
  self.avg_q_object.get_epoch_around_finish(postprocess+'''
baseline_subtract
reject_bandwidth %(reject_bandwidth)f
average
Post:
query nrofaverages stdout
writeasc -b %(avgECGfile)s
-
''' % {
  'reject_bandwidth': self.ECG_reject_bandwidth, 
  'avgECGfile': self.avgECGfile})
  rdr=self.avg_q_object.runrdr()
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
   return self.avg_q_object.get_description(ascfile,'nrofaverages')
  readasc=''
  for ECGfile in ECGfiles:
   if os.path.exists(ECGfile):
    readasc+='readasc %s\n' % ECGfile
  nrofaverages=0
  if readasc!='':
   self.avg_q_object.write(readasc+'''
average -W
Post:
query nrofaverages stdout
writeasc -b %s
-
''' % self.sessionaverage_ECGfile)
   rdr=self.avg_q_object.runrdr()
   for line in rdr:
    nrofaverages=int(line)
  return nrofaverages
 def sessionAverage(self,infiles):
  if self.sessionaverage_ECGfile and os.path.exists(self.sessionaverage_ECGfile):
   return
  ecgfiles=[]
  for infile in infiles:
   if isinstance(infile,str):
    infile=avg_q.avg_q_file(infile)
   self.set_infile(infile)
   self.average_ECG()
   ecgfiles.append(self.avgECGfile)
  self.sessionaverage_ECG(ecgfiles)
