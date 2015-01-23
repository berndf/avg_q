# Copyright (C) 2013-2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
import avg_q
import numpy

# Layout helper function to get a 2D arrangement of nplots plots
def nrows_ncols_from_nplots(nplots):
 ncols=numpy.sqrt(nplots)
 if ncols.is_integer():
  nrows=ncols
 else:
  ncols=numpy.ceil(ncols)
  nrows=nplots/ncols
  if not nrows.is_integer():
   nrows=numpy.ceil(nrows)
 return int(nrows),int(ncols)

class numpy_epoch(object):
 def __init__(self,data=None):
  self.comment=None
  self.channelnames=[]
  self.channelpos=[]
  self.nr_of_points=0
  self.nr_of_channels=0
  self.itemsize=0
  self.xdata=None
  if data is None:
   self.data=None
   self.nr_of_points=0
   self.nr_of_channels=0
   self.sfreq=None
  else:
   if type(data) is numpy.array:
    self.data=data
   else:
    self.data=numpy.array(data,'float32')
   self.nr_of_points,self.nr_of_channels=self.data.shape
   self.sfreq=1
 def __str__(self):
  return(((self.comment+': ') if self.comment else "")+
  "%d x %d sfreq %g" % (self.nr_of_points,self.nr_of_channels,self.sfreq))

class numpy_Epochsource(avg_q.Epochsource):
 def __init__(self, epochs=None):
  self.epochs=epochs
  self.branch=[]
 def set_trigpoints(self,trigpoints):
  raise Exception('numpy_Epochsource: set_trigpoints not implemented')
 def send(self,avg_q_instance):
  if len(self.epochs)==0: return
  nr_of_points,nr_of_channels=self.epochs[0].data.shape
  avg_q_instance.write('''
read_generic -c %(readx)s -s %(sfreq)g -C %(nr_of_channels)d -e %(epochs)d stdin 0 %(aftertrig)d float32
''' % {
   'readx': '-x xdata' if self.epochs[0].xdata is not None else '',
   'sfreq': self.epochs[0].sfreq if self.epochs[0].sfreq else 100.0,
   'epochs': len(self.epochs),
   'aftertrig': nr_of_points,
   'nr_of_channels': nr_of_channels,
  })
  if self.epochs[0].channelnames:
   channelnames=self.epochs[0].channelnames
   if self.epochs[0].channelpos:
    channelpos=self.epochs[0].channelpos
   else:
    # If channelpos is not available, make it up
    channelpos=[(i,0,0) for i in range(len(channelnames))]
   methodline='>set_channelposition -s '+' '.join(["%s %g %g %g" % (channelnames[channel],channelpos[channel][0],channelpos[channel][1],channelpos[channel][2]) for channel in range(len(self.epochs[0].channelnames))])
   avg_q_instance.write(methodline+'\n')
  for methodline in self.branch:
   avg_q_instance.write(methodline+'\n')
 def send_trigpoints(self,avg_q_instance):
  # This is actually used to send the data.
  if len(self.epochs)==0: return
  for epoch in self.epochs:
   # It's a bit unfortunate that array.array does support tofile() with pipes but numpy.array doesn't...
   # So we have to take the route via a string buffer just as with reading
   thisdata=numpy.append(epoch.xdata.reshape((epoch.xdata.shape[0],1)),epoch.data,axis=1) if epoch.xdata is not None else epoch.data
   avg_q_instance.avg_q.stdin.write(thisdata.tostring())
   avg_q_instance.avg_q.stdin.flush()

class numpy_Script(avg_q.Script):
 epochs=[] # List of numpy_epoch objects
 def read(self):
  '''Read the current epoch into numpy array self.data, channels=columns
     We support both reading all epochs from the iterated queue (if no collect
     method is set) and reading the single result of the post-processing queue.
  '''
  # Save and restore the current state, since we work by (temporally)
  # adding our own transforms
  self.save_state()
  transform="""
echo -F stdout Epoch Dataset\\n
query channelpositions stdout
query -N comment stdout
query -N sfreq stdout
query -N nr_of_points stdout
query -N itemsize stdout
echo -F stdout Data:\\n
write_generic -x stdout float32
"""
  if self.collect=='null_sink':
   self.add_transform(transform)
  else:
   self.add_postprocess(transform)
  rdr=self.runrdr()
  self.epochs=[]
  while True:
   try:
    n=next(rdr)
   except StopIteration:
    break
   if n!='Epoch Dataset': break
   epoch=numpy_epoch()
   for r in rdr:
    if '=' in r: break
    channelname,x,y,z=r.split('\t')
    epoch.channelnames.append(channelname)
    epoch.channelpos.append((float(x),float(y),float(z)))
   while r:
    if r=='Data:': break
    name,value=r.split('=',maxsplit=1)
    if name=='comment':
     epoch.comment=value
    elif name=='sfreq':
     epoch.sfreq=float(value)
    elif name=='nr_of_points':
     epoch.nr_of_points=int(value)
    elif name=='itemsize':
     epoch.itemsize=int(value)
    r=next(rdr)
   epoch.nr_of_channels=len(epoch.channelnames)
   #print(epoch)

   # Problem: If executed too quickly, the read() below will return only partial data...
   datapoints=epoch.nr_of_points*(1+epoch.nr_of_channels*epoch.itemsize)
   datalength=4*datapoints
   buf=self.avg_q_instance.avg_q.stdout.read(datalength)
   while len(buf)!=datalength:
    buf2=self.avg_q_instance.avg_q.stdout.read(datalength-len(buf))
    buf+=buf2

   #print(len(buf))
   # http://docs.scipy.org/doc/numpy-1.7.0/reference/generated/numpy.frombuffer.html
   data=numpy.frombuffer(buf,dtype=numpy.float32,count=datapoints)
   data.shape=(epoch.nr_of_points,1+epoch.nr_of_channels*epoch.itemsize)
   epoch.xdata=data[:,0]
   epoch.data=data[:,1:]
   self.epochs.append(epoch)
  self.restore_state()
 def plot_maps(self, vmin=None, vmax=None, globalscale=False, isolines=[0]):
  '''globalscale arranges for vmin,vmax to actually be -1,+1 after global max(abs) scaling.'''
  import matplotlib.mlab as mlab
  import matplotlib.pyplot as plt

  def mapplot(xpos,ypos,z,nsteps=50):
   #ncontours=15
   xmin,xmax=xpos.min(),xpos.max()
   ymin,ymax=ypos.min(),ypos.max()
   xi=numpy.linspace(xmin,xmax,nsteps)
   yi=numpy.linspace(ymin,ymax,nsteps)
   nplots=z.shape[0]
   nrows,ncols=nrows_ncols_from_nplots(nplots)
   # Default 'nn' interpolation of griddata requires package mpl_toolkits.natgrid, currently py2 only
   try:
    import mpl_toolkits.natgrid
    interpolation = 'nn'
   except ImportError:
    interpolation = 'linear'
   for thisplot in range(0,nplots):
    # cf. http://www.scipy.org/Cookbook/Matplotlib/Gridding_irregularly_spaced_data
    zi=mlab.griddata(xpos,ypos,z[thisplot],xi,yi,interp=interpolation)
    plt.subplot(nrows,ncols,thisplot+1)
    # pcolormesh is described to be much faster than pcolor
    # Note that the default for edgecolors appears to be 'None' resulting in transparent lines between faces...
    gplot=plt.pcolormesh(xi,yi,zi,norm=plt.Normalize(vmin=vmin,vmax=vmax),shading='flat',edgecolors='face',antialiaseds=False)
    #gplot=plt.contourf(g,ncontours)
    #plt.scatter(xpos,ypos,marker='o',c='black',s=5) # Draw sample points
    if isolines:
     plt.contour(xi,yi,zi,isolines,colors='black',linestyles='solid')
    gplot.axes.set_axis_off()
    gplot.axes.set_xlim(xmin,xmax)
    gplot.axes.set_ylim(ymin,ymax)

  self.save_state()
  self.add_transform('extract_item 0')
  # Arrange for epochs to be appended for plotting maps
  if self.collect=='null_sink':
   self.set_collect('append')
  self.read()
  for epoch in self.epochs:
   if globalscale:
    vmin=epoch.data.min()
    vmax=epoch.data.max()
    # Ensure symmetric scaling around 0
    scale=numpy.max([-vmin,vmax])
    epoch.data=epoch.data/scale
    vmin= -1
    vmax=  1
   mapplot(numpy.array([xyz[0] for xyz in epoch.channelpos]),numpy.array([xyz[1] for xyz in epoch.channelpos]),epoch.data)
  self.restore_state()
 def plot_traces(self, vmin=None, vmax=None, xlim=None, ylim=None, x_is_latency=False):
  '''This creates one 2d plot for each channel, like for time-freq data (freq=x, time=epoch).
  If x_is_latency=True, each matrix is transposed so x and y swapped.'''
  import matplotlib.pyplot as plt

  def traceplot(z,xlim=None,ylim=None,transpose=False):
   #ncontours=15
   nplots=len(z)
   nrows,ncols=nrows_ncols_from_nplots(nplots)
   thisplot=0
   for z1 in z:
    z1=numpy.array(z1)
    if transpose: z1=numpy.transpose(z1)
    plt.subplot(nrows,ncols,thisplot+1)
    x=numpy.linspace(xlim[0],xlim[1],num=z1.shape[1]) if xlim else numpy.array(range(z1.shape[1]))
    y=numpy.linspace(ylim[0],ylim[1],num=z1.shape[0]) if ylim else numpy.array(range(z1.shape[0]))

    # pcolormesh is described to be much faster than pcolor
    # Note that the default for edgecolors appears to be 'None' resulting in transparent lines between faces...
    gplot=plt.pcolormesh(x,y,z1,norm=plt.Normalize(vmin=vmin,vmax=vmax),shading='flat',edgecolors='face',antialiaseds=False)
    #gplot=plt.contourf(z1,ncontours)
    gplot.axes.set_axis_off()
    #print z1.shape
    gplot.axes.set_xlim(min(x),max(x))
    gplot.axes.set_ylim(min(y),max(y))
    thisplot+=1

  self.save_state()
  self.add_transform('extract_item 0')
  self.read()
  if self.epochs[0].xdata is not None:
   if x_is_latency:
    if xlim is None:
     xlim=(min(self.epochs[0].xdata),max(self.epochs[0].xdata))
   else:
    if ylim is None:
     ylim=(min(self.epochs[0].xdata),max(self.epochs[0].xdata))
  z=[] # One array per *channel*, each array collects all time points and epochs, epochs varying fastest
  # z[channel][point] is a list of values (epochs)
  for epoch in self.epochs:
   for point in range(epoch.nr_of_points):
    channels=epoch.data[point,:]
    for channel in range(0,len(channels)):
     if len(z)<=channel:
      z.append([[channels[channel]]])
     else:
      if len(z[channel])<=point:
       z[channel].append([channels[channel]])
      else:
       z[channel][point].append(channels[channel])
    point+=1

  traceplot(z,xlim=xlim,ylim=ylim,transpose=x_is_latency)
  self.restore_state()
