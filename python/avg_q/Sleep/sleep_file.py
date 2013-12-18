# Copyright (C) 2008-2011,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
This provides a derived version of avg_q_file which encapsulates access to sleep EEG files.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import avg_q
from .. import avg_q_file
import os

rawfile_paths=(
 '/ncp/slserver/sldaten/Sl-Daten/',
 '/ncp/slserver/sldaten/Sl-Daten/EDF/',
 '/usermount/charly/sleepdata/01/',
 '/usermount/charly/sleepdata/EDF_Axxx/',
 '/usermount/charly/sleepdata2/A3000_Axxxx/',
 '/usermount/charly/sleepdata2/A4000_Axxxx/',
 '/usermount/charly/sleepdata2/A5000_Axxxx/',
 '/usermount/charly/sleepdata2/A6000_Axxxx/',
 '/usermount/charly/sleepdata2/A7000_Axxxx/',
 '/usermount/charly/sleepdata2/A8000_Axxxx/',
 '/usermount/charly/sleepdata2/A9000_Axxxx/',
)

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
   self.filename=None
   self.filename=dcache.find(rawfile_paths,file_bookno)
   if self.filename:
    self.first,self.ext=os.path.splitext(self.filename)
   else:
    raise Exception('Can\'t locate raw file for book number %s' % filename)
  if self.ext.lower()=='.co':
   from . import freiburg_setup
   from . import channelnames2channelpos
   self.f=avg_q_file(self.first,fileformat='freiburg')
   a=avg_q.avg_q()
   nr_of_channels=a.get_description(self.f,'nr_of_channels')
   del a
   self.setup=channelnames2channelpos.channelnames2channelpos(freiburg_setup.sleep_channels(self.first,nr_of_channels))
  elif self.ext.lower()=='.rec' or self.ext.lower()=='.edf':
   self.f=avg_q_file(self.filename,fileformat='rec')
  elif self.ext.lower()=='.eeg':
   self.f=avg_q_file(self.filename,fileformat='neurofile')
  else:
   raise Exception("Unknown sleep file %s" % filename)
  self.fileformat=self.f.fileformat
  self.addmethods=None
  self.trigfile=None
 def getepoch(self, beforetrig='0', aftertrig='30s', continuous=False, fromepoch=None, epochs=None, offset=None, triglist=None, trigfile=None, trigtransfer=False):
  self.f.addmethods=self.addmethods
  self.f.trigfile=self.trigfile
  # This indicates sleep-lab epoch reading
  # Note that we also want to be useable as target for avg_q.get_description(), so we cannot
  # simply assume sleeplab type reading.
  sleeplabepochs=(continuous and beforetrig=='0' and aftertrig=='30s')
  if sleeplabepochs:
   # Ensure that reading starts with the second 30s epoch
   if fromepoch:
    fromepoch+=1
   else:
    fromepoch=2
   useappendices=['A', 'B', 'C']
  else:
   useappendices=[]
  retval=''
  if self.ext.lower()=='.co':
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
  elif self.ext.lower()=='.rec' or self.ext.lower()=='.edf':
   retval+='''
!echo -F stderr Reading EDF file %s...\\n
''' % self.filename
   retval+=self.f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
   for appendix in useappendices:
    contfile=self.first + appendix + self.ext
    if os.path.exists(contfile):
     # Note that according to BT, skipping the first epoch is automatic in the
     # stager, so the same happens in the second section of data.
     f=avg_q_file(contfile,fileformat='rec')
     f.addmethods=self.addmethods
     f.trigfile=self.trigfile
     retval+=f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
  elif self.ext.lower()=='.eeg':
   retval+='''
!echo -F stderr Reading Nihon Kohden file %s...\\n
''' % self.filename
   retval+=self.f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
   for appendix in useappendices:
    contfile=self.first + appendix + self.ext
    if os.path.exists(contfile):
     # Note that according to BT, skipping the first epoch is automatic in the
     # stager, so the same happens in the second section of data.
     f=avg_q_file(contfile,fileformat='neurofile')
     f.addmethods=self.addmethods
     f.trigfile=self.trigfile
     retval+=f.getepoch(beforetrig, aftertrig, continuous, fromepoch, epochs, offset, triglist, trigfile, trigtransfer)
  return retval
