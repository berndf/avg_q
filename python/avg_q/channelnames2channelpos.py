# Copyright (C) 2008,2010,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Utility function to make a set_channelposition compatible string
from a vector of channel names
"""
import math
def channelnames2channelpos(channelnames):
 nr_of_channels=len(channelnames)
 ncols=int(round(math.sqrt(nr_of_channels)))
 setup=''
 for channel in range(nr_of_channels):
  div,mod=divmod(channel,ncols)
  # Note that we don't use escape_channelname for channelnames because
  # set_channelposition doesn't treat '-' specially as channel list arguments do.
  setup+=" %s %g %g %g" % (channelnames[channel].replace(' ','\\ '), mod, -div, 0)
 return setup
