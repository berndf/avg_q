# Copyright (C) 2016 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Expyriment utilities.
"""

from . import trgfile

class ExpyrimentLog(object):
 # Basic log file reading.
 def __init__(self,logfile):
  import charset_normalizer
  self.logfile=logfile
  with open(self.logfile,"rb") as binfile:
   cn=charset_normalizer.from_fp(binfile)
  self.log=open(self.logfile,"r",encoding=cn.best().encoding)
  fileheader=next(self.log).rstrip('\r\n')
  if not fileheader.startswith('#Expyriment '):
   raise Exception("ExpyrimentLog: File doesn't start with '#Expyriment '")
  dateline=next(self.log).rstrip('\r\n')
  #print("dateline: %s" % dateline)
  if dateline.startswith('#date: '):
   import datetime
   import locale
   try:
    # Try with en_US locale
    locale.setlocale(locale.LC_TIME, 'en_US.UTF-8')
    self.timestamp=datetime.datetime.strptime(dateline[7:],"%a %b %d %Y %H:%M:%S")
   except Exception:
    # Try the current locale
    locale.setlocale(locale.LC_ALL, '')
    self.timestamp=datetime.datetime.strptime(dateline[7:],"%a %b %d %Y %H:%M:%S")
   #print(self.timestamp)
  else:
   raise Exception("ExpyrimentLog: Date line doesn't start with '#date: '")
  self.header_fields=None
  for line in self.log:
   if line.startswith('#'): continue
   fields=line.rstrip('\r\n').split(',')
   if len(fields)<=1: continue
   self.header_fields=fields
   break
 def __iter__(self):
  for line in self.log:
   if line.startswith('#'): continue
   fields=line.rstrip('\r\n').split(',')
   if len(fields)<=1: continue
   yield fields
 def __del__(self):
  self.close()
 def close(self):
  if self.log:
   self.log.close()
   self.log=None

class ExpyrimentLogfile(trgfile.trgfile):
 def __init__(self,logfile):
  self.EL=ExpyrimentLog(logfile)
  trgfile.trgfile.__init__(self,self.EL)
  self.preamble['Sfreq']=1000.0
 def rdr(self):
  for fields in self.reader:
   #print(fields)
   data=dict(zip(self.EL.header_fields,fields))
   if data['Type']=='design': continue
   point=int(data['Time'])
   if data['Type']=='Experiment':
    # StartStop
    code=128
    description="StartStop"
   else:
    description=' '.join([data[fieldname] for fieldname in ['Type','Event','Value'] if fieldname in data])
    code=1
    if 'Value' in data:
     firstnondigit=0
     while firstnondigit<len(data['Value']):
      if not data['Value'][firstnondigit].isdigit(): break
      firstnondigit+=1
     if firstnondigit>0:
      code=int(data['Value'][:firstnondigit])
   yield (point, code, description)
 def close(self):
  if self.EL:
   self.EL.close()
   self.EL=None
 def gettuples_abstime(self):
  self.gettuples()
  self.start_datetime=self.EL.timestamp
  return trgfile.trgfile.gettuples_abstime(self)

