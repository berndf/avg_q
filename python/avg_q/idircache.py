# Copyright (C) 2008,2009,2011,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
idircache class for repeated case-independent searches in large directories
"""

__author__ = "Dr. Bernd Feige <Bernd.Feige@gmx.net>"

import os

class idircache(object):
 def __init__(self,extensionstrip=None):
  self.dircache={}
  self.extensionstrip=None
  if extensionstrip:
   if type(extensionstrip)==str:
    extensionstrip=[extensionstrip]
   self.extensionstrip=['.'+e.upper() for e in extensionstrip]
 def normalize(self,n,nameonly=False):
  '''Normalize the file name n. Pass nameonly=True to *not* apply the extensionstrip filter.
  '''
  if not nameonly and self.extensionstrip:
   n,ext=os.path.splitext(n)
   if ext is None or not ext.upper() in self.extensionstrip:
    return None
  return n.upper()
 def populate(self,p):
  # If the directory doesn't exist, treat it as empty instead of throwing an error
  try:
   ldir=os.listdir(p)
  except Exception:
   ldir=[]
  self.dircache[p]={}
  for filename in ldir:
   normalized=self.normalize(filename)
   if normalized is not None:
    self.dircache[p].setdefault(normalized,[]).append(filename)
 def find_all(self,pathlist,name):
  '''Return a list of all paths matching the given normalized name'''
  resultlist=[]
  if name:
   name=self.normalize(name,nameonly=True)
   if type(pathlist)==str:
    pathlist=[pathlist]
   for p in pathlist:
    if p not in self.dircache:
     self.populate(p)
    resultlist.extend([os.path.join(p,r) for r in self.dircache[p].get(name,[])])
  return resultlist
 def find(self,pathlist,name):
  '''Return only the first of the find_all results'''
  result=None
  if name:
   resultlist=self.find_all(pathlist,name)
   if len(resultlist)>0:
    result=resultlist[0]
  return result
