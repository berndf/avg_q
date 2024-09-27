# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Module to run paradigm data plus arbitrary regressor time series through waver for HRF convolution,
filter and potentially orthogonalize the resulting regressors.
"""

import avg_q
from . import trgfile
from . import channelnames2channelpos
from .avg_q import escape_filename
from .avg_q import escape_channelname
from .avg_q import channel_list2arg
import subprocess

class regressors(object):
 def __init__(self,avg_q_instance,hdffile,paradigm=None,offset_ms=None):
  '''paradigm, if given, is a avg_q.paradigm instance.
     The hdffile name is only used to construct the name of a .crs file to read EPI timing data from.
  '''
  self.avg_q_instance=avg_q_instance
  self.paradigm=paradigm

  epifile=hdffile.replace('_corr','').replace('.hdf','.crs')
  epi_t=trgfile.trgfile(epifile)
  epi_t.gettuples() # Actually read the file - necessary to access the header
  #epi_sfreq=float(epi_t.preamble['Sfreq'])
  self.TR=float(epi_t.preamble['TR'])/1000.0 # in seconds
  #self.nscans=len(epi_triggers)
  self.nscans=None

  self.allonsets=[]
  self.regressor_names=[]
  if self.paradigm:
   for condition in self.paradigm.correct_conditions:
    if len(self.paradigm.trials[condition])==0:
     continue
    self.allonsets.append([trial[0][0] for trial in self.paradigm.trials[condition]])
    self.regressor_names.append(condition)
   erroronsets=[]
   for condition in self.paradigm.error_conditions:
    erroronsets.extend([trial[0][0] for trial in self.paradigm.trials[condition]])
   if len(erroronsets)>0:
    erroronsets.sort()
    self.allonsets.append(erroronsets)
    self.regressor_names.append("Errors")
   if not offset_ms:
    offset_ms=min(min(x) for x in self.allonsets)/self.paradigm.sfreq*1000.0
   self.offset_s=offset_ms/1000.0
  else:
   self.offset_s=0.0

  self.regressors=None

 def add_regressor(self,regressor):
  if not self.regressors:
   self.regressors=[]
  else:
   if len(regressor)>len(self.regressors[0]):
    regressor=regressor[:len(self.regressors[0])]
   elif len(regressor)<len(self.regressors[0]):
    regressor=regressor+['0']*(len(self.regressors[0])-len(regressor))
  self.regressors.append(regressor)

 def add_paradigm_regressors(self):
  for onsets in self.allonsets:
   if len(onsets)==0:
    continue
   exec_arg=['waver','-WAV','-peak','1.0','-dt', str(self.TR)]
   if self.nscans:
    exec_arg+=['-numout', str(self.nscans)]
   exec_arg+=['-tstim']+[str(onset/self.paradigm.sfreq-self.offset_s) for onset in onsets]
   waver=subprocess.Popen(exec_arg, shell=False, bufsize=0, stdin=None, stdout=subprocess.PIPE)
   regressor=[l.rstrip(b'\r\n').decode('utf8') for l in waver.stdout]
   waver.stdout.close()
   self.add_regressor(regressor)

 def add_regressor_file(self,regressor_name,regressor_file):
  exec_arg=['waver','-WAV','-peak','1.0','-dt', str(self.TR),'-input',regressor_file]
  waver=subprocess.Popen(exec_arg, shell=False, bufsize=0, stdin=None, stdout=subprocess.PIPE)
  regressor=[l.rstrip(b'\r\n').decode('utf8') for l in waver.stdout]
  waver.stdout.close()
  self.add_regressor(regressor)
  self.regressor_names.append(regressor_name)

 def add_regressor_avg_q_file(self,regressor_avg_q_file):
  import tempfile
  tempchannelfile=tempfile.NamedTemporaryFile()
  channelnames=self.avg_q_instance.get_description(regressor_avg_q_file,'channelnames')
  for channelname in channelnames:
   script=avg_q.Script(self.avg_q_instance)
   script.add_Epochsource(avg_q.Epochsource(regressor_avg_q_file))
   script.add_transform('remove_channel -k %s' % escape_channelname(channelname))
   script.add_transform('detrend') # An offset produces irreparable problems with waver's padding
   script.add_transform('write_generic %s string' % tempchannelfile.name)
   script.run()
   self.add_regressor_file(channelname,'%s' % tempchannelfile.name)
  tempchannelfile.close() # The temp file will be deleted at this point.

 def write_table(self,orthogonalize_order=None,with_column_names=True):
  '''Write the regressor table to stdout.
     By default (with_column_names=True), the first line contains column names and the first column contains the time in ms.
  '''
  if not self.regressors:
   self.add_paradigm_regressors()
  if orthogonalize_order is None:
   ordernames=None
  else:
   ordernames=orthogonalize_order.split(',')
   if len(ordernames)!=len(self.regressor_names) or any([x not in self.regressor_names for x in ordernames]):
    print('''Error: orthogonalize_order must mention all regressor names exactly once.
Current regressor names: %s''' % ','.join(self.regressor_names))
    return
  freqstart=1.0/128.0
  freqend=freqstart*1.1
  self.avg_q_instance.write('''
read_generic -c -e 1 -C %(nchannels)d -s %(sfreq)g stdin 0 %(nscans)d string
set_channelposition -s %(channelpositions)s
change_axes %(x_offset)g 1000 time 0 1 *
#push
fftfilter 0 0 %(freqstart)gHz %(freqend)gHz
%(reorder_orthogonalizeorder)s
orthogonalize
%(reorder_originalorder)s
#posplot
write_generic %(write_generic_flags)s stdout string
null_sink
-
''' % {
   'nchannels': len(self.regressors),
   'sfreq': 1.0/self.TR,
   'nscans': len(self.regressors[0]),
   'channelpositions': channelnames2channelpos.channelnames2channelpos(self.regressor_names),
   'x_offset': self.offset_s,
   'freqstart': freqstart,
   'freqend': freqend,
   'reorder_orthogonalizeorder': '' if ordernames is None else 'remove_channel -k %s' % channel_list2arg(ordernames),
   'reorder_originalorder':      '' if ordernames is None else 'remove_channel -k %s' % channel_list2arg(self.regressor_names),
   'write_generic_flags': '-x -N' if with_column_names else '',
  })

  for i in range(len(self.regressors[0])):
   row=[self.regressors[j][i] for j in range(len(self.regressors))]
   self.avg_q_instance.write("\t".join(row)+'\n')

  self.avg_q_instance.run()
