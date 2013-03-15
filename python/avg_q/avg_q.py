# Copyright (C) 2007-2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Python interface to avg_q.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import sys
import subprocess

__all__ = ['avg_q']

valuetype={
'sfreq': float,
'z_value': float,

'nr_of_points': int,
'nr_of_channels': int,
'itemsize': int,
'leaveright': int,
'length_of_output_region': int,
'beforetrig': int,
'aftertrig': int,
'points_in_file': int,
'nroffreq': int,
'nrofaverages': int,
'accepted_epochs': int,
'rejected_epochs': int,
'failed_assertions': int,
'condition': int,

'z_label': str,
'comment': str,
'datetime': str,
'xchannelname': str,
'CWD': str,

'channelnames':list,
'xdata':list,
}
#channelpositions
#triggers
#triggers_for_trigfile

break_events=[256,257] # NAV_STARTSTOP, NAV_DCRESET

outtuple=[]
collectvalue=None
listvalue=[]
class avg_q(object):
 def __init__(self,avg_q="avg_q_vogl",endstring="End of script",tracelevel=0):
  """Start avg_q."""
  self.endstring=endstring
  call=[avg_q,'stdin']
  if tracelevel>0:
   call.insert(1,'-t %d' % tracelevel)

  try:
   self.avg_q=subprocess.Popen(call, shell=False, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  except:
   import os
   # See if this is a Windows install with python/ within the avg_q binary directory
   call[0]=os.path.join(os.path.dirname(__file__),'..','..',avg_q+'.exe')
   self.avg_q=subprocess.Popen(call, shell=False, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

  self.recorded_trigpoints=[]
  self.debug=False
 def __del__(self):
  """Destructor."""
  self.close()
 def write(self,script):
  """Send a string (or whole script) to avg_q"""
  if self.debug:
   sys.stdout.write(script) # Output for debugging
  self.avg_q.stdin.write(script.encode('utf8'))
  self.avg_q.stdin.flush()
 def runrdr(self):
  """Yield output lines from avg_q, waiting for endstring."""
  self.write('''
!echo -F stdout %s\\n
null_sink
-
''' % self.endstring)
  while 1:
   if sys.version<"3":
    line=self.avg_q.stdout.readline().rstrip('\r\n')
   else:
    line=self.avg_q.stdout.readline().decode('utf8').rstrip('\r\n')
   if not line or line==self.endstring:
    break
   yield line
 def __iter__(self):
  return self.runrdr()
 def run(self):
  """Run script, printing the output."""
  for line in self:
   print(line)
 def close(self):
  """Close input and output handles, terminate avg_q."""
  self.avg_q.stdin.close()
  self.avg_q.stdout.close()
 # Helper functions accepting avg_q style time designations ('12s', '330ms', 533) and returning a float in the target unit
 def time_to_any(self,time,lambdas):
  if type(time)==int or type(time)==float:
   condition=0
  elif time.endswith('ms'):
   condition=2
  elif time.endswith('s'):
   condition=1
  else:
   condition=0
  return lambdas[condition](time)
 def time_to_points(self,time,sfreq):
  return self.time_to_any(time,[lambda x: float(x),lambda x: float(x[:-1])*sfreq,lambda x: float(x[:-2])/1000.0*sfreq])
 def time_to_s(self,time,sfreq):
  return self.time_to_any(time,[lambda x: float(x)/sfreq,lambda x: float(x[:-1]),lambda x: float(x[:-2])/1000.0])
 def time_to_ms(self,time,sfreq):
  return self.time_to_any(time,[lambda x: float(x)*1000.0/sfreq,lambda x: float(x[:-1])*1000.0,lambda x: float(x[:-2])])
 def get_breakpoints(self,infile):
  '''
  Get those file events causing discontinuities in the (continuous) data. Start at point 0
  and end at end of file are not explicitly reported.
  '''
  triggers=self.get_filetriggers(infile)
  return [point for point,code,description in triggers if code in break_events and point>0]
 def getepoch(self, infile, beforetrig=0, aftertrig=0, continuous=False, fromepoch=None, epochs=None, offset=None, triglist=None, trigfile=None, trigtransfer=False):
  """Write a get-epoch method line."""
  self.write(infile.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer))
 def getcontepoch(self, infile, beforetrig, aftertrig, fromepoch=None, epochs=None, trigfile=None, trigtransfer=False):
  self.getepoch(infile, beforetrig, aftertrig, continuous=True, fromepoch=fromepoch, epochs=epochs, trigfile=trigfile, trigtransfer=trigtransfer)
 def get_file_sections_excluding_breakpoints(self,start_point,end_point,breakpoints,margin_points):
  '''Iterator of (startsegment,endsegment) avoiding breakpoints by margin_points.
     'breakpoint' elements can be (start,end) tuples as well, in which case the whole segment between
     start and end (and added margin) is excluded'''
  laststart=None
  # List of (start,end) tuples *sorted by start*
  br=sorted([(element[0]-margin_points,element[1]+margin_points) if isinstance(element,tuple) else (element-margin_points,element+margin_points) for element in breakpoints],key=lambda x: x[0])
  #print(start_point,end_point,br)
  # startsegment holds the next candidate for starting a clear segment
  # not touched by any exclude range
  startsegment=start_point
  current_exclude_range=0
  while current_exclude_range<len(br) and br[current_exclude_range][0]<end_point:
   # Check if new startsegment is within the next exclude range
   # Note that the sorting by start restricts possibilities here
   if startsegment<br[current_exclude_range][0]:
    # current_exclude_range only starts after startsegment: Found a clear patch...
    yield (startsegment,br[current_exclude_range][0])
    startsegment=br[current_exclude_range][1]
   else:
    if startsegment<br[current_exclude_range][1]:
     # startsegment is within current_exclude_range, skip to the end of this range
     startsegment=br[current_exclude_range][1]
    # else: Both start and end of range are below startsegment, skip this range
   current_exclude_range+=1
  if startsegment<end_point:
   yield (startsegment,end_point)
 def get_description(self,infile,getvars):
  class collect_results(object):
   import re
   querymatch=re.compile('^([^=]+)=(.+)$')
   def __init__(self,getvars):
    self.getvars=getvars
    self.outtuple=[]
    self.collectvalue=None
    self.listvalue=[]
   def closelist(self):
    if self.collectvalue and len(self.listvalue)>0:
     self.outtuple.append(self.listvalue)
     self.listvalue=[]
    self.collectvalue=False
   def addline(self,line):
    m=self.querymatch.match(line)
    if m:
     var,value=m.groups()
     if var in self.getvars:
      collect.closelist()
      if valuetype[var]==list:
       self.listvalue=[value]
       self.collectvalue=True
      else:
       value=valuetype[var](value)
       self.outtuple.append(value)
       self.collectvalue=False
     else: m=None
    if not m:
     if self.collectvalue:
      self.listvalue.append(line)
  if not isinstance(getvars,tuple):
   getvars=(getvars,)
  self.getcontepoch(infile, 0, 1, epochs=1)
  for var in getvars:
   self.write('''
query -N %s stdout
''' % var)
  self.write('''
null_sink
-
''')
  collect=collect_results(getvars)
  for r in self:
   collect.addline(r)
  collect.closelist()
  return collect.outtuple[0] if len(getvars)==1 else tuple(collect.outtuple)
 def get_filetriggers(self,infile):
  from . import trgfile
  if infile.fileformat=='asc':
   # 'asc' is epoched only; make 'filetriggers' mean epoch numbers (starting at 0), conditions and comments
   self.getepoch(infile)
   self.write('''
query condition stdout
query comment stdout
null_sink
-
''')
   position=0
   tuples=[]
   condition=None
   for l in self.runrdr():
    if condition is None:
     condition=int(l)
    else:
     tuples.append((position,condition,l))
     position+=1
     condition=None
   filetriggers=trgfile.trgfile(tuples)
  else:
   self.getcontepoch(infile, 0, 1, epochs=1,trigtransfer=True)
   self.write('''
query filetriggers_for_trigfile stdout
null_sink
-
''')
   filetriggers=trgfile.trgfile(self.runrdr())
  return filetriggers

class Epochsource(object):
 '''This class contains everything necessary for representing an epoch source.
    This includes the ability to read from triggers contained in the file, contained in an external file (trigfile) 
    or reading around explicitly specified points (trigpoints).
 '''
 def __init__(self, infile, beforetrig=0, aftertrig=0, continuous=False, fromepoch=None, epochs=None, offset=None, triglist=None, trigfile=None, trigtransfer=False):
  # For convenience, allow infile to be passed as a file name
  if isinstance(infile,str):
   from . import avg_q_file
   self.infile=avg_q_file(infile)
  else:
   self.infile=infile
  self.beforetrig=beforetrig
  self.aftertrig=aftertrig
  self.continuous=continuous
  self.fromepoch=fromepoch
  self.epochs=epochs
  self.offset=offset
  self.triglist=triglist
  self.trigfile=trigfile
  self.trigtransfer=trigtransfer
  self.branch=[]
  self.trigpoints=None
 def add_branchtransform(self,transform):
  # Add a branch script fragment applying only to epochs from this epoch source.
  # Will accept lines already preceded by the branch mark '>' but otherwise will add '>'.
  # Take care here that every line becomes an item in the 'branch' list
  for methodline in transform.split('\n'):
   methodline=methodline.strip()
   if methodline=='': continue
   if methodline.startswith('>'):
    self.branch.append(methodline)
   else:
    self.branch.append('>'+methodline)
 def set_trigpoints(self,trigpoints):
  if self.trigfile is not None:
   raise Exception("Epochsource: cannot specify both trigpoints and trigfile!")
  self.trigfile="stdin"
  if isinstance(trigpoints,list):
   self.trigpoints=trigpoints
  else:
   # Allow a single point to be specified
   self.trigpoints=[trigpoints]
 def send(self,avg_q_instance):
  if self.infile.fileformat=='asc' and self.trigpoints is not None:
   # Allow 'trigger' lists to be used for 'asc' files just as in
   # avg_q.get_filetriggers, namely with (epoch number-1) as position value
   for trigpoint in self.trigpoints:
    epoch=(trigpoint[0] if isinstance(trigpoint,tuple) else trigpoint)+1
    avg_q_instance.getepoch(self.infile, beforetrig=self.beforetrig, aftertrig=self.aftertrig, offset=self.offset, fromepoch=epoch, epochs=1)
   self.trigpoints=None
  else:
   avg_q_instance.getepoch(self.infile, beforetrig=self.beforetrig, aftertrig=self.aftertrig, continuous=self.continuous, fromepoch=self.fromepoch, epochs=self.epochs, offset=self.offset, triglist=self.triglist, trigfile=self.trigfile, trigtransfer=self.trigtransfer)
  for methodline in self.branch:
   avg_q_instance.write(methodline+'\n')
 def send_trigpoints(self,avg_q_instance):
  if self.trigpoints is not None:
   if len(self.trigpoints)>0 and isinstance(self.trigpoints[0],tuple):
    from . import trgfile
    t=trgfile.trgfile()
    t.writetuples(self.trigpoints,avg_q_instance)
   else:
    for trigpoint in self.trigpoints:
     avg_q_instance.write(str(trigpoint) + '\t1\n')
   avg_q_instance.write('0\t0\n')

class Script(object):
 '''Script class to represent the concept of an avg_q (sub-)script as a whole.
    We collect all parts within this object and only send the complete thing to avg_q.
    This constitutes a higher-level interface allowing more error checking while
    constructing input for avg_q. It also encapsulates the technical means necessary
    to read epochs around supplied points, which requires sending the full script followed
    by triggers read by the get_epoch methods.
 '''
 def __init__(self,avg_q_instance):
  self.avg_q_instance=avg_q_instance
  self.Epochsource_list=[]
  self.transforms=[]
  self.collect='null_sink'
  self.postprocess_transforms=[]
 def add_Epochsource(self, epochsource):
  self.Epochsource_list.append(epochsource)
 def add_Epochsource_contfile_excluding_breakpoints(self,infile,start_point,end_point,breakpoints,margin_points,nr_of_points=None,step_points=None):
  '''Read a continuous file in segments, from start_point to end_point, excluding margin_points on
     either side of every point in the breakpoints list.
     Returns the number of valid segments; This must be checked for 0, in which case no epoch source
     method was added.
     If nr_of_points is not set, epochs of varying length will be returned; otherwise, all epochs will
     have exactly this number of points.
     If step_points is not set but nr_of_points is, the epochs covering sections will be contiguous;
     otherwise, step_points gives the number of points to advance the epoch start point. This enables
     for example using overlapping or sparse coverage.'''
  n_segments=0
  if nr_of_points is None or nr_of_points<=0:
   # Return full variable-length segments, one epoch source per segment with single trigger
   for startsegment,endsegment in self.avg_q_instance.get_file_sections_excluding_breakpoints(start_point,end_point,breakpoints,margin_points):
    epochsource=Epochsource(infile, 0, endsegment-startsegment)
    epochsource.set_trigpoints(startsegment)
    self.add_Epochsource(epochsource)
    n_segments+=1
  else:
   # Return fixed-length epochs covering the segments, single epoch source with multiple triggers
   if step_points is None or step_points<=0: step_points=nr_of_points
   trigpoints=[]
   for startsegment,endsegment in self.avg_q_instance.get_file_sections_excluding_breakpoints(start_point,end_point,breakpoints,margin_points):
    while startsegment+nr_of_points<=endsegment:
     trigpoints.append(startsegment)
     n_segments+=1
     startsegment+=step_points
   if n_segments>0:
    epochsource=Epochsource(infile, 0, nr_of_points)
    epochsource.set_trigpoints(trigpoints)
    self.add_Epochsource(epochsource)
  return n_segments
 def add_transform(self,transform):
  self.transforms.append(transform)
 def set_collect(self,collect):
  self.collect=collect
 def add_postprocess(self,transform):
  self.postprocess_transforms.append(transform)
 def runrdr(self):
  if len(self.Epochsource_list)==0:
   raise Exception("Script: No epoch source given!")
  for epochsource in self.Epochsource_list:
   epochsource.send(self.avg_q_instance)
  for transform in self.transforms:
   self.avg_q_instance.write(transform+"\n")
  self.avg_q_instance.write(self.collect+"\n")
  if len(self.postprocess_transforms)>0:
   self.avg_q_instance.write("Post:\n")
   for transform in self.postprocess_transforms:
    self.avg_q_instance.write(transform+"\n")
  self.avg_q_instance.write("-\n")
  for epochsource in self.Epochsource_list:
   epochsource.send_trigpoints(self.avg_q_instance)
  return self.avg_q_instance.runrdr()
 def __iter__(self):
  return self.runrdr()
 def run(self):
  """Run script, printing the output."""
  for line in self:
   print(line)

if __name__ == '__main__':
 # A simple self-contained test case: Run 5 scripts with varying sampling frequencies
 a=avg_q()

 for i in range(5):
  sfreq=(i+1)*10
  a.write('''
null_source %(sfreq)d 1 30 1s 1s
query -N sfreq stdout
query -N nr_of_points stdout
null_sink
-
''' % {'sfreq' : sfreq})

 a.run()

 a.close()
