# Copyright (C) 2008-2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
This provides a standard way of handling epoch-based scoring,
with implementation for Bernd Tritschler's SL sleep stage file format.
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

sl2file_paths=(
 '/AD/Group/PPT-A-Schlaflabor-Daten/tabneu',
 '/AD/Group/PPT-A-Schlaflabor-Daten/tab_bu',
 '/AD/Group/PPT-A-Schlaflabor-Daten/tab_bu/pl',
 '/AD/Group/PPT-A-Schlaflabor-Daten/tab_bu/niko'
)
# New somno5 *.sl3 files are below tab_slx
# Main sep: '#' (epoch specifier, stage specifier, details ...); each line also starts with '#'
# '*' is used instead of whitespace...
# Stage specifier: 'Stage*[W1234R]'
# Details are: 'rem*xx' for REMs, 'Arousal', 'LM', 'LMA', 'BM'
sl3file_paths=(
 '/AD/Group/PPT-A-Schlaflabor-Daten/tab_slx',
)
stages2={
 'W': 0,
 '1': 1,
 '2': 2,
 '3': 3,
 '4': 4,
 'R': 5,
 'MT': 6,
}
stages3={
 'W': 0,
 '1': 1,
 '2': 2,
 '3': 3,
 '4': 4,
 'R': 5,
 'M': 6,
 'MT': 6,
 'TA': 6,
 'T': 6,
}
stagenames={
 0: 'Wake',
 1: 'N1',
 2: 'N2',
 3: 'N3',
 4: 'N4',
 5: 'REM',
 6: 'MT',
}

sl2cache=None
sl3cache=None

def int_or_1(s):
 try:
  result=int(s)
 except:
  result=1
 return(result)

import collections

import os
class slfile(object):
 # This is the type returned by the reader and of the tuple list elements:
 sltuple=collections.namedtuple('sltuple',('time','stage','checks','arousals','myos','eyemovements'))
 remc_tuple=collections.namedtuple('remc_tuple',('remcycle','nremcycle'))
 def __init__(self,filename=None,minutes_per_epoch=None):
  self.Date=None
  self.Time=None
  self.slfile=None
  self.lights_out=[] # List of lights_out events of form {'hour': hour, 'minute': minute, 'offset': offset}
  self.lights_on=[] # List of lights_on events of form {'hour': hour, 'minute': minute, 'offset': offset}
  # Allow the object to be created uninitialized as well
  if filename:
   self.init_from_file(filename,minutes_per_epoch)
 def init_from_file(self,filename,minutes_per_epoch=None):
  self.filename=filename
  self.minutes_per_epoch=0.5 if not minutes_per_epoch else minutes_per_epoch
  self.first,self.ext=os.path.splitext(filename)
  self.rdr=self.rdr3
  if self.ext:
   if self.ext.lower()=='.sl':
    self.rdr=self.rdr2
  else:
   # No extension: Assume this is a book number
   self.locate_sl3file_via_bookno(filename)
   if self.filename:
    self.first,self.ext=os.path.splitext(self.filename)
    #print self.filename
   else:
    raise Exception('Can\'t locate SL file for book number %s' % filename)
  self.slfile=open(self.filename,'r')
 def locate_sl2file_via_bookno(self,booknumber):
  global sl2cache
  if sl2cache is None:
   from .. import idircache
   sl2cache=idircache.idircache(extensionstrip='sl')
  from . import bookno
  file_bookno=bookno.file_bookno(booknumber)
  self.filename=sl2cache.find(sl2file_paths,file_bookno)
 def locate_sl3file_via_bookno(self,booknumber):
  global sl3cache
  if sl3cache is None:
   from .. import idircache
   sl3cache=idircache.idircache(extensionstrip='sl3')
  from . import bookno
  file_bookno=bookno.file_bookno(booknumber)
  self.filename=sl3cache.find(sl3file_paths,file_bookno)
 def __iter__(self):
  return self.rdr()
 def rdr2(self):
  '''Reader for sl2 (old sl) files.'''
  (index,time,stage,checks,arousals,myos,eyemovements)=(0,-1, 0, 0, 0, 0, 0);
  while True:
   sl=self.slfile.read(14)
   if len(sl)<14: break
   if sl.startswith('WA'):
    sl=sl.rstrip()
    if len(sl)==6:
     hh,mm=int(sl[2:4]),int(sl[4:6])
    elif len(sl)==5:
     hh,mm=int(sl[2:3]),int(sl[3:5])
    self.lights_on.append({'hour': hh, 'minute': mm, 'offset': index})
   elif sl.startswith('LA'):
    sl=sl.rstrip()
    if len(sl)==6:
     hh,mm=int(sl[2:4]),int(sl[4:6])
    elif len(sl)==5:
     hh,mm=int(sl[2:3]),int(sl[3:5])
    self.lights_out.append({'hour': hh, 'minute': mm, 'offset': index})
   else:
    eyemovements=0
    if sl[0]!=' ':
     for k in stages2:
      if sl.startswith(k):
       stage=stages2[k]
       sl=sl[len(k):]
       if k=='R':
        if sl[0]=='a':
         eyemovements=10
        elif sl[0].isdigit():
         eyemovements=int(sl[0])
        sl=sl[1:]
       break

   checks=sl.count('C')
   arousals=sl.count('A')
   myos=sl.count('M')
   if time<0 and stage==2:	# Time starts from first stage 2
    time=0
   index+=1
   yield self.sltuple(time,stage,checks,arousals,myos,eyemovements)
   if time>=0:
    time+=self.minutes_per_epoch 
 def rdr3(self):
  '''Reader for sl3 files.'''
  (index,time,stage)=(0,-1, 0);
  while True:
   sl=self.slfile.readline()
   if not sl.startswith('#'): break
   sl=sl[1:].rstrip("\r\n") # Remove #
   if sl.startswith('Epoche*'):
    self.minutes_per_epoch=float(sl[7:])/60
    #print("self.minutes_per_epoch=%g" % self.minutes_per_epoch)
    continue
   if sl.startswith('Time*'):
    self.Time=sl[5:]
    #print("self.Time=%s" % self.Time)
    continue
   if sl.startswith('Date*'):
    self.Date=sl[5:]
    #print("self.Date=%s" % self.Date)
    # Nonsensical Date values observed in older sl3 files
    if self.Date=='1.1.1900':
     self.Date=None
    continue
   if not sl.lower().startswith('ep*'): continue
   fields=sl.split('#')
   epochfields=fields[0].split('*') # 'Ep',index,time(s)
   fields=fields[1:]

   (checks,arousals,myos,eyemovements)=(0, 0, 0, 0);
   for field in fields:
    if field.startswith('LA*'):
     timepart=field[3:]
     if len(timepart)==4:
      hh,mm=int(timepart[0:2]),int(timepart[2:4])
     elif len(timepart)==3:
      hh,mm=int(timepart[0:1]),int(timepart[1:3])
     else:
      print("Bad LA time: %s %s" % (self.filename,field))
     self.lights_out.append({'hour': hh, 'minute': mm, 'offset': index})
    elif field.startswith('WA*'):
     timepart=field[3:]
     if len(timepart)==4:
      hh,mm=int(timepart[0:2]),int(timepart[2:4])
     elif len(timepart)==3:
      hh,mm=int(timepart[0:1]),int(timepart[1:3])
     self.lights_on.append({'hour': hh, 'minute': mm, 'offset': index})
    elif field.startswith('Stage*'):
     stagecode=field[6:]
     if stagecode=='V': # No idea what this means, pbly 'has apnea diagnostics'?
      continue
     if stagecode in stages3:
      stage=stages3[stagecode]
     elif stagecode.startswith('R*'):
      # This happened in one old file (M0517.sl3)
      if stagecode[2:]==' ':
       eyemovements=0
      else:
       eyemovements=int(stagecode[2:])
     else:
      print("Oops, unknown stage %s!" % stagecode)
    elif field=='Arousal':
     arousals+=1
    elif field=='LM':
     myos+=1
    elif field=='LMA':
     myos+=1
     arousals+=1
    elif field=='BM':
     checks+=1
    elif field.startswith('rem*'):
     if field[4:]==' ':
      eyemovements=0
     else:
      eyemovements=int(field[4:])
    elif field.startswith('ApZA*'): # Central apnea with arousal
     apnea_za=int_or_1(field[5:])
    elif field.startswith('ApZ*'): # Central apnea without arousal
     apnea_z=int_or_1(field[4:])
    elif field.startswith('ApOA*'): # Obstructive apnea with arousal
     apnea_oa=int_or_1(field[5:])
    elif field.startswith('ApO*'): # Obstructive apnea without arousal
     apnea_o=int_or_1(field[4:])
    elif field.startswith('ApGA*'): # General apnea with arousal
     apnea_ga=int_or_1(field[5:])
    elif field.startswith('ApG*'): # General apnea without arousal
     apnea_g=int_or_1(field[4:])
    elif field.startswith('HypA*'): # Hypopnea with arousal
     hypopnea_a=int_or_1(field[5:])
    elif field.startswith('Hyp*'): # Hypopnea without arousal
     hypopnea=int_or_1(field[4:])
    else:
     print("Oops, unknown field %s!" % field)

   if time<0 and stage==2:	# Time starts from first stage 2
    time=0
   index+=1
   yield self.sltuple(time,stage,checks,arousals,myos,eyemovements)
   if time>=0:
    time+=self.minutes_per_epoch 
 def close(self):
  if self.slfile:
   self.slfile.close()
   self.sfile=None
 def find_datetime(self,datetime):
  '''Convenience function to locate the stage for a given absolute time.'''
  if datetime<self.abstime[0]: return None
  for row,rt in enumerate(self.abstime):
   if datetime>=rt and datetime<rt+self.step: return row
  return None
 # Lazy evaluation functions to get the epoch-wise data (tuples) and REM/NREM cycle (remcycles)
 def create_tuples(self):
  self.tuples=[tup for tup in self]
 def create_step(self):
  import datetime
  self.step=datetime.timedelta(minutes=self.minutes_per_epoch)
 def create_start_datetime(self):
  import datetime
  len(self.tuples) # Ensure that the sl file was read
  if self.Date is not None:
   day,month,year=(int(x) for x in self.Date.split('.'))
   hour,minute,second=(int(x) for x in self.Time.split(':'))
   self.start_datetime=datetime.datetime(year,month,day,hour,minute,second)
  else:
   import klinik
   import sqlalchemy as sa
   from . import bookno
   klinikdb = sa.create_engine(klinik.klinikurl)
   somno= sa.Table('somno',sa.MetaData(klinikdb),autoload=True)
   booknumber,ext=os.path.splitext(self.filename)
   path,booknumber=os.path.split(booknumber)
   booknumber=bookno.database_bookno(booknumber)
   abl_datum=sa.select(columns=[sa.func.date(somno.c.abldatum)],whereclause=somno.c.bn==booknumber).execute().fetchone()[0]
   la_time=datetime.datetime.combine(abl_datum,datetime.time(self.lights_out[0]['hour'],self.lights_out[0]['minute']))
   self.start_datetime=la_time-self.lights_out[0]['offset']*self.step
 def create_abstime(self):
  self.abstime=[]
  for row,tup in enumerate(self.tuples):
   self.abstime.append(self.start_datetime+row*self.step)
 def create_remcycles(self):
  self.remcycles=[]
  self.n_remcycles,self.n_nremcycles,in_REM= -1,0,False
  for pos in range(len(self.tuples)):
   time,stage=self.tuples[pos][0:2]
   if in_REM:
    if stage!=5:
     in_REM=False
     self.n_nremcycles+=1
     # Remain within this REM phase if it is not interrupted by NREM of at least 15 minutes
     i=pos+1
     while i<len(self.tuples) and self.tuples[i].time-time<15:
      if self.tuples[i].stage==5:
       in_REM=True
       self.n_nremcycles-=1
       break
      i+=1
   else:
    if self.n_remcycles== -1 and stage>=2:
     # Sleep onset
     self.n_remcycles,self.n_nremcycles=0,1
    # Note that sleep onset REM will yield remcycle=1 directly
    if stage==5:
     in_REM=True
     self.n_remcycles+=1
   self.remcycles.append(self.remc_tuple(self.n_remcycles,self.n_nremcycles))
 def create_rem_period_durations(self):
  self.rem_period_durations=[]
  for p in range(1,self.n_remcycles+1):
   self.rem_period_durations.append(sum([self.minutes_per_epoch for c in self.remcycles if c.remcycle==p and c.nremcycle==p]))
 def create_nrem_period_durations(self):
  self.nrem_period_durations=[]
  for p in range(1,self.n_nremcycles+1):
   self.nrem_period_durations.append(sum([self.minutes_per_epoch for c in self.remcycles if c.remcycle==p-1 and c.nremcycle==p]))
 def create_rem_period_arousals(self):
  self.rem_period_arousals=[]
  for p in range(1,self.n_remcycles+1):
   self.rem_period_arousals.append(sum([self.tuples[n].arousals for n,c in enumerate(self.remcycles) if c.remcycle==p and c.nremcycle==p]))
 def create_nrem_period_arousals(self):
  self.nrem_period_arousals=[]
  for p in range(1,self.n_nremcycles+1):
   self.nrem_period_arousals.append(sum([self.tuples[n].arousals for n,c in enumerate(self.remcycles) if c.remcycle==p-1 and c.nremcycle==p]))
 def create_rem_period_awakenings(self):
  self.rem_period_awakenings=[]
  for p in range(1,self.n_remcycles+1):
   wake_count=0
   skip=False
   for n,c in enumerate(self.remcycles):
    if c.remcycle==p and c.nremcycle==p:
     stage=self.tuples[n].stage
     if skip:
      if stage!=0:
       skip=False
     else:
      if stage==0:
       wake_count+=1
       skip=True
   self.rem_period_awakenings.append(wake_count)
 def create_nrem_period_awakenings(self):
  self.nrem_period_awakenings=[]
  for p in range(1,self.n_nremcycles+1):
   wake_count=0
   skip=False
   for n,c in enumerate(self.remcycles):
    if c.remcycle==p-1 and c.nremcycle==p:
     stage=self.tuples[n].stage
     if skip:
      if stage!=0:
       skip=False
     else:
      if stage==0:
       wake_count+=1
       skip=True
   self.nrem_period_awakenings.append(wake_count)
 def create_rem_period_density(self):
  # Note that this is the mean density only of REM epochs within this REM period
  microepochs_per_epoch=self.minutes_per_epoch*60.0/3 # 3s microepochs
  self.rem_period_density=[]
  for p in range(1,self.n_remcycles+1):
   epochs=[n for n,c in enumerate(self.remcycles) if c.remcycle==p and c.nremcycle==p and self.tuples[n].stage==5]
   n_epochs=len(epochs)
   self.rem_period_density.append(float(sum([self.tuples[n].eyemovements for n in epochs]))/n_epochs*100/microepochs_per_epoch)
 creator_functions={
  'tuples': create_tuples,
  'step': create_step,
  'start_datetime': create_start_datetime,
  'abstime': create_abstime,
  'remcycles': create_remcycles,
  'n_remcycles': create_remcycles,
  'n_nremcycles': create_remcycles,
  'rem_period_durations': create_rem_period_durations,
  'nrem_period_durations': create_nrem_period_durations,
  'rem_period_arousals': create_rem_period_arousals,
  'nrem_period_arousals': create_nrem_period_arousals,
  'rem_period_awakenings': create_rem_period_awakenings,
  'nrem_period_awakenings': create_nrem_period_awakenings,
  'rem_period_density': create_rem_period_density,
 }
 def __getattr__(self,name):
  if name in self.creator_functions:
   self.creator_functions[name](self)
   return self.__dict__[name]
  else:
   raise AttributeError('\'slfile\' object has no attribute \''+name+'\'');
