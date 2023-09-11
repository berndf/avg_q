# Copyright (C) 2020 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Basic reading of sleep stages from a CSV file.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

from . import slfile as parent_slfile

stages={
 'W': 0,
 '?': 0,
 'N1': 1,
 'N2': 2,
 'N3': 3,
 'N4': 4,
 'REM': 5,
 'MT': 6,
}

import os
import csv
class slfile(parent_slfile.slfile):
 def init_from_file(self,filename,minutes_per_epoch=None):
  self.filename=filename
  self.minutes_per_epoch=0.5 if not minutes_per_epoch else minutes_per_epoch
  self.first,self.ext=os.path.splitext(filename)
  self.lights_out=[] # List of lights_out events of form {'hour': hour, 'minute': minute, 'offset': offset}
  self.lights_on=[] # List of lights_on events of form {'hour': hour, 'minute': minute, 'offset': offset}
  csvfile=open(self.filename,"r")
  csvrdr=csv.reader(csvfile,dialect=csv.excel_tab())
  self.staging=[]
  for line in csvrdr:
   self.staging.append(line)
  csvfile.close()
 def __iter__(self):
  return self.rdr()
 def rdr(self):
  (index,time,stage)=(0,-1, 0)
  for stagename, in self.staging:
   stage=stages[stagename]
   (checks,arousals,myos,eyemovements,apnea_z,apnea_za,apnea_o,apnea_oa,apnea_g,apnea_ga,hypopnea,hypopnea_a)=(0,)*12
   if time<0 and stage==2:	# Time starts from first stage 2
    time=0
   index+=1
   yield self.sltuple(time,stage,checks,arousals,myos,eyemovements,apnea_z,apnea_za,apnea_o,apnea_oa,apnea_g,apnea_ga,hypopnea,hypopnea_a)
   if time>=0:
    time+=self.minutes_per_epoch
 def close(self):
  pass
