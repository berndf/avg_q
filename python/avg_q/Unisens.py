# vim: set fileencoding=utf-8 :
# Copyright (C) 2015 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
The "Unisens" data format, FZI Karlsruhe http://www.unisens.org/
It consists of a directory with the XML file "unisens.xml" in it,
containing the metadata for the single .bin or .csv files residing in the
same directory.
"""

import os
import datetime
import xml.etree.ElementTree as ET
from . import channelnames2channelpos

def get_channels(entry):
 item=1
 channels=[]
 while item<len(entry):
  if entry[item].tag.endswith('channel'):
   name=entry[item].items()[0][1]
   channels.append('val' if name=="" else name)
  item+=1
 return channels

class UnisensFile(object):
 def __init__(self,topdir):
  if not os.path.isdir(topdir):
   # If a file within the directory is given, remove the file part
   topdir,f=os.path.split(topdir)
  self.topdir=topdir

  tree = ET.parse(os.path.join(topdir,'unisens.xml'))
  root = tree.getroot()
  self.timestampStart=datetime.datetime.strptime(root.get('timestampStart'),"%Y-%m-%dT%H:%M:%S.%f")
  self.measurementId=root.get('measurementId')
  self.comment=root.get('comment')
  self.members=[]
  for member in root:
   # Attribute "id" is actually a file name with extension
   idfile=member.get('id')
   if idfile:
    id,ext=os.path.splitext(idfile)
    infile=os.path.join(topdir,idfile)
   else:
    id,ext=None,None
   getepochmethod=None
   if member.tag.endswith('valuesEntry'):
    # CSV time stamps are in ticks of the clock as given in the sampleRate attribute (ie 500Hz)
    sampleRate=float(member.get('sampleRate'))
    channels=get_channels(member)
   elif member.tag.endswith('signalEntry'):
    sampleRate=float(member.get('sampleRate'))
    baseline=float(member.get('baseline',0))
    lsbValue=float(member.get('lsbValue',1))
    dataType=member.get('dataType',1)
    channels=get_channels(member)
    nr_of_channels=len(channels)
    read_generic_format=None
    if ext=='.csv':
     read_generic_format='string'
    elif dataType=='int16':
     read_generic_format='int16'
    elif dataType=='int32':
     read_generic_format='int32'
    elif dataType=='float':
     read_generic_format='float32'
    elif dataType=='double':
     read_generic_format='float64'
    getepochstart='read_generic -C %d -s %g ' % (nr_of_channels,sampleRate)
    getepochmethod=getepochstart+'%(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s '+read_generic_format+'''
>set_channelposition -s %(channelpositions)s
>add %(negbaseline)g
>scale_by %(lsbValue)g
>set_comment %(comment)s
''' % {
     'channelpositions': channelnames2channelpos.channelnames2channelpos(channels),
     'negbaseline': -baseline,
     'lsbValue': lsbValue,
     'comment': ' '.join([self.timestampStart.strftime("%Y-%m-%d %H:%M:%S"),self.measurementId,self.comment]),
    }
   elif member.tag.endswith('eventEntry'):
    sampleRate=float(member.get('sampleRate'))
   self.members.append((idfile,getepochmethod))

from . import avg_q_file
class avg_q_Unisensfile(avg_q_file):
 def __init__(self,filename):
  self.filename=filename
  self.fileformat="Unisens"
  self.addmethods=None
  self.getepochmethod=None
  self.trigfile=None
  self.UnisensFile=UnisensFile(filename)
  p,fname=os.path.split(filename)
  methods=[x[1] for x in self.UnisensFile.members if x[0]==fname]
  self.getepochmethod=methods[0] if len(methods)>0 else None
