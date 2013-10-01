import avg_q
import numpy
import copy

# Layout helper function to get a 2D arrangement of nplots plots
def nrows_ncols_from_nplots(nplots):
 import numpy as np
 ncols=np.sqrt(nplots)
 if ncols.is_integer():
  nrows=ncols
 else:
  ncols=np.ceil(ncols)
  nrows=nplots/ncols
  if not nrows.is_integer():
   nrows=np.ceil(nrows)
 return int(nrows),int(ncols)

class numpy_epoch(object):
 def __init__(self):
  self.sfreq=None
  self.comment=None
  self.channelnames=[]
  self.channelpos=[]
  self.nr_of_points=0
  self.nr_of_channels=0
  self.itemsize=0
  self.data=None
 def __str__(self):
  return(((self.comment+': ') if self.comment else "")+
  "%d x %d sfreq %g" % (self.nr_of_points,self.nr_of_channels,self.sfreq))

class numpy_Epochsource(avg_q.Epochsource):
 def __init__(self, epochs=None):
  self.epochs=epochs
  self.branch=[]
 def set_trigpoints(self,trigpoints):
  raise('numpy_Epochsource: set_trigpoints not implemented')
 def send(self,avg_q_instance):
  if len(self.epochs)==0: return
  nr_of_points,nr_of_channels=self.epochs[0].data.shape
  avg_q_instance.write('''
read_generic -c -s %(sfreq)g -C %(nr_of_channels)d -e %(epochs)d stdin 0 %(aftertrig)d float32
''' % {
   'sfreq': self.epochs[0].sfreq if self.epochs[0].sfreq else 100.0,
   'epochs': len(self.epochs),
   'aftertrig': nr_of_points,
   'nr_of_channels': nr_of_channels,
  })
  if self.epochs[0].channelnames:
   # Note that channelpos must be correctly set as well
   channelnames=self.epochs[0].channelnames
   channelpos=self.epochs[0].channelpos
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
   avg_q_instance.avg_q.stdin.write(epoch.data.tostring())
   avg_q_instance.avg_q.stdin.flush()

class numpy_Script(avg_q.Script):
 epochs=[] # List of numpy_epoch objects
 def read(self):
  '''Read the current epoch into numpy array self.data, channels=columns
  '''
  # Save and restore the current list of transforms, since we work by (temporally)
  # appending to this list
  storetransforms=copy.copy(self.transforms)
  self.add_transform("""
echo -F stdout Epoch Dataset\\n
query channelpositions stdout
query -N comment stdout
query -N sfreq stdout
query -N nr_of_points stdout
query -N itemsize stdout
echo -F stdout Data:\\n
write_generic stdout float32
""")
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
    channelname,x,y,z=r.split()
    epoch.channelnames.append(channelname)
    epoch.channelpos.append((float(x),float(y),float(z)))
   while r:
    if r=='Data:': break
    name,value=r.split('=')
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
   datalength=4*epoch.nr_of_points*epoch.nr_of_channels*epoch.itemsize
   buf=self.avg_q_instance.avg_q.stdout.read(datalength)
   while len(buf)!=datalength:
    buf2=self.avg_q_instance.avg_q.stdout.read(datalength-len(buf))
    buf+=buf2

   #print(len(buf))
   # http://docs.scipy.org/doc/numpy-1.7.0/reference/generated/numpy.frombuffer.html
   epoch.data=numpy.frombuffer(buf,dtype=numpy.float32,count=epoch.nr_of_points*epoch.nr_of_channels*epoch.itemsize)
   epoch.data.shape=(epoch.nr_of_points,epoch.nr_of_channels*epoch.itemsize)
   self.epochs.append(epoch)
  self.transforms=storetransforms
 def plot_maps(self):
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
   for thisplot in range(0,nplots):
    # cf. http://www.scipy.org/Cookbook/Matplotlib/Gridding_irregularly_spaced_data
    zi=mlab.griddata(xpos,ypos,z[thisplot],xi,yi)
    # Ensure a color mapping symmetric around 0
    zmin,zmax=zi.min(),zi.max()
    if -zmin>zmax:
     zmax= -zmin
    plt.subplot(nrows,ncols,thisplot+1)
    gplot=plt.pcolor(xi,yi,zi,norm=plt.Normalize(vmin=-zmax,vmax=zmax),antialiaseds=False) # shading="faceted"
    #gplot=plt.contourf(g,ncontours)
    #plt.scatter(xpos,ypos,marker='o',c='black',s=5) # Draw sample points
    plt.contour(xi,yi,zi,[0],colors='black') # Draw zero line
    gplot.axes.set_axis_off()
    gplot.axes.set_xlim(xmin,xmax)
    gplot.axes.set_ylim(ymin,ymax)

  storetransforms=copy.copy(self.transforms)
  self.add_transform('extract_item 0')
  self.read()
  for epoch in self.epochs:
   mapplot(numpy.array([xyz[0] for xyz in epoch.channelpos]),numpy.array([xyz[1] for xyz in epoch.channelpos]),epoch.data)
  self.transforms=storetransforms
 def plot_traces(self, vmin=None, vmax=None):
  '''This creates one 2d plot for each channel, like for time-freq data (freq=x, time=epoch)'''
  import matplotlib.pyplot as plt

  def traceplot(z):
   #ncontours=15
   nplots=len(z)
   nrows,ncols=nrows_ncols_from_nplots(nplots)
   thisplot=0
   for z1 in z:
    z1=numpy.array(z1)
    plt.subplot(nrows,ncols,thisplot+1)
    gplot=plt.pcolor(z1,norm=plt.Normalize(vmin=vmin,vmax=vmax)) # shading="faceted"
    #gplot=plt.contourf(z1,ncontours)
    gplot.axes.set_axis_off()
    #print z1.shape
    gplot.axes.set_xlim(0,z1.shape[1])
    gplot.axes.set_ylim(0,z1.shape[0])
    thisplot+=1

  storetransforms=copy.copy(self.transforms)
  self.add_transform('extract_item 0')
  self.read()
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

  traceplot(z)
  self.transforms=storetransforms
