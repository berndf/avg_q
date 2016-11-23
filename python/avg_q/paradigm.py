# Copyright (C) 2008-2015 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Paradigm base class to classify an event train into trial types.
A trial is a sequence of events characterized by a trigger (time,code,description) tuple.
Note that events not included in either stimulus_set or response_set are
ignored for the classification of trials but are still included in the event
list of each trial to enable separate analysis of these events.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

unset_stimulus_name='Stimulus'

def mean(x):
 return sum(x)/len(x)

import math
def sdev(x):
 n=len(x)
 if n<=1:
  return None
 else:
  s=sum(x)
  ss=sum([y*y for y in x])
  # sd(x)==sqrt(mean(x^2)+mean(x)^2-2*mean(x)^2)*sqrt(n/(n-1)) # Second term for df correction
  # numerically var<0 (e.g., var= -1e-7) may happen numerically when variance is indeed zero
  var=(ss-s*s/n)/(n-1.0)
  return 0.0 if var<0.0 else math.sqrt(var)

def quantiles(x,q):
 theValues = sorted(x)
 retvals=[]
 for quantile in q:
  N=len(theValues)
  upperindex=int(math.ceil(N*quantile))
  if upperindex>=N:
   retvals.append(theValues[-1])
  elif upperindex<=0:
   retvals.append(theValues[0])
  else:
   fraction=N % (1.0/quantile)
   if fraction>1.0: fraction=1.0
   retvals.append(theValues[upperindex-1]*fraction+theValues[upperindex]*(1.0-fraction))
 return retvals

class paradigm(object):
 Name='UNKNOWN'
 stimulus_set=set()
 response_set=set()
 # This defines a low-level correction of occurrence times per code (stimulus or response),
 # for example to account for delayed onset of the sound in a stimulus sound file (positive)
 # or for a delay in registering a key press (negative).
 onset_correction_ms={}
 # This defines the order of the conditions appearing as keys in the output of get_trials
 correct_conditions=[]
 error_conditions=[]
 # stimuli, stimcode2stimulus and stimulus_assignments may be left unmodified if you don't
 # need the statistics to be able to calculate % correct of each stimulus class
 # This defines the order of the stimulus conditions appearing in the output
 stimuli=[]
 # Defines the correspondence between (stimulus) condition codes and stimulus names
 stimcode2stimulus={}
 # Maps condition to stimulus name or a list of stimulus names.
 stimulus_assignments={}
 # This is optional as well and allows programs to know for which of the
 # conditions we may have meaningful response parameters.
 response_conditions=[]
 # A hierarchy of dicts with codes as key on each level and either a dict or a code as
 # value. This defines the code to be used for any matching code sequence.
 # For example, this can be used to define a signal followed by a stop signal as a stop condition,
 # or for associating a sequence of 5 times a given code with a new code.
 # max_sequence_length_ms defines the maximum duration of a sequence between last and first stimulus.
 # The point in time will remain that of the first trigger. If no fit is found, the first code
 # will be used unmodified, continuing normally (matching will be tried again with the next code).
 stimulus_sequence_codes={}
 max_sequence_length_ms=5000
 minRT_ms=None # If set, RTs below this threshold are not considered as responses
 maxRT_ms=None # If set, RTs above this threshold are not considered as responses
 def __init__(self,triggers=[],sfreq=100):
  '''
  The base class can also be instantiated to use the basic triggerstats() function.
  '''
  self.sfreq=sfreq
  # Transfer the events, with onset correction if requested
  self.triggers=[]
  for point, code, description in triggers:
   if code in self.onset_correction_ms:
    point+=self.onset_correction_ms[code]*self.sfreq/1000
   self.triggers.append((point, code, description))
  self.conditions=self.correct_conditions+self.error_conditions

  # These are automatically computed on first access:
  # dict with key stimulus and value = count
  #self.stimulus_count={}
  # dict with key condition and value = list of [point1, point2, ...]
  # where each point is actually a full trigger tuple (point,code,description)
  #self.trials={}
 def __getattr__(self,name):
  '''Lazy trial classification on first access, allowing triggers to be
     analyzed through parse_trials or triggerstats without full classification.'''
  if name=='stimulus_count' or name=='trials':
   self.stimulus_count={}
   self.trials={}
   self.get_trials()
   return self.__dict__[name]
  else:
   raise AttributeError('\'paradigm\' object has no attribute \''+name+'\'');
 def is_ignored_code(self,code):
  # Note that if both stimulus_set and response_set are empty, we accept any code
  return not(len(self.stimulus_set)==0 and len(self.response_set)==0 or code in self.stimulus_set or code in self.response_set)
 def parse_trials(self):
  '''Iterator to output single trials together with their classification.
     This can be used from outside to obtain output in the order of trials, rather than sorted by condition.'''
  if self.stimuli:
   for stimulus in self.stimuli:
    self.stimulus_count[stimulus]=0
  else:
   self.stimulus_count[unset_stimulus_name]=0
  self.minRT_count=0
  self.maxRT_count=0
  i=0
  while i<len(self.triggers):
   trial=[self.triggers[i]]
   (point, code, description)=self.triggers[i]
   if code in self.stimulus_set:
    if code in self.stimulus_sequence_codes:
     i_start=i # Recorded here since i is increased by "ignored" codes
     def is_valid_continuation(seq,j):
      return(i+j<len(self.triggers) and self.triggers[i+j][1] in seq and (self.triggers[i+j][0]-point)/self.sfreq*1000.0<=self.max_sequence_length_ms)
     sequencedepth=0
     sequence=self.stimulus_sequence_codes[code]
     while True:
      # The end of a sequence is not a dict. Are we within the sequence?
      if type(sequence)==dict:
       if i+sequencedepth+1<len(self.triggers) and self.is_ignored_code(self.triggers[i+sequencedepth+1][1]):
        # Ignored code: Add it to the trial
        trial.append(self.triggers[i+sequencedepth+1])
        i+=1
        continue
       if i+sequencedepth+1>=len(self.triggers) or not is_valid_continuation(sequence,sequencedepth+1):
	# Bail out, sequence doesn't fit
        i=i_start
        trial=[self.triggers[i]]
        break
       sequencedepth+=1
       trial.append(self.triggers[i+sequencedepth])
       sequence=sequence[self.triggers[i+sequencedepth][1]]
      else:
       # Successful match
       code=sequence
       i+=sequencedepth
       #print("Sequence %s->%d" % (str(trial),code))
       break
    condition=None
    # Now look for response codes, scooping up inbetween ignored codes
    while i+1<len(self.triggers):
     (rpoint, rcode, rdescription)=self.triggers[i+1]
     if self.is_ignored_code(rcode):
      # Ignored code: Add it to the trial
      trial.append(self.triggers[i+1])
      i+=1
      continue
     if rcode in self.response_set:
      response_latency_ms=self.get_RT(trial+[self.triggers[i+1]])
      #print(code, rcode, response_latency_ms)
      # Classify RTs > maxRT_ms or <minRT as non-response,
      # counting the number of occurrences of these conditions as diagnostic
      if self.minRT_ms and response_latency_ms<self.minRT_ms:
       self.minRT_count+=1
       condition=self.classify_nonresponsetrial(point,code)
      elif self.maxRT_ms and response_latency_ms>self.maxRT_ms:
       self.maxRT_count+=1
       condition=self.classify_nonresponsetrial(point,code)
      else:
       condition=self.classify_responsetrial(point,code,rcode,response_latency_ms)
       trial.append(self.triggers[i+1])
     else:
      condition=self.classify_nonresponsetrial(point,code)
     break
    if not condition:
     condition=self.classify_nonresponsetrial(point,code)
    if condition:
     stimulus=self.stimcode2stimulus[code] if self.stimuli else unset_stimulus_name
     self.stimulus_count[stimulus]+=1
     yield condition,trial
   i+=1
 def get_trials(self):
  for condition in self.conditions:
   self.trials[condition]=[]
  for condition,trial in self.parse_trials():
   self.trials[condition].append(trial)
 def triggerstats(self):
  # Output statistics of code counts as well as time differences between two successive
  # instances of the same code, as used in check_crossings
  import collections
  result_tuple=collections.namedtuple('Triggerstats',('n','mean','sd','min','q25','q50','q75','max'))
  codes={}
  for t in self.triggers:
   lat=t[0]/self.sfreq*1000
   code=t[1]
   codes.setdefault(code,[]).append(lat)
  stats={}
  for code in codes:
   diffs=[codes[code][i]-codes[code][i-1] for i in range(1,len(codes[code]))]
   if diffs:
    q0,q25,q50,q75,q100=quantiles(diffs,[0.0,0.25,0.5,0.75,1.0])
    stats[code]=result_tuple(len(codes[code]),mean(diffs),sdev(diffs),q0,q25,q50,q75,q100)
   else:
    stats[code]=result_tuple(len(codes[code]),None,None,None,None,None,None,None)
  return stats
 def get_RT(self,trial):
  Stim_index=None
  Response_index=None
  for i in range(len(trial)):
   code=trial[i][1]
   if Stim_index is None and code in self.stimulus_set:
    Stim_index=i
   elif Response_index is None and code in self.response_set:
    Response_index=i
  if Stim_index is not None and Response_index is not None:
   return (trial[Response_index][0]-trial[Stim_index][0])/self.sfreq*1000.0
  else:
   return None
 def get_totalN(self):
  totalN=0
  for stimulus in self.stimulus_count:
   totalN+=self.stimulus_count[stimulus]
  return totalN
 def getstats(self,condition):
  import collections
  result_tuple=collections.namedtuple('RTStats',('n','N','stimulus','meanRT','sdRT','minRT','q25','q50','q75','maxRT'))
  assigned_stimuli=self.stimulus_assignments[condition] if self.stimuli else unset_stimulus_name
  if not isinstance(assigned_stimuli,list):
   assigned_stimuli=[assigned_stimuli]
  assigned_stimulus_count=sum([self.stimulus_count[x] for x in assigned_stimuli])
  response_latencies=[self.get_RT(t) for t in self.trials[condition]]
  response_latencies=[r for r in response_latencies if r is not None]
  if response_latencies:
   meanRT=mean(response_latencies)
   sdRT=sdev(response_latencies)
   minRT,q25,q50,q75,maxRT=quantiles(response_latencies,[0.0,0.25,0.5,0.75,1.0])
  else:
   meanRT,sdRT,minRT,q25,q50,q75,maxRT=(None,)*7

  return result_tuple(len(self.trials[condition]),assigned_stimulus_count,','.join(assigned_stimuli),meanRT,sdRT,minRT,q25,q50,q75,maxRT)
 def __str__(self):
  totalN=self.get_totalN()
  if totalN==0:
   s= "--- No valid trials ---\n"
  else:
   s=''
   for condition in self.conditions:
    RTStats=self.getstats(condition)
    if RTStats.N==0:
     s+= "%s: --- No trials ---" % condition
    else:
     s+= "%s: %d of %dx%s (%3.2f%%)" % (condition,RTStats.n,RTStats.N,RTStats.stimulus,RTStats.n*100.0/RTStats.N)
    if RTStats.meanRT:
     if RTStats.sdRT:
      s+=" RT: mean+-SD=%.0f+-%.0f, q0=%.0f q25=%.0f q50=%.0f q75=%.0f q100=%.0f\n" % (RTStats.meanRT,RTStats.sdRT,RTStats.minRT,RTStats.q25,RTStats.q50,RTStats.q75,RTStats.maxRT)
     else:
      # i.e., n==1
      s+=" RT: Single response %.0f\n" % RTStats.meanRT
    else:
     s+="\n"
  return s
 def write_onsets_mat(self,filename,fMRI_onset_s=0.0,TR_s=1.0,duration_s=0.0):
  '''Write a SPM cell array .mat file containing the names and onsets of all events.
     fMRI_onset_s is the difference between the time basis of the paradigm and that
     of the fMRI (including dummy scans) in seconds.
     TR_s is a divisor for the output - the default of 1.0 will write onsets in seconds,
     if you set this to the actual TR in seconds, output will be in units of scans.
  '''
  import scipy
  import scipy.io
  names=[]
  onsets=[]
  durations=[]
  for condition in self.conditions:
   names.append(condition)
   onsets.append([(x[0][0]/self.sfreq-fMRI_onset_s)/TR_s for x in self.trials[condition]])
   durations.append([duration_s/TR_s]*len(self.trials[condition]))
  ons={
   'names': scipy.array(names,dtype=object),
   'onsets': onsets,
   'durations': durations,
  }
  scipy.io.savemat(filename,ons,oned_as='row')

 # These need to be overridden by the concrete class:
 def classify_responsetrial(self,point,code,rcode,response_latency_ms):
  pass
 def classify_nonresponsetrial(self,point,code):
  pass
