# Copyright (C) 2008-2011,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
This provides a derived version of avg_q_file which encapsulates access to sleep EEG files.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
import avg_q.Channeltypes
from .. import avg_q_file
from avg_q import channel_list2arg
import os

rawfile_paths=(
 '/AD/sldaten01/',
 '/AD/sldaten01/EDF/',
 '/AD/slarchiv/01/',
 '/AD/slarchiv/EDF_Axxx/',
 '/AD/slarchiv/A3000_Axxxx/',
 '/AD/slarchiv/A4000_Axxxx/',
 '/AD/slarchiv/A5000_Axxxx/',
 '/AD/slarchiv/A6000_Axxxx/',
 '/AD/slarchiv/A7000_Axxxx/',
 '/AD/slarchiv/A8000_Axxxx/',
 '/AD/slarchiv/A9000_Axxxx/',
)

# lowercase bookno bad channels database
bad_channels={
 'a8164': set(['M1']),
}

from .. import idircache

dcache=idircache.idircache(extensionstrip=('edf','rec','co'))

class sleep_file(avg_q_file):
 def __init__(self,filename):
  self.filename=filename
  self.first,self.ext=os.path.splitext(filename)
  if not self.ext:
   # No extension: Assume this is a book number
   from . import bookno
   file_bookno=bookno.file_bookno(filename)
   self.filename=dcache.find(rawfile_paths,file_bookno)
   if self.filename:
    self.first,self.ext=os.path.splitext(self.filename)
   else:
    raise Exception('Can\'t locate raw file for book number %s' % filename)
  self.addmethods=None
  self.f=avg_q_file(self.filename)
  self.fileformat=self.f.fileformat
  if self.fileformat=='freiburg':
   from . import freiburg_setup
   from .. import channelnames2channelpos
   a=avg_q.avg_q()
   nr_of_channels=a.get_description(self.f,'nr_of_channels')
   del a
   self.setup=channelnames2channelpos.channelnames2channelpos(freiburg_setup.sleep_channels(self.first,nr_of_channels))
  elif self.fileformat=='rec':
   a=avg_q.avg_q()
   comment,channelnames=a.get_description(self.f,('comment','channelnames'))
   del a
   if comment is not None and ('Somnoscreen' in comment or 'Somnodcreen' in comment):
    # Recording reference for Somnoscreen is Cz, makes no sense to analyze the channels like this
    # M1 is the more common name but there are some recordings with A1 instead
    channelnames=set(channelnames)
    # Exclude NonEEGChannels from rereferencing and also channels containing a colon, since they
    # should keep the indicated reference (eg 'C3:M2')
    exclude_channelnames=channelnames.intersection(avg_q.Channeltypes.NonEEGChannels).union([x for x in channelnames if ':' in x])
    p,fname=os.path.split(self.first)
    fname=fname.lower()
    if fname in bad_channels:
     channelnames=channelnames.difference(bad_channels[fname])
    reference=channelnames.intersection(set(['M1','M2','A1','A2']))
    self.addmethods='''
>add_zerochannel Cz 0 0 0
>set_channelposition =grid
>rereference -e %(exclude_channelnames)s %(reference)s
''' % {
     'exclude_channelnames': channel_list2arg(exclude_channelnames),
     'reference': channel_list2arg(reference),
    }
  self.trigfile=None
 def getepoch(self, beforetrig='0', aftertrig='30s', continuous=False, fromepoch=None, epochs=None, offset=None, triglist=None, trigfile=None, trigtransfer=False):
  self.f.addmethods=self.addmethods
  self.f.trigfile=self.trigfile
  # This indicates sleep-lab epoch reading
  # Note that we also want to be useable as target for avg_q.get_description(), so we cannot
  # simply assume sleeplab type reading.
  sleeplabepochs=(continuous and beforetrig=='0' and aftertrig=='30s')
  if sleeplabepochs:
   useappendices=['A', 'B', 'C']
  else:
   useappendices=[]
  retval=''
  if self.fileformat=='freiburg':
   if sleeplabepochs:
    aftertrig=3072 # 512=5s; 3072=512*6 (30s). Can't say `30s' because sampling rate is different
   retval+='''
!echo -F stderr Reading BT file %s...\\n
''' % self.first
   retval+=self.f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
   for appendix in useappendices:
    # NOTE that n3039.co is a special case; n3039 and n3039b were started
    # with 9 channels, n3039c with 12, which is correct! So n3039c becomes
    # the full n3039.edf (must be done by hand)
    contfile=self.first + appendix
    if os.path.exists(contfile + self.ext):
     # Note that according to BT, skipping the first epoch is automatic in the
     # stager, so the same happens in the second section of data.
     f=avg_q_file(contfile,fileformat='freiburg')
     f.addmethods=self.addmethods
     f.trigfile=self.trigfile
     retval+=f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
   retval+='''
set sfreq 102.4
set_channelposition -s %s
''' % self.setup
  else:
   retval+='''
!echo -F stderr Reading %s file %s...\\n
''' % (self.fileformat, self.filename)
   retval+=self.f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
   for appendix in useappendices:
    contfile=self.first + appendix + self.ext
    if os.path.exists(contfile):
     # Note that according to BT, skipping the first epoch is automatic in the
     # stager, so the same happens in the second section of data.
     f=avg_q_file(contfile,fileformat=self.fileformat)
     f.addmethods=self.addmethods
     f.trigfile=self.trigfile
     retval+=f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
  return retval
