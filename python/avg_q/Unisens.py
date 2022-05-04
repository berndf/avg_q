# vim: set fileencoding=utf-8 :
# Copyright (C) 2018 Bernd Feige
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

def get_channels(member):
 channels=[]
 for item in member:
  if item.tag.endswith('channel'):
   name=item.items()[0][1]
   channels.append('val' if name=="" else name)
 return channels

class UnisensFile(object):
 def __init__(self,topdir):
  if not os.path.isdir(topdir):
   # If a file within the directory is given, remove the file part
   topdir,f=os.path.split(topdir)
  self.topdir=topdir

  tree = ET.parse(os.path.join(topdir,'unisens.xml'))
  root = tree.getroot()
  timestampStart=root.get('timestampStart')
  self.timestampStart=datetime.datetime.strptime(timestampStart,'%Y-%m-%dT%H:%M:%S.%f' if '.' in timestampStart else '%Y-%m-%dT%H:%M:%S')
  self.measurementId=root.get('measurementId')
  self.comment=root.get('comment')
  self.members=[]
  add_argument=None
  for member in root:
   # Attribute "id" is actually a file name with extension
   id_value=member.get('id')
   if id_value:
    # Fix in case id_value contains a path
    p,filename=os.path.split(id_value)
    ID,ext=os.path.splitext(filename)
    filename=os.path.join(topdir,filename)
   else:
    filename,ID,ext=None,None,None
   getepochmethod=None
   if member.tag.endswith('valuesEntry') or member.tag.endswith('signalEntry'):
    # For valuesEntry, CSV time stamps are in ticks of the clock as given in the sampleRate attribute (ie 500Hz)
    if member.get('sampleRate')=='':
     # Just avoid a conversion error here
     sampleRate=None
    else:
     sampleRate=float(member.get('sampleRate'))
    baseline=float(member.get('baseline',0))
    lsbValue=float(member.get('lsbValue',1))
    dataType=member.get('dataType',1)
    channels=get_channels(member)
    nr_of_channels=len(channels)
    valuesEntry=member.tag.endswith('valuesEntry')
    read_generic_format=None
    if ext=='.csv':
     read_generic_format='string'
     for item in member:
      if item.tag.endswith('csvFileFormat'):
       decimalSeparator=item.get('decimalSeparator')
       if decimalSeparator is not None:
        add_argument='-D '+decimalSeparator
    elif dataType=='int16':
     read_generic_format='int16'
    elif dataType=='int32':
     read_generic_format='int32'
    elif dataType=='float':
     read_generic_format='float32'
    elif dataType=='double':
     read_generic_format='float64'
    if valuesEntry:
     getepochstart='read_generic -x TIME[s] -C %d -s %g ' % (nr_of_channels,sampleRate)
    else:
     getepochstart='read_generic -C %d -s %g ' % (nr_of_channels,sampleRate)
    if add_argument is not None:
     getepochstart+=add_argument+' '
    comment_parts=[self.timestampStart.strftime("%Y-%m-%d %H:%M:%S"),self.measurementId]
    if self.comment:
     # Single '%' signs in the comment must be replaced since the result is
     # used as input for expansion
     comment_parts.append(self.comment.replace('%','%%'))
    getepochmethod=getepochstart+'%(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s '+read_generic_format+'''
>set_channelposition -s %(channelpositions)s
>add %(negbaseline)g
>scale_by %(lsbValue)g
>set_comment %(comment)s
''' % {
     'channelpositions': channelnames2channelpos.channelnames2channelpos(channels),
     'negbaseline': -baseline,
     'lsbValue': lsbValue,
     'comment': ' '.join(comment_parts),
    }
    if valuesEntry:
     getepochmethod+='''
>change_axes 0 %(invsfreq)g * 0 1 *
''' % {
      'invsfreq': 1.0/sampleRate,
     }
   elif member.tag.endswith('eventEntry'):
    sampleRate=float(member.get('sampleRate'))
   self.members.append((filename,getepochmethod))

from . import avg_q_file
class avg_q_Unisensfile(avg_q_file):
 def __init__(self,filename):
  self.filename=filename
  self.fileformat="Unisens"
  self.addmethods=None
  self.getepochmethod=None
  self.trigfile=None
  self.UnisensFile=UnisensFile(filename)
  methods=[x[1] for x in self.UnisensFile.members if x[0]==filename]
  self.getepochmethod=methods[0] if len(methods)>0 else None
