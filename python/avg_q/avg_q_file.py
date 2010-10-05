# Copyright (C) 2008-2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import os

# Format, list_of_extensions
formats_and_extensions=[
 ('NeuroScan', ['.avg','.eeg','.cnt']),
 ('BrainVision', ['.vmrk','.vhdr','.eeg']),
 ('asc', ['.asc']),
 ('hdf', ['.hdf']),
 ('rec', ['.edf','.rec']),
 ('freiburg', ['.co']),
 ('neurofile', ['.eeg']),
 ('Inomed', ['.emg','.trg']),
 ('sound', ['.wav','.WAV','.au','.AU','.snd','.SND']),
 ('Coherence', ['.Eeg']),
]

class avg_q_file(object):
 def __init__(self,filename,fileformat=None):
  if not fileformat:
   filename,fileformat=self.guessformat(filename)
  self.filename=filename
  self.fileformat=fileformat
  self.addmethods=None
  self.getepochmethod=None
  self.trigfile=None
  if fileformat=='BrainVision':
   self.getepochmethod='''
read_brainvision %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s.vhdr %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='NeuroScan':
   self.getepochmethod='''
read_synamps %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='asc':
   self.getepochmethod='''
readasc %(fromepoch_arg)s %(epochs_arg)s %(filename)s.asc
'''
  elif fileformat=='hdf':
   self.getepochmethod='''
read_hdf %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s.hdf %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='rec':
   self.getepochmethod='''
read_rec %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(filename)s %(aftertrig)s
'''
  elif fileformat=='freiburg':
   self.getepochmethod='''
read_freiburg %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(filename)s %(aftertrig)s
'''
  elif fileformat=='neurofile':
   self.getepochmethod='''
read_neurofile %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='Inomed':
   self.getepochmethod='''
read_inomed %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='sound':
   self.getepochmethod='''
read_sound %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(filename)s %(aftertrig)s
'''
  elif fileformat=='Coherence':
   from . import Coherence
   coherencefile=Coherence.avg_q_Coherencefile(filename)
   self.getepochmethod=coherencefile.getepochmethod
  else:
   raise Exception("Unknown fileformat %s" % fileformat)
 def __str__(self):
  return self.filename+': '+self.fileformat
 def getepoch(self, beforetrig=0, aftertrig=0, continuous=False, fromepoch=None, epochs=None, offset=None, triglist=None, trigfile=None, trigtransfer=False):
  '''Construct a get_epoch line using the filetype-specific template and the
  parameters. We allow self.trigfile to be the default trigfile if set; this
  eases the construction e.g. of trigger transfer without having to pass
  trigfile for every getepoch call.'''
  if not trigfile and self.trigfile:
   trigfile=self.trigfile
  return self.getepochmethod % {
   'continuous_arg': '-c' if continuous else '', 
   'fromepoch_arg': '-f %d' % fromepoch if fromepoch!=None else '', 
   'epochs_arg': '-e %d' % epochs if epochs!=None else '', 
   'offset_arg': '-o %s' % offset if offset!=None else '', 
   'triglist_arg': '-t %s' % triglist if triglist!=None else '', 
   'trigfile_arg': '-R %s' % trigfile if trigfile!=None else '', 
   'trigtransfer_arg': '-T' if trigtransfer else '', 
   'filename': self.filename.replace(' ','\\ '),
   'beforetrig': str(beforetrig),
   'aftertrig': str(aftertrig)
   } + (self.addmethods if self.addmethods else '')
 def guessformat(self,filename):
  name,ext=os.path.splitext(filename)
  def findformat(ext):
   for format,extlist in formats_and_extensions:
    if ext in extlist:
     return format
   return None
  fileformat=findformat(ext)
  if not fileformat:
   fileformat=findformat(ext.lower())
  if not fileformat:
   raise Exception("Can't guess format of %s!" % filename)
  if fileformat in ['asc','hdf','BrainVision']:
   filename=name
  return filename,fileformat
