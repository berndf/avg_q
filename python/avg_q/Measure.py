# Copyright (C) 2010-2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Measure class.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
from .avg_q import escape_channelname
import copy

class Measure_Script(avg_q.Script):
 def __init__(self,avg_q_instance):
  super().__init__(avg_q_instance)
  self.sum_range=False # If set, measure the sum, not the average of each range
  self.output_comment=False # If set, output the epoch comment as second element of the output list
  self.output_averages=False # If set, output nrofaverages as second/third element of the output list
 def measure(self,channels_and_lat_ranges):
  '''Measure mean amplitude and center-of-gravity latency in latency ranges.
     channels_and_lat_ranges is a list of channels and associated latency ranges to measure:
     [['PO3',[[90,120],[150,180]]],['Cz',[[370,550]]]]
     Channels can actually be any avg_q channel list specification like 'PO3,Cz' or 'ALL'.
     The output is a list of [epoch,channelnames,rangename,amp1,lat1,amp2,lat2,...] with
     amp1,amp2 etc. the measurements for the channels. channelnames is the explicit
     comma-separated list of measured channels.
     Note that the latency algorithm assumes positive-going amplitudes by default. To measure in negative
     direction, add a third value -1 to the given range.
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
    'channel': escape_channelname(channel),
   })
   for lat_range in lat_ranges:
    self.add_transform("""
push
trim -x %(lower)g %(upper)g
extract_item 0 0
%(calc_neg)s
add -i 1 negpointmin
scale_by -i 1 invpointsum
scale_by -i 1 xdata
# This arranges for item 1 to actually be SUMMED:
set leaveright 1
trim %(average_or_sum)s 0 0
query -t accepted_epochs stdout
%(output_comment)s
%(output_averages)s
query -t channelnames stdout
echo -F stdout %(lower)g %(upper)g\\t
write_generic stdout string
pop
""" % {
     'average_or_sum': '-s' if self.sum_range else '-a',
     'output_comment': 'query -t comment stdout' if self.output_comment else '',
     'output_averages': 'query -t nrofaverages stdout' if self.output_averages else '',
     'lower': lat_range[0],
     'upper': lat_range[1],
     'calc_neg': 'calc -i 1 neg' if len(lat_range)==3 and lat_range[2]== -1 else '',
    })
   self.add_transform('pop')
  rdr=self.runrdr()
  result=[]
  # Fixed elements are accepted_epochs [output_comment output_averages] rangename
  fixed_elements=2+self.output_comment+self.output_averages
  for l in rdr:
   values=l.split('\t')
   # With N channels we get:
   # epoch,[comment,averages,]channelname*N,rangename,(amp,lat)*N
   # So we have fixed_elements+N*3 values
   N=int((len(values)-fixed_elements)/3)
   epoch=int(values[0])+1
   other_elements=[]
   if self.output_comment:
    other_elements.append(values[1])
   if self.output_averages:
    other_elements.append(float(values[fixed_elements-2]))
   channelnames=",".join(values[(fixed_elements-1):(fixed_elements-1+N)])
   rangename=values[1+N]
   #epoch,channelname,rangename,amp,lat=l.split('\t')
   result.append([epoch]+other_elements+[channelnames,rangename]+[float(x) for x in values[(fixed_elements+N):]])
  self.transforms=storetransforms
  return result
