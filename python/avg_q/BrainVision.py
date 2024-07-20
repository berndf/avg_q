# Copyright (C) 2008-2010,2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
BrainVision utilities.
"""

from . import trgfile

triggercodes={
 "New Segment,": 256, # similar to NAV_STARTSTOP for synamps
 "DC Correction,": 257,
 "SyncStatus Sync On":0, # Dropped.
 "SyncStatus Sync Off":0, # Dropped.
}

class vmrkfile(trgfile.trgfile):
 starttimes=[]
 def rdr(self):
  version=self.getline().rstrip('\r\n')
  if version not in ['Brain Vision Data Exchange Marker File, Version 1.0', 'BrainVision Data Exchange Marker File Version 1.0']:
   print("BV: This does not seem to be a VMRK file!")
   return
  Section=None
  self.starttimes=[]
  while 1:
   line=self.getline()
   # EOF:
   if not isinstance(line,str):
    break
   line=line.rstrip('\r\n')
   # EOF:
   if line=='EOF':
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
      import datetime
      Type,Description,Position,size,channel,timestamp=vals
      self.starttimes.append(datetime.datetime.strptime(timestamp, '%Y%m%d%H%M%S%f'))
     else:
      continue
     point=int(Position)-1 # Positions in BrainVision start at 1
     Description=Description.replace('\\1',',')
     description=','.join((Type,Description))
     if description in triggercodes:
      code=triggercodes[description]
     else:
      if description.startswith("Stimulus,S"):
       code=int(description[10:])
      elif description.startswith("Response,S"):
       code= -int(description[10:])
     if code!=0:
      yield (point, code, description)
 def write_vmrk(self,tuples,vmrkfilename):
  import os
  eegfilepath,eegfilename=os.path.split(vmrkfilename.replace('.vmrk','.eeg'))
  with open(vmrkfilename,"w") as outfile:
   outfile.write('''Brain Vision Data Exchange Marker File, Version 1.0

[Common Infos]
Codepage=UTF-8
DataFile=%s

[Marker Infos]
; Each entry: Mk<Marker number>=<Type>,<Description>,<Position in data points>,
; <Size in data points>, <Channel number (0 = marker is related to all channels)>
; Fields are delimited by commas, some fields might be omitted (empty).
; Commas in type or description text are coded as "\\1".
''' % eegfilename)
   for i,tup in enumerate(tuples):
    if len(tup)==3:
     point,code,description=tup
    else:
     point,code=tup
     description=None
    if code==256: description='New Segment,'
    if code==257: description='DC Correction,'
    if description is None:
     description=('Stimulus,S' if code>0 else 'Response,R')+('%3d' % abs(code))
    outfile.write("Mk%d=%s,%ld,1,0\n" % (i+1,description,point+1))
