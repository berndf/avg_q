# Copyright (C) 2007-2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Python interface to avg_q.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import re
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

# Layout helper function to get a 2D arrangement of nplots plots
def nrows_ncols_from_nplots(nplots):
 import numpy as np
 ncols=np.sqrt(nplots)
 if ncols.is_integer():
  nrows=ncols
 else:
  ncols=np.ceil(ncols)
  nrows=nplots/ncols
  if not nrows.is_integer():
   nrows=np.ceil(nrows)
 return int(nrows),int(ncols)

outtuple=[]
collectvalue=None
listvalue=[]
class avg_q(object):
 def __init__(self,avg_q="avg_q_vogl",endstring="End of script",tracelevel=0):
  """Start avg_q."""
  self.endstring=endstring
  self.querymatch=re.compile('^([^=]+)=(.+)$')
  call=[avg_q,'stdin']
  if tracelevel>0:
   call.insert(1,'-t %d' % tracelevel)
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
  '''Iterator of (startsegment,endsegment) avoiding breakpoints by margin_points'''
  br=[point for point in breakpoints] # Copy the points
  br.sort()
  br.append(end_point+margin_points) # Needed so that the last segment will really extend to end_point
  startsegment=start_point
  for point in br:
   endsegment=point-margin_points
   if endsegment>startsegment:
    yield (startsegment,endsegment)
    startsegment=point+margin_points
 def get_description(self,infile,getvars):
  if not isinstance(getvars,tuple):
   getvars=(getvars,)
  self.getcontepoch(infile, 0, 1, epochs=1)
  global outtuple
  outtuple=[]
  for var in getvars:
   self.write('''
query -N %s stdout
''' % var)
  self.write('''
null_sink
-
''')
  global collectvalue,listvalue
  collectvalue=None
  listvalue=[]
  def closelist():
   global collectvalue,listvalue
   if collectvalue and len(listvalue)>0:
    outtuple.append(listvalue)
    listvalue=[]
   collectvalue=False
  for r in self:
   m=self.querymatch.match(r)
   if m:
    var,value=m.groups()
    if var in getvars:
     closelist()
     if valuetype[var]==list:
      listvalue=[value]
      collectvalue=True
     else:
      value=valuetype[var](value)
      outtuple.append(value)
      collectvalue=False
    else: m=None
   if not m:
    if collectvalue:
      listvalue.append(r)
  closelist()
  if len(getvars)==1: outtuple=outtuple[0]
  else: outtuple=tuple(outtuple)
  return outtuple
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
 def plot_maps(self, rest_of_script='\nnull_sink\n-\n'):
  import numpy as np
  import matplotlib.mlab as mlab
  import matplotlib.pyplot as plt

  def mapplot(xpos,ypos,z,nsteps=50):
   #ncontours=15
   xmin,xmax=xpos.min(),xpos.max()
   ymin,ymax=ypos.min(),ypos.max()
   xi=np.linspace(xmin,xmax,nsteps)
   yi=np.linspace(ymin,ymax,nsteps)
   nplots=z.shape[0]
   nrows,ncols=nrows_ncols_from_nplots(nplots)
   for thisplot in range(0,nplots):
    # cf. http://www.scipy.org/Cookbook/Matplotlib/Gridding_irregularly_spaced_data
    zi=mlab.griddata(xpos,ypos,z[thisplot],xi,yi)
    # Ensure a color mapping symmetric around 0
    zmin,zmax=zi.min(),zi.max()
    if -zmin>zmax:
     zmax= -zmin
    plt.subplot(nrows,ncols,thisplot+1)
    gplot=plt.pcolor(xi,yi,zi,norm=plt.normalize(vmin=-zmax,vmax=zmax),antialiaseds=False) # shading="faceted"
    #gplot=plt.contourf(g,ncontours)
    #plt.scatter(xpos,ypos,marker='o',c='black',s=5) # Draw sample points
    plt.contour(xi,yi,zi,[0],colors='black') # Draw zero line
    gplot.axes.set_axis_off()
    gplot.axes.set_xlim(xmin,xmax)
    gplot.axes.set_ylim(ymin,ymax)

  self.write('''
query channelpositions stdout
extract_item 0
echo -F stdout Data:\\n
write_generic stdout string
''')
  self.write(rest_of_script)
  rdr=self.runrdr()
  xpos=[]
  ypos=[]
  for r in rdr:
   if r=='Data:':
    break
   channelname,x,y,z=r.split()
   xpos.append(float(x))
   ypos.append(float(y))
  z=[]
  for r in rdr:
   z.append([float(x) for x in r.split()])
  mapplot(np.array(xpos),np.array(ypos),np.array(z))
 def plot_traces(self, vmin=None, vmax=None, rest_of_script='\nnull_sink\n-\n'):
  import numpy as np
  import matplotlib.pyplot as plt

  def traceplot(z):
   #ncontours=15
   nplots=len(z)
   nrows,ncols=nrows_ncols_from_nplots(nplots)
   thisplot=0
   for z1 in z:
    z1=np.array(z1)
    plt.subplot(nrows,ncols,thisplot+1)
    gplot=plt.pcolor(z1,norm=plt.normalize(vmin=vmin,vmax=vmax)) # shading="faceted"
    #gplot=plt.contourf(z1,ncontours)
    gplot.axes.set_axis_off()
    #print z1.shape
    gplot.axes.set_xlim(0,z1.shape[1])
    gplot.axes.set_ylim(0,z1.shape[0])
    thisplot+=1

  self.write('''
echo -F stdout Epoch Start\\n
write_generic stdout string
''')
  self.write(rest_of_script)
  z=[] # One array per *channel*, each array collects all time points and epochs, epochs varying fastest
  point=0
  for r in self:
   if r=='Epoch Start':
    point=0
   else:
    channels=[float(x) for x in r.split()]
    for channel in range(0,len(channels)):
     if len(z)<=channel:
      z.append([[channels[channel]]])
     else:
      if len(z[channel])<=point:
       z[channel].append([channels[channel]])
      else:
       z[channel][point].append(channels[channel])
    point+=1
  traceplot(z)

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
  self.branch=None
  self.trigpoints=None
 def set_branch(self,branch):
  self.branch=branch
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
  if self.branch:
   # Send a branch script fragment applying only to epochs from this epoch source.
   # Will accept lines already preceded by the branch mark '>' but otherwise will add '>'.
   for methodline in self.branch.split('\n'):
    methodline=methodline.strip()
    if methodline=='': continue
    if methodline.startswith('>'):
     avg_q_instance.write(methodline+'\n')
    else:
     avg_q_instance.write('>'+methodline+'\n')
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
