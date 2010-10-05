# Copyright (C) 2008-2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
BrainVision utilities.
"""

from . import trgfile

triggercodes={
 "New Segment ": 256, # similar to NAV_STARTSTOP for synamps
 "SyncStatus Sync On":0, # Dropped.
 "SyncStatus Sync Off":0, # Dropped.
}

class vmrkfile(trgfile.trgfile):
 def rdr(self):
  version=self.getline()
  if version!="Brain Vision Data Exchange Marker File, Version 1.0":
   print("BV: This does not seem to be a VMRK file!")
   return
  Section=None
  while 1:
   line=self.getline()
   # EOF:
   if not isinstance(line,str):
    break
   if len(line)==0 or line[0]==';':
    pass
   elif line[0]=='[':
    Section=line[1:-1]
    #print Section
   else:
    if Section=='Marker Infos':
     mk,equal,rest=line.partition('=')
     vals=rest.split(',')
     if len(vals)==5:
      Type,Description,Position,size,channel=vals
     elif len(vals)==6:
      Type,Description,Position,size,channel,timestamp=vals
     else:
      continue
     point=int(Position)-1 # Positions in BrainVision start at 1
     Description=Description.replace('\\1',',')
     description=' '.join((Type,Description))
     if description in triggercodes:
      code=triggercodes[description]
     else:
      if description.startswith("Stimulus S"):
       code=int(description[10:])
      elif description.startswith("Response S"):
       code= -int(description[10:])
     if code!=0:
      yield (point, code, description)
