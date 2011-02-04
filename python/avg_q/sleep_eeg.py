# vim: set fileencoding=utf-8 :
# Copyright (C) 2009 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Specialized class derived from avg_q including methods for
sleep spectral analysis.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
import os

# Helpers for the get_measures method
stagenames2stagelist={
 '2': ['2'],
 'NREM': ['2','3','4'],
 'REM': ['5'],
}

cntfile_paths=(
 '/home/charly/tmp/autosleep/',
)

class sleep_eeg(avg_q.avg_q):
 dcache=None
 defaultbands=[
  # From, To, Name (all strings)
  ['0.1', '48', 'Total'],
  ['0.1', '1', 'Delta1'],
  ['1+1', '3.5', 'Delta2'],
  ['3.5+1', '8', 'Theta'],

  ['8+1', '12', 'Alpha'],
  ['12+1', '16', 'Sigma'],

  ['16+1', '24', 'Beta1'],
  ['24+1', '32', 'Beta2'],
  ['32+1', '48', 'Gamma']
 ]
 rejection_bands=[
  ['0.1', '48', 'Total'],
  ['32+1', '48', 'Gamma']
 ]
 rejection_median_length=20
 def locate_cnt(self,bookno):
  import klinik
  if not self.dcache:
   import idircache
   self.dcache=idircache.idircache(extensionstrip=('cnt'))
  file_bookno=klinik.file_bookno(bookno)
  cntfile=self.dcache.find(cntfile_paths,file_bookno)
  return cntfile
 def channelname_subset(self,channelnames,fromHz=None,toHz=None):
  subset=[]
  for channelname in channelnames:
   Hz=channelname.index('Hz')
   freq=float(channelname[0:Hz])
   # If any bound is left out, the selection is unbounded to that side
   if not ((fromHz and freq<fromHz) or (toHz and freq>=toHz)):
    subset.append(channelname)
  return subset
 def get_channel_collapse_args(self,infile,bands=defaultbands):
  '''For a continuous spectral analysis using channel names of the form 'xxxHz',
  get a list of collapse arguments for collapsing the given bands'''
  channelnames=self.get_description(infile,'channelnames')
  #print channelnames
  collapse_args=[]
  for band in bands:
   fromHz,toHz,bandname=band
   toHz=float(toHz)
   if fromHz.endswith('+1'):
    fromHz=fromHz[0:-2]
   fromHz=float(fromHz)
   subset=self.channelname_subset(channelnames,fromHz,toHz)
   collapse_args.append(",".join(subset)+':'+bandname)
  #print collapse_args
  return collapse_args
 def get_cntbands(self,infile,collapse_args):
  self.getcontepoch(infile,0,0)
  self.write('''
# NeuroScan files store the sampling freq as int; we have 1/30s...
>set sfreq 0.03333333333
>calc exp
>set leaveright 1
>collapse_channels %(collapse_args)s
>set leaveright 0
>calc log
''' % {'collapse_args': ' '.join(collapse_args)})
 def cnt2trg(self,cntfile):
  from . import avg_q_file
  first,ext=os.path.splitext(cntfile)
  if not ext:
   # No extension: Assume this is a book number
   cntfile=self.locate_cnt(cntfile)
   if not cntfile:
    print("cnt2trg: Can't locate cnt file for book number %s" % first)
    return
  tfile=cntfile.replace('.cnt','.trg')
  if os.path.exists(tfile) and os.path.getsize(tfile)>0:
   return
  import slfile
  cntdir,bookno=os.path.split(first)
  try:
   sl=slfile.slfile(bookno)
  except:
   print("Can't locate sl file for %s" % bookno)
   return
  print("Found %s" % sl.filename)

  c=avg_q_file(cntfile)
  collapse_args=self.get_channel_collapse_args(c,bands=self.rejection_bands)
  self.get_cntbands(c,collapse_args)
  self.write('''
write_generic stdout string
echo -F stdout End of bands\\n
sliding_average -M %(median_length)d 1
write_generic stdout string
echo -F stdout End of medbands\\n
null_sink
-
''' % {
  'median_length': self.rejection_median_length,
  })
  rdr=self.runrdr()
  nr_of_bands=len(self.rejection_bands)
  bands=[[] for x in range(nr_of_bands)]
  for r in rdr:
   if r=='End of bands':
    break
   r=r.split('\t')
   for i in range(nr_of_bands):
    bands[i].append(float(r[i]))
  medbands=[[] for x in range(nr_of_bands)]
  for r in rdr:
   if r=='End of medbands':
    break
   r=r.split('\t')
   for i in range(nr_of_bands):
    medbands[i].append(float(r[i]))
  for r in rdr:
   pass
  #print bands
  #print medbands
  import copy
  sorted_medbands=copy.deepcopy(medbands)
  medians_of_medianfiltered=[]
  quartiles_of_medianfiltered=[]
  diff_threshold=[]
  for i in range(nr_of_bands):
   sorted_medbands[i].sort()
   medians_of_medianfiltered.append(sorted_medbands[i][len(sorted_medbands[i])/2])
   quartiles_of_medianfiltered.append(sorted_medbands[i][len(sorted_medbands[i])/4])
   diff_threshold.append(medians_of_medianfiltered[i]-quartiles_of_medianfiltered[i])
  print("medians_of_medianfiltered=" + str(medians_of_medianfiltered))
  print("quartiles_of_medianfiltered=" + str(quartiles_of_medianfiltered))
  del sorted_medbands

  sl_pos=1
  point=0
  nr_of_points=len(bands[0])
  tuples=[]
  while point<nr_of_points:
   checkmark=0
   checkmarks=[0 for i in range(nr_of_bands)]
   if point<0:
    time,stage,checks,arousals,myos,eyemovements,remcycle,nremcycle=-1,0,0,0,0,0,-1,-1
   elif sl_pos>=len(sl.tuples):
    # Let the REM cycle remain on the last value it had, otherwise we'd always
    # be averaging wake phases at the beginning and end of the night...
    time, stage, checks, arousals, myos, eyemovements=-1, 0, 0, 0, 0, 0
   else:
    time, stage, checks, arousals, myos, eyemovements=sl.tuples[sl_pos]
    remcycle, nremcycle=sl.remcycles[sl_pos]
    for i in range(nr_of_bands):
     if abs(bands[i][point]-medbands[i][point])>diff_threshold[i]:
      checkmark+= 1<<i+3
      checkmarks[i]+=1
    sl_pos+=1

   check=arousals+2*checks+checkmark
   code=(0 if remcycle<0 else remcycle)*8+stage+1
   if check!=0: code= -code 
   tuples.append([point, code, stage, remcycle, nremcycle, arousals, myos, eyemovements, checks]+checkmarks)
   point+=1

  # Finally write the result...
  from . import trgfile
  t=trgfile.trgfile()
  t.preamble['fields']="\t".join(['point', 'code', 'stage', 'remcycle', 'nremcycle', 'arousals', 'myos', 'emts', 'checks']+['checkmark_'+band[2] for band in self.rejection_bands])
  f=open(tfile,"w")
  t.writetuples(tuples,f)
  f.close()
  sl.close()
 def spect_from_cnt(self,cntfile,checkpoint,postprocess,average_options=''):
  """Average epoch spectra using .trg file.
  checkpoint is a callback function deciding which points to average
  postprocess defines what to do with the resulting single epoch
  average_options may e.g. be '-t -u' for variance assessments"""
  first,ext=os.path.splitext(cntfile)
  if not ext:
   # No extension: Assume this is a book number
   cntfile=self.locate_cnt(cntfile)
   if not cntfile:
    print("spect_from_cnt: Can't locate cnt file for book number %s" % first)
    return
  tfile=cntfile.replace('.cnt','.trg')
  self.write('''
read_synamps -R stdin %(cntfile)s 0 1
average %(average_options)s
Post:
swap_fc
%(postprocess)s
-
''' % {
   'cntfile': cntfile, 
   'postprocess': postprocess,
   'average_options': average_options},
  )

  from . import trgfile
  t=trgfile.trgfile(tfile)
  for point,code,codes in t:
   point=int(point)
   code=int(code)
   (stage, remcycle, nremcycle, arousals, myos, eyemovements, checks, checkmark_Total, checkmark_Gamma)=codes.split("\t")
   remcycle=int(remcycle)
   nremcycle=int(nremcycle)
   arousals=int(arousals)
   myos=int(myos)
   eyemovements=int(eyemovements)
   checks=int(checks)
   checkmark_Total=(checkmark_Total=='1')
   checkmark_Gamma=(checkmark_Gamma=='1')

   if checkpoint(point,code,stage,remcycle,nremcycle,arousals,myos,eyemovements,checks,checkmark_Total,checkmark_Gamma):
    self.write("%d\t1\n" % point)
  self.write("0\t0\n") # A 0 code indicates end of the trigger list
 def get_spect_trim(self,bands=defaultbands):
  trim='trim -x -s'
  for fromHz,toHz,name in bands:
   trim+= ' '+fromHz+' '+toHz
  return trim
 def get_epoch_selector(self,stagename,cycle=None):
  stagelist=stagenames2stagelist[stagename]
  return (lambda point,code,stage,remcycle,nremcycle,arousals,myos,eyemovements,checks,checkmark_Total,checkmark_Gamma:
   stage in stagelist and code>0 and (cycle==None or remcycle==cycle))
 def average_spectra(self,cntfiles,stage,cycle,postprocess,average_flags=''):
  import tempfile
  tmpfile=tempfile.mktemp()
  append=''
  for cntfile in cntfiles:
   self.spect_from_cnt(cntfile,self.get_epoch_selector(stage,cycle),'''
writeasc %(append)s -b %(tmpfile)s
''' % {'tmpfile': tmpfile, 'append': append})
   append='-a'
  self.write('''
readasc %(tmpfile)s
average %(average_flags)s
Post:
%(postprocess)s
-
''' % {'tmpfile': tmpfile, 'average_flags': average_flags, 'postprocess': postprocess})
  self.run()
  os.unlink(tmpfile)
 def get_measures(self,cntfile,stage,cycle,bands=defaultbands):
  '''Average epochs from cnt and directly measure the result'''
  self.spect_from_cnt(cntfile,self.get_epoch_selector(stage,cycle), '''
trim -x 0+1 48
calc exp
'''+self.get_spect_trim(bands)+'''
calc log
query -N nrofaverages
write_generic -P stdout string
''')
  outline=[]
  for line in self.runrdr():
   if '=' in line:
    varname,value=line.split('=')
    if varname=='nrofaverages':
     outline.append(int(value))
   else:
    yield outline+[float(x) for x in line.split('\t')]
    outline=[]
 def spect_measure(self,bands=defaultbands,postprocess=''):
  """Generator yielding spectral measurements"""
  epochendstring="End of epoch"
  self.write(self.get_spect_trim(bands) + '''
%(postprocess)s
write_generic -P stdout string
echo -F stdout %(epochendstring)s\\n
null_sink
-
''' % {'postprocess': postprocess, 'epochendstring': epochendstring}
)
  # One frequency list per channel
  spect=[]
  for line in self.runrdr():
   if line==epochendstring:
    yield spect
    spect=[]
   else:
    spect.append([float(x) for x in line.split('\t')])
 def get_Delta_slope(self,bookno):
  from . import sleep_file
  from . import trgfile
  try:
   f=sleep_file.sleep_file(bookno)
  except:
   print("Raw file for %s can't be located!" % bookno)
   return None
  self.getcontepoch(f,'0','0')
  self.write('''
remove_channel -k ?C3,EEGC3,EEG_C3,EEG\\ C3,EEG\\ C3\\-A2,EEG1
# Bandpass 0.5-2.0Hz
add negpointmean
fftfilter 0 0 0.4Hz 0.5Hz 2.0Hz 2.1Hz 1 1
write_crossings -E ALL 0 stdout
# Terminate the first crossings list:
echo -F stdout EOF\\n
differentiate
write_crossings -E ALL 0 stdout
null_sink
--
''')
  rdr=self.runrdr()
  trg=trgfile.trgfile(rdr)
  crs=trg.gettuples()
  sfreq=float(trg.preamble['Sfreq'])
  dif=trgfile.trgfile(rdr).gettuples() # Differential extrema

  lastneg=None
  intervening_pos=[]
  difindex=0
  #print("\t".join(('Time','nPos','Amp','SlopePlus','SlopeMinus','SlopeMax','SlopeMin')))
  values=[]
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
     values.append([maxpospoint/sfreq,len(intervening_pos),maxposamp,(maxposamp-lastneg[1])/((maxpospoint-lastneg[0])/sfreq),(maxposamp-amp)/((maxpospoint-point)/sfreq),maxdif*sfreq,mindif*sfreq])
     intervening_pos=[]
    lastneg=(point,amplitude)
   else:
    if lastneg:
     intervening_pos.append((point,amplitude))
  return values
