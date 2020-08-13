# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Create a staging plot (Polysomnogram PSG) using matplotlib.
"""

import matplotlib.pyplot as plt
import matplotlib.ticker
import matplotlib.dates
import numpy as np

stage_to_Y={
  0: 5,
  1: 3,
  2: 2,
  3: 1,
  4: 1,
  5: 4, # REM
  6: 5, # MT
}

Y_to_color={
  1: 'blue',
  2: 'darkgreen',
  3: 'lightgreen',
  4: 'red',
  5: 'orange',
}
# Relative to linewidthOne:
Y_to_lw={
  1: 20,
  2: 20,
  3: 20,
  4: 20,
  5: 20,
}

arousal_tick_length=0.2
EMplot_unitlength=0.05

def slplot(sl,lightsonoff=True,arousalplot=True,EMplot=True,abstime=False,trim=True):
 '''
 lightsonoff: Show lights on and off times by vertical lines
 arousalplot: Show arousals
 EMplot: Show eye movements
 abstime: Show clock time instead of time since sleep onset
 trim: Remove Wake times at beginning and end
 '''
 axes = plt.gca() # Get Current Axis
 # Force the lazy mechanism to actually populate the file
 sl.create_tuples()
 #print(sl.tuples)
 if not abstime:
  if len(sl.lights_out)>0:
   # Use first lights_out as zero time, as I don't think anybody wants time since
   # sleep onset which is in x.time
   offset=sl.lights_out[0]['offset']
   X=(np.linspace(1,len(sl.tuples),num=len(sl.tuples))-1-offset)*sl.minutes_per_epoch
  else:
   X=np.array([x.time for x in sl.tuples])
  axes.xaxis.set_label_text('Time[min]')
 else:
  X=np.array(sl.abstime)
  axes.xaxis.set_label_text('Time')
  # Just use HH:MM format, not the date/time default which also needs vertical rotation...
  axes.xaxis.set_major_formatter(matplotlib.dates.DateFormatter('%H:%M'))
  #plt.xticks(rotation='vertical')
  plt.subplots_adjust(bottom=.2) # Allow more space for the vertical labels
 # Default font size is 12; Scale with that
 linewidthOne=plt.matplotlib.rcParams['font.size']/12
 Y=np.array([stage_to_Y[x.stage] for x in sl.tuples])
 if trim:
  nowake=np.where(Y!=5)[0]
  xmin=X[nowake[0]]
  xmax=X[nowake[-1]]
  # In any case plot between first lights out and last lights on
  if len(sl.lights_out)>0:
   xmin=min(xmin,X[sl.lights_out[0]['offset']])
  if len(sl.lights_on)>0:
   xmax=max(xmax,X[sl.lights_on[0]['offset']])
 else:
  xmin=X.min()
  xmax=X.max()
 ymin=min(stage_to_Y.values())-1-(1 if EMplot else 0)
 arousalplot_Y=max(stage_to_Y.values())+1
 EMplot_Y= 0
 ymax=arousalplot_Y+(1 if arousalplot else 0)
 axes.set_xlim(xmin,xmax)
 axes.yaxis.set_label_text('Stage')
 axes.set_ylim(ymin,ymax)
 yformatter=matplotlib.ticker.FixedFormatter((['EM'] if EMplot else [])+['N3','N2','N1','REM','W']+(['Arousal'] if arousalplot else []))
 # Without this the labels jump to other values when panning...
 ylocator=matplotlib.ticker.FixedLocator(range(ymin+1,ymax))
 axes.yaxis.set_major_formatter(yformatter)
 axes.yaxis.set_major_locator(ylocator)
 if lightsonoff:
  axes.vlines(X[[x['offset'] for x in sl.lights_out]],ymin,ymax,color='green',linewidth=linewidthOne,linestyle='solid')
  axes.vlines(X[[x['offset'] for x in sl.lights_on]],ymin,ymax,color='red',linewidth=linewidthOne,linestyle='solid')
 if arousalplot:
  #axes.hlines(arousalplot_Y,X.min(),X.max(),color='grey',linewidth=1,linestyle='solid')
  arousals=np.array([x.arousals for x in sl.tuples])
  axes.vlines(X[arousals>0],arousalplot_Y-arousal_tick_length,arousalplot_Y+arousal_tick_length*(arousals[arousals>0]-1),color=Y_to_color[stage_to_Y[0]],linewidth=linewidthOne,linestyle='solid')
 if EMplot:
  EM=np.array([x.eyemovements for x in sl.tuples])
  axes.vlines(X[EM>0],EMplot_Y-EMplot_unitlength,EMplot_Y+EMplot_unitlength*(EM[EM>0]-1),color=Y_to_color[stage_to_Y[5]],linewidth=linewidthOne,linestyle='solid')
 if False:
  # A simple single-line plot
  axes.plot(X,Y, color='blue', linewidth=1, linestyle='solid')
 else:
  # Plot each stage with separate horizontal lines
  # Note that we go through some lengths so that the vertical grey lines
  # don't obscure the broad colored stage lines: The vlines are plotted first...
  uniqueY=np.unique(Y)
  startend={}
  for y in uniqueY:
   ind=np.where(Y==y)[0]
   # Find indexes where stage Y changes from y (at Y[ind]) to a different stage (Y[ind+1])
   jumps=np.where(np.diff(ind)>1)[0]
   startind=ind[np.concatenate(([0],jumps+1))]
   # endind is already the first index of the next stage
   endind=ind[np.concatenate((jumps,[len(ind)-1]))]+1
   if endind[-1]==len(X): endind[-1]-=1
   startend[y]=[startind,endind]

  # Plot vertical lines where the current stage ends
  for y in uniqueY:
   startind,endind=startend[y]
   axes.vlines(X[endind],Y[startind],Y[endind],color='lightgrey',linewidth=linewidthOne,linestyle='solid')
  # Plot the horizontal lines
  for y in uniqueY:
   startind,endind=startend[y]
   axes.hlines(Y[startind],X[startind],X[endind],color=Y_to_color[y],linewidth=Y_to_lw[y]*linewidthOne,linestyle='solid')
 return axes
