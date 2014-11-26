# vim: set fileencoding=utf-8 :
# Copyright (C) 2009-2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Specialized class derived from avg_q including methods for
sleep spectral analysis.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
import os

class sleep_eeg(avg_q.avg_q):
 defaultbands=[
  # From, To, Name (all strings)
  ['0.1', '48', 'Total'],
  ['0.1', '1', 'Delta1'],
  ['1+1', '3.5', 'Delta2'],
  ['3.5+1', '8', 'Theta'],

  ['8+1', '12', 'Alpha'],
  ['12+1', '14', 'Sigma1'],
  ['14+1', '16', 'Sigma2'],

  ['16+1', '24', 'Beta1'],
  ['24+1', '32', 'Beta2'],
  ['32+1', '48', 'Gamma']
 ]
 rejection_bands=[
  ['0.1', '48', 'Total'],
  ['32+1', '48', 'Gamma']
 ]
 rejection_median_length=20
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
 def add_cntbandssource(self,script,cntsource,bands=defaultbands):
  collapse_args=self.get_channel_collapse_args(cntsource.infile,bands)
  script.add_Epochsource(cntsource)
  # Sum frequency bins across the given bands
  cntsource.add_branchtransform('''
calc exp
collapse_channels -s %(collapse_args)s
calc log
''' % {'collapse_args': ' '.join(collapse_args)})
 def cnt2trg(self,sl,cntfile):
  '''sl must be an slfile.slfile instance yielding the sleep staging
     for the given night'''
  c=cntspectsource(cntfile)
  if c.filename is None:
   return
  tfile=c.filename.replace('.cnt','.trg')
  # trg file exists; leave 0-length files untouched if sl is still None
  if os.path.exists(tfile) and (os.path.getsize(tfile)>0 or sl is None):
   return
  # Create 0-length file
  if sl is None:
   with open(tfile,'w') as f:
    pass
   return

  c.aftertrig=0 # Read the whole cnt file as one epoch
  script=avg_q.Script(self)
  self.add_cntbandssource(script,c,bands=self.rejection_bands)
  script.add_transform('''
write_generic stdout string
echo -F stdout End of bands\\n
sliding_average -M %(median_length)d 1
write_generic stdout string
echo -F stdout End of medbands\\n
''' % {
  'median_length': self.rejection_median_length,
  })
  rdr=script.runrdr()
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
  #print(bands)
  #print(medbands)
  import copy
  sorted_medbands=copy.deepcopy(medbands)
  medians_of_medianfiltered=[]
  quartiles_of_medianfiltered=[]
  diff_threshold=[]
  for i in range(nr_of_bands):
   sorted_medbands[i].sort()
   medians_of_medianfiltered.append(sorted_medbands[i][int(len(sorted_medbands[i])/2)])
   quartiles_of_medianfiltered.append(sorted_medbands[i][int(len(sorted_medbands[i])/4)])
   diff_threshold.append(medians_of_medianfiltered[i]-quartiles_of_medianfiltered[i])
  print("medians_of_medianfiltered=" + str(medians_of_medianfiltered))
  print("quartiles_of_medianfiltered=" + str(quartiles_of_medianfiltered))
  del sorted_medbands

  sl_pos=1
  point=0
  nr_of_points=len(bands[0])
  tuples=[]
  time,stage,checks,arousals,myos,eyemovements,remcycle,nremcycle=-1,0,0,0,0,0,-1,-1
  print(len(sl.tuples))
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
  from .. import trgfile
  t=trgfile.trgfile()
  t.preamble['fields']="\t".join(['point', 'code', 'stage', 'remcycle', 'nremcycle', 'arousals', 'myos', 'emts', 'checks']+['checkmark_'+band[2] for band in self.rejection_bands])
  f=open(tfile,"w")
  t.writetuples(tuples,f)
  f.close()
  sl.close()
 def get_spect_trim(self,bands=defaultbands):
  trim='trim -x -s'
  for fromHz,toHz,name in bands:
   trim+= ' '+fromHz+' '+toHz
  return trim
 def get_measures_using_epochfilter(self,cntfile,epochfilter,bands=defaultbands):
  '''Average epochs from cnt and directly measure the result.
     If bands is None, don't collapse bands at all but average frequency bins as-is.'''
  if isinstance(cntfile,cntspectsource):
   c=cntfile
  else:
   c=cntspectsource(cntfile)
  if c.filename is None: return
  c.set_epochfilter(epochfilter)
  if len(c.trigpoints)==0: return
  script=avg_q.Script(self)
  script.add_Epochsource(c)
  script.set_collect('average')
  script.add_postprocess('''
swap_fc
trim -x 0+1 48
'''+('''
calc exp
'''+self.get_spect_trim(bands)+'''
calc log
''' if bands else '')+'''
query -N nrofaverages
write_generic -P stdout string
''')
  outline=[]
  for line in script.runrdr():
   if '=' in line:
    varname,value=line.split('=')
    if varname=='nrofaverages':
     outline.append(int(value))
   else:
    yield outline+[float(x) for x in line.split('\t')]
    outline=[]
 def get_measures(self,cntfile,stage,cycle=None,bands=defaultbands):
  '''Average epochs from cnt and directly measure the result'''
  return self.get_measures_using_epochfilter(cntfile,get_epochfilter_stage_cycle(stage,cycle),bands)
 def get_Delta_slope(self,booknumber):
  # cf. Esser:2007
  from . import sleep_file
  from .. import trgfile
  try:
   f=sleep_file.sleep_file(booknumber)
  except:
   print("Raw file for %s can't be located!" % booknumber)
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

cntspectfile_paths=(
 '/home/charly/tmp/autosleep/',
)
# Shortcuts for cntspectsource
stagenames2stagelist={
 'NREM': ['2','3','4'],
 'REM': ['5'],
}
def get_epochfilter_stage_cycle(stagename,cycle=None):
 if isinstance(stagename,list):
  stagenames=stagename
 else:
  stagenames=[stagename]
 stagelist=[x for x in stagenames2stagelist.get(stagename,[stagename]) for stagename in stagenames]
 return(lambda point,code,stage,remcycle,nremcycle,arousals,myos,eyemovements,checks,checkmark_Total,checkmark_Gamma:
  (stage in stagelist) and code>0 and (cycle is None or remcycle==cycle))

cntfilecache=None

class cntspectsource(avg_q.Epochsource):
 '''Encapsulate the concept of an epoch source yielding spectra for defined epochs.
    These are read from .cnt files and .trg files used to yield subsets of epochs.
 '''
 def __init__(self,filename):
  first,ext=os.path.splitext(filename)
  if ext:
   self.filename=filename
  else:
   # No extension: Assume this is a book number
   self.locate(first)
   if self.filename is None:
    print("cntspectsource: Can't locate cnt file for book number %s" % first)
    return
  # This prepares for reading continuously - changed when using set_epochfilter
  avg_q.Epochsource.__init__(self,self.filename,beforetrig=0,aftertrig=1,continuous=True)
  self.add_branchtransform('''
# NeuroScan files store the sampling freq as int; we have 1/30s...
set sfreq 0.03333333333
''')
 def locate(self,booknumber):
  from . import bookno
  global cntfilecache
  if not cntfilecache:
   from .. import idircache
   cntfilecache=idircache.idircache(extensionstrip=('cnt'))
  file_bookno=bookno.file_bookno(booknumber)
  self.filename=cntfilecache.find(cntspectfile_paths,file_bookno)
 def set_epochfilter(self,epochfilter):
  tfile=self.filename.replace('.cnt','.trg')
  from .. import trgfile
  t=trgfile.trgfile(tfile)
  trigpoints=[]
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

   if epochfilter(point,code,stage,remcycle,nremcycle,arousals,myos,eyemovements,checks,checkmark_Total,checkmark_Gamma):
    trigpoints.append(point)

  self.continuous=False
  self.set_trigpoints(trigpoints)
 def set_stage_cycle(self,stagename,cycle=None):
  self.set_epochfilter(get_epochfilter_stage_cycle(stagename,cycle))
