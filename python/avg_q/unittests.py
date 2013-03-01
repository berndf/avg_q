# Copyright (C) 2011-2012 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
avg_q unit testing.
Usage example:

from avg_q.unittests import avg_q_test_case

class avg_q_test_case1(avg_q_test_case):
 script='xx'
 expected_output='yy'

import unittest
unittest.main()
"""

import unittest
from . import avg_q

class avg_q_test_case(unittest.TestCase):
 script=None
 expected_output=None

 def setUp(self):
  self.avg_q_instance = avg_q()

 def tearDown(self):
  self.avg_q_instance.close()
  self.avg_q_instance = None

 # This getResult method can be overridden;
 # The method implemented here or simply sends self.script to avg_q.
 # In any case, self.expected_output must be set.
 def getResult(self):
  self.avg_q_instance.write(self.script)
  output=[]
  for line in self.avg_q_instance:
   output.append(line)
  return output

 def runTest(self):
  # Catch unittest running the base class as a test since it has runTest()...
  if self.__class__.__name__=='avg_q_test_case':
   return
  result=self.getResult()
  self.assertEqual('\n'.join(result), self.expected_output)

 def outputResult(self):
  self.avg_q_instance.write(self.script)
  for line in self.avg_q_instance:
   print(line)
