# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Presentation utilities.
"""

from . import trgfile

class PresLog(object):
 # Basic log file reading.
 def __init__(self,logfile,part='events'):
  '''part can be 'events' or 'trials' for the first or second part'''
  self.logfile=logfile
  self.log=open(self.logfile,"r")
  fileheader=next(self.log).rstrip('\r\n')
  if not(fileheader.startswith('Scenario -')):
   raise "PresLog: File doesn't start with 'Scenario'"
  self.scenario=fileheader[11:]
  #print("Scenario: %s" % self.scenario)
  table_start=['Subject','Trial'] if part=='events' else ['Event Type']
  self.header_fields=None
  for line in self.log:
   fields=line.rstrip('\r\n').split('\t')
   if len(fields)<=1: continue
   if self.header_fields is None:
    # The first table is skipped...
    if fields[0] in table_start:
     self.header_fields=fields
     self.atstart=True
     break
 def __iter__(self):
  for line in self.log:
   fields=line.rstrip('\r\n').split('\t')
   if len(fields)<=1: 
    # Only at the start skip empty line(s)
    if self.atstart: continue
    else: break
   self.atstart=False
   yield fields
 def __del__(self):
  self.close()
 def close(self):
  if self.log:
   self.log.close()
   self.log=None

class PresLogfile(trgfile.trgfile):
 def __init__(self,logfile,part='events'):
  self.PL=PresLog(logfile,part)
  trgfile.trgfile.__init__(self,self.PL)
  self.preamble['Sfreq']=10000.0
 def rdr(self):
  for fields in self.reader:
   data=dict(zip(self.PL.header_fields,fields))
   point=int(data['Time'])
   description=data['Event Type']
   try:
    code=int(data['Code'])
   except:
    code= -1
    description=' '.join([description,data['Code']])
   yield (point, code, description)
 def close(self):
  if self.PL:
   self.PL.close()
   self.PL=None

