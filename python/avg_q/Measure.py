# Copyright (C) 2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Measure class.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

class Measure(object):
 def __init__(self,avg_q_object):
  self.avg_q_object=avg_q_object
 def get_measurescript(self,channels_and_lat_ranges):
  script=""
  for channel,lat_ranges in channels_and_lat_ranges:
   script+="""
extract_item 0
push
remove_channel -k %(channel)s
""" % {
 'channel': channel,
   }
   for lat_range in lat_ranges:
    script+="""
push
trim -x %(lower)g %(upper)g
extract_item 0 0
# Subtract a line between the first and the last data point:
#detrend -i 1 -I
calc -i 1 abs
scale_by -i 1 invpointsum
scale_by -i 1 xdata
# This arranges for item 1 to actually be SUMMED:
set leaveright 1
trim -a 0 0
query -t accepted_epochs stdout
query -t channelnames stdout
echo -F stdout %(lower)g %(upper)g\\t
write_generic stdout string
pop
""" % {
    'lower': lat_range[0],
    'upper': lat_range[1],
    }
   script+="""
pop
"""
  script+="""
null_sink
-
"""
  return script
 def read_result(self):
  rdr=self.avg_q_object.runrdr()
  result=[]
  for l in rdr:
   epoch,channelname,rangename,amp,lat=l.split('\t')
   result.append((int(epoch)+1,channelname,rangename,float(amp),float(lat)))
  return result
 def measure(self,channels_and_lat_ranges):
  self.avg_q_object.write(self.get_measurescript(channels_and_lat_ranges))
  return self.read_result()
 def measure_finish(self,channels_and_lat_ranges,preprocess=''):
  '''Measure version to be used after get_epoch_around_add.
  This allows to read arbitrary sections from a continuous file and measure them.'''
  self.avg_q_object.get_epoch_around_finish(preprocess+self.get_measurescript(channels_and_lat_ranges))
  return self.read_result()
