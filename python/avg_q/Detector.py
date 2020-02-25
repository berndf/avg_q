# Copyright (C) 2008-2011,2014,2016 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Detector base class.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>, Tanja Schmitt <schmitt.tanja@web.de>"

import avg_q
from . import trgfile

def get_trigger_dict(tuples):
 '''Separate triggers by channel.
 '''
 trigger_dict={}
 for lat,code,description in tuples:
  if '\t' in description:
   condition, channel= description.split('\t')
  else:
   channel=description
  trigger_dict.setdefault(channel,[]).append((lat,code))
 return trigger_dict

class Detector(avg_q.Script):
 distance_from_breakpoint_s=0.5 # Events within that distance are discarded
 def __init__(self,avg_q_instance):
  avg_q.Script.__init__(self,avg_q_instance)
  # Detect within the whole file by default
  self.detection_start=None
  self.detection_length=None
  self.sfreq=None
  self.breakpoints_in_s=None
  self.distance_from_breakpoint_points=None
 def detect(self,outtrigfile=None,maxvalue=None):
  '''
  Returns a list of trigger tuples and also, if outtrigfile is given, writes the
  data to a new file of that name for convenience.
  Note that the script 'self' should be set up with transform methods including an appropriate
  "write_crossings channelnames threshold stdout".
  The 'description' element of the tuples is appended with the epoch number and channel name,
  separated by '\t'.
  '''
  # Getting breakpoints can be expensive, so it can be disabled by setting self.distance_from_breakpoint_s to None
  # Note that we convert into time units because we may have resampling going on before detection
  if self.distance_from_breakpoint_s is not None:
   sfreq=self.avg_q_instance.get_description(self.Epochsource_list[0].infile,'sfreq')
   self.breakpoints_in_s=[point/sfreq for point in self.avg_q_instance.get_breakpoints(self.Epochsource_list[0].infile)]

  crossings=trgfile.trgfile(self.runrdr())
  outtuples=[]
  for point,code,description in crossings:
   if not self.sfreq:
    if 'Sfreq' not in crossings.preamble:
     raise Exception('Detector: Sfreq not in trigger preamble, cannot continue!')
    self.sfreq=float(crossings.preamble['Sfreq'])
   if self.detection_start is not None and (point<self.detection_start or point>=self.detection_start+self.detection_length) \
    or maxvalue is not None and float(description)>maxvalue \
    or self.distance_from_breakpoint_s is not None and any([abs(point/self.sfreq-breakpoint_in_s)<self.distance_from_breakpoint_s for breakpoint_in_s in self.breakpoints_in_s]):
    continue
   descriptionitems=[] if description is None else [description]
   if 'Epoch' in crossings.preamble:
    descriptionitems.append(crossings.preamble['Epoch'])
   if 'Channel' in crossings.preamble:
    descriptionitems.append(crossings.preamble['Channel'])
   outtuples.append((point,code,'\t'.join(descriptionitems)))
  if outtrigfile:
   t=trgfile.trgfile()
   if self.sfreq: t.preamble['Sfreq']=str(self.sfreq)
   trgout=open(outtrigfile, 'w')
   t.writetuples(outtuples,trgout)
   trgout.close()
  return outtuples

 def get_ranges(self,outtuples,direction=1,min_length=None,debounce_dist=None):
  '''
  takes the trigger tuples and returns a list of channel names and latency ranges in which
  the threshold is crossed in the given direction.
  The target ranges are between a positive and a negative crossing for
  positive direction and between a negative and a positive for negative direction.
  If min_length is defined, ranges shorter than this are discarded.
  If debounce_dist is defined, adjacent accepted ranges separated by less than this distance
  are joined.
  '''
  expected_code=1 if direction>0 else -1

  trigger_dict=get_trigger_dict(outtuples)

  channel_latrange_list=[]
  for channel, crossings in trigger_dict.items():
   latrange_list=[]
   last_start=last_end=None
   debounce=False
   for lat,code in crossings:
    if code==expected_code:
     if debounce_dist and last_end and lat-last_end<=debounce_dist:
      debounce=True
     else:
      debounce=False
     last_start=lat
    elif last_start:
     if not min_length or lat-last_start>=min_length:
      if debounce:
       # Extend the last range instead of creating a new one
       latrange_list[-1]=(latrange_list[-1][0],lat)
      else:
       latrange_list.append((last_start,lat))
      last_end=lat
     last_start=None

   # Construct a [[channel, [start,end]],...] list
   channel_latrange_list.extend([[channel, r] for r in latrange_list])
  return channel_latrange_list
