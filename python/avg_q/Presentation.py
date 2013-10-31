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
   raise Exception("PresLog: File doesn't start with 'Scenario'")
  self.scenario=fileheader[11:]
  #print("Scenario: %s" % self.scenario)
  fileheader2=next(self.log).rstrip('\r\n')
  #print("fileheader2: %s" % fileheader2)
  if fileheader2.startswith('Logfile written - '):
   import datetime
   self.timestamp=datetime.datetime.strptime(fileheader2[18:],"%m/%d/%Y %H:%M:%S")
   #print(self.timestamp)
  else:
   self.timestamp=None
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
 def gettuples_abstime(self):
  # We are calculating backwards from the time the log was written, which is given
  # in local time, and it may happen that a DST switch occurred between start and end.
  # Most plots, simply working for a given time from the start, are totally okay if you don't
  # mind that the end times are still in the old frame, but since the local time here may
  # already be in the new frame we have to correct to achieve this "work-from-start" behavior.
  import pytz
  tuples=self.gettuples()
  sfreq=float(self.preamble.get('Sfreq'))
  last_s=pytz.datetime.timedelta(seconds=tuples[-1][0]/sfreq)
  tz_aware_end=pytz.timezone('Europe/Berlin').localize(self.PL.timestamp)
  # This computes the correct local start time considering a possible DST switch and
  # converts it to the TZ-unaware local time we really want...
  tz_unaware_start=tz_aware_end.tzinfo.normalize(tz_aware_end-last_s).replace(tzinfo=None)
  for i,t in enumerate(tuples):
   tuples[i]=tz_unaware_start+pytz.datetime.timedelta(seconds=t[0]/sfreq),t[1],t[2]
  return tuples

