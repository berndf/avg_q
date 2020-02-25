# Copyright (C) 2008-2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Class to classify and project out artifacts by using ICA
"""

from . import Channeltypes
from .avg_q import channel_list2arg
import os

class ICA_ArtControl(object):
 def __init__(self,avg_q_instance):
  self.avg_q_instance=avg_q_instance
  self.base=None
  self.tmpfiles=[]
  # Bipolar EOG channels do not need to be discarded for ICA
  self.remove_channels=Channeltypes.NonEEGChannels.difference(Channeltypes.BipolarEOGchannels)
  self.clearComponents()
 def clearComponents(self):
  self.ArtComponents=set()
  self.nr_of_components=None
 def remove_tmpfiles(self):
  for tmpfile in self.tmpfiles:
   if os.path.exists(tmpfile): os.unlink(tmpfile)
  self.tmpfiles=[]
 def __del__(self):
  # Destructor.
  self.remove_tmpfiles()
 def set_base(self,base):
  '''This collects information that we will need downstream'''
  self.remove_tmpfiles()
  self.clearComponents()
  self.base=base
  self.avg_q_instance.write('''
readasc %(base)s_weights_scaled.asc
query nr_of_points stdout
null_sink
-
''' % {
   'base': self.base,
  })
  rdr=self.avg_q_instance.runrdr()
  self.nr_of_components=int(next(rdr))
  for line in rdr:
   print(line)
 def get_remove_channels(self):
  '''Helper function to get the necessary remove_channel command'''
  return 'remove_channel -n ?' + channel_list2arg(self.remove_channels) if self.remove_channels else ''
 def set_ArtComponents(self,ArtComponents):
  self.ArtComponents=set(ArtComponents)
 def add_ArtComponents(self,AddComponents):
  self.ArtComponents.update(AddComponents)
 def notArtComponents(self):
  return set(range(1,self.nr_of_components+1)).difference(self.ArtComponents)
 def get_Artifact_Components(self,artifactfile,relative_cutoff,fromepoch=1):
  '''This version is optimized for working with averaged artifact traces
     such as averaged EOG. The components are extracted by relative importance,
     assuming that there must be some components clearly representing this
     activity. Due to 'scale_by invmax', the maximum component will carry the
     weight 1 and therefore will always be selected.
     Compare get_Artifact_Components2 below.'''
  self.avg_q_instance.write('''
readasc -f %(fromepoch)d -e 1 %(artifactfile)s
%(remove_channels)s
project -C -n -p 0 %(base)s_weights_scaled.asc 0
#posplot
calc abs
trim -a 0 0
scale_by invmax
write_generic -N -P stdout string
null_sink
-
''' % {
   'fromepoch': fromepoch,
   'remove_channels': self.get_remove_channels(),
   'artifactfile': artifactfile,
   'base': self.base,
  })
  components=[]
  for line in self.avg_q_instance.runrdr():
   component,value=line.rstrip('\r\n').split('\t')
   value=float(value)
   component=int(component)
   if value>relative_cutoff:
    components.append(component)
  return components
 def get_trim_selecting_strong_maps(self,getepochscript,relative_strength_cutoff):
  from . import trgfile
  self.avg_q_instance.write(getepochscript)
  self.avg_q_instance.write('''
demean_maps
calc abs
collapse_channels
scale_by invpointmaxabs
query nr_of_points stdout
write_crossings collapsed %g stdout
null_sink
-
''' % relative_strength_cutoff)
  r=self.avg_q_instance.runrdr()
  nr_of_points=int(next(r))
  t=trgfile.trgfile(r)
  ranges=[]
  start=None
  for point,code,description in t:
   if code== -1:
    if start: ranges.append((start,point))
    else: ranges.append((0,point))
    start=None
   else:
    start=point
  if start: ranges.append((start,nr_of_points))
  if len(ranges)==0: ranges.append((0,nr_of_points))
  return '  '.join(["%d %d" % (start,end-start) for start,end in ranges])
 def get_Artifact_Components2(self,artifactfile,relative_cutoff,relative_strength_cutoff=0.3,fromepoch=1):
  '''In contrast to get_Artifact_Components, This version tries to normalize
     the incoming maps, without enforcing that the maximum component will
     always be selected. Components will only be selected if the similarity
     to the presented maps is sufficiently high. To do this, we first have to
     extract maps with sufficient power from the input.
     Set relative_strength_cutoff=0 to really include all incoming maps.'''
  getepochscript='''
readasc -f %(fromepoch)d -e 1 %(artifactfile)s
%(remove_channels)s
''' % {
   'fromepoch': fromepoch,
   'remove_channels': self.get_remove_channels(),
   'artifactfile': artifactfile,
  }
  trim=self.get_trim_selecting_strong_maps(getepochscript,relative_strength_cutoff)
  self.avg_q_instance.write(getepochscript)
  self.avg_q_instance.write('''
trim %(trim)s
scale_by invmaxabs
project -C -n -p 0 %(base)s_weights_scaled.asc 0
#posplot
calc abs
trim -h 0 0
write_generic -N -P stdout string
null_sink
-
''' % {
   'trim': trim,
   'base': self.base,
  })
  components=[]
  for line in self.avg_q_instance.runrdr():
   component,value=line.rstrip('\r\n').split('\t')
   value=float(value)
   component=int(component)
   if value>relative_cutoff:
    components.append(component)
  return components
 def get_trim_from_components(self,components):
  '''Utility function to get the 'trim' statement for selecting points corresponding to components.
  Note that this selection is positive, ie includes points for these components, therefore the
  order is important (we use sorted(components) here).'''
  if len(components)==0:
   trim=''
  else:
   trim='trim '+' '.join(['%d 1' % (x-1) for x in sorted(components)])
  return trim
 def get_remove_channels_from_components(self,components,keep=False):
  '''Utility function to get the 'remove_channel' statement for removing channels corresponding to components.
  Note that this selection is negative by default, ie removes channels for these components;
  With keep=True it keeps these channels instead. In this case order is important, which is why we sort components
  just as in get_trim_from_components.'''
  if not keep and len(components)==0:
   remove_channel=''
  else:
   remove_channel='remove_channel ' +('-k ' if keep else '-n ')+channel_list2arg(["%d" % comp for comp in sorted(components)])
  return remove_channel
 def get_backproject_script(self,components=None):
  if components is None:
   components=self.notArtComponents()
  weightstmp='%s_weights_scaled_tmpproject.asc' % self.base
  mapstmp='%s_maps_scaled_tmpproject.asc' % self.base
  self.avg_q_instance.write('''
readasc %(base)s_weights_scaled.asc
%(trim)s
writeasc -b %(weightstmp)s
null_sink
-
readasc %(base)s_maps_scaled.asc
%(trim)s
writeasc -b %(mapstmp)s
null_sink
-
''' % {
   'base': self.base,
   'trim': self.get_trim_from_components(components),
   'weightstmp': weightstmp,
   'mapstmp': mapstmp,
  })
  self.tmpfiles.extend([weightstmp, mapstmp])
  return '''
%(remove_channels)s
project -C -n -p 0 %(weightstmp)s 0
project -C -n -p 0 -m %(mapstmp)s 0
''' % {
   'remove_channels': self.get_remove_channels(),
   'weightstmp': weightstmp,
   'mapstmp': mapstmp,
  }
 def get_extract_script(self,components=None):
  '''Only the first step of backprojection, return the traces.'''
  if components is None:
   components=self.notArtComponents()
  weightstmp='%s_weights_scaled_tmpproject.asc' % self.base
  self.avg_q_instance.write('''
readasc %(base)s_weights_scaled.asc
%(trim)s
writeasc -b %(weightstmp)s
null_sink
-
''' % {
   'base': self.base,
   'weightstmp': weightstmp,
   'trim': self.get_trim_from_components(components),
  })
  self.tmpfiles.append(weightstmp)
  return '''
%(remove_channels)s
project -C -n -p 0 %(weightstmp)s 0
''' % {
   'remove_channels': self.get_remove_channels(),
   'weightstmp': weightstmp,
  }
 def get_reconstruct_script(self,components=None):
  '''Only the second step of backprojection, start with traces.'''
  if components is None:
   components=self.notArtComponents()
  mapstmp='%s_maps_scaled_tmpproject.asc' % self.base
  self.avg_q_instance.write('''
readasc %(base)s_maps_scaled.asc
%(trim)s
writeasc -b %(mapstmp)s
null_sink
-
''' % {
   'base': self.base,
   'mapstmp': mapstmp,
   'trim': self.get_trim_from_components(components),
  })
  self.tmpfiles.append(mapstmp)
  return '''
%(remove_channels)s
project -C -n -p 0 -m %(mapstmp)s 0
''' % {
   'remove_channels': self.get_remove_channels_from_components(self.ArtComponents),
   'mapstmp': mapstmp,
  }
 def get_weighted_extractor_script(self,components=None):
  '''Special application: Linear superposition of weights, like reconstruct but with weights instead of maps'''
  if components is None:
   components=self.notArtComponents()
  weightstmp='%s_weights_scaled_tmpproject.asc' % self.base
  self.avg_q_instance.write('''
readasc %(base)s_weights_scaled.asc
%(trim)s
writeasc -b %(weightstmp)s
null_sink
-
''' % {
   'base': self.base,
   'weightstmp': weightstmp,
   'trim': self.get_trim_from_components(components),
  })
  self.tmpfiles.append(weightstmp)
  return '''
%(remove_channels)s
project -C -n -p 0 -m %(weightstmp)s 0
''' % {
   'remove_channels': self.get_remove_channels_from_components(self.ArtComponents),
   'weightstmp': weightstmp,
  }

