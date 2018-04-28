# Copyright (C) 2018 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

import os
import copy
import avg_q
import avg_q.Detector
import avg_q.trgfile

class Delta_slope(avg_q.Detector.Detector):
 '''Delta slope measure, cf. Esser:2007
 '''
 # This is 0.5-2.0Hz as in Esser:2007 but can be configured from outside
 bandpass='0 0 0.4Hz 0.5Hz 2.0Hz 2.1Hz 1 1'
 def __init__(self,avg_q_instance):
  avg_q.Detector.Detector.__init__(self,avg_q_instance)
 def set_Epochsource(self,epochsource):
  # This must be used instead of the Script base class 'add_Epochsource' for any file output
  self.Epochsource_list=[epochsource]
  self.base,ext=os.path.splitext(self.Epochsource_list[0].infile.filename)
  self.indir,name=os.path.split(self.base)
  self.values=None
 def get_Delta_slope(self):
  '''The contents of the script at execution must ensure that the data contains only a single channel!
  '''
  # Save and restore the current list of transforms, since we measure by (temporally)
  # appending to this list
  storetransforms=copy.copy(self.transforms)
  self.add_transform('''
detrend -0
fftfilter %(bandpass)s
write_crossings -E ALL 0 stdout
# Terminate the first crossings list:
echo -F stdout EOF\\n
differentiate
write_crossings -E ALL 0 stdout
''' % {
   'bandpass': self.bandpass,
  })
  rdr=self.runrdr()
  trg=avg_q.trgfile.trgfile(rdr)
  crs=trg.gettuples()
  sfreq=float(trg.preamble['Sfreq'])
  dif=avg_q.trgfile.trgfile(rdr).gettuples() # Differential extrema

  lastneg=None
  intervening_pos=[]
  difindex=0
  #print("\t".join(('Time','nPos','Amp','SlopePlus','SlopeMinus','SlopeMax','SlopeMin')))
  self.values=[]
  self.detect
  for point,code,amplitude in crs:
   amplitude=float(amplitude)
   #print("%gs %d %g" % (point/sfreq,code,amplitude))
   if code== -1:
    if lastneg and intervening_pos:
     # Positive wave detected...
     # Find the maximum and minimum slope within the wave
     while difindex<len(dif) and dif[difindex][0]<lastneg[0]: difindex+=1
     if difindex==len(dif):
      mindif=maxdif=None
     else:
      mindif=maxdif=float(dif[difindex][2])
      difindex+=1
      while True:
       amp=float(dif[difindex][2])
       if   amp<mindif: mindif=amp
       elif amp>maxdif: maxdif=amp
       difindex+=1
       if difindex>=len(dif) or dif[difindex][0]>=point: break
     # Find the maximum positivity in the wave
     maxindex=max(range(len(intervening_pos)),key=lambda i: intervening_pos[i][1])
     #print("\t".join([str(x) for x in lastneg+(len(intervening_pos),)+intervening_pos[maxindex]+(point,amplitude,mindif,maxdif)]))
     # Time[s] Amp[µV] SlopePlus[µV/s] SlopeMinus[µV/s] SlopeMax[µV/s] SlopeMin[µV/s]
     maxpospoint,maxposamp=intervening_pos[maxindex]
     #print("%.3f\t%d\t%g\t%g\t%g\t%g\t%g" % (maxpospoint/sfreq,len(intervening_pos),maxposamp,(maxposamp-lastneg[1])/((maxpospoint-lastneg[0])/sfreq),(maxposamp-amp)/((maxpospoint-point)/sfreq),maxdif*sfreq,mindif*sfreq))
     self.values.append([maxpospoint/sfreq,len(intervening_pos),maxposamp,(maxposamp-lastneg[1])/((maxpospoint-lastneg[0])/sfreq),(maxposamp-amp)/((maxpospoint-point)/sfreq),maxdif*sfreq,mindif*sfreq])
     intervening_pos=[]
    lastneg=(point,amplitude)
   else:
    if lastneg:
     intervening_pos.append((point,amplitude))
  self.transforms=storetransforms
