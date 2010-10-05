# Copyright (C) 2008,2009 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Class to classify and project out artifacts by using ICA
"""

import os

class ICA_ArtControl(object):
 def __init__(self,avg_q_object):
  self.avg_q_object=avg_q_object
  self.base=None
  self.tmpfiles=[]
  self.remove_channels=set(['Ekg1','Ekg2','Ekg','ECG','BEMG1','BEMG2','EDA','GSR_MR_100_EDA'])
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
  self.avg_q_object.write('''
readasc %(base)s_weights_scaled.asc
query nr_of_points stdout
null_sink
-
''' % {
  'base': self.base,
  })
  rdr=self.avg_q_object.runrdr()
  self.nr_of_components=int(next(rdr))
  for line in rdr:
   print(line)
 def get_remove_channels(self):
  '''Helper function to get the necessary remove_channel command'''
  return 'remove_channel -n ?' + ",".join(self.remove_channels) if self.remove_channels else ''
 def set_ArtComponents(self,ArtComponents):
  self.ArtComponents=set(ArtComponents)
 def add_ArtComponents(self,AddComponents):
  self.ArtComponents.update(AddComponents)
 def notArtComponents(self):
  return set(range(1,self.nr_of_components+1)).difference(self.ArtComponents)
 def get_Artifact_Components(self,artifactfile,relative_cutoff):
  '''This version is optimized for working with averaged artifact traces
     such as averaged EOG. The components are extracted by relative importance,
     assuming that there must be some components clearly representing this
     activity. Due to 'scale_by invmax', the maximum component will carry the
     weight 1 and therefore will always be selected. 
     Compare get_Artifact_Components2 below.'''
  self.avg_q_object.write('''
readasc %(artifactfile)s
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
  'remove_channels': self.get_remove_channels(),
  'artifactfile': artifactfile,
  'base': self.base,
  })
  components=[]
  for line in self.avg_q_object.runrdr():
   component,value=line.rstrip('\r\n').split('\t')
   value=float(value)
   component=int(component)
   if value>relative_cutoff:
    components.append(component)
  return components
 def get_trim_selecting_strong_maps(self,getepochscript,relative_strength_cutoff):
  from . import trgfile
  self.avg_q_object.write(getepochscript)
  self.avg_q_object.write('''
demean_maps
calc abs
collapse_channels
scale_by invpointmaxabs
query nr_of_points stdout
write_crossings collapsed %g stdout
null_sink
-
''' % relative_strength_cutoff)
  r=self.avg_q_object.runrdr()
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
 def get_Artifact_Components2(self,artifactfile,relative_cutoff,relative_strength_cutoff=0.3):
  '''In contrast to get_Artifact_Components, This version tries to normalize
     the incoming maps, without enforcing that the maximum component will
     always be selected. Components will only be selected if the similarity
     to the presented maps is sufficiently high. To do this, we first have to
     extract maps with sufficient power from the input.
     Set relative_strength_cutoff=0 to really include all incoming maps.'''
  getepochscript='''
readasc %(artifactfile)s
%(remove_channels)s
''' % {
  'remove_channels': self.get_remove_channels(),
  'artifactfile': artifactfile,
  }
  trim=self.get_trim_selecting_strong_maps(getepochscript,relative_strength_cutoff)
  self.avg_q_object.write(getepochscript)
  self.avg_q_object.write('''
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
  for line in self.avg_q_object.runrdr():
   component,value=line.rstrip('\r\n').split('\t')
   value=float(value)
   component=int(component)
   if value>relative_cutoff:
    components.append(component)
  return components
 def get_backproject_script(self):
  trim='trim '+' '.join(['%d 1' % (x-1) for x in self.notArtComponents()])
  self.avg_q_object.write('''
readasc %(base)s_weights_scaled.asc
%(trim)s
writeasc -b %(base)s_weights_scaled_tmpproject.asc
null_sink
-
readasc %(base)s_maps_scaled.asc
%(trim)s
writeasc -b %(base)s_maps_scaled_tmpproject.asc
null_sink
-
''' % {
  'base': self.base,
  'trim': trim,
  })
  self.tmpfiles.extend(['%s_weights_scaled_tmpproject.asc' % self.base, '%s_maps_scaled_tmpproject.asc' % self.base])
  return '''
%(remove_channels)s
project -C -n -p 0 %(base)s_weights_scaled_tmpproject.asc 0
project -C -n -p 0 -m %(base)s_maps_scaled_tmpproject.asc 0
''' % {
  'remove_channels': self.get_remove_channels(),
  'base': self.base,
  'trim': trim,
  }

