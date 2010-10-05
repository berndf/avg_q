# Copyright (C) 2008-2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Detector base class.
There are two ways in which this class can be used:
*Either* call set_infile and have detect() do the get_epoch (allowing Detector to query the file for additional information)
*Or* call detect() without prior set_infile(), which will assume the caller also sent its own get_epoch method(s) before.
Note that the calling semantics are a bit different: In the second case, multiple calls to detect() require re-sending the
get_epoch block, while in the first case new get_epoch calls are created for every call to detect().
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>, Tanja Schmitt <schmitt.tanja@web.de>"

from . import trgfile
import os

class Detector(object):
 distance_from_breakpoint_s=0.5 # Events within that distance are discarded
 def __init__(self,avg_q_object):
  self.avg_q_object=avg_q_object
  self.infile=None
  self.base=None
  self.indir=None
  self.sfreq=None
  # Detect within the whole file by default
  self.detection_start=0
  self.detection_length=0
  self.breakpoints=[]
 def set_infile(self,infile):
  '''Collect information that we will need downstream'''
  self.infile=infile
  self.base,ext=os.path.splitext(infile.filename)
  self.indir,name=os.path.split(self.base)
  if not self.indir:
   self.indir='.'
  self.sfreq=self.avg_q_object.get_description(self.infile,'sfreq')
  self.breakpoints=self.avg_q_object.get_breakpoints(self.infile)
  self.distance_from_breakpoint_points=self.distance_from_breakpoint_s*self.sfreq
 def detect(self,detectionscript,outtrigfile=None,maxvalue=None):
  '''
  Returns a trgfile object and also, if outtrigfile is given, writes the
  data to a new file of that name for convenience.
  Note that detectionscript should be the whole rest of the script, i.e.
  write_crossings, null_sink, and '-'.
  '''
  if self.infile:
   if self.detection_length==0:
    self.avg_q_object.getcontepoch(self.infile, 0, 0)
    self.avg_q_object.write(detectionscript)
   else:
    self.avg_q_object.get_epoch_around_add(self.infile,self.detection_start,0,self.detection_length)
    self.avg_q_object.get_epoch_around_finish(rest_of_script=detectionscript)
  else:
   self.avg_q_object.write(detectionscript)
  crossings=trgfile.trgfile(self.avg_q_object.runrdr())
  outtuples=[]
  for point,code,description in crossings:
   if not maxvalue or float(description)<=maxvalue:
    outpoint=point+self.detection_start
    for breakpoint in self.breakpoints:
     if abs(outpoint-breakpoint)<self.distance_from_breakpoint_points:
      point=None
      break
    if point:
     if not description and 'Channel' in crossings.preamble:
      channel=crossings.preamble['Channel']
      description=' '.join((crossings.preamble['Epoch'],channel)) if 'Epoch' in crossings.preamble else channel
     outtuples.append((outpoint,code,description))
  if not self.sfreq and 'Sfreq' in crossings.preamble:
   self.sfreq=float(crossings.preamble['Sfreq'])
  if outtrigfile:
   t=trgfile.trgfile()
   if self.sfreq: t.preamble['Sfreq']=str(self.sfreq)
   trgout=open(outtrigfile, 'w')
   t.writetuples(outtuples,trgout)
   trgout.close()
  return outtuples
