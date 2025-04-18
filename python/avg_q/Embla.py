# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Embla utilities.
"""

from . import Sagura
import struct
import datetime

class EmblaChannel(object):
 def __init__(self,filename):
  aStream=open(filename,"rb")
  ss=aStream.read(48)
  if not ss.startswith(b'Embla data file'):
   raise Exception('Not an Embla data file!')
  while True:
   try:
    tag=Sagura.readint32(aStream)
   except Exception:
    break
   size=Sagura.readint32(aStream)
   #print(tag,size)
   if tag==32:
    # This is the actual data, don't read it, and assume this is the last tag
    # At least in matlab implementation data type is always int16
    self.nr_of_points=size/2
    self.data_offset=aStream.tell()
    break
   buf=aStream.read(size)
   if tag==48:
    self.DateGuid=buf.rstrip(b'\0').decode('latin1')
   elif tag==64:
    self.DateRecGuid=buf.rstrip(b'\0').decode('latin1')
   elif tag==128:
    self.Version=struct.Struct("<H").unpack(buf)[0]
   elif tag==132:
    n=struct.Struct("<HBBBBBB").unpack(buf)
    # Last field is hundredths of a second
    self.time=datetime.datetime(year=n[0], month=n[1], day=n[2], hour=n[3], minute=n[4], second=n[5], microsecond=n[6]*10)
   elif tag==133:
    self.Channel=struct.Struct("<H").unpack(buf)[0]
   elif tag==134:
    self.sfreq=struct.Struct("<L").unpack(buf)[0]/1000.0
   elif tag==135:
    self.Cal=struct.Struct("<L").unpack(buf)[0]*1e-9
   elif tag==144:
    self.Channelname=buf.rstrip(b'\0').decode('latin1')
   elif tag==153:
    self.Physdim=buf.rstrip(b'\0').decode('latin1')
   elif tag==208:
    self.Subject=buf.rstrip(b'\0').decode('latin1')
  aStream.close()


from . import avg_q_file
class avg_q_Emblafile(avg_q_file):
 def __init__(self,filename):
  self.chan=EmblaChannel(filename)
  self.filename=filename
  self.fileformat="Embla"
  self.epoched=False
  self.addmethods=None
  self.getepochmethod='''
read_generic -s %(sfreq)g -C 1 -O %(data_offset)d %(avg_q_file_args)s int16
>set_channelposition -s %(Channelname)s 0 0 0
>scale_by %(scale)g
>set_comment %(comment)s
 ''' % {
   'sfreq': self.chan.sfreq,
   'data_offset': self.chan.data_offset,
   'Channelname': self.chan.Channelname,
   'scale': (self.chan.Cal*1e6 if self.chan.Physdim=='V' else self.chan.Cal),
   'comment': '%s %s' % (self.chan.Subject,self.chan.time.strftime('%m/%d/%Y,%H:%M:%S')),
   'avg_q_file_args':'%(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s'
  }
  self.trigfile=None
