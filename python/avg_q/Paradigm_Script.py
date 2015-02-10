# Copyright (C) 2011,2013-2015 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Paradigm_Script class working together with paradigm.py-derived trial lists.
In particular, we abstract here the idea of reading separately around
several different events within the epochs (eg Stimulus, Response) while
referring to the same baseline (before the first event) and shifting the
averages around the second and following events to exactly overlay the
first average.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>, Tanja Schmitt <schmitt.tanja@web.de>"

import avg_q

class Paradigm_Script(avg_q.Script):
 def calc_shiftwidth(self, list_of_latencies):
  '''
  Calculate the latency at which non-event0index trials will be centered within
  the latency frame of event0index, in an attempt to overlay equivalent
  sections of trials around this event index and event0index.
  The actual formula implements the geometric mean.
  '''
  import math
  return math.exp(sum([math.log(x) for x in list_of_latencies])/len(list_of_latencies))

 def add_Paradigm_Epochsource(self,infile,paradigm_instance,condition,event0index,eventindex,beforetrig='0.2s',aftertrig='1s',preprocess='baseline_subtract'):
  '''
  This encapsulates reading and shifting all single epochs of a given condition and eventindex, ready for averaging.
  event0index is the 'reference' event around which the epochs are actually read, baseline subtracted
  and then recentered around event eventindex.
  If event0index==eventindex, obviously, epochs are read normally without shift.
  shiftwidth_points is returned, giving amount by which the re-centered event was shifted.
  '''
  self.sfreq=self.avg_q_instance.get_description(infile,'sfreq')
  beforetrig_points=self.avg_q_instance.time_to_points(beforetrig,self.sfreq)
  aftertrig_points =self.avg_q_instance.time_to_points(aftertrig,self.sfreq)

  def get_event(thistrial,i):
   ''' Since we keep ignored codes (codes that are neither stimuli nor
   responses in the paradigm) in the trial, simple indexing does not suffice
   but we need to clean the trial first.
   '''
   cleantrial=[x for x in thistrial if not paradigm_instance.is_ignored_code(x[1])]
   return cleantrial[i]

  if eventindex==event0index:
   point_list=[get_event(trial,eventindex) for trial in paradigm_instance.trials[condition]]
   epochsource=avg_q.Epochsource(infile, beforetrig, aftertrig)
   epochsource.set_trigpoints(point_list)
   epochsource.add_branchtransform(preprocess)
   self.add_Epochsource(epochsource)
   shiftwidth_points=0
  else:
   latency_points=[get_event(trial,eventindex)[0]-get_event(trial,event0index)[0] for trial in paradigm_instance.trials[condition]]
   shiftwidth_points=round(self.calc_shiftwidth(latency_points))
   # Warn if our desired shift point is not within the averaged epoch
   if aftertrig_points<=shiftwidth_points:
    print("Warning: shiftwidth_points=%d but aftertrig_points=%d!" % (shiftwidth_points,aftertrig_points))
   # This is the total target epoch length:
   trimlength=beforetrig_points+aftertrig_points
   win_before_trig_points=shiftwidth_points+beforetrig_points
   win_after_trig_points=trimlength-win_before_trig_points
   for trial in paradigm_instance.trials[condition]:
    go_point=get_event(trial,event0index)[0]
    secondpoint=get_event(trial,eventindex)[0]
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
    epochsource=avg_q.Epochsource(infile, beforetrig_points, winend_points-beforetrig_points)
    epochsource.set_trigpoints(go_point)
    epochsource.add_branchtransform(branch_text)
    self.add_Epochsource(epochsource)
  return shiftwidth_points
