"""
Create a staging plot (Polysomnogram PSG) using matplotlib.
"""

import matplotlib.pyplot as plt
import matplotlib.ticker
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
Y_to_lw={
  1: 20,
  2: 20,
  3: 20,
  4: 20,
  5: 20,
}

arousal_tick_length=0.2

def slplot(sl,lightsonoff=True,arousalplot=True,realtime=False):
 axes = plt.subplot(111)
 #print(sl.tuples)
 if not realtime:
  X=np.array([x.time for x in sl.tuples])
  axes.xaxis.set_label_text('Time[min]')
 else:
  X=np.array(sl.realtime)
  axes.xaxis.set_label_text('Time')
  plt.xticks(rotation='vertical')
  plt.subplots_adjust(bottom=.2) # Allow more space for the vertical labels
 Y=np.array([stage_to_Y[x.stage] for x in sl.tuples])
 xmin=X.min()
 xmax=X.max()
 ymin=min(stage_to_Y.values())-1
 arousalplot_Y=max(stage_to_Y.values())+1
 ymax=arousalplot_Y+(1 if arousalplot else 0)
 axes.set_xlim(xmin,xmax)
 axes.yaxis.set_label_text('Stage')
 axes.set_ylim(ymin,ymax)
 yformatter=matplotlib.ticker.FixedFormatter(['N3','N2','N1','REM','W']+(['Arousal'] if arousalplot else []))
 # Without this the labels jump to other values when panning...
 ylocator=matplotlib.ticker.FixedLocator(range(ymin+1,ymax))
 axes.yaxis.set_major_formatter(yformatter)
 axes.yaxis.set_major_locator(ylocator)
 if lightsonoff:
  axes.vlines(X[[x['offset'] for x in sl.lights_out]],ymin,ymax,color='green',linewidth=1,linestyle='solid')
  axes.vlines(X[[x['offset'] for x in sl.lights_on]],ymin,ymax,color='red',linewidth=1,linestyle='solid')
 if arousalplot:
  #axes.hlines(arousalplot_Y,X.min(),X.max(),color='grey',linewidth=1,linestyle='solid')
  arousals=np.array([x.arousals for x in sl.tuples])
  axes.vlines(X[arousals>0],arousalplot_Y-arousal_tick_length,arousalplot_Y+arousal_tick_length*(arousals[arousals>0]-1),color=Y_to_color[stage_to_Y[0]],linewidth=1,linestyle='solid')
 if False:
  # A simple single-line plot
  axes.plot(X,Y, color = 'blue', linewidth=1, linestyle='solid')
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
   startind=ind[np.concatenate( ([0],jumps+1) )]
   # endind is already the first index of the next stage
   endind=ind[np.concatenate( (jumps,[len(ind)-1]) )]+1
   if endind[-1]==len(X): endind[-1]-=1
   startend[y]=[startind,endind]

  # Plot vertical lines where the current stage ends
  for y in uniqueY:
   startind,endind=startend[y]
   axes.vlines(X[endind],Y[startind],Y[endind],color='lightgrey',linewidth=1,linestyle='solid')
  # Plot the horizontal lines
  for y in uniqueY:
   startind,endind=startend[y]
   axes.hlines(Y[startind],X[startind],X[endind],color=Y_to_color[y],linewidth=Y_to_lw[y],linestyle='solid')
 return axes
