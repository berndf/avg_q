# Copyright (C) 2009-2011 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Average class working together with paradigm.py-derived trial lists.
In particular, we abstract here the idea of averaging separately around
several different events within the epochs (eg Stimulus, Response) while
referring to the same baseline (before the first event) and shifting the
averages around the second and following events to exactly overlay the
first average.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>, Tanja Schmitt <schmitt.tanja@web.de>"

from . import Paradigm_Script

class Average(object):
 def __init__(self,avg_q_instance,infile,paradigm_instance,conditions=None,event_indices=0,event0index=0):
  '''
  event_indices can either be a scalar or list, or a dict of lists where the keys correspond
  to the conditions.
  '''
  self.avg_q_instance=avg_q_instance
  self.infile=infile
  self.paradigm_instance=paradigm_instance
  self.sfreq=self.paradigm_instance.sfreq
  if conditions:
   self.conditions=conditions
  else:
   self.conditions=self.paradigm_instance.conditions
  self.event_indices=event_indices
  self.event0index=event0index

 def get_event_indices(self,condition):
  t=type(self.event_indices)
  if t==int:
   return [self.event_indices]
  elif t==list:
   return self.event_indices
  else:
   return self.event_indices.get(condition, [])

 def get_event0index(self,condition):
  t=type(self.event0index)
  if t==int:
   return self.event0index
  else:
   return self.event0index.get(condition, 0)

 def average_trials(self,beforetrig='0.2s',aftertrig='1s',preprocess='baseline_subtract',pre_average='',postprocess='posplot',average_options='-t'):
  '''
  beforetrig and aftertrig define the length of the final epoch. This is used as-is for the first event_index, while the
  averages around other events are read as needed to be finally shifted on top of the first average.
  preprocess is added as a branch for every epoch, therefore methods that don't need to run before trimming
  should better go in pre_average.
  postprocess is used to actually save the averages. Note that a '-a' append flag will be necessary in a PUT_EPOCH_METHOD
  since every condition leads to a separate average sub-script with postprocess at the end.
  '''

  if '-t' in average_options and '-u' not in average_options:
   calclog='''
calc -i 2 log10
calc -i 2 neg
'''
  else:
   calclog=''
  for condition in self.conditions:
   event_indices=self.get_event_indices(condition)
   if len(event_indices)==0 or len(self.paradigm_instance.trials[condition])==0:
    # Nothing to average...
    continue
   event0index=self.get_event0index(condition)
   for eventindex in event_indices:
    script=Paradigm_Script.Paradigm_Script(self.avg_q_instance)
    shiftwidth_points=script.add_Paradigm_Epochsource(self.infile,self.paradigm_instance,condition,event0index,eventindex,beforetrig,aftertrig,preprocess)
    # Now average the epochs
    script.add_transform(pre_average)
    script.set_collect('average %s' % average_options)
    script.add_postprocess('''
%(calclog)s
set_comment Condition %(condition)s event %(eventindex)d shift %(shiftwidth)gms
%(postprocess)s
''' % {
     'calclog': calclog,
     'condition':condition,
     'eventindex':eventindex,
     'shiftwidth': shiftwidth_points*1000.0/self.sfreq,
     'postprocess': postprocess
    })
    script.run()


"""
Next comes the GrandAverage class, handy for doing grand averages.
Usage: Init giving a list of the input files, then usually call
set_outfile() to specify that all averages should be written to that
asc file, and call single_average(condition,eventindex) repeatedly.
For eventindex!=event0index (presently we set event0index=0 by default),
the curves are automatically corrected so that their individual shift
will coincide with standardized_RT_ms.
"""
def get_shift_from_description(description):
 shift_str=description.split("shift ")[1]
 return float(shift_str.split("ms")[0])


from . import avg_q_file
import os
class GrandAverage(object):
 event0index=0
 standardized_RT_ms=600.0 # Fix the reaction here
 def __init__(self,avg_q_instance,infiles):
  self.avg_q_instance=avg_q_instance
  self.outfile=None
  self.alltuples={}
  self.infiles=[]
  self.sfreq=None
  self.epochlength_points=None
  self.beforetrig_points=None
  self.session_shift_points=None
  for avgfile in infiles:
   f=avg_q_file(avgfile)
   if not os.path.exists(f.filename):
    print("GrandAverage: %s doesn't exist, omitting this file!" % avgfile)
    continue
   if not self.sfreq:
    self.sfreq,self.epochlength_points,self.beforetrig_points=self.avg_q_instance.get_description(f,('sfreq','nr_of_points','beforetrig'))
   self.alltuples[avgfile]=self.avg_q_instance.get_filetriggers(f).gettuples()
   self.infiles.append(avgfile)
 def get_one_average(self,avgfile,conditions,eventindex=None):
  '''Get one single average for these conditions and eventindex, properly aligned and ready for averaging.
     This allows to process the input files one by one if so desired.
     Otherwise use get_averages below to read all inputs for a given condition and eventindex.
  '''
  if type(conditions) is not list:
   # Allow passing a single condition
   conditions=[conditions]
  condstrs=conditions if eventindex is None else [cond+(' event %d' % eventindex) for cond in conditions]
  f=avg_q_file(avgfile)
  n_averages=0
  for position,code,description in\
    [(position,code,description) for position,code,description in self.alltuples[avgfile]
     if any(condstr in description for condstr in condstrs)]:
   #print position,code,description
   self.avg_q_instance.getepoch(f,fromepoch=position+1,epochs=1)
   n_averages+=1
   if eventindex is not None and eventindex!=self.event0index:
    shift=get_shift_from_description(description)
    shift_points=int(round((shift-self.standardized_RT_ms)/1000.0*self.sfreq))
    if isinstance(self.session_shift_points,list):
     self.session_shift_points.append(shift_points)
    self.avg_q_instance.write('''
>trim %(shift_points)d %(epochlength_points)d
>set beforetrig %(beforetrig_points)d
>set xdata 1
''' % {
     'shift_points': shift_points,
     'epochlength_points': self.epochlength_points,
     'beforetrig_points': self.beforetrig_points,
    })
  return n_averages
 def get_averages(self,conditions,eventindex=None):
  '''Get all single averages, properly aligned and ready for averaging.
     The caller has to issue a collect method etc. if the returned
     number of averages is not zero.
  '''
  self.session_shift_points=[] # Updated by get_one_average
  n_averages=0 # Bail out if no averages are available at all
  for avgfile in self.infiles:
   n_averages+=self.get_one_average(avgfile,conditions,eventindex)
  if n_averages>0 and eventindex is not None and eventindex!=self.event0index:
   print("%s event %d Mean shift points: %g" % (str(conditions),eventindex,float(sum(self.session_shift_points))/len(self.session_shift_points)))
  return n_averages
 def set_outfile(self,outfile,append=False):
  '''Arranges for single_average to write the result to an ASC file.
     Note that if this isn't used, you'll have to add the rest of a post-processing queue
     plus the script separator and run() yourself.'''
  self.outfile=outfile
  self.append=append
 def posplot(self,condition,eventindex=None):
  '''Debug: show all epochs as they would be averaged by single_average.'''
  if self.get_averages(condition,eventindex)==0:
   return
  self.avg_q_instance.write('''
extract_item 0
append -l
Post:
posplot
-
''')
  self.avg_q_instance.run()
 def single_average(self,conditions,eventindex=None,average_options='-W -t'):
  if self.get_averages(conditions,eventindex)==0:
   return
  if '-t' in average_options and '-u' not in average_options:
   calclog='''
calc -i 2 log10
calc -i 2 neg
'''
  else:
   calclog=''
  self.avg_q_instance.write('''
extract_item 0
average %(average_options)s
Post:
%(calclog)s
set_comment %(comment)s
''' % {
   'average_options': average_options,
   'calclog': calclog,
   'comment': str(conditions)+((' event %d' % eventindex)+(' shift %gms' % (self.standardized_RT_ms if eventindex is not None and eventindex!=self.event0index else 0)) if eventindex is not None else ''),
  })
  if self.outfile:
   self.avg_q_instance.write('''
writeasc %(append)s -b %(outfile)s
-
''' % {
    'append': '-a' if self.append else '',
    'outfile': self.outfile,
   })
   self.avg_q_instance.run()
   self.append=True
