# Copyright (C) 2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
from . import Epochsource
from . import Detector
from . import avg_q_file

def get_no_avgs(a,infile,fromepoch):
 a.getepoch(infile,fromepoch=fromepoch,epochs=1)
 a.write('''
query nrofaverages
null_sink
-
''')
 out=[x for x in a.runrdr()]
 no_avgs=int(out[0])
 return no_avgs 
   
class T_avgs(Detector.Detector):
 '''
uses the Detector module to evaluate crossings of z-scores, calculated from the t-averages
 '''
 # The item to detect on
 itempart=1
 # The filter to use before detection. Set to None to disable!
 filterspec='0.5 0.55 1 1'

 def detect_z_crossings(self,theta,raw=False): 
  '''
  detects the crossings of IC's z_scores above threshold
  this must be called for the positive and negative threshold separately if desired
  ''' 
  self.add_transform('''
%(filtercmd)s
trim -x 0 Inf
%(scale)s
write_crossings -x -i %(itempart)s ALL %(threshold)f stdout
''' % {
   'filtercmd': '' if self.filterspec is None else 'fftfilter -i %(itempart)s %(filterspec)s' % {'itempart': self.itempart, 'filterspec': self.filterspec},
   'itempart': self.itempart,
   'threshold': theta,
   'scale': '' if raw else 'scale_by -i %s invsqrtnrofaverages' % self.itempart,
  })   
  return self.detect()

 def measure_ranges(self,infile,available_epochs,IC_latrange_list,process='extract_item 0'):
  '''
  for latency intervals of the ICs in each condition, extract the means in the
  ranges of all the conditions,e.g.{1: {'39': ['5.14966', '5.26555'...
  '''
    
  values=[]
  for IC,latrange in IC_latrange_list:
   trim="%g %g" % (latrange[0],latrange[1])
   for epochs in available_epochs: 
    for epoch in epochs:
     self.avg_q_instance.getepoch(infile,fromepoch=epoch,epochs=1) 
   self.avg_q_instance.write('''
remove_channel -k %(IC)s
%(process)s
trim -a -x %(trim)s
write_generic stdout string
null_sink
-
 '''%{'IC':IC, 'process': process, 'trim':trim})
   results=[float(x) for x in self.avg_q_instance.runrdr()]
   values.append(results)
  return values

 def measure_z(self,infile,available_epochs,IC_latrange_list,raw=False):
  '''
  for latency intervals of the ICs in each condition, extract the t-values of all the conditions,e.g.{1: {'39': ['5.14966', '5.26555'
  and calculate z-scores
  '''
    
  return self.measure_ranges(infile,available_epochs,IC_latrange_list,'''
extract_item %(itempart)d
%(scale)s
''' % {
   'itempart': self.itempart,
   'scale': '' if raw else 'scale_by invsqrtnrofaverages',
  })
 
 def measure_conditions(self,ascfile,conditions,zthreshold,raw=False):
  infile=avg_q_file(ascfile)
  triggers=self.avg_q_instance.get_filetriggers(infile).gettuples()

  available_epochs=[]
  for condition in conditions:
   found_epochs=[position+1 for position,code,description in triggers if condition in description]
   available_epochs.append(found_epochs)
  
  allmeasures=[]
  IC_latrange_list=[]
  for condition_index,condition in enumerate(conditions):
   for fromepoch in available_epochs[condition_index]:
    position,code,description=triggers[fromepoch-1]
    for theta in zthreshold,-zthreshold:
     self.Epochsource_list=[Epochsource(infile,fromepoch=fromepoch,epochs=1)]
     self.transforms=[]
     outtuples=self.detect_z_crossings(theta,raw)
     if outtuples:
      IC_latrange_list.extend(self.get_ranges(outtuples, theta))

  # Now find and remove ranges detected multiple times for different conditions
  found_ICs={}
  for index, (IC,latrange) in enumerate(IC_latrange_list):
   found_ICs.setdefault(IC,[]).append(index)
  ICs=found_ICs.keys()
  ICs.sort(cmp=lambda x,y: cmp(int(x),int(y)))
  
  for IC in ICs:
   found_ICs[IC].sort(cmp=lambda x,y: cmp(IC_latrange_list[x][1][0],IC_latrange_list[y][1][0]))
   for i,index in enumerate(found_ICs[IC]):
    if index==None: continue
    redo=True
    while redo:
     redo=False
     IC1,latrange1=IC_latrange_list[index]
     for j in range(i+1,len(found_ICs[IC])):
      index2=found_ICs[IC][j]
      if index2==None: continue
      IC2,latrange2=IC_latrange_list[index2]
      overlaprange=[max(latrange1[0],latrange2[0]),min(latrange1[1],latrange2[1])]
      if overlaprange[1]>overlaprange[0]:
       # There is an overlap. Now calculate how much
       overlap1=[max(latrange1[0],overlaprange[0]),min(latrange1[1],overlaprange[1])]
       overlap2=[max(latrange2[0],overlaprange[0]),min(latrange2[1],overlaprange[1])]
       len_overlaprange=overlaprange[1]-overlaprange[0]
       len_overlap1=overlap1[1]-overlap1[0]
       len_overlap2=overlap2[1]-overlap2[0]
       if len_overlap1>0.8*len_overlaprange or len_overlap2>0.8*len_overlaprange:
	#print "Overlap!",IC_latrange_list[index],IC_latrange_list[index2]
	IC_latrange_list[index]=[IC,[min(latrange1[0],latrange2[0]),max(latrange1[1],latrange2[1])]]
	found_ICs[IC][j]=None
	redo=True
	break

  outlist=[]
  for IC in ICs:
   for index in found_ICs[IC]:
    if index!=None: outlist.append(IC_latrange_list[index])
  IC_latrange_list=outlist

  z_scores=self.measure_z(infile,available_epochs,IC_latrange_list,raw)
  header=['component','crossing_lat_ms_start','crossing_lat_ms_end']
  for epochs in available_epochs:
   for epoch in epochs:
    header.append('z_'+triggers[epoch-1][2].replace(' ','_'))
  for index, (IC,latrange) in enumerate(IC_latrange_list):
   allmeasures.append([IC,latrange[0],latrange[1]]+z_scores[index])
  return header,allmeasures
