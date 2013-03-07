# Copyright (C) 2008-2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Module to detect and subtract fMRI gradient artifacts
"""

import avg_q
from . import avg_q_file
from . import trgfile
from . import BrainVision
from . import Channeltypes
import os
import math
import glob

class fMRI_Correct(object):
 def __init__(self,avg_q_instance):
  self.avg_q_instance=avg_q_instance
  self.infile=None
  self.base=None
  self.indir=None
  self.sfreq=None
  self.points_in_file=None
  self.TS=None
  self.TR=None
  self.TS_points=None
  self.TR_points=None
  self.remove_channels=Channeltypes.NonEEGChannels
  self.collapseit=None
  self.overviewfilename=None
  self.templatefilename=None
  self.upsample=1 # Must be integer. Don't upsample by default - It's not necessary for SyncBox measurements.
  self.template_length=12.4 # 62 points at 5kHz
  self.refine_length=2.4 # 12 points
  self.template_points=None
  self.refine_points=None
  self.min_scan_duration_s=5*60; # 5 minutes: For the automatic detection of scans
  self.checkmode=False
  self.threshold=None
  self.haveTemplate=False
  self.convolvethreshold=0.9

  # The EPI-subtracted file is written with this sfreq
  self.correctedfile_sfreq=100.0
  self.margin_in_seconds=2.0 # How many seconds to transfer before the first and after the last EPI
  self.avgEPI_Amplitude_Reject_fraction=None # A way to pass this parameter to avgEPI()
 def get_remove_channels(self):
  '''Helper function to get the necessary remove_channel command'''
  return 'remove_channel -n ?' + ",".join(self.remove_channels) if self.remove_channels else ''
 def set_infile(self,infile):
  '''Collect information that we will need downstream'''
  self.infile=infile
  self.base,ext=os.path.splitext(infile.filename)
  self.indir,name=os.path.split(self.base)
  if not self.indir:
   self.indir='.'
  self.sfreq,self.points_in_file=self.avg_q_instance.get_description(self.infile,('sfreq','points_in_file'))
  if not self.sfreq:
   raise Exception("Oops, can't determine sfreq for %s" % self.base)
  if not self.points_in_file:
   raise Exception("Oops, can't determine points_in_file for %s" % self.base)
  self.template_points=int(self.template_length/1000.0*self.sfreq)
  self.refine_points=int(self.refine_length/1000.0*self.sfreq)
  # Fall back to sensible values here
  if self.template_points<20: self.template_points=20
  if self.refine_points<5: self.refine_points=5
  self.collapseit='''
# Using all channels on the scalp has proven to be *much* more sensitive than the gradient
# between any two channels!
%(remove_channels)s
add negpointmean
calc abs
collapse_channels
''' % {
  'remove_channels': self.get_remove_channels()
  }
  if self.upsample==1:
   self.upsampleit=''
  else:
   sampling_step=1.0/self.upsample
   filter_zerostart=1.0/self.upsample
   filter_start=0.9*filter_zerostart
   self.upsampleit='''
sliding_average 1 %(sampling_step)g
fftfilter %(filter_start)g %(filter_zerostart)g 1 1
''' % {
   'sampling_step': sampling_step,
   'filter_start': filter_start,
   'filter_zerostart': filter_zerostart,
   }
  self.overviewfilename=self.base + '_overview.asc'
  self.templatefilename=self.base + '_template.asc'
 def get_fromtoepoch(self,start_s,end_s):
  fromepoch=int(math.floor(start_s*1000.0/self.TR))+1
  if end_s:
   epochs=int(math.ceil(end_s*1000.0/self.TR))+1-fromepoch 
  else:
   epochs=None
  return (fromepoch,epochs)
 def get_overview(self):
  # We have to do the overview exactly in steps of TR in order to get constant
  # overview values also in the interleaved case.
  if os.path.exists(self.overviewfilename):
   print("Reusing %s." % self.overviewfilename)
  else:
   print("Generating %s..." % self.overviewfilename)
   self.avg_q_instance.getcontepoch(self.infile, 0, self.TR_points)
   self.avg_q_instance.write(self.collapseit)
   self.avg_q_instance.write('''
trim -a 0 0
append
Post:
set sfreq %g
set xdata 0
writeasc -b %s
-
''' % (1000.0/self.TR,self.overviewfilename))
   self.avg_q_instance.run()
 def get_fromto_from_overview(self):
  '''Analyze scans using the overview.'''
  self.get_overview()
  # We used to do scale_by invpointmax below but sometimes there are
  # strong spikes... Assuming that scanning spans more than half of the
  # recorded time, using the median should give the typical scanning amplitude...
  self.avg_q_instance.write('''
readasc %s
add negpointmin
scale_by invpointquantile 0.5
query nr_of_points stdout
write_crossings collapsed 0.5 stdout
null_sink
-
''' % self.overviewfilename)
  rdr=self.avg_q_instance.runrdr()
  overview_points=int(next(rdr))
  trgfile_overview=trgfile.trgfile(rdr)
  scanpoints=trgfile_overview.gettuples()
  # These two vars are modified in add_start_end() below:
  global start_s,fromto
  fromto=[]
  start_s=None
  end_s=None
  def add_start_end():
   global start_s,fromto
   start_s1=start_s if start_s!=None else 0
   end_s1=end_s if end_s!=None else overview_points*self.TR/1000.0
   if end_s1-start_s1<self.min_scan_duration_s:
    print("Overview: Not using short 'scan' %g-%g!" % (start_s1,end_s1))
   else:
    fromto.append((start_s,end_s))
   start_s=None
  for point,code,description in scanpoints:
   if code==1:
    start_s=(point-0.5)*self.TR/1000.0
    end_s=None
   elif code== -1:
    end_s=(point+0.5)*self.TR/1000.0
    add_start_end()
  if start_s and not end_s:
   add_start_end()
  return fromto
 def print_EPIstats(self,tuples):
  lastpoint=None
  dd={}
  count=0
  for t in tuples:
   point=t.floatvalue()
   if lastpoint:
    # Using float directly is problematic due to minuscule differences in representation
    d=str(point-lastpoint)
    if d in dd:
     dd[d]+=1
    else:
     dd[d]=1
    count+=1
   lastpoint=point
  print("%d %s" % (count, str(dd)))
 def getTemplate(self,trigpoint):
  if not self.haveTemplate:
   # The template is normalized so that 1 should be obtained when convolved with the original trace
   get_template_script='''
scale_by invpointsquarenorm
scale_by nr_of_points
writeasc -b %(templatefilename)s
''' % { 'templatefilename': self.templatefilename }
   script=avg_q.Script(self.avg_q_instance)
   epochsource=avg_q.Epochsource(self.infile,self.template_points/2,self.template_points/2)
   epochsource.set_trigpoints(trigpoint)
   script.add_Epochsource(epochsource)
   script.add_transform(self.collapseit)
   script.add_transform(self.upsampleit)
   script.add_transform(get_template_script)
   script.run()
   self.haveTemplate=True
 def get_threshold(self,start_s):
   self.avg_q_instance.getcontepoch(self.infile, '0s', '1s', fromepoch=start_s, epochs=5)
   self.avg_q_instance.write(self.collapseit)
   self.avg_q_instance.write('''
trim -h 0 0
minmax
Post:
write_generic stdout string
-
''')
   for line in self.avg_q_instance.runrdr():
    minval,maxval=line.split()
    self.threshold=float(maxval)*0.5
    print("Automatically set threshold to %g" % self.threshold)
 def set_TSTR(self,TS,TR=None):
  self.TS=TS
  if TR:
   self.TR=TR
   if self.TR<self.TS:
    raise Exception("TR must be >= TS!")
  else:
   self.TR=self.TS
  self.TR_points=int(self.TR/1000.0*self.sfreq)
  self.TS_points=int(self.TS/1000.0*self.sfreq)
 def get_epitrigs(self,fromto):
  runindex=0
  for start_s,end_s in fromto:
   self.haveTemplate=False
   fromepoch,epochs=self.get_fromtoepoch(start_s,end_s)
   runindex+=1

   if not self.checkmode:
    crsfilename=self.base + '%02d.crs' % runindex
    if os.path.exists(crsfilename):
     print("%s exists!" % crsfilename)
     continue

   if not self.threshold:
    self.get_threshold(start_s)
    if not self.threshold:
     raise Exception("threshold is undefined!")

   outtuples=trgfile.HighresTriggers(self.upsample)

   # First, only look for the first EPI peak and extract a short template
   self.avg_q_instance.getcontepoch(self.infile, '0s', '%gms' % self.TR, fromepoch=fromepoch, epochs=epochs)
   detect_first_EPI_script='''
write_crossings -E -R %(refractory_time)gms collapsed %(threshold)g triggers
%(posplot)s
query triggers_for_trigfile stdout
assert -S nr_of_triggers == 0
null_sink
-
''' % {
   'refractory_time': self.TR,
   'threshold': self.threshold,
   'posplot': 'posplot' if self.checkmode else ''
   }
   self.avg_q_instance.write(self.collapseit)
   self.avg_q_instance.write(detect_first_EPI_script)
   trgfile_crs=trgfile.trgfile(self.avg_q_instance.runrdr())
   trgpoints=trgfile_crs.gettuples()
   if len(trgpoints)==0:
    print("Can't find a single trigger in %s, continuing..." % self.base)
    continue
   pointoffset=int((fromepoch-1)*self.TR/1000.0*self.sfreq) if fromepoch else 0
   trigpoint=pointoffset+trgpoints[0][0]
   self.getTemplate(trigpoint)
   
   # Read the whole fMRI run in steps of TR
   # Empirical correction for idempotency
   correct_correct=None
   while not end_s or trigpoint<end_s*self.sfreq:
    readpoint,trimcorrection=trgfile.get_ints(trigpoint,self.upsample)
    epi_detection_script='''
convolve %(templatefilename)s 1
trim %(trimstart)f %(trimlength)f
write_crossings -E collapsed %(convolvethreshold)g stdout
%(posplot)s
''' % {
    'templatefilename': self.templatefilename,
    'trimstart': self.upsample*(self.template_points/2)+trimcorrection,
    'trimlength': self.upsample*self.refine_points,
    'convolvethreshold': self.convolvethreshold,
    'posplot': 'set_comment Detecting EPI...\nposplot' if self.checkmode else ''
    }
    before=int((self.template_points+self.refine_points)/2)
    script=avg_q.Script(self.avg_q_instance)
    epochsource=avg_q.Epochsource(self.infile,before,before+1)
    epochsource.set_trigpoints(readpoint)
    script.add_Epochsource(epochsource)
    script.add_transform(self.collapseit)
    script.add_transform(self.upsampleit)
    script.add_transform(epi_detection_script)
    trgfile_crs=trgfile.trgfile(script.runrdr())
    trgpoints=trgfile_crs.gettuples()
    if len(trgpoints)==0: break; # Trigger position beyond file length
    correction=float(trgpoints[0][0])/self.upsample-self.refine_points/2
    if correct_correct==None:
     correct_correct=correction
    else:
     correction-=correct_correct
     trigpoint+=correction
    print("Correction %g Position %d" % (correction, trigpoint))
    outtuples.append(trigpoint)
    # Comment this out to prove idempotency:
    trigpoint+=self.TR_points

   # Check the end of the scan - does it fit with TR?
   epi_end_detection_script='''
scale_by invpointmax
write_crossings -E collapsed 0.75 stdout
#write_crossings -E collapsed 0.75 triggers
#posplot
'''
   readpoint=trigpoint-self.TR_points
   points_to_read=2*self.TR_points
   # Sigh... Detect whether the EEG was stopped *immediately* after the scan
   if readpoint+points_to_read>self.points_in_file:
    points_to_read=self.points_in_file-readpoint
   script=avg_q.Script(self.avg_q_instance)
   epochsource=avg_q.Epochsource(self.infile,0,points_to_read)
   epochsource.set_trigpoints(readpoint)
   script.add_Epochsource(epochsource)
   script.add_transform(self.collapseit)
   script.add_transform(epi_end_detection_script)
   trgfile_crs=trgfile.trgfile(script.runrdr())
   trgpoints=trgfile_crs.gettuples()
   point,code,description=trgpoints[-1]
   deviation=(point-self.TS_points)/self.TS_points
   print("Last peak %gs after last EPI, deviation %g*TS" % (point/self.sfreq,deviation))
   if deviation< -0.05:
    print("Deviation too large, dropping last EPI - Check TS=%gms!" % self.TS)
    outtuples.pop(-1)
   elif deviation>=0:
    print("Positive deviation, check end_s=" + str(end_s) + "!")

   self.print_EPIstats(outtuples)
   if not self.checkmode:
    trgfile_crs.preamble['TS']=str(self.TS)
    trgfile_crs.preamble['TR']=str(self.TR)
    trgfile_crs.preamble['upsample']=str(self.upsample)
    crsfile=open(crsfilename,mode='w')
    trgfile_crs.writetuples(outtuples.as_triglist(),crsfile)
    crsfile.close()

   if self.haveTemplate:
    os.unlink(self.templatefilename)
    self.haveTemplate=False

 def transferSection(self,sectionstart, sectionbefore, sectionafter, addmethods=None):
  doscript=addmethods if addmethods else ''
  '''
fftfilter 40Hz 45Hz 1 1
sliding_average 1 10ms
write_generic %(append_arg)s %(correctedfile)s.eeg int16
writeasc %(append_arg)s %(correctedfile)s.asc
write_synamps -c %(append_arg)s %(correctedfile)s.cnt %(sensitivity)g
'''
  writefile='''
sliding_average %(sliding_size)sms %(sliding_step)sms
write_hdf -c %(append_arg)s %(correctedfile)s.hdf
  ''' % {
  'sliding_size': 2000.0/self.correctedfile_sfreq,
  'sliding_step': 1000.0/self.correctedfile_sfreq,
  'append_arg': '-a' if self.append else '',
  'correctedfile': self.correctedfile,
  'sensitivity': 10,
  }

  script=avg_q.Script(self.avg_q_instance)
  epochsource=avg_q.Epochsource(self.infile,sectionbefore,sectionafter)
  epochsource.set_trigpoints(sectionstart)
  script.add_Epochsource(epochsource)
  script.add_transform(doscript)
  script.add_transform(writefile)
  script.run()
 def subtract_EPI(self):
  for crsfile in glob.glob(self.base+'[0-9][0-9].crs'):
   runindex=int(crsfile[-6:-4])
   self.correctedfile=self.base + '_corr%02d' % runindex # Extension added by writefile()
   if self.checkmode:
    print("Processing %s (Checkmode)" % self.infile.filename)
   else:
    if os.path.exists(self.correctedfile + '.hdf'):
     print("%s already exists!" % (self.correctedfile + '.hdf'))
     continue
    print("Processing %s, %s->%s" % (self.infile.filename,crsfile,self.correctedfile+'.hdf'))

   # Average EPI (avgEPI will test whether the average already exists)
   myEPI=avgEPI(self.avg_q_instance,self.base,self.infile,self.sfreq,self.upsample)
   # Pass parameters to avgEPI()
   if self.avgEPI_Amplitude_Reject_fraction:
    myEPI.avgEPI_Amplitude_Reject_fraction=self.avgEPI_Amplitude_Reject_fraction
   myEPI.checkmode=self.checkmode
   myEPI.avgEPI(crsfile,runindex)

   # Now actually do the subtraction
   lastpoint=None
   self.append=False
   for EPI in myEPI.EPIs:
    if myEPI.upsample==1:
     readpoint=EPI[0]
    else:
     readpoint,trimcorrection=trgfile.HighresTrigger(EPI)
     # Note that this trimming is applied to avgEPIfile, therefore shifts in 
     # the opposite direction from the averaging code above
     correction_for_resampling=myEPI.upsample/2
     trimcorrection=myEPI.upsample-trimcorrection-correction_for_resampling
    # Now we read first an 'intermediate' section, then an EPI section
    # and so on, appending everything to the corrected file.
    if not lastpoint:
     # Note that beforetrig_points need to be added to really get margin_in_seconds seconds
     intermediatestart=readpoint-self.sfreq*self.margin_in_seconds-myEPI.beforetrig_points
     if intermediatestart<0:
      intermediatestart=0
     # Parameters saved for the transfer of triggers at the end
     outfile_startpoint=intermediatestart
     EPI_startpoint=readpoint
    else:
     intermediatestart=lastpoint+myEPI.aftertrig_points
    intermediatelength=readpoint-myEPI.beforetrig_points-intermediatestart
    print("Intermediate points %d-%d..." % (intermediatestart,intermediatestart+intermediatelength-1))
    lefttrim=''
    if intermediatelength>0:
     self.transferSection(intermediatestart, 0, intermediatelength)
     self.append=True
    elif intermediatelength<0:
     lefttrim='trim %d 0' % -intermediatelength
    if myEPI.upsample!=1:
     myEPI.get_tmpEPI(trimcorrection)
    # Use this for checking single-epoch matches:
    '''
set beforetrig 0
set xdata 1
add_channels -l %(avgEPIfile)s
link_order 2
set sfreq 5000
set beforetrig 0
set xdata 1
posplot
'''
    self.transferSection(readpoint, myEPI.beforetrig_points, myEPI.aftertrig_points, addmethods='''
subtract %(avgEPIfile)s
%(lefttrim)s
''' % {
    'avgEPIfile': myEPI.tmpEPIfile if myEPI.upsample!=1 else myEPI.avgEPIfile,
    'lefttrim': lefttrim
    })
    self.avg_q_instance.run()
    self.append=True
    lastpoint=readpoint
   # Read some section of EEG after the last EPI.
   readpoint=lastpoint+myEPI.aftertrig_points
   points_to_read=self.sfreq*self.margin_in_seconds
   # Sigh... Detect whether the EEG was stopped *immediately* after the scan
   if readpoint+points_to_read>self.points_in_file:
    points_to_read=self.points_in_file-readpoint
   self.transferSection(readpoint, 0, points_to_read)
   self.avg_q_instance.run()
   outfile_endpoint=readpoint+points_to_read

   # Create a trigger file fitting the current run
   vmrk=BrainVision.vmrkfile(self.base + '.vmrk')
   vmrktuples=vmrk.gettuples()
   vmrk.close()
   point_in_s=lambda p: "%gs" % ((p-outfile_startpoint)/self.sfreq)
   trgtuples=[]
   trgtuples.append((point_in_s(EPI_startpoint),256,"EPI Start"))
   for point,code,description in vmrktuples:
    if point>=outfile_startpoint and point<outfile_endpoint:
     trgtuples.append((point_in_s(point),code,description))
   trgtuples.append((point_in_s(readpoint),256,"EPI End"))
   trgout=open(self.base+'_corr%02d.trg' % runindex,mode='w')
   t=trgfile.trgfile()
   t.preamble['Sfreq']=self.correctedfile_sfreq
   t.writetuples(trgtuples,trgout)
   trgout.close()

class avgEPI(object):
 def __init__(self,avg_q_instance,base,infile,sfreq,upsample):
  self.avg_q_instance=avg_q_instance
  self.base=base
  self.infile=infile
  self.sfreq=sfreq
  self.upsample=upsample
  self.tmpEPIfile='tmpEPI'
  # Reject EPI epochs if the max difference to singleEPI is above 6% of
  # maximum EPI amplitude
  self.avgEPI_Amplitude_Reject_fraction=0.06
  # Accept at most 20% EPI rejections
  self.avgEPI_max_rejection_fraction=0.2
  self.checkmode=False
  self.TR=None
  self.TS=None
  self.EPIs=None
  self.override_upsample=None
  self.upsampleit=None
  self.filterit=None
 def set_upsample(self,upsample):
  self.upsample=upsample
  if self.upsample==1:
   self.upsampleit=''
   self.filterit=''
  else:
   sampling_step=1.0/self.upsample
   filter_zerostart=1.0/self.upsample
   filter_start=0.9*filter_zerostart
   self.upsampleit='''
>sliding_average 1 %(sampling_step)g
''' % {
  'sampling_step': sampling_step,
   }
   self.filterit='''
>fftfilter %(filter_start)g %(filter_zerostart)g 1 1
''' % {
  'filter_start': filter_start,
  'filter_zerostart': filter_zerostart,
   }
 def set_crsfile(self,crsfile):
  trgfile_crs=trgfile.trgfile(crsfile)
  self.EPIs=trgfile_crs.gettuples()
  trgfile_crs.close()
  if len(self.EPIs)==0:
   raise Exception("No EPI triggers known!")
  if 'TS' in trgfile_crs.preamble:
   self.TS=float(trgfile_crs.preamble['TS'])
  else:
   raise Exception("Sorry, trigger file has no TS property!")
  if 'TR' in trgfile_crs.preamble:
   self.TR=float(trgfile_crs.preamble['TR'])
  else:
   self.TR=self.TS
  if not self.override_upsample and 'upsample' in trgfile_crs.preamble:
   self.set_upsample(int(trgfile_crs.preamble['upsample']))
  print("EPI triggers: TS=%gms, TR=%gms, upsample=%d" % (self.TS, self.TR, self.upsample))

  beforetrig=1.0 # Seconds
  aftertrig=self.TR-beforetrig
  self.beforetrig_points=int(beforetrig/1000.0*self.sfreq)
  epochlength=int((beforetrig+aftertrig)/1000.0*self.sfreq)
  self.aftertrig_points=epochlength-self.beforetrig_points
 def CheckIfAvgEPICompatible(self):
  # See whether the available average is compatible with the current run
  epifile=avg_q_file(self.avgEPIfile,'asc')
  avg_sfreq,nr_of_points=self.avg_q_instance.get_description(epifile,('sfreq','nr_of_points'))
  avg_sfreq=round(avg_sfreq)
  expected_sfreq=self.sfreq*self.upsample
  if avg_sfreq!=expected_sfreq:
   print("Average file sfreq mismatch, %g!=%g" % (avg_sfreq, expected_sfreq))
   return False
  if nr_of_points==None:
   print("Average file nr_of_points missing!")
   return False
  TR1=(nr_of_points-(2*self.upsample if self.upsample!=1 else 0))*1000.0/avg_sfreq
  if TR1!=self.TR:
   print("Oops, average file has TR=%g, recalculating with current TR=%g" % (TR1, self.TR))
   return False
  else:
   return True
 def EPI_Epochsource(self,EPI):
  if self.upsample==1:
   trimcorrection=0
   beforetrig,aftertrig=self.beforetrig_points,self.aftertrig_points
   trimit=''
  else:
   trimcorrection=trgfile.HighresTrigger(EPI)[1]
   # One data point is extra so that trim doesn't add zeros (negative offset)
   trimcorrection+=self.upsample
   trimit='''
>trim %(trimstart)f %(trimlength)f
''' % {
   'trimstart': trimcorrection,
   'trimlength': (self.beforetrig_points+self.aftertrig_points+2)*self.upsample,
   }
   # We need 2 points to each side as 'trim space', once for averaging, second before subtraction
   beforetrig,aftertrig=self.beforetrig_points+2,self.aftertrig_points+2
  branch='''
>detrend -0
%(upsampleit)s
%(trimit)s
''' % {
  'upsampleit': self.upsampleit,
  'trimit': trimit,
  }
  epochsource=avg_q.Epochsource(self.infile,beforetrig,aftertrig)
  epochsource.set_trigpoints(EPI[0])
  epochsource.add_branchtransform(branch)
  return epochsource
 def avgEPI(self,crsfile,runindex):
  self.set_crsfile(crsfile)
  self.avgEPIfile=self.base + '_AvgEPI%02d.asc' % runindex
  if os.path.exists(self.avgEPIfile) and self.CheckIfAvgEPICompatible():
   print("Reusing average file %s" % self.avgEPIfile)
   return
  print("Averaging EPI... -> %s" % self.avgEPIfile)
  singleEPIepoch=1 # Use first EPI as template by default
  singleEPIfile=self.base + '_SingleEPI%02d' % runindex
  residualsfile=self.base + '_residuals%02d' % runindex
  if not self.checkmode and os.path.exists(residualsfile+'.hdf'):
   os.unlink(residualsfile+'.hdf')
  # Store "best" template in case finding one with the strict criterion fails
  min_rejection_fraction=1
  base_avgEPIfile,ext_avgEPIfile=os.path.splitext(self.avgEPIfile)
  # Repeat averaging if too many EPIs are rejected (specimen itself is bad):
  while True:
   print("Using EPI template epoch %d" % singleEPIepoch)
   # Get the single epoch specimen and its amplitude
   script=avg_q.Script(self.avg_q_instance)
   script.add_Epochsource(self.EPI_Epochsource(self.EPIs[singleEPIepoch-1]))
   script.add_transform('''
writeasc -b -c %(singleEPIfile)s.asc
calc abs
trim -h 0 0
writeasc -b %(singleEPIfile)s_Amplitude.asc
''' % {
   'singleEPIfile': singleEPIfile,
   })
   script.run()
   '''
#add_channels -l %(singleEPIfile)s.asc
#posplot
#link_order 2
#pop
'''
   rejection_script='''
push
subtract %(singleEPIfile)s.asc
calc abs
trim -h 0 0
subtract -d %(singleEPIfile)s_Amplitude.asc
# Let each run through all EPIs correspond to exactly "1s"
set sfreq %(nr_of_EPIs)d
write_hdf -a -c %(residualsfile)s.hdf
# Don't consider non-EEG channels to judge the fit
collapse_channels -h !?%(NonEEGChannels)s:collapsed
reject_bandwidth -m %(avgEPI_Amplitude_Reject_fraction)g
pop
''' % {
   'singleEPIfile': singleEPIfile,
   'nr_of_EPIs': len(self.EPIs),
   'residualsfile': residualsfile,
   'NonEEGChannels': ','.join(Channeltypes.NonEEGChannels),
   'avgEPI_Amplitude_Reject_fraction': self.avgEPI_Amplitude_Reject_fraction,
   }
   script=avg_q.Script(self.avg_q_instance)
   for EPI in self.EPIs:
    script.add_Epochsource(self.EPI_Epochsource(EPI))
   if self.checkmode:
    script.add_transform('''
set beforetrig 0
set xdata 1
append -l
Post:
posplot
-
''')
    script.set_collect('append -l')
    script.add_postprocess('posplot')
   else:
    script.add_transform(rejection_script)
    script.set_collect('average')
    script.add_postprocess('''
writeasc -b %(avgEPIfile)s
query accepted_epochs stdout
query rejected_epochs stdout
''' % {
    'avgEPIfile': self.avgEPIfile,
    })
   lines=script.runrdr()
   accepted_epochs=int(next(lines))
   rejected_epochs=int(next(lines))
   # Empty the line buffer just in case
   for line in lines:
    print(line)
   os.unlink(singleEPIfile + '.asc')
   os.unlink(singleEPIfile + '_Amplitude.asc')
   rejection_fraction=float(rejected_epochs)/(accepted_epochs+rejected_epochs)
   #print accepted_epochs,rejected_epochs,rejection_fraction
   if rejection_fraction>self.avgEPI_max_rejection_fraction:
    if rejection_fraction<min_rejection_fraction:
     os.rename(self.avgEPIfile, base_avgEPIfile + '_Best.asc')
     min_rejection_fraction=rejection_fraction
    else:
     os.unlink(self.avgEPIfile)
    # Try the next EPI template...
    singleEPIepoch+=1
    if singleEPIepoch>=len(self.EPIs):
     if os.path.exists(base_avgEPIfile + '_Best.asc'):
      # Okay, we pragmatically use the one with the least rejection
      print("No EPI templates would fit, using best rejection fraction (%g)" % rejection_fraction)
      os.rename(base_avgEPIfile + '_Best.asc', self.avgEPIfile)
      break
     else:
      print("Oops, not even a best rejection_fraction fit?")
      raise Exception("No EPI templates would fit, giving up.")
    print("EPI Rejection fraction too large (%g), retrying..." % rejection_fraction)
   else:
    break
  if os.path.exists(base_avgEPIfile + '_Best.asc'):
   os.unlink(base_avgEPIfile + '_Best.asc')
 def get_tmpEPI(self,trimcorrection):
  self.avg_q_instance.write('''
readasc %(avgEPIfile)s
trim %(trimstart)f %(trimlength)f
sliding_average 1 %(upsample)d
writeasc -b %(tmpEPIfile)s.asc
null_sink
-
''' % {
  'avgEPIfile': self.avgEPIfile,
  'trimstart': trimcorrection,
  'trimlength': (self.beforetrig_points+self.aftertrig_points)*self.upsample,
  'upsample': self.upsample,
  'tmpEPIfile': self.tmpEPIfile,
  })
