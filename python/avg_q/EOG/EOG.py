# Copyright (C) 2008-2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import avg_q.trgfile
import avg_q.Detector
import os

default_sessionaverage_EOGfile='avgEOG.asc'

standard_protect_channels=[
 'ECG',
 'EKG',
 'EDA',
 'EDAShape',
 'GSR_MR_100_EDA',
 'Ekg1','Ekg2',
 'Eog',
 'Ekg',
 'BEMG1',
 'BEMG2',
 'EVT',
]

class EOG(avg_q.Detector.Detector):
 VEOG_beforetrig='0.2s'
 VEOG_aftertrig='0.5s'
 VEOG_offset='-0.25s'
 VEOG_minamp,VEOG_maxamp=60,500

 def __init__(self,avg_q_object):
  avg_q.Detector.Detector.__init__(self,avg_q_object)
  self.get_VEOG_script="remove_channel -k VEOG\n"
  self.mapfile=None
  self.sessionaverage_EOGfile=None
 def set_infile(self,infile,protect_channels=standard_protect_channels):
  avg_q.Detector.Detector.set_infile(self,infile)
  self.VEOGtriggers=None
  self.avgEOGfile=self.base+'_EOG.asc'
  self.channelnames=self.avg_q_object.get_description(self.infile,'channelnames')
  self.protect_channels=[x for x in protect_channels if x in self.channelnames]
 def detect_VEOG(self,outtrigfile=None):
  # self.VEOGtriggers will be reused if already set
  if self.VEOGtriggers is not None:
   return
  self.VEOGtriggers=self.detect(self.get_VEOG_script+'''
set_channelposition -s EOG 0 0 0
fftfilter 0 0 0.3Hz 0.4Hz 3Hz 3.2Hz 1 1
recode -Inf 0 0 0
write_crossings -E EOG %(VEOG_minamp)f stdout
#write_crossings -E EOG %(VEOG_minamp)f triggers
#posplot
null_sink
-
''' % {
  'VEOG_minamp': self.VEOG_minamp,
 }, outtrigfile, maxvalue=self.VEOG_maxamp)
 def average_EOG(self):
  '''Average EOG events with strict checks for duration and surroundings.
     Returns the number of accepted EOGs.'''
  if os.path.exists(self.avgEOGfile):
   ascfile=avg_q.avg_q_file(self.avgEOGfile)
   return self.avg_q_object.get_description(ascfile,'nrofaverages')
  self.detect_VEOG()
  self.avg_q_object.get_epoch_around_add(self.infile, self.VEOGtriggers, self.VEOG_beforetrig, self.VEOG_aftertrig, offset=self.VEOG_offset)
  if len(self.protect_channels)>0:
   postprocess='''
scale_by -n %(protect_channels)s 0
''' % {
   'protect_channels': ','.join(self.protect_channels), 
   }
  else:
   postprocess=''
  # Note that we require the VEOG signal to be "close to baseline" before and after the maximum
  self.avg_q_object.get_epoch_around_finish(postprocess+'''
baseline_subtract
push
%(get_VEOG_script)s
trim -x 0 100 400 500
calc abs
reject_bandwidth -m %(max_VEOG_amp_outside_window)f
pop
average
Post:
query nrofaverages stdout
writeasc -b %(avgEOGfile)s
-
''' % {
  'get_VEOG_script': self.get_VEOG_script, 
  'max_VEOG_amp_outside_window': self.VEOG_minamp, 
  'avgEOGfile': self.avgEOGfile})
  rdr=self.avg_q_object.runrdr()
  nrofaverages=0
  for line in rdr:
   nrofaverages=int(line)
  return nrofaverages
 def sessionaverage_EOG(self,EOGfiles):
  '''Average the EOG files. The output file self.sessionaverage_EOGfile is default_sessionaverage_EOGfile
  in the current directory, unless set differently by the caller.
  Returns the total number of averages.'''
  if not self.sessionaverage_EOGfile:
   self.sessionaverage_EOGfile=os.path.join(self.indir,default_sessionaverage_EOGfile)
  if os.path.exists(self.sessionaverage_EOGfile):
   ascfile=avg_q.avg_q_file(self.sessionaverage_EOGfile)
   return self.avg_q_object.get_description(ascfile,'nrofaverages')
  readasc=''
  for EOGfile in EOGfiles:
   if os.path.exists(EOGfile):
    readasc+='readasc %s\n' % EOGfile
  nrofaverages=0
  if readasc!='':
   self.avg_q_object.write(readasc+'''
average -W
Post:
query nrofaverages stdout
writeasc -b %s
-
''' % self.sessionaverage_EOGfile)
   rdr=self.avg_q_object.runrdr()
   for line in rdr:
    nrofaverages=int(line)
  return nrofaverages
 def get_Gratton_map(self,avgEOGfile):
  self.mapfile,ext=os.path.splitext(avgEOGfile)
  self.mapfile+='map.asc'
  if os.path.exists(self.mapfile):
   return
  self.avg_q_object.write('''
readasc %(avgEOGfile)s
%(get_VEOG_script)s
trim -x -a 100 400
write_generic stdout string
null_sink
-
''' % {
  'avgEOGfile': avgEOGfile,
  'get_VEOG_script': self.get_VEOG_script, 
  })
  rdr=self.avg_q_object.runrdr()
  meanval=float(next(rdr))
  for line in rdr:
   print(line)
  self.avg_q_object.write('''
readasc %(avgEOGfile)s
trim -x -a 100 400
scale_by %(factor)g
#posplot
writeasc -b %(mapfile)s
null_sink
-
''' % {
  'avgEOGfile': avgEOGfile,
  'factor': 1.0/meanval, 
  'mapfile': self.mapfile,
  })
  self.avg_q_object.run()
 def Gratton(self,avgEOGfile=None):
  self.correctedfile=self.base+'_corrected.hdf'
  if os.path.exists(self.correctedfile):
   return
  if not avgEOGfile:
   if self.sessionaverage_EOGfile:
    avgEOGfile=self.sessionaverage_EOGfile
   else:
    avgEOGfile=self.avgEOGfile
  self.get_Gratton_map(avgEOGfile)
  EOGcomponentfile=self.base+'_EOGcomponent.asc'
  self.avg_q_object.getcontepoch(self.infile, 0, '1s')
  self.avg_q_object.write('''
%(get_VEOG_script)s
project -n -m %(mapfile)s 0
writeasc -b %(EOGcomponentfile)s
null_sink
-
''' % {
  'get_VEOG_script': self.get_VEOG_script, 
  'mapfile': self.mapfile,
  'EOGcomponentfile': EOGcomponentfile, 
  })
  self.avg_q_object.getcontepoch(self.infile, 0, '1s',trigtransfer=True)
  self.avg_q_object.write('''
subtract -e %(EOGcomponentfile)s
write_hdf -c %(correctedfile)s
null_sink
-
''' % {
  'EOGcomponentfile': EOGcomponentfile,
  'correctedfile': self.correctedfile, 
  })
  self.avg_q_object.run()
  os.unlink(EOGcomponentfile)
 def sessionAverage(self,infiles):
  if self.sessionaverage_EOGfile and os.path.exists(self.sessionaverage_EOGfile):
   return
  eogfiles=[]
  for infile in infiles:
   if isinstance(infile,str):
    infile=avg_q.avg_q_file(infile)
   self.set_infile(infile)
   self.average_EOG()
   eogfiles.append(self.avgEOGfile)
  self.sessionaverage_EOG(eogfiles)
 def sessionGratton(self,infiles):
  self.sessionAverage(infiles)
  for infile in infiles:
   if isinstance(infile,str): infile=avg_q.avg_q_file(infile)
   self.set_infile(infile)
   self.Gratton()
  if self.mapfile:
   os.unlink(self.mapfile)
   self.mapfile=None
  if self.sessionaverage_EOGfile:
   os.unlink(self.sessionaverage_EOGfile)
   self.sessionaverage_EOGfile=None
