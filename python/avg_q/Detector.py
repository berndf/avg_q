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

 def get_ranges(self,outtuples,direction=1):
  '''
  takes the trigger tuples and returns a list of channel names and latency ranges in which
  the threshold is crossed in the given direction.
  The target ranges are between a positive and a negative crossing for
  positive direction and between a negative and a positive for negative direction.
  '''
  expected_slope=1 if direction>0 else -1
  
  ###sort triggers####
  trigger_dict={}  
  for outtuple in outtuples:
   #print outtuple
   lat,slope,description=[x for x in outtuple] 
   condition, channel= description.split(' ')
   trigger_dict.setdefault(channel,[]).append([lat,slope]) 

  channel_latrange_list=[]
  for channel, entry in trigger_dict.items():
   ###check and delete single crossings###
   if entry[-1][-1]==expected_slope:  
    del entry[-1]
   if entry==[]:# if channel was only crossing at one point and after deletion, no cross is left-> delete key 
    del trigger_dict[channel]
   else:
    if not entry[0][1]==expected_slope:
     del entry[0]
   
   # Build a [[start, end],...] list; if two latency windows are only separated by 1 sampling point, fuse them
   lastend=None
   for x in range(0,len(entry),2):
    start,end=entry[x][0],entry[x+1][0]
    if lastend and (start-lastend)*self.sfreq/1000.0<=1:
     # Modify the last entry, extending it to the new end
     channel_latrange_list[-1]=[channel, [channel_latrange_list[-1][1][0],end]]
    else:
     channel_latrange_list.append([channel, [start,end]])
    lastend=end
  return channel_latrange_list
