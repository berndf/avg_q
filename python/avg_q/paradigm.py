# Copyright (C) 2008-2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Paradigm base class to classify an event train into trial types
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
  return math.sqrt((ss-s*s/n)/(n-1.0))

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
 # max_sequence_pause_ms defines the maximum duration of a sequence between last and first stimulus.
 # The point in time will remain that of the first trigger. If no fit is found, the first code
 # will be used unmodified, continuing normally (matching will be tried again with the next code).
 stimulus_sequence_codes={}
 max_sequence_length_ms=5000
 minRT_ms=None # If set, RTs below this threshold are not considered as responses
 maxRT_ms=None # If set, RTs above this threshold are not considered as responses
 def __init__(self,triggers=[],sfreq=100):
  '''
  The base class can also be instantated to use the basic triggerstats() function.
  '''
  # Filter the events so that we can do clear decisions
  self.triggers=[]
  for point, code, description in triggers:
   if len(self.stimulus_set)==0 and len(self.response_set)==0 or code in self.stimulus_set or code in self.response_set:
    self.triggers.append((point, code, description))
  self.sfreq=sfreq
  # dict with key stimulus and value = count
  self.stimulus_count={}
  # dict with key condition and value = list of [point1, point2, ...]
  # where each point is actually a full trigger tuple (point,code,description)
  self.trials={}
  self.conditions=self.correct_conditions+self.error_conditions
  self.get_trials()
 def get_trials(self):
  if self.stimuli:
   for stimulus in self.stimuli:
    self.stimulus_count[stimulus]=0
  else:
   self.stimulus_count[unset_stimulus_name]=0
  for condition in self.conditions:
   self.trials[condition]=[]
  i=0
  while i<len(self.triggers):
   epochtriggers=[self.triggers[i]]
   (point, code, description)=self.triggers[i]
   if code in self.stimulus_set:
    def is_valid_continuation(seq,j):
     return(i+j<len(self.triggers) and self.triggers[i+j][1] in seq and (self.triggers[i+j][0]-point)/self.sfreq*1000.0<=self.max_sequence_length_ms)
    if code in self.stimulus_sequence_codes:
     sequencedepth=0
     sequence=self.stimulus_sequence_codes[code]
     while True:
      if type(sequence)==dict:
       if not is_valid_continuation(sequence,sequencedepth+1):
	# Bail out, sequence doesn't fit
        epochtriggers=[self.triggers[i]]
	break
       sequencedepth+=1
       epochtriggers.append(self.triggers[i+sequencedepth])
       sequence=sequence[self.triggers[i+sequencedepth][1]]
      else:
       # Successful match
       code=sequence
       i+=sequencedepth
       #print("Sequence %s->%d" % (str(epochtriggers),code))
       break
    condition=None
    if i+1<len(self.triggers):
     (rpoint, rcode, rdescription)=self.triggers[i+1]
     if rcode in self.response_set:
      response_latency_ms=(rpoint-point)/self.sfreq*1000.0
      #print code, rcode, response_latency_ms
      # Classify RTs > maxRT_ms or <minRT as non-response
      if ((self.minRT_ms and response_latency_ms<self.minRT_ms) or
	  (self.maxRT_ms and response_latency_ms>self.maxRT_ms)):
       condition=self.classify_nonresponsetrial(point,code)
      else:
       condition=self.classify_responsetrial(point,code,rcode,response_latency_ms)
       epochtriggers.append(self.triggers[i+1])
     else:
      condition=self.classify_nonresponsetrial(point,code)
    else:
     condition=self.classify_nonresponsetrial(point,code)
    if condition:
     stimulus=self.stimcode2stimulus[code] if self.stimuli else unset_stimulus_name
     self.stimulus_count[stimulus]+=1
     self.trials[condition].append(epochtriggers)
   i+=1
 def triggerstats(self):
  codes={}
  for t in self.triggers:
   lat=t[0]/self.sfreq*1000
   code=t[1]
   codes.setdefault(code,[]).append(lat)
  stats={}
  for code in codes:
   diffs=[codes[code][i]-codes[code][i-1] for i in range(1,len(codes[code]))]
   if diffs:
    stats[code]=(len(codes[code]),mean(diffs),sdev(diffs),)+tuple(quantiles(diffs,[0.0,0.25,0.5,0.75,1.0]))
   else:
    stats[code]=(len(codes[code]),)+(None,)*7
  return stats
 def get_RT(self,epochtriggers):
  Stim_index=None
  Response_index=None
  for i in range(len(epochtriggers)):
   code=epochtriggers[i][1]
   if Stim_index==None and code in self.stimulus_set:
    Stim_index=i
   elif Response_index==None and code in self.response_set:
    Response_index=i
  if Stim_index!=None and Response_index!=None:
   return (epochtriggers[Response_index][0]-epochtriggers[Stim_index][0])/self.sfreq*1000.0
  else:
   return None
 def get_totalN(self):
  totalN=0
  for stimulus in self.stimulus_count:
   totalN+=self.stimulus_count[stimulus]
  return totalN
 def getstats(self,condition):
  assigned_stimuli=self.stimulus_assignments[condition] if self.stimuli else unset_stimulus_name
  if not isinstance(assigned_stimuli,list):
   assigned_stimuli=[assigned_stimuli]
  assigned_stimulus_count=sum([self.stimulus_count[x] for x in assigned_stimuli])
  response_latencies=[self.get_RT(r) for r in self.trials[condition]]
  response_latencies=[r for r in response_latencies if r]
  if response_latencies:
   meanRT=mean(response_latencies)
   sdRT=sdev(response_latencies)
   minRT,q25,q50,q75,maxRT=quantiles(response_latencies,[0.0,0.25,0.5,0.75,1.0])
  else:
   meanRT,sdRT,minRT,q25,q50,q75,maxRT=(None,)*7

  return (len(self.trials[condition]),assigned_stimulus_count,','.join(assigned_stimuli),meanRT,sdRT,minRT,q25,q50,q75,maxRT)
 def __str__(self):
  totalN=self.get_totalN()
  if totalN==0:
   s= "--- No valid trials ---\n"
  else:
   s=''
   for condition in self.conditions:
    n,N,stimulus,meanRT,sdRT,minRT,q25,q50,q75,maxRT=self.getstats(condition)
    if N==0:
     s+= "%s: --- No trials ---" % condition
    else:
     s+= "%s: %d of %dx%s (%3.2f%%)" % (condition,n,N,stimulus,n*100.0/N)
    if meanRT:
     if sdRT:
      s+=" RT: mean+-SD=%.0f+-%.0f, q0=%.0f q25=%.0f q50=%.0f q75=%.0f q100=%.0f\n" % (meanRT,sdRT,minRT,q25,q50,q75,maxRT)
     else:
      # i.e., n==1
      s+=" RT: Single response %.0f\n" % meanRT
    else:
     s+="\n"
  return s
 # These need to be overridden by the concrete class:
 def classify_responsetrial(self,point,code,rcode,response_latency_ms):
  pass
 def classify_nonresponsetrial(self,point,code):
  pass
