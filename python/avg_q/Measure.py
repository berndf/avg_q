# Copyright (C) 2010-2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Measure class.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
import copy

class Measure_Script(avg_q.Script):
 def __init__(self,avg_q_instance):
  avg_q.Script.__init__(self,avg_q_instance)
  self.sum_range=False # If set, measure the sum, not the average of each range
  self.output_comment=False # If set, output the epoch comment as second element of the output list
 def measure(self,channels_and_lat_ranges):
  '''channels_and_lat_ranges is a list of channels and associated latency ranges:
     [['PO3',[[90,120],[150,180]]],['Cz',[[370,550]]]]
     Channels can actually be any avg_q channel list specification like 'PO3,Cz' or 'ALL'.
     The output is a list of [epoch,channelnames,rangename,amp1,lat1,amp2,lat2,...] with
     amp1,amp2 etc. the measurements for the channels. channelnames is the explicit
     comma-separated list of measured channels.
  '''
  # Save and restore the current list of transforms, since we measure by (temporally)
  # appending to this list
  storetransforms=copy.copy(self.transforms)
  for channel,lat_ranges in channels_and_lat_ranges:
   self.add_transform("""
extract_item 0
push
remove_channel -k %(channel)s
""" % {
 'channel': channel,
   })
   for lat_range in lat_ranges:
    self.add_transform("""
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
trim %(average_or_sum)s 0 0
query -t accepted_epochs stdout
%(output_comment)s
query -t channelnames stdout
echo -F stdout %(lower)g %(upper)g\\t
write_generic stdout string
pop
""" % {
    'average_or_sum': '-s' if self.sum_range else '-a',
    'output_comment': 'query -t comment stdout' if self.output_comment else '',
    'lower': lat_range[0],
    'upper': lat_range[1],
    })
   self.add_transform('pop')
  rdr=self.runrdr()
  result=[]
  fixed_elements=3 if self.output_comment else 2
  for l in rdr:
   values=l.split('\t')
   # With N channels we get:
   # epoch,channelname*N,rangename,(amp,lat)*N
   # So we have fixed_elements+N*3 values
   N=int((len(values)-fixed_elements)/3)
   epoch=int(values[0])+1
   channelnames=",".join(values[1:(1+N)])
   rangename=values[1+N]
   #epoch,channelname,rangename,amp,lat=l.split('\t')
   result.append([epoch,channelnames,rangename]+[float(x) for x in values[(fixed_elements+N):]])
  self.transforms=storetransforms
  return result
