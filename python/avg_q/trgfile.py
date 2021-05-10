# Copyright (C) 2007-2010,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Trigger file interface.
Takes either a string pointing to a trigger file or an iterator yielding lines
"""

import re

class Preamble(dict):
 def __init__(self,*args,**kwdargs):
  dict.__init__(self,*args,**kwdargs)
  self.strings=[]
  self.varmatch=re.compile("^# ([^=]+)=([^=]+)")
 def append(self,string):
  m=self.varmatch.match(string)
  if m:
   name,value=m.groups()
   self[name]=value
  else:
   self.strings.append(string)
 def __str__(self):
  retstr=''
  if len(self.strings)>0:
   retstr="\n".join(self.strings) + "\n"
  for key in list(self.keys()):
   retstr+='# ' + key + '=' + str(self[key]) + "\n"
  return retstr

class trgfile(object):
 def __init__(self,source=None):
  self.trgfile=None
  self.reader=None
  self.tuples=None
  self.description_codes=None
  self.filename=source
  if source:
   if type(source) is not str:
    if hasattr(source, '__iter__'):
     # Allow to pass a list or generator as source
     self.filename=None
     self.reader=iter(source)
    elif hasattr(source, '__fspath__'):
     # Allow source to be os.PathLike
     self.filename=source.__fspath__()
    else:
     self.filename=str(source)
   if self.filename:
    self.trgfile=open(self.filename,mode='r')

  self.preamble=Preamble()
  self.start_datetime=None
 def __del__(self):
  """Destructor."""
  self.close()
 # The "with avg_q.trgfile.trgfile() as xxx" API
 def __enter__(self):
  return self
 def __exit__(self,exc_type,exc_value,traceback):
  self.close()
 def set_description_codes(self,description_codes):
  self.description_codes=description_codes
  self.unknown_descriptions={}
 def close(self):
  if self.trgfile:
   self.trgfile.close()
   self.trgfile=None
 def getline(self):
  # Note that 'yield from' was tried here but consumes the input stream and
  # doesn't allow us to continue reading another stream of tuples after EOF
  try:
   if self.trgfile:
    line=next(self.trgfile)
   else:
    line='EOF' if self.reader is None else next(self.reader)
  except StopIteration:
   line='EOF'
  return line
 def __iter__(self):
  return self.rdr()
 def translate(self,tuple):
  '''Every tuple read passes through this function for translation.
  Here we implement the description_codes translation.'''
  if self.description_codes is None: return tuple
  point, code, description=tuple
  if description in self.description_codes:
   code=self.description_codes[description]
  else:
   self.unknown_descriptions[description]=code
  return (point,code,description)
 def rdr(self):
  while True:
   line=self.getline()
   if isinstance(line,tuple):
    yield self.translate(line)
    continue
   # EOF:
   if not isinstance(line,str):
    break
   line=line.rstrip('\r\n')
   # EOF:
   if line=='EOF':
    break
   if line=='':
    continue
   if line[0]=='#':
    self.preamble.append(line)
    continue
   tup=line.split('\t')
   if len(tup)<2:
    # Try again with any whitespace sep
    tup=line.split()
   if len(tup)<2:
    continue
   elif len(tup)==2:
    (point, code)=tup
    description=None
   else:
    (point, code)=tup[0:2]
    description='\t'.join(tup[2:])
   # Alternative EOF marker, for streams
   if code=='0':
    break
   # Process values written by write_crossings -x (Lat[ms]=xxx)
   if point.find('=')>=0:
    (eqbefore,eqafter)=point.rsplit('=',2)
    point=eqafter
   sfreq=float(self.preamble['Sfreq']) if 'Sfreq' in self.preamble else None
   factor=1.0
   if point.endswith('ms'):
    point=point[:-2]
    if sfreq: factor=sfreq/1000
   elif point.endswith('s'):
    point=point[:-1]
    if sfreq: factor=sfreq
   try:
    point=float(point)*factor
   except:
    point=None
   yield self.translate((point, int(code), description))
 def gettuples(self):
  if not self.tuples:
   self.tuples=[tup for tup in self.rdr()]
  return self.tuples
 def gettuples_abstime(self):
  import datetime
  if not self.start_datetime:
   self.start_datetime=datetime.datetime.strptime('2013-01-01 00:00:00', '%Y-%m-%d %H:%M:%S')
  tuples=self.gettuples()
  sfreq=float(self.preamble.get('Sfreq',100.0))
  for i,t in enumerate(tuples):
   tuples[i]=self.start_datetime+datetime.timedelta(seconds=t[0]/sfreq),t[1],t[2]
  return tuples
 def writetuples(self,tuples,outfile):
  outfile.write(str(self.preamble))
  for tup in tuples:
   outfile.write("\t".join([str(x) for x in tup if x is not None]))
   outfile.write("\n")

# Beware: There are nasty roundoff problems if you pass a point value to avg_q
# that is xx.5 when printed and thus is rounded up by avg_q but xx.499999 internally
# so not rounded by python. So we do the rounding here and pass integer values to avg_q...
def get_ints(value,upsample):
 readpoint=round(value)
 return (int(readpoint),int(round(upsample*(value-readpoint))))
class HighresTrigger(tuple):
 def __new__(self,value,upsample=1):
  self.upsample=upsample
  if isinstance(value,tuple):
   if len(value)==2:
    return tuple.__new__(self,value)
   else:
    return tuple.__new__(self,(value[0],int(value[2])))
  elif isinstance(value,float):
   return tuple.__new__(self,get_ints(value,self.upsample))
  elif isinstance(value,int):
   return tuple.__new__(self,(value,0))
 def floatvalue(self):
  return self[0]+float(self[1])/self.upsample
 def as_trigger(self):
  return (self[0],1,str(self[1]))

class HighresTriggers(list):
 def __init__(self,upsample=1):
  self.upsample=upsample
  list.__init__(self)
 def append(self,value):
  list.append(self,HighresTrigger(value,upsample=self.upsample))
 def as_triglist(self):
  return [h.as_trigger() for h in self]
