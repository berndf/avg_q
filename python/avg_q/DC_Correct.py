# Copyright (C) 2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

'''DC-Correct a continuous file by sampling and subtracting a low-frequency representation.
   DC_Correct_Script must be set up complete with Epochsource etc, possibly transform
   methods, but without collect method, to yield those sections of the data on which the
   interpolation is to be based.
   Each section is collapsed to a single point by averaging across points, representing
   one sample of the time series to interpolate.
   In a second step, the file is read continuously in epochs of epochlen_s, subtracting the
   slowly varying offset, and written to the output file.
'''

import avg_q
import avg_q.numpy_Script

class DC_Correct_Script(avg_q.numpy_Script.numpy_Script):
 def __init__(self,avg_q_instance,epochlen_s=1):
  avg_q.numpy_Script.numpy_Script.__init__(self,avg_q_instance)
  self.interpolators=None
  self.epochlen_s=epochlen_s

 def read_subsampled(self):
  # Read the samples to interpolate across the input file
  self.save_state()
  # xdata must be constructed in seconds.
  # Note that this order of methods sets xdata of the single point to the middle
  # point of the interval.
  # 'set xdata' after 'trim' would give the starting point of the interval.
  self.add_transform('''
set xdata abs_s
trim -a 0 0
''')
  self.set_collect('append')
  self.read()
  self.restore_state()

 # http://www.nehalemlabs.net/prototype/blog/2014/04/12/how-to-fix-scipys-interpolating-spline-default-behavior/
 # Instead of tuning the smoothing parameter s, we compute the estimated signal variance
 # by smoothing with a gaussian kernel
 def moving_average(self, series, sigma=3):
  import numpy
  import scipy.signal
  import scipy.ndimage.filters
  b = scipy.signal.gaussian(39, sigma)
  average = scipy.ndimage.filters.convolve1d(series, b/b.sum())
  var = scipy.ndimage.filters.convolve1d(numpy.power(series-average,2), b/b.sum())
  return average, var
 def interpolate(self):
  # Construct the interpolators
  import numpy
  import scipy.interpolate

  epoch=self.epochs[0]
  x=epoch.xdata

  self.interpolators=[]
  for channel in range(epoch.nr_of_channels):
   y=epoch.data[:,channel]
   _, var = self.moving_average(y)
   self.interpolators.append(scipy.interpolate.UnivariateSpline(x,y,w=1/numpy.sqrt(var)))

 def write_DC_Corrected(self,outfilename):
  # Write the output file
  import numpy
  import tempfile
  tempepochfile=tempfile.NamedTemporaryFile()
  self.save_state()
  epochsource=self.Epochsource_list[0]
  infile=epochsource.infile
  sfreq,points_in_file=self.avg_q_instance.get_description(infile,('sfreq','points_in_file'))
  # Prepare our epoch source for continuous reading
  epochsource.beforetrig=0
  epochsource.aftertrig=self.epochlen_s*sfreq
  epochsource.continuous=True
  epochsource.epochs=1
  epochsource.trigtransfer=True
  self.add_transform('subtract %s' % tempepochfile.name)
  if outfilename.endswith('.cnt'):
   self.add_transform('write_synamps -a -c %s 1' % outfilename)
  else:
   self.add_transform('write_brainvision -a %s IEEE_FLOAT_32' % outfilename)
  epoch=self.epochs[0]
  import os
  if os.path.exists(outfilename): os.unlink(outfilename)
  for epochno in range(int(points_in_file/sfreq/self.epochlen_s)):
   # x is computed in seconds
   x=epochno*self.epochlen_s+numpy.arange(self.epochlen_s*sfreq)/sfreq
   data=numpy.empty((self.epochlen_s*sfreq,epoch.nr_of_channels))
   for channel,s in enumerate(self.interpolators):
    data[:,channel]=s(x)
   newepoch=avg_q.numpy_Script.numpy_epoch(data)
   newepoch.channelnames=epoch.channelnames
   newepoch.channelpos=epoch.channelpos
   newepoch.sfreq=sfreq
   tmpepochsource=avg_q.numpy_Script.numpy_Epochsource([newepoch])
   tmpscript=avg_q.Script(self.avg_q_instance)
   tmpscript.add_Epochsource(tmpepochsource)
   tmpscript.add_transform('writeasc -b %s' % tempepochfile.name)
   tmpscript.run()
   epochsource.fromepoch=epochno+1
   self.run()
  self.restore_state()
  tempepochfile.close() # The temp file will be deleted at this point.

 def DC_Correct(self,outfilename):
  self.read_subsampled()
  self.interpolate()
  self.write_DC_Corrected(outfilename)
