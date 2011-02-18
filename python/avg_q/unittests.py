#!/bin/env python

# avg_q unit testing. Note that many tests will create files in the
# current directory, so it's best to start this from some work directory
# by 'python -m avg_q.unittests'

import unittest
import avg_q

class avg_q_test_case(unittest.TestCase):
 script=None
 expectedlines=None

 def setUp(self):
  self.avg_q_object = avg_q.avg_q()

 def tearDown(self):
  self.avg_q_object.close()
  self.avg_q_object = None

 def runTest(self):
  # unittest will try to execute this base class since it has runTest()...
  if self.script==None:
   return
  self.avg_q_object.write(self.script)
  for line in self.avg_q_object:
   self.assertEqual(line, next(self.expectedlines))

 def outputResult(self):
  self.avg_q_object.write(self.script)
  for line in self.avg_q_object:
   print(line)

# CNT files, triggers: Generate CNT data
class avg_q_test_case1(avg_q_test_case):
 script='''
dip_simulate 100 5 1s 1s eg_source
!echo -F stdout End of script\\n
set trigger 500ms:5
set trigger 1500ms:1
write_synamps -c o.cnt 1
query -N triggers
null_sink
-
'''
 expectedlines=iter('''triggers=File position: 0
triggers=50 5
150 1
triggers=File position: 200
triggers=50 5
150 1
triggers=File position: 400
triggers=50 5
150 1
triggers=File position: 600
triggers=50 5
150 1
triggers=File position: 800
triggers=50 5
150 1'''.splitlines())

# Read CNT file - Trigger lists
class avg_q_test_case2(avg_q_test_case):
 script='''
read_synamps -t 1,3,7,9,5 o.cnt 100ms 400ms 
query -N condition
query -N accepted_epochs
null_sink
-
'''
 expectedlines=iter('''condition=5
accepted_epochs=0
condition=1
accepted_epochs=1
condition=5
accepted_epochs=2
condition=1
accepted_epochs=3
condition=5
accepted_epochs=4
condition=1
accepted_epochs=5
condition=5
accepted_epochs=6
condition=1
accepted_epochs=7
condition=5
accepted_epochs=8
condition=1
accepted_epochs=9'''.splitlines())

# Sub-scripts, fftspect, averaging
class avg_q_test_case3(avg_q_test_case):
 script='''
read_synamps o.cnt 100ms 400ms 
>fftspect 0 1 1
read_synamps -f 3 o.cnt 100ms 400ms 
>set nrofaverages 2
>fftspect 0 1 1
average -M -W
Post:
collapse_channels
write_generic stdout string
-
'''
 expectedlines=iter('''4.86859	962
32.0764	962
67.6091	962
72.8508	962
2.28588	962
2.19235	962
1.94942	962
2.09882	962
2.1839	962
2.58577	962
1.56762	962
2.46027	962
1.70455	962
1.14678	962
2.22797	962
1.97253	962'''.splitlines())

# collapse_channels
class avg_q_test_case4(avg_q_test_case):
 script='''
read_synamps o.cnt 100ms 400ms 
collapse_channels A1-A15:A1bisA15 A16-A37:A16bisA37
average
Post:
trim -x 0 0
write_generic -N stdout string
echo -F stdout End of script\\n
-
-
'''
 expectedlines=iter('''A1bisA15	A16bisA37
4.69333	-2.21818'''.splitlines())

if __name__ == '__main__':
 import sys
 if len(sys.argv)>1:
  # Output the result of avg_q running the given test
  for testname in sys.argv[1:]:
   t=eval('avg_q_test_case%s()' % testname)
   t.setUp()
   t.outputResult()
   t.tearDown()
 else:
  unittest.main()
