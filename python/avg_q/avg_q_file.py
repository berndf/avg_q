# Copyright (C) 2008-2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import os
from .avg_q import escape_filename

# Format, list_of_extensions
# Extensions are first matched case sensitive, then lowercased.
formats_and_extensions=[
 ('NeuroScan', ['.avg','.eeg','.cnt']),
 ('BrainVision', ['.vmrk','.vhdr','.eeg']),
 ('asc', ['.asc']),
 ('hdf', ['.hdf']),
 ('rec', ['.edf','.rec', '.bdf']),
 ('freiburg', ['.co']),
 ('neurofile', ['.eeg']),
 ('Inomed', ['.emg','.trg']),
 ('sound', ['.wav','.au','.snd','.aiff','.mp3','.ogg']),
 ('Coherence', ['.Eeg']),
 ('Konstanz', ['.sum', '.raw']),
 ('Vitaport', ['.vpd', '.raw']),
 ('Tucker', ['.raw']),
 ('Embla', ['.ebm']),
 ('Unisens', ['.bin','.csv']),
 ('CFS', ['.cfs']),
]

class avg_q_file(object):
 def __init__(self,filename=None,fileformat=None):
  if not fileformat:
   filename,fileformat=self.guessformat(filename)
  self.filename=filename
  self.fileformat=fileformat
  self.addmethods=None
  self.getepochmethod=None
  self.trigfile=None
  if fileformat=='BrainVision':
   self.getepochmethod='''
read_brainvision %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='NeuroScan':
   self.getepochmethod='''
read_synamps %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='asc':
   self.getepochmethod='''
readasc %(fromepoch_arg)s %(epochs_arg)s %(filename)s
'''
  elif fileformat=='hdf':
   self.getepochmethod='''
read_hdf %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='rec':
   self.getepochmethod='''
read_rec %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='freiburg':
   if os.path.exists(self.filename):
    # Remove trailing .co - see documentation of read_freiburg, which needs
    # only the name without extension to read an SL .co + .coa combination
    if self.filename.lower().endswith('.co'):
     self.filename=self.filename[:-3]
   self.getepochmethod='''
read_freiburg %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(filename)s %(aftertrig)s
'''
  elif fileformat=='Vitaport':
   self.getepochmethod='''
read_vitaport %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
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
read_sound %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(filename)s %(aftertrig)s
'''
  elif fileformat=='Coherence':
   from . import Coherence
   coherencefile=Coherence.avg_q_Coherencefile(filename)
   self.getepochmethod=coherencefile.getepochmethod
  elif fileformat=='Embla':
   from . import Embla
   emblafile=Embla.avg_q_Emblafile(filename)
   self.getepochmethod=emblafile.getepochmethod
  elif fileformat=='Unisens':
   from . import Unisens
   Unisensfile=Unisens.avg_q_Unisensfile(filename)
   self.getepochmethod=Unisensfile.getepochmethod
  elif fileformat=='Konstanz':
   self.getepochmethod='''
read_kn %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(filename)s
'''
  elif fileformat=='Tucker':
   self.getepochmethod='''
read_tucker %(continuous_arg)s %(fromepoch_arg)s %(epochs_arg)s %(offset_arg)s %(triglist_arg)s %(trigfile_arg)s %(trigtransfer_arg)s %(filename)s %(beforetrig)s %(aftertrig)s
'''
  elif fileformat=='CFS':
   self.getepochmethod='''
read_cfs %(fromepoch_arg)s %(epochs_arg)s %(filename)s
'''
  elif fileformat in ['dip_simulate', 'null_source']:
   # Handled specially
   self.getepochmethod=None
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
  if self.fileformat=='dip_simulate':
   return '''
dip_simulate 100 %(epochs_arg)s %(beforetrig)s %(aftertrig)s eg_source
''' % {
    'epochs_arg': str(epochs),
    'beforetrig': str(beforetrig),
    'aftertrig': str(aftertrig)
    }
  elif self.fileformat=='null_source':
   return '''
null_source 100 %(epochs_arg)s 32 %(beforetrig)s %(aftertrig)s
''' % {
    'epochs_arg': str(epochs),
    'beforetrig': str(beforetrig),
    'aftertrig': str(aftertrig)
    }
  return self.getepochmethod % {
   'continuous_arg': '-c' if continuous else '',
   'fromepoch_arg': '-f %d' % fromepoch if fromepoch is not None else '',
   'epochs_arg': '-e %d' % epochs if epochs is not None else '',
   'offset_arg': '-o %s' % offset if offset is not None else '',
   'triglist_arg': '-t %s' % triglist if triglist is not None else '',
   'trigfile_arg': '-R %s' % escape_filename(trigfile) if trigfile is not None else '',
   'trigtransfer_arg': '-T' if trigtransfer else '',
   'filename': escape_filename(self.filename),
   'beforetrig': str(beforetrig),
   'aftertrig': str(aftertrig)
   } + (self.addmethods if self.addmethods else '')
 def guessformat(self,filename):
  name,ext=os.path.splitext(filename)
  def findformat(ext):
   for format,extlist in formats_and_extensions:
    if ext in extlist:
     return format
   for format,extlist in formats_and_extensions:
    if ext.lower() in extlist:
     return format
   return None
  fileformat=findformat(ext)
  if not fileformat:
   fileformat=findformat(ext.lower())
  if not fileformat:
   raise Exception("Can't guess format of %s!" % filename)
  return filename,fileformat
