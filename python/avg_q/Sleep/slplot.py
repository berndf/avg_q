"""
Create a staging plot (Polysomnogram PSG) using matplotlib.
"""

import matplotlib.pyplot as plt
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

import matplotlib.ticker
yformatter=matplotlib.ticker.FixedFormatter(['N3','N2','N1','REM','W'])
# Without this the labels jump to other values when panning...
ylocator=matplotlib.ticker.FixedLocator(range(1,6))

def slplot(sl,realtime=False):
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
 axes.set_xlim(X.min(),X.max())
 Y=np.array([stage_to_Y[x.stage] for x in sl.tuples])
 axes.yaxis.set_label_text('Stage')
 axes.yaxis.set_major_formatter(yformatter)
 axes.yaxis.set_major_locator(ylocator)
 axes.set_ylim(min(stage_to_Y.values())-1,max(stage_to_Y.values())+1)
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
