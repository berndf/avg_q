# Copyright (C) 2022 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
(f)NIRS utilities.
"""

import scipy

from . import numpy_Script
class nirs_Epochsource(numpy_Script.numpy_Epochsource):
 def __init__(self, infile, beforetrig=0, aftertrig=0, continuous=False, fromepoch=None, epochs=None, offset=None, triglist=None, trigfile=None, trigtransfer=False):
  from . import avg_q_file
  filename=infile.filename if isinstance(infile,avg_q_file) else infile
  nirs=scipy.io.matlab.loadmat(filename)

  #nirs.keys()
  # dict_keys(['__header__', '__version__', '__globals__', 't', 'd', 'SD', 's', 'aux'])

  # t is time, d data, s a mask, aux aux data matrix
  t=nirs['t']
  d=nirs['d'] # 44 columns
  SD=nirs['SD']
  # SD is a ndarray (1,1); SD[0,0] is numpy.generic with 7 elements
  S=SD[0,0]
  # S[2] and S[3] are 3D coordinates of senders and receivers
  # S[0] Seems to have the channels (=combinations between senders and receivers) with 4 integer columns
  # (here 44 rows)

  epoch=numpy_Script.numpy_epoch(d)
  epoch.xdata=t
  epoch.xchannelname='Time[s]'
  epoch.channelnames=['-'.join([('%d' % ss) for ss in s]) for s in S[0]]
  super().__init__(epochs=[epoch])
