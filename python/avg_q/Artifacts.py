# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Artifact related classes.
Currently contains the Artifact_Segmentation Script class detecting artifacts in continuous data
and selecting artifact-free segments.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
from .avg_q import channel_list2arg
from . import trgfile

class collect_crossings(object):
 '''Process the artifact trigger stream (Super-threshold Extrema, Crossings=Start/Stop ranges)
    into a mixed point / range 'breakpoints' list, collected in self.artifacts
 '''
 def __init__(self,min_blocking_points):
  # min_blocking_points sets a minimum number of adjacent points with difference=0 to detect as blocking
  self.min_blocking_points=min_blocking_points
  self.artifacts=[]
  self.positive_flank=None
  self.start_point=None
  self.end_point=None
 def finish_crossings(self,detection_type):
  '''If positive_flank was seen, close segment ending at end_point'''
  if self.positive_flank is not None:
   self.artifacts.append((self.positive_flank+self.start_point,self.end_point))
   self.positive_flank=None
  #print("Detection type=%s %d" % (detection_type,len(self.artifacts)))
 def add_crossings(self,crossings,start_point,end_point):
  self.start_point=start_point
  self.end_point=end_point
  # Crossings is a trgfile
  preamble_strings_len=0
  detection_type=None
  for point,code,description in crossings:
   if len(crossings.preamble.strings)!=preamble_strings_len:
    # Something was added to the preamble list, i.e. a new detection block has started
    detection_type=crossings.preamble.strings[-1][2:] # Crossings or Extrema
    preamble_strings_len=len(crossings.preamble.strings)
    self.finish_crossings(detection_type)
   if detection_type=='Crossings':
    # This means start,end of 'blocking', zero-line detection
    if code==1:
     # Positive flank
     self.positive_flank=point
    else:
     # Negative flank, close segment
     if self.positive_flank is None: self.positive_flank=0
     if point-self.positive_flank>self.min_blocking_points:
      self.artifacts.append((self.positive_flank+self.start_point,point+self.start_point))
     self.positive_flank=None
   else:
    self.artifacts.append(point+self.start_point)
  self.finish_crossings(detection_type)
 def get_tuples(self):
  atuples=[]
  for artifact in self.artifacts:
   if isinstance(artifact,tuple):
    atuples.extend([(artifact[0],2,"Blocking start"),(artifact[1],-2,"Blocking end")])
   else:
    atuples.append((artifact,1))
  return atuples
 def write_artifacts(self,outfilename):
  atrg=trgfile.trgfile()
  with open(outfilename,"w") as afile:
   atrg.writetuples(self.get_tuples(),afile)

class Artifact_Segmentation(avg_q.Script):
 '''Artifact_Segmentation Script class to detect artifacts in a section of a continuous file
    and work on the artifact-free segments between them.
    Three types of artifacts are detected: Peaks exceeding ArtifactDetectionThreshold,
    Jumps (discontinuities) exceeding JumpDetectionThreshold and blocking (zero difference
    between adjacent points) exceeding a duration of min_blocking_points.
    The threshold amplitudes are relative to the median across all points - each channel is normalized
    by its median before the operation because the typical amplitude of each channel usually varies
    with its distance from the reference. By this procedure the operation is scale free.
    There are two causes of blocking - during a recording, it indicates amplifier saturation;
    in some environments (eg sleep), disconnecting the amplifiers results in zero lines written.
    For this reason, blocking marks possibly extended regions, while the other two artifact types
    are detected as single points.'''
 ArtifactDetectionThreshold=4.0 # Detect phasic artifacts exceeding this threshold
 JumpDetectionThreshold=20.0 # Detect jumps (after differentiate) by this threshold
 min_blocking_points=3 # Do not add single or 3 repeated points as blocking
 def __init__(self,avg_q_instance,infile,start_point,end_point):
  self.infile=infile
  self.start_point=start_point
  self.end_point=end_point
  self.collected=None
  avg_q.Script.__init__(self,avg_q_instance)
 def collect_artifacts(self,remove_channels=None,preprocess=''):
  '''Analyze the given section of the continuous file for artifacts. 
     remove_channels is a list of channel names to exclude for detection.'''
  epochsource=avg_q.Epochsource(self.infile,0,self.end_point-self.start_point)
  epochsource.set_trigpoints(self.start_point)
  script=avg_q.Script(self.avg_q_instance)
  script.add_Epochsource(epochsource)
  script.set_collect('append')
  import tempfile
  tempscalefile=tempfile.NamedTemporaryFile()
  script.add_postprocess('''
%(remove_channels)s
%(preprocess)s
push
differentiate
calc abs
push
trim -M 0 0
writeasc -b -c %(tempscalefile)s
pop
subtract -d -P -c %(tempscalefile)s
push
collapse_channels -h
write_crossings -E collapsed %(JumpDetectionThreshold)g stdout
pop
collapse_channels -l
recode 0 0 1 1  0 Inf 0 0
write_crossings collapsed 0.5 stdout
pop
fftfilter 0 0 30Hz 35Hz
subtract -d -P -c %(tempscalefile)s
calc abs
collapse_channels -h
write_crossings -E collapsed %(ArtifactDetectionThreshold)g stdout
''' % {
 'remove_channels': 'remove_channel -n ?' + channel_list2arg(remove_channels) if remove_channels else '',
   'preprocess': preprocess,
   'tempscalefile': tempscalefile.name,
   'JumpDetectionThreshold': self.JumpDetectionThreshold,
   'ArtifactDetectionThreshold': self.ArtifactDetectionThreshold})
  crossings=trgfile.trgfile(script.runrdr())
  self.collected=collect_crossings(self.min_blocking_points)
  self.collected.add_crossings(crossings,self.start_point,self.end_point)
  tempscalefile.close() # The temp file will be deleted at this point.
 def add_Epochsource_contfile_excluding_artifacts(self,margin_points,nr_of_points=None,step_points=None,additional_breakpoints=None):
  return self.add_Epochsource_contfile_excluding_breakpoints(self.infile,self.start_point,self.end_point,self.collected.artifacts+additional_breakpoints if additional_breakpoints else self.collected.artifacts,margin_points,nr_of_points,step_points)
 def show_artifacts(self, epochlength=0):
  '''Read the full continuous file and add triggers showing the breakpoints (and -regions).
     Note that this currently works for avg_q_vogl only, not for avg_q_ui.'''
  epochsource=avg_q.Epochsource(self.infile,aftertrig=epochlength,continuous=True,trigtransfer=True)
  epochsource.set_trigpoints(self.collected.get_tuples())
  script=avg_q.Script(self.avg_q_instance)
  script.add_Epochsource(epochsource)
  script.set_collect('append -l')
  script.add_postprocess('posplot')
  script.run()
