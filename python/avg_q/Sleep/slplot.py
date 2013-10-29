"""
Create a staging plot using matplotlib.
"""

import matplotlib.pyplot as plt
import numpy as np

stage_to_Y={
  0: 6,
  1: 4,
  2: 3,
  3: 2,
  4: 1,
  5: 5,
}

Y_to_color={
  1: 'blue',
  2: 'blue',
  3: 'blue',
  4: 'blue',
  5: 'red',
  6: 'blue',
}
Y_to_lw={
  1: 2,
  2: 2,
  3: 2,
  4: 2,
  5: 6,
  6: 2,
}

import matplotlib.ticker
yformatter=matplotlib.ticker.FixedFormatter(['','4','3','2','1','REM','W'])

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
 axes.set_ylim(0,7)
 if False:
  # A simple single-line plot
  axes.plot(X,Y, color = 'blue', linewidth=1, linestyle="-")
 else:
  # Plot each stage with separate horizontal lines
  for y in np.unique(Y):
  #for y in [6]:
   ind=np.where(Y==y)[0]
   jumps=np.where(ind[1:]-ind[:(len(ind)-1)]>1)[0]
   for jumpind in range(-1,len(jumps)):
    xstart=X[ind[0]] if jumpind== -1 else X[ind[jumps[jumpind]+1]]
    xend=X[ind[len(ind)-1]] if jumpind==len(jumps)-1 else X[ind[jumps[jumpind+1]]]
    #print(xstart,xend)
    axes.hlines(y,xstart,xend,color=Y_to_color[y],linewidth=Y_to_lw[y],linestyle="-")
 return axes
