# Copyright (C) 2018 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

import avg_q
import avg_q.Detector
import os
import copy

class Spindles(avg_q.Detector.Detector):
 threshold_µV=4
 threshold_quantile=0.95
 min_duration_ms=500

 def __init__(self,avg_q_instance,fixed_threshold=False):
  '''Normally threshold_quantile is used, however if the file contains either sections with
     many movements (strong signals) or empty sections (zeros) it can be preferable to use
     a given fixed threshold threshold_µV.
  '''
  avg_q.Detector.Detector.__init__(self,avg_q_instance)
  self.fixed_threshold=fixed_threshold
 def __del__(self):
  """Destructor."""
  if self.filteredfile and os.path.exists(self.filteredfile):
   os.unlink(self.filteredfile.replace('.vhdr','.eeg'))
   os.unlink(self.filteredfile.replace('.vhdr','.vmrk'))
   os.unlink(self.filteredfile)
 def set_Epochsource(self,epochsource):
  # This must be used instead of the Script base class 'add_Epochsource' for any file output
  self.Epochsource_list=[epochsource]
  self.base,ext=os.path.splitext(self.Epochsource_list[0].infile.filename)
  self.indir,name=os.path.split(self.base)
  self.Spindletriggers=None
 def detect_Spindles(self,outtrigfile=None,maxvalue=None,filteredfile=None):
  '''If filteredfile is given, the filtered data will be saved to this file in HDF format
     for later classification of the spindle frequency and topography.
     Note that this file is deleted when this detector is deleted.
  '''
  # self.Spindletriggers will be reused if already set
  if self.Spindletriggers is not None: return
  self.filteredfile=filteredfile
  # Save and restore the current list of transforms, since we measure by (temporally)
  # appending to this list
  storetransforms=copy.copy(self.transforms)
  self.add_transform('''
fftfilter 0 0 10.5Hz 11.5Hz  15.5Hz 16.5Hz 1 1
%(save_filtered)s
calc abs
sliding_average 200ms 100ms
collapse_channels
''' % {
  'save_filtered': ('write_brainvision '+self.filteredfile+' IEEE_FLOAT_32') if self.filteredfile else '',
  })
  if self.fixed_threshold:
   self.add_transform('''
write_crossings collapsed %(threshold)g stdout
''' % {
   'threshold': self.threshold_µV,
   })
  else:
   self.add_transform('''
scale_by invpointquantile %(threshold_quantile)g
write_crossings collapsed 1 stdout
''' % {
   'threshold_quantile': self.threshold_quantile,
   })
  self.Spindletriggers=self.detect(outtrigfile, maxvalue)
  self.Spindleranges=self.get_ranges(self.Spindletriggers,min_length=self.min_duration_ms/1000*self.sfreq)
  self.transforms=storetransforms
 def classify_Spindles(self):
  f=avg_q.avg_q_file(self.filteredfile,'BrainVision')
  self.Spindles=[] # Tuples (start_s,duration_ms,Spindle_info dict)
  for collapsedchannel,(start,end) in self.Spindleranges:
   duration_ms=(end-start)/self.sfreq*1000
   # Count the zero crossings
   detector=avg_q.Detector.Detector(self.avg_q_instance)
   epochsource=avg_q.Epochsource(f,'100ms','%gms' % (duration_ms+200)) # Add 100ms on both ends
   epochsource.set_trigpoints('%gs' % (start/self.sfreq))
   detector.add_Epochsource(epochsource)
   detector.add_transform('''
write_crossings ALL 0 stdout
''')
   crossings=detector.detect()
   trigger_dict=avg_q.Detector.get_trigger_dict(crossings)
   Spindle_info={}  # Will contain lists [freq_Hz,amplitude]
   for channel,cr in trigger_dict.items():
    period_s=2*(cr[-1][0]-cr[0][0])/(len(cr)-1)/detector.sfreq
    Spindle_info[channel]=[1.0/period_s]

   script=avg_q.Script(self.avg_q_instance)
   script.add_Epochsource(epochsource)
   script.add_transform('''
calc abs
trim -a 0 0
write_generic -P -N stdout string
''')
   for line in script.runrdr():
    channel,amp=line.split('\t')
    Spindle_info[channel].append(float(amp))
   self.Spindles.append((start/self.sfreq,duration_ms,Spindle_info))
