# Copyright (C) 2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
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
 def detect_z_crossings(self,theta,itempart=1): 
  '''
  detects the crossings of IC's z_scores above threshold
  this is done for the positive and negative threshold in two steps
  ''' 
  outtuples=self.detect('''
fftfilter -i %(itempart)s 0.5 0.55 1 1
trim -x 0 Inf
scale_by -i %(itempart)s invsqrtnrofaverages
write_crossings -x -i %(itempart)s ALL %(threshold)f stdout
null_sink
-
''' % {'threshold': theta, 'itempart': itempart})   
  return outtuples

 def get_ranges(self,outtuples,theta):
  '''
  takes the triggertuples and sorts the conditions
  returns a trigger_dict sorted by key=condition, values=dicts of ICs with their lat and slope of crossing as values: {1:{'39': [[110.0, 1]...]}, 2:...}
  here the no_cond is taken from the triggers, which is also done later on, check if consistent!
  '''
  #print outtuples
  expected_slope=1 if theta>0 else -1
  
  ###sort triggers####
  trigger_dict={}  
  for outtuple in outtuples:
   #print outtuple
   lat,slope,description=[x for x in outtuple] 
   condition, IC= description.split(' ')
   trigger_dict.setdefault(IC,[]).append([lat,slope]) 

  IC_latrange_list=[]
  for IC_key, entry in trigger_dict.items():
   ###check and delete single crossings###
   if entry[-1][-1]==expected_slope:  
    del entry[-1]
   if entry==[]:# if IC was only crossing at one point and after deletion, no cross is left-> delete key 
    del trigger_dict[IC_key]
   else:
    if not entry[0][1]==expected_slope:
     del entry[0]
   
   # Build a [[start, end],...] list; if two latency windows are only separated by 1 sampling point, fuse them
   lastend=None
   for x in range(0,len(entry),2):
    start,end=entry[x][0],entry[x+1][0]
    if lastend and (start-lastend)*self.sfreq/1000.0<=1:
     # Modify the last entry, extending it to the new end
     IC_latrange_list[-1]=[IC_key, [IC_latrange_list[-1][1][0],end]]
    else:
     IC_latrange_list.append([IC_key, [start,end]])
    lastend=end
  return IC_latrange_list
  
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
     self.avg_q_object.getepoch(infile,fromepoch=epoch,epochs=1) 
   self.avg_q_object.write('''
remove_channel -k %(IC)s
%(process)s
trim -a -x %(trim)s
write_generic stdout string
null_sink
-
 '''%{'IC':IC, 'process': process, 'trim':trim})
   results=[float(x) for x in self.avg_q_object.runrdr()]
   values.append(results)
  return values

 def measure_z(self,infile,available_epochs,IC_latrange_list):
  '''
  for latency intervals of the ICs in each condition, extract the t-values of all the conditions,e.g.{1: {'39': ['5.14966', '5.26555'
  and calculate z-scores
  '''
    
  return self.measure_ranges(infile,available_epochs,IC_latrange_list,'''
extract_item 1
scale_by invsqrtnrofaverages
''')
 
 def measure_conditions(self,ascfile,conditions,zthreshold):
  infile=avg_q_file(ascfile) # Take care not to call set_infile on tdetector since we do the get_epoch ourself!
  triggers=self.avg_q_object.get_filetriggers(infile).gettuples()

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
     self.avg_q_object.getepoch(infile,fromepoch=fromepoch,epochs=1) 
     outtuples=self.detect_z_crossings(theta)
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

  z_scores=self.measure_z(infile,available_epochs,IC_latrange_list)
  header=['IC_no','crossing_lat_ms_start','crossing_lat_ms_end']
  for epochs in available_epochs:
   for epoch in epochs:
    header.append('z_'+triggers[epoch-1][2].replace(' ','_'))
  for index, (IC,latrange) in enumerate(IC_latrange_list):
   allmeasures.append([IC,latrange[0],latrange[1]]+z_scores[index])
  return header,allmeasures
