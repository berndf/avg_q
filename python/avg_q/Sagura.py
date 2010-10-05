# Copyright (C) 2008,2009 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
Sagura utilities.
"""

import configparser

class SaguraFile(object):
 def __init__(self,filename):
  if filename.upper().endswith('.INI'):
   self.ini=configparser.RawConfigParser()
   self.ini.read(filename)
   self.Patient={}
   for key,val in self.ini.items('Patient'):
    self.Patient[key]=val
