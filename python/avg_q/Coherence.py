# vim: set fileencoding=utf-8 :
# Copyright (C) 2008-2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Utilities for the "Coherence" file format.
"""

from . import trgfile
import struct
import sys

headerfields=[
 (0x2B, 'ScanID'),
 (0x13A, 'LastName'),
 (0x16C, 'FirstName'),
 (0x18A, 'DateOfBirth'),
 (0x1A9, 'LabName'),
 (0x1D0, 'SetupName'),
 (0x2EC, 'DerivationName'),
]

unknown_marker_code= 9
# This is used to detect corrupted event lists:
max_stringlength=150
import string
accepted_characters=string.printable+"öäüÖÄÜß"
markercodes={
 "DAY": 0,
 "HV": 0,
 "<IMPEDANZ-WERTE>": 0,
 "PAUSE": 256, # NAV_STARTSTOP
 "Auswahl": -2,
 "Auswahl Ende": -3,
 "Augen auf": 1,
 "Augen zu": 2,
 "Klopfen": 3,
 "Bewegung": 4,
 "schlucken": 5,
 "husten": 6,
 "HV START": 10,
 "HV END": 11,
 "post hv": 12,
}

def readstring(filehandle,start=b""):
 # Could be that "start" already contains a string delimiter...
 zeroindex=start.find(b"\0")
 if zeroindex>=0:
  return start[:zeroindex].decode('latin1')
 string=start.decode('latin1')
 # Garbage detection
 if any([char not in accepted_characters for char in string]):
  return None
 while True:
  char=filehandle.read(1).decode('latin1')
  if ord(char)==0: break
  # Garbage detection
  if char not in accepted_characters or len(string)>=max_stringlength:
   return None
  if sys.version<"3":
   string+=char.encode('utf-8')
  else:
   string+=char
 return string

data_offset=5359
bytespermarker=268

class CoherenceFile(object):
 def __init__(self,filename,sfreq=256):
  self.filehandle=open(filename,"rb")
  self.sfreq=sfreq
 def getHeader(self):
  header=[]
  for pos,name in headerfields:
   self.filehandle.seek(pos,0)
   value=readstring(self.filehandle)
   header.append((name,value))
  return header
 def getEvents(self):
  events=[]
  def push_event(pos,marker):
   code=None
   if pos==4294967295:
    # Max 32-bit int value, occurs as first item starting 2012, drop it
    return
   if marker in markercodes:
    code=markercodes[marker]
   else:
    for m in markercodes:
     if marker.startswith(m):
      code=markercodes[m]
   if code==None:
    code=unknown_marker_code
   if code!=0:
    if self.sfreq:
     outpos="%gs" % (float(pos)/self.sfreq)
    events.insert(0,(outpos,code,marker))
  self.filehandle.seek(0,2)
  filepos=self.filehandle.tell()
  while True:
   filepos-=bytespermarker
   self.filehandle.seek(filepos,0)
   pos=self.filehandle.read(4)
   duration=self.filehandle.read(4)
   pos,=struct.unpack("I",pos)
   # Duration of selected event
   duration,=struct.unpack("I",duration)
   # The first entry regularly is pos=2, marker="TEST"
   if pos<=2: break
   pause_field=self.filehandle.read(4)
   if pause_field[2]==0:
    # PAUSE
    pause_length,=struct.unpack("I", pause_field)
    marker=readstring(self.filehandle)
    if len(marker)==0:
     hours=int(pause_length/3600.0)
     minutes=int((pause_length-hours*3600)/60.0)
     seconds=(pause_length-hours*3600-minutes*60.0)
     marker="DAY TIME %02d:%02d:%02d" % (hours, minutes, seconds)
    else:
     if duration>0:
      push_event(pos+duration,marker+' Ende')
     marker+=" = %ds" % pause_length
   else:
    marker=readstring(self.filehandle,pause_field)
    # readstring's garbage detection kicked in
    if marker is None: break
    # At any rate, avoid CRs in markers which would corrupt the marker file
    marker=marker.translate(str.maketrans('','','\r\n'))
   push_event(pos,marker)
  return events
 def close(self):
  self.filehandle.close()

class CoherenceTriggers(trgfile.trgfile):
 def __init__(self,source=None,sfreq=256):
  if not isinstance(source,str):
   raise Exception("CoherenceTriggers: Can only work with file names!")
  trgfile.trgfile.__init__(self,None)
  coherencefile=CoherenceFile(source,sfreq)
  for name,value in coherencefile.getHeader():
   self.preamble[name]=value
  self.tuples=coherencefile.getEvents()
  coherencefile.close()
 def close(self):
  pass
 def reader(self):
  pass
 def getline(self):
  pass
 def rdr(self):
  for event in self.tuples:
   yield event
 def gettuples(self):
  return self.tuples

from . import avg_q_file
# Files are stored uncompressed starting with 020705AA.Eeg
first_uncompressed='020705AA.Eeg'
class avg_q_Coherencefile(avg_q_file):
 def __init__(self,filename,nr_of_channels=24,sfreq=256,compressed=None):
  self.filename=filename
  self.nr_of_channels=nr_of_channels
  self.sfreq=sfreq
  if compressed==None:
   import os
   eegpath,eegfilename=os.path.split(filename)
   self.compressed=eegfilename<first_uncompressed
  else:
   self.compressed=compressed
  self.fileformat="Coherence"
  self.addmethods=None
  getepochstart='read_generic -O %d -C %d -s %d ' % (data_offset,self.nr_of_channels,self.sfreq)
  if self.compressed:
   self.getepochmethod=getepochstart+'''%(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s int8
>integrate
'''
  else:
   self.getepochmethod=getepochstart+'''%(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s int16
'''

