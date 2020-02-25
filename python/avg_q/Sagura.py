# Copyright (C) 2008-2010,2012,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Sagura utilities.
"""

import configparser

class SaguraFile(object):
 def __init__(self,filename):
  if filename.upper().endswith('.INI'):
   self.ini=configparser.RawConfigParser()
   self.ini.read(filename)
   self.Patient={}
   for key,val in self.ini.items('Patient'):
    self.Patient[key]=val


# MRK format cf.
# function TViewerFrm.readMarker in EDFviewer/fViewer.pas

import struct
import datetime

def readint16(aStream):
 nbuf=aStream.read(2)
 n=struct.Struct("<H").unpack(nbuf)[0]
 return n

def readint32(aStream):
 nbuf=aStream.read(4)
 n=struct.Struct("<L").unpack(nbuf)[0]
 return n

def readint64(aStream):
 nbuf=aStream.read(8)
 n=struct.Struct("<Q").unpack(nbuf)[0]
 return n

def readdouble(aStream):
 nbuf=aStream.read(8)
 n=struct.Struct("<d").unpack(nbuf)[0]
 return n

def readDateTime(aStream):
 # in OPENLIB/datetime.pas: TDT=record Year, Month, Day, Hour, Min, Sec, msec : word;
 nbuf=aStream.read(14)
 n=struct.Struct("<HHHHHHH").unpack(nbuf)
 return datetime.datetime(year=n[0], month=n[1], day=n[2], hour=n[3], minute=n[4], second=n[5], microsecond=n[6]*1000)

# Unicode string
# short length
# name (UTF16 if MarkerFormat>8)
def readStreamString(aStream):
 n=readint16(aStream)
 nbuf=str(aStream.read(2*n),'utf16')
 return(nbuf)

MarkerMsg2Code={
 'Ableitungsbeginn': 256, # NAV_STARTSTOP
 'Ableitungsende': 256, # NAV_STARTSTOP
 'Initialisierung': 256, # NAV_STARTSTOP
}

class Marker(object):
 def __init__(self,aStream):
  try:
   self._DT=readDateTime(aStream)
   self._MarkerMsg=readStreamString(aStream)
   self._SamplesBegin=readint64(aStream)
   self._SamplesEnd=readint64(aStream)
   self._MarkerType=readint16(aStream)
   self._MarkerNum=readint16(aStream)
   self._MarkerGroup=readint16(aStream)
  except:
   self._DT=None
 def __str__(self):
  return str(self._DT)+" "+self._MarkerMsg

class Markers(object):
 def __init__(self,filename):
  mrkf=open(filename,"rb")
  self.MarkerFormat=readint32(mrkf) & 0xff
  self.MarkerStream=readStreamString(mrkf) if self.MarkerFormat>6 else None
  self.TracesFormat=readint32(mrkf) if self.MarkerFormat>3 else None
  self._baseSampleRate=readdouble(mrkf)
  self.markers=[]
  while True:
   m=Marker(mrkf)
   if m._DT is None: break
   self.markers.append(m)
  mrkf.close()
 def __str__(self):
  return str([str(m) for m in self.markers])
 def find(self,whichmsg):
  for m1 in self.markers:
   if m1._MarkerMsg==whichmsg:
    return m1
  return None
 def gettuples(self):
  #startmarker=print(markers.find('Ableitungsbeginn'))
  return [(
   "%gs" % (m._SamplesBegin/self._baseSampleRate),
   MarkerMsg2Code.get(m._MarkerMsg,1),
   m._MarkerMsg
  ) for m in self.markers]
