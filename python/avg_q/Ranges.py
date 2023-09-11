import datetime

class Range(object):
 def __init__(self,start,end,sensors=[]):
  '''Form a new range start-end, where sensors can be either a list or
     sensor names or a single name'''
  self.start=start
  self.end=end
  self.sensors=set(sensors) if type(sensors) is list else set([sensors])
 def __repr__(self):
  d=self.duration_h()
  if d is not None:
   return(("%.1fh: " % d)+str(self.start)+' - '+str(self.end)+' '+str(sorted(self.sensors)))
  else:
   return(str(self.start)+' '+str(self.end))
 def duration_h(self):
  return (self.end-self.start).total_seconds()/60/60 if isinstance(self.start,datetime.datetime) else None
 def join_with_range(self,r2):
  self.start=min(self.start,r2.start)
  self.end=max(self.end,r2.end)
  self.sensors=self.sensors.union(r2.sensors)
 def overlap_with_range(self,r2):
  newstart=max(self.start,r2.start)
  newend=min(self.end,r2.end)
  return Range(newstart,newend) if newstart<=newend else None
 def overlaps(self,r2):
  return(max(self.start,r2.start)<min(self.end,r2.end))
 def contains(self,ts):
  return ts>=self.start and ts<=self.end
 def length(self):
  return self.end-self.start

class Ranges(list):
 def __init__(self,ranges=[]):
  super().__init__(ranges)
 def duration_h(self):
  return sum([x.duration_h() for x in self])
 def add_range(self,r2):
  toadd=[]
  none_overlapped=True
  for r in self:
   if r.overlaps(r2):
    r.join_with_range(r2)
    none_overlapped=False
    break
  if none_overlapped:
   toadd.append(r2)
  self.extend(toadd)
 def add_ranges(self,addranges):
  for r2 in addranges:
   self.add_range(r2)
 def for_sensor(self,sensors):
  if type(sensors) is not list:
   sensors=[sensors]
  return [r for r in self if r.sensors.intersection(sensors)]
 def contains(self,ts):
  return any([r.contains(ts) for r in self])
 def range_overlap(self,r2):
  overlap=[r.overlap_with_range(r2) for r in self]
  return Ranges([x for x in overlap if x is not None])
 def length(self):
  return sum([r.length() for r in self])


def get_TrueRanges(b,minduration_True_s=0,minduration_False_s=0,name='True'):
 '''Helper function to convert a binary time series b to ranges for which b is
 True, fulfilling minduration_True_s and minduration_False_s
 '''
 TrueRanges=Ranges()
 # Start with False
 last_obs=(b.index[0],False) # This is (ind,state)
 for ind, state in b.iteritems():
  if state==last_obs[1]:
   # Unchanged state: Just skip
   continue
  # This is reached only for changed state
  if state:
   # True
   if (ind-last_obs[0]).total_seconds()>minduration_False_s:
    last_obs=(ind, state)
   elif len(TrueRanges)>0 and TrueRanges[-1].end==last_obs[0]:
    # If a True follows too quickly after False we must
    # drop the last range to join it with the next
    last_obs=(TrueRanges[-1].start, b.loc[TrueRanges[-1].start])
    del TrueRanges[-1]
   else:
    # No recorded range to correct, just continue
    last_obs=(ind, state)
  else:
   # False
   if (ind-last_obs[0]).total_seconds()>minduration_True_s:
    TrueRanges.add_range(Range(last_obs[0],ind,name))
   last_obs=(ind, state)
 if last_obs is not None and last_obs[1]:
  # Last observation is True: Add True up to the end
  ind=b.index[-1]
  if (ind-last_obs[0]).total_seconds()>minduration_True_s:
   TrueRanges.add_range(Range(last_obs[0],ind,name))
 return TrueRanges

