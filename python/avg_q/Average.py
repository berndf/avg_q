# Copyright (C) 2009,2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Average class working together with paradigm.py-derived epochtriggers lists.
In particular, we abstract here the idea of averaging separately around
several different events within the epochs (eg Stimulus, Response) while
referring to the same baseline (before the first event) and shifting the
averages around the second and following events to exactly overlay the 
first average.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>, Tanja Schmitt <schmitt.tanja@web.de>"

class Average(object):
 def __init__(self,avg_q_object,infile,paradigm_instance,conditions=None,event_indices=0):
  '''
  event_indices can either be a scalar or list, or a dict of lists where the keys correspond
  to the conditions.
  '''
  self.avg_q_object=avg_q_object
  self.infile=infile
  self.paradigm_instance=paradigm_instance
  self.sfreq=self.paradigm_instance.sfreq
  if conditions:
   self.conditions=conditions
  else:
   self.conditions=self.paradigm_instance.conditions
  self.event_indices=event_indices

 def get_event_indices(self,condition):
  t=type(self.event_indices)
  if t==int:
   return [self.event_indices]
  elif t==list:
   return self.event_indices
  else:
   return self.event_indices.get(condition, [])

 def calc_shiftwidth(self, list_of_latencies):
  '''
  this is needed in average_trials(),
  calculate a shiftwidth around which the response avgs will be shifted in order
  to plot equivalent sections of the component over each other. 
  '''
  import math
  return math.exp(sum([math.log(x) for x in list_of_latencies])/len(list_of_latencies))

 def read_epochs_around_condition(self,condition,event0index,eventindex,beforetrig='0.2s',aftertrig='1s',preprocess='baseline_subtract'):
  '''
  This encapsulates reading and shifting all single epochs of a given condition and eventindex, ready for averaging.
  event0index is the 'reference' event around which the epochs are actually read, baseline subtracted
  and then recentered around event eventindex.
  If event0index==eventindex, obviously, epochs are read normally without shift.
  shiftwidth_points is returned, giving amount by which the re-centered event was shifted.
  '''
  beforetrig_points=self.avg_q_object.time_to_points(beforetrig,self.sfreq)
  aftertrig_points =self.avg_q_object.time_to_points(aftertrig,self.sfreq)

  if eventindex==event0index:
   point_list=[epochtriggers[eventindex] for epochtriggers in self.paradigm_instance.trials[condition]] 
   self.avg_q_object.get_epoch_around_add(self.infile, point_list, beforetrig, aftertrig, branch=preprocess)
   shiftwidth_points=0
  else:
   latency_points=[epochtriggers[eventindex][0]-epochtriggers[event0index][0] for epochtriggers in self.paradigm_instance.trials[condition]]
   shiftwidth_points=round(self.calc_shiftwidth(latency_points))
   # Warn if our desired shift point is not within the averaged epoch
   if aftertrig_points<=shiftwidth_points:
    print("Warning: shiftwidth_points=%d but aftertrig_points=%d!" % (shiftwidth_points,aftertrig_points))
   # This is the total target epoch length:
   trimlength=beforetrig_points+aftertrig_points
   win_before_trig_points=shiftwidth_points+beforetrig_points
   win_after_trig_points=trimlength-win_before_trig_points
   for epochtriggers in self.paradigm_instance.trials[condition]:
    go_point=epochtriggers[event0index][0]
    secondpoint=epochtriggers[eventindex][0]
    pointdiff=secondpoint-go_point
     
    winstart_points = beforetrig_points+pointdiff-win_before_trig_points
    winend_points   = beforetrig_points+pointdiff+win_after_trig_points
    # Numerically, if the response is sufficiently earlier than the shift width, we get a negative epoch
    # length for reading. Don't do this.
    if winend_points<=0:
     print("Warning: Not reading epoch completely outside of target window!")
     continue
    
    branch_text = preprocess+'''
trim %(winstart_points)d %(trimlength)d
set beforetrig %(beforetrig)s
set xdata 1
''' % {'winstart_points': winstart_points,'trimlength':trimlength,'beforetrig':beforetrig}
    self.avg_q_object.get_epoch_around_add(self.infile,go_point,beforetrig_points,winend_points-beforetrig_points,branch=branch_text)
  return shiftwidth_points

 def average_trials(self,beforetrig='0.2s',aftertrig='1s',preprocess='baseline_subtract',pre_average='',postprocess='posplot',test_options='-t'):
  '''
  beforetrig and aftertrig define the length of the final epoch. This is used as-is for the first event_index, while the
  averages around other events are read as needed to be finally shifted on top of the first average.
  preprocess is added as a branch for every epoch, therefore methods that don't need to run before trimming
  should better go in pre_average.
  postprocess is used to actually save the averages. Note that a '-a' append flag will be necessary in a PUT_EPOCH_METHOD
  since every condition leads to a separate average sub-script with postprocess at the end.
  '''
 
  if '-t' in test_options and not '-u' in test_options:
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
   event0index=event_indices[0]
   for eventindex in event_indices:
    shiftwidth_points=self.read_epochs_around_condition(condition,event0index,eventindex,beforetrig,aftertrig,preprocess)
    # Now average the epochs
    self.avg_q_object.get_epoch_around_finish(rest_of_script=pre_average+'''
average %(test_options)s
Post:
%(calclog)s
set_comment Condition %(condition)s event %(eventindex)d shift %(shiftwidth)gms
%(postprocess)s
-
''' % {
     'test_options': test_options,
     'calclog': calclog,
     'condition':condition,
     'eventindex':eventindex,
     'shiftwidth': shiftwidth_points*1000.0/self.sfreq,
     'postprocess': postprocess
    })
    self.avg_q_object.run() 

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
 def __init__(self,avg_q_object,infiles):
  self.avg_q_object=avg_q_object
  self.outfile=None
  self.alltuples={}
  self.infiles=[]
  self.sfreq=None
  self.epochlength_points=None
  self.beforetrig_points=None
  for avgfile in infiles:
   if not os.path.exists(avgfile):
    print("Oops, %s doesn't exist!" % avgfile)
    continue
   f=avg_q_file(avgfile)
   if not self.sfreq:
    self.sfreq,self.epochlength_points,self.beforetrig_points=self.avg_q_object.get_description(f,('sfreq','nr_of_points','beforetrig'))
   self.alltuples[avgfile]=self.avg_q_object.get_filetriggers(f).gettuples()
   self.infiles.append(avgfile)
 def set_outfile(self,outfile,append=False):
  '''Arranges for the standard use of writing the result to an ASC file.
     Note that if this isn't used, you'll have to add the rest of a post-processing queue
     plus the script separator and run() yourself.'''
  self.outfile=outfile
  self.append=append
 def single_average(self,condition,eventindex=None):
  condstr=condition+' event %d' % eventindex if eventindex!=None else condition
  session_shift_points=[]
  n_averages=0 # Bail out if no averages are available at all
  for avgfile in self.infiles:
   f=avg_q_file(avgfile)
   tuples=self.alltuples[avgfile]
   for position,code,description in [(position,code,description) for position,code,description in tuples if condstr in description]:
    #print position,code,description
    self.avg_q_object.getepoch(f,fromepoch=position+1,epochs=1)
    n_averages+=1
    if eventindex!=None and eventindex!=self.event0index:
     shift=get_shift_from_description(description)
     shift_points=int(round((shift-self.standardized_RT_ms)/1000.0*self.sfreq))
     session_shift_points.append(shift_points)
     self.avg_q_object.write('''
>trim %(shift_points)d %(epochlength_points)d
>set beforetrig %(beforetrig_points)d
>set xdata 1
''' % {
     'shift_points': shift_points,
     'epochlength_points': self.epochlength_points,
     'beforetrig_points': self.beforetrig_points,
     })
  if n_averages==0: return
  self.avg_q_object.write('''
extract_item 0
average -W -t
Post:
calc -i 2 log10
calc -i 2 neg
set_comment %(condstr)s
''' % {
  'condstr': condstr+(' shift %gms' % (self.standardized_RT_ms if eventindex!=None and eventindex!=self.event0index else 0)) if eventindex!=None else condstr,
  })
  if self.outfile:
   self.avg_q_object.write('''
writeasc %(append)s -b %(outfile)s
-
''' % {
   'append': '-a' if self.append else '',
   'outfile': self.outfile,
   })
   self.avg_q_object.run()
   self.append=True
  if eventindex!=None and eventindex!=self.event0index:
   print("%s Mean shift points: %g" % (condstr,float(sum(session_shift_points))/len(session_shift_points)))
