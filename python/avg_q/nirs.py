# Copyright (C) 2022 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
(f)NIRS utilities.
"""

from . import trgfile
import datetime

class nirx_trifile(trgfile.trgfile):
 def __init__(self,infile):
  self.infile=infile
  super().__init__(infile)
  self.firsttime=self.secondtime=None
 def rdr(self):
  while True:
   line=self.getline()
   if not isinstance(line,str):
    break
   line=line.rstrip('\r\n')
   # EOF:
   if line=='EOF':
    break
   tup=line.split(';')
   dt=datetime.datetime.fromisoformat(tup[0])
   point=int(tup[1])
   if self.firsttime is None:
    self.firsttime=dt
    self.firstpoint=point
   elif self.secondtime is None and point-self.firstpoint>10:
    self.secondtime=dt
    self.secondpoint=point
    self.sfreq=(self.secondpoint-self.firstpoint)/(self.secondtime-self.firsttime).total_seconds()
    self.preamble['Sfreq']=self.sfreq
   code=int(tup[4])
   description=None
   yield (point, code, description)
 def write_tri(self,tuples,trifilename):
  with open(trifilename,"w") as outfile:
   # 2025-07-28T10:18:34.120759;644;12;4;6;45
   for point,code,description in tuples:
    outfile.write(";".join([str(x) for x in [
     (self.firsttime+datetime.timedelta(seconds=(point-self.firstpoint)/self.sfreq)).isoformat(),
     int(point),0,0,code,10,
    ]]))
    outfile.write("\n")


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
