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
 def normalize(self,n):
  if self.extensionstrip:
   n,ext=os.path.splitext(n)
   if ext and not ext.upper() in self.extensionstrip:
    return None
  return n.upper()
 def populate(self,p):
  # If the directory doesn't exist, treat it as empty instead of throwing an error
  try:
   l=os.listdir(p)
  except:
   l=None
  if l:
   self.dircache[p]=dict(list(zip([self.normalize(n) for n in l],l)))
  else:
   self.dircache[p]={}
 def find(self,pathlist,name):
  result=None
  if name:
   name=self.normalize(name)
   if type(pathlist)==str:
    pathlist=[pathlist]
   for p in pathlist:
    if not p in self.dircache:
     self.populate(p)
    result=self.dircache[p].get(name)
    if result:
     result=os.path.join(p,result)
     break
  return result
