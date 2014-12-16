# Copyright (C) 2008-2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import avg_q
import avg_q.Detector
import avg_q.Channeltypes
import os
import copy

default_sessionaverage_EOGfile='avgEOG.asc'
default_sessionaverage_HEOGfile='avgHEOG.asc'
from ..avg_q import channel_list2arg

class EOG(avg_q.Detector.Detector):
 VEOG_beforetrig='0.2s'
 VEOG_aftertrig='0.5s'
 VEOG_offset='-0.25s'
 VEOG_minamp,VEOG_maxamp=60,500
 HEOG_minamp=60

 def __init__(self,avg_q_instance):
  avg_q.Detector.Detector.__init__(self,avg_q_instance)
  self.get_VEOG_script="remove_channel -k VEOG\n"
  self.mapfile=None
  self.sessionaverage_EOGfile=None
  self.sessionaverage_HEOGfile=None
  self.protect_channels=avg_q.Channeltypes.NonEEGChannels
  self.VEOGtriggers=None
  self.HEOGtriggers=None
  # Used when averaging
  self.preprocess=None
 def set_Epochsource(self,epochsource):
  # This must be used instead of the Script base class 'add_Epochsource' for any file output
  self.Epochsource_list=[epochsource]
  self.base,ext=os.path.splitext(self.Epochsource_list[0].infile.filename)
  self.indir,name=os.path.split(self.base)
  self.avgEOGfile=self.base+'_EOG.asc'
  self.avgHEOGfile=self.base+'_HEOG.asc'
  self.VEOGtriggers=None
  self.HEOGtriggers=None
 def detect_VEOG(self,outtrigfile=None):
  # self.VEOGtriggers will be reused if already set
  if self.VEOGtriggers is not None: return
  # Save and restore the current list of transforms, since we measure by (temporally)
  # appending to this list
  self.save_state()
  self.add_transform(self.get_VEOG_script)
  self.add_transform('''
set_channelposition -s VEOG 0 0 0
fftfilter 0 0 0.3Hz 0.4Hz 3Hz 3.2Hz 1 1
recode -Inf 0 0 0
write_crossings -E VEOG %(VEOG_minamp)f stdout
#write_crossings -E VEOG %(VEOG_minamp)f triggers
#posplot
''' % {
  'VEOG_minamp': self.VEOG_minamp,
  })
  self.VEOGtriggers=self.detect(outtrigfile, maxvalue=self.VEOG_maxamp)
  self.restore_state()
 def detect_HEOG(self,outtrigfile=None):
  # self.VEOGtriggers will be reused if already set
  if self.HEOGtriggers is not None: return
  # Save and restore the current list of transforms, since we measure by (temporally)
  # appending to this list
  self.save_state()
  self.add_transform(self.get_HEOG_script)
  # To be able to measure both deviations in positive and negative directions,
  # we duplicate the channel as HEOGp and HEOGm, inverting the latter.
  self.add_transform('''
set_channelposition -s HEOG 0 0 0
fftfilter 0 0 0.05Hz 0.1Hz 1Hz 1.1Hz 1 1
remove_channel -k HEOG,HEOG
set_channelposition -s HEOGp 0 0 0 HEOGm 1 0 0
scale_by -n HEOGm -1
write_crossings ALL %(HEOG_minamp)f stdout
#write_crossings ALL %(HEOG_minamp)f triggers
#posplot
''' % {
  'HEOG_minamp': self.HEOG_minamp,
  })
  self.HEOGtriggers=self.detect(outtrigfile)
  self.restore_state()
 def average_EOG(self):
  '''Average EOG events with strict checks for duration and surroundings.
     Returns the number of accepted EOGs.'''
  if os.path.exists(self.avgEOGfile):
   ascfile=avg_q.avg_q_file(self.avgEOGfile)
   return self.avg_q_instance.get_description(ascfile,'nrofaverages')
  self.detect_VEOG()
  epochsource=avg_q.Epochsource(self.Epochsource_list[0].infile,self.VEOG_beforetrig, self.VEOG_aftertrig, offset=self.VEOG_offset)
  epochsource.set_trigpoints(self.VEOGtriggers)
  script=avg_q.Script(self.avg_q_instance)
  script.add_Epochsource(epochsource)
  if self.protect_channels is not None and len(self.protect_channels)>0:
   script.add_transform('''
scale_by -n ?%(protect_channels)s 0
''' % {
   'protect_channels': channel_list2arg(self.protect_channels),
   })
  if self.preprocess:
   script.add_transform(self.preprocess)
  # Note that we require the VEOG signal to be "close to baseline" before and after the maximum
  script.add_transform('''
baseline_subtract
push
%(get_VEOG_script)s
trim -x 0 100 400 500
calc abs
reject_bandwidth -m %(max_VEOG_amp_outside_window)f
pop
''' % {
  'get_VEOG_script': self.get_VEOG_script,
  'max_VEOG_amp_outside_window': self.VEOG_minamp
  })
  script.set_collect('average')
  script.add_postprocess('''
query nrofaverages stdout
writeasc -b %(avgEOGfile)s
''' % {
  'avgEOGfile': self.avgEOGfile})
  rdr=script.runrdr()
  nrofaverages=0
  for line in rdr:
   nrofaverages=int(line)
  return nrofaverages
 def average_HEOG(self):
  '''Average HEOG sections.
  '''
  if os.path.exists(self.avgHEOGfile):
   return
  self.detect_HEOG()
  nrofaverages=0
  ranges=self.get_ranges(self.HEOGtriggers)
  for direction in ['HEOGp','HEOGm']:
   script=avg_q.Script(self.avg_q_instance)
   for d,(start,end) in ranges:
    if d!=direction: continue
    epochsource=avg_q.Epochsource(self.Epochsource_list[0].infile,0,end-start)
    epochsource.set_trigpoints(start)
    script.add_Epochsource(epochsource)
   if self.protect_channels is not None and len(self.protect_channels)>0:
    script.add_transform('''
scale_by -n ?%(protect_channels)s 0
''' % {
    'protect_channels': channel_list2arg(self.protect_channels),
    })
   if self.preprocess:
    script.add_transform(self.preprocess)
   script.add_transform('''
trim -a 0 0
''')
   script.set_collect('average')
   script.add_postprocess('''
query nrofaverages stdout
writeasc -a -b %(avgHEOGfile)s
''' % {
   'avgHEOGfile': self.avgHEOGfile})
   rdr=script.runrdr()
   for line in rdr:
    nrofaverages+=int(line)
  return nrofaverages
 def sessionaverage_EOG(self,EOGfiles):
  '''Average the EOG files. The output file self.sessionaverage_EOGfile is default_sessionaverage_EOGfile
  in the current directory, unless set differently by the caller.
  Returns the total number of averages.'''
  if not self.sessionaverage_EOGfile:
   self.sessionaverage_EOGfile=os.path.join(self.indir,default_sessionaverage_EOGfile)
  if os.path.exists(self.sessionaverage_EOGfile):
   ascfile=avg_q.avg_q_file(self.sessionaverage_EOGfile)
   return self.avg_q_instance.get_description(ascfile,'nrofaverages')
  readasc=''
  for EOGfile in EOGfiles:
   if os.path.exists(EOGfile):
    readasc+='readasc %s\n' % EOGfile
  nrofaverages=0
  if readasc!='':
   self.avg_q_instance.write(readasc+'''
average -W
Post:
query nrofaverages stdout
writeasc -b %s
-
''' % self.sessionaverage_EOGfile)
   rdr=self.avg_q_instance.runrdr()
   for line in rdr:
    nrofaverages=int(line)
  return nrofaverages
 def sessionaverage_HEOG(self,HEOGfiles):
  '''Average the HEOG files. The output file self.sessionaverage_HEOGfile is default_sessionaverage_HEOGfile
  in the current directory, unless set differently by the caller.
  Returns the total number of averages.'''
  if not self.sessionaverage_HEOGfile:
   self.sessionaverage_HEOGfile=os.path.join(self.indir,default_sessionaverage_HEOGfile)
  if os.path.exists(self.sessionaverage_HEOGfile):
   ascfile=avg_q.avg_q_file(self.sessionaverage_HEOGfile)
   return self.avg_q_instance.get_description(ascfile,'nrofaverages')
  nrofaverages=0
  for fromepoch in range(1,3):
   readasc=''
   for HEOGfile in HEOGfiles:
    if os.path.exists(HEOGfile):
     readasc+='readasc -f %d -e 1 %s\n' % (fromepoch,HEOGfile)
   if readasc!='':
    self.avg_q_instance.write(readasc+'''
average -W
Post:
query nrofaverages stdout
writeasc -a -b %s
-
''' % self.sessionaverage_HEOGfile)
    rdr=self.avg_q_instance.runrdr()
    for line in rdr:
     nrofaverages+=int(line)
  return nrofaverages
 def get_Gratton_map(self,avgEOGfile):
  self.mapfile,ext=os.path.splitext(avgEOGfile)
  self.mapfile+='map.asc'
  if os.path.exists(self.mapfile):
   return
  self.avg_q_instance.write('''
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
  rdr=self.avg_q_instance.runrdr()
  meanval=float(next(rdr))
  for line in rdr:
   print(line)
  self.avg_q_instance.write('''
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
  self.avg_q_instance.run()
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
  self.avg_q_instance.getcontepoch(self.Epochsource_list[0].infile, 0, '1s')
  self.avg_q_instance.write('''
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
  self.avg_q_instance.getcontepoch(self.Epochsource_list[0].infile, 0, '1s',trigtransfer=True)
  self.avg_q_instance.write('''
subtract -e %(EOGcomponentfile)s
write_hdf -c %(correctedfile)s
null_sink
-
''' % {
  'EOGcomponentfile': EOGcomponentfile,
  'correctedfile': self.correctedfile,
  })
  self.avg_q_instance.run()
  os.unlink(EOGcomponentfile)
 def sessionAverage(self,infiles):
  eogfiles=[]
  for infile in infiles:
   self.set_Epochsource(avg_q.Epochsource(infile,continuous=True))
   self.average_EOG()
   eogfiles.append(self.avgEOGfile)
  return self.sessionaverage_EOG(eogfiles)
 def sessionAverageHEOG(self,infiles):
  heogfiles=[]
  for infile in infiles:
   self.set_Epochsource(avg_q.Epochsource(infile,continuous=True))
   self.average_HEOG()
   heogfiles.append(self.avgHEOGfile)
  return self.sessionaverage_HEOG(heogfiles)
 def sessionGratton(self,infiles):
  self.sessionAverage(infiles)
  for infile in infiles:
   self.set_Epochsource(avg_q.Epochsource(infile,continuous=True))
   self.Gratton()
  if self.mapfile:
   os.unlink(self.mapfile)
   self.mapfile=None
  if self.sessionaverage_EOGfile:
   os.unlink(self.sessionaverage_EOGfile)
   self.sessionaverage_EOGfile=None
