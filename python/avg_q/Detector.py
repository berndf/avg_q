# Copyright (C) 2008-2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Detector base class.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>, Tanja Schmitt <schmitt.tanja@web.de>"

import avg_q
from . import trgfile
import os

class Detector(avg_q.Script):
 distance_from_breakpoint_s=0.5 # Events within that distance are discarded
 def __init__(self,avg_q_instance):
  avg_q.Script.__init__(self,avg_q_instance)
  # Detect within the whole file by default
  self.detection_start=None
  self.detection_length=None
 def detect(self,outtrigfile=None,maxvalue=None):
  '''
  Returns a trgfile object and also, if outtrigfile is given, writes the
  data to a new file of that name for convenience.
  Note that the script 'self' should be set up with transform methods including an appropriate
  "write_crossings channelnames threshold stdout" (formerly called detectionscript).
  '''
  self.sfreq=self.avg_q_instance.get_description(self.Epochsource_list[0].infile,'sfreq')
  self.breakpoints=self.avg_q_instance.get_breakpoints(self.Epochsource_list[0].infile)
  self.distance_from_breakpoint_points=self.distance_from_breakpoint_s*self.sfreq

  crossings=trgfile.trgfile(self.runrdr())
  outtuples=[]
  for point,code,description in crossings:
   if self.detection_start is not None and (point<self.detection_start or point>=self.detection_start+self.detection_length) \
    or maxvalue is not None and float(description)>maxvalue \
    or any([abs(point-breakpoint)<self.distance_from_breakpoint_points for breakpoint in self.breakpoints]):
    continue
   if not description and 'Channel' in crossings.preamble:
    channel=crossings.preamble['Channel']
    description=' '.join((crossings.preamble['Epoch'],channel)) if 'Epoch' in crossings.preamble else channel
   outtuples.append((point,code,description))
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
