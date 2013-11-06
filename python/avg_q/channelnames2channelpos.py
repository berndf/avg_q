# Copyright (C) 2008 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Utility function to make a set_channelposition compatible string
from a vector of channel names
"""
import math
from .avg_q import escape_channelname
def channelnames2channelpos(channelnames):
 nr_of_channels=len(channelnames)
 ncols=int(round(math.sqrt(nr_of_channels)))
 setup=''
 for channel in range(nr_of_channels):
  div,mod=divmod(channel,ncols)
  setup+=" %s %g %g %g" % (escape_channelname(channelnames[channel]), mod, -div, 0)
 return setup
