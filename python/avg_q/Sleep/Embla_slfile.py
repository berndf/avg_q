# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
This provides a standard way of handling Embla event staging files.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

from . import slfile as parent_slfile

stages={
 'Wake': 0,
 'S1': 1,
 'S2': 2,
 'S3': 3,
 'S4': 4,
 'REM': 5,
 'MT': 6,
}

import os
class slfile(parent_slfile.slfile):
 def init_from_file(self,filename,minutes_per_epoch=None):
  self.filename=filename
  self.minutes_per_epoch=0.5 if not minutes_per_epoch else minutes_per_epoch
  self.first,self.ext=os.path.splitext(filename)
  self.lights_out=[] # List of lights_out events of form {'hour': hour, 'minute': minute, 'offset': offset}
  self.lights_on=[] # List of lights_on events of form {'hour': hour, 'minute': minute, 'offset': offset}
  events=open(self.filename,"r")
  self.staging=[]
  started=False
  for event in events:
   if not started:
    if event.startswith('Sleep Stage'):
     started=True
   else:
    self.staging.append(event.rstrip().split("\t"))
  events.close()
 def __iter__(self):
  return self.rdr()
 def rdr(self):
  (index,time,stage)=(0,-1, 0);
  for fullname,walltime,stagename,duration in self.staging:
   stage=stages[stagename]
   (checks,arousals,myos,eyemovements,apnea_z,apnea_za,apnea_o,apnea_oa,apnea_g,apnea_ga,hypopnea,hypopnea_a)=(0,)*12
   if time<0 and stage==2:	# Time starts from first stage 2
    time=0
   index+=1
   yield self.sltuple(time,stage,checks,arousals,myos,eyemovements,apnea_z,apnea_za,apnea_o,apnea_oa,apnea_g,apnea_ga,hypopnea,hypopnea_a)
   if time>=0:
    time+=float(duration)/60
 def close(self):
  pass
