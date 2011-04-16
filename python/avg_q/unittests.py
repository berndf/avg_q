#!/bin/env python

# avg_q unit testing. Note that many tests will create files in the
# current directory, so it's best to start this from some work directory
# by 'python -m avg_q.unittests'

import unittest
import avg_q

class avg_q_test_case(unittest.TestCase):
 script=None
 expected_output=None

 def setUp(self):
  self.avg_q_instance = avg_q.avg_q()

 def tearDown(self):
  self.avg_q_instance.close()
  self.avg_q_instance = None

 # This getResult method can be overridden, or simply self.script set.
 # In any case, self.expected_output must be set.
 def getResult(self):
  self.avg_q_instance.write(self.script)
  output=[]
  for line in self.avg_q_instance:
   output.append(line)
  return output

 def runTest(self):
  if self.getResult.__func__ is avg_q_test_case.getResult.__func__ and self.script is None:
   # Catch unittest running the base class as a test since it has runTest()...
   return
  result=self.getResult()
  self.assertEqual('\n'.join(result), self.expected_output)

 def outputResult(self):
  self.avg_q_instance.write(self.script)
  for line in self.avg_q_instance:
   print(line)

# CNT files, triggers: Generate CNT data
class avg_q_test_case1(avg_q_test_case):
 script='''
dip_simulate 100 5 1s 1s eg_source
set trigger 500ms:5
set trigger 1500ms:1
write_synamps -c o.cnt 1
query -N triggers
null_sink
-
'''
 expected_output='''triggers=File position: 0
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
150 1'''

# Read CNT file - Trigger lists
class avg_q_test_case2(avg_q_test_case):
 script='''
read_synamps -t 1,3,7,9,5 o.cnt 100ms 400ms 
query -N condition
query -N accepted_epochs
null_sink
-
'''
 expected_output='''condition=5
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
accepted_epochs=9'''

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
 expected_output='''4.86859	962
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
1.97253	962'''

# collapse_channels
class avg_q_test_case4(avg_q_test_case):
 script='''
read_synamps o.cnt 100ms 400ms 
collapse_channels A1-A15:A1_to_A15 A16-A37:A16_to_A37
average
Post:
trim -x 0 0
write_generic -N stdout string
-
'''
 expected_output='''A1_to_A15	A16_to_A37
4.69333	-2.21818'''

# scale_by: Test behavior when no channel is selected 
class avg_q_test_case5(avg_q_test_case):
 script='''
read_synamps o.cnt 100ms 400ms 
scale_by -n ?ECG,EDA,other_unknown_channel 0
average
Post:
trim -x 0 0
write_generic -N stdout string
-
'''
 expected_output='''A1	A2	A3	A4	A5	A6	A7	A8	A9	A10	A11	A12	A13	A14	A15	A16	A17	A18	A19	A20	A21	A22	A23	A24	A25	A26	A27	A28	A29	A30	A31	A32	A33	A34	A35	A36	A37
0.5	-3.4	7.7	12.9	13.9	-8.8	-7.2	-1.3	4	9.2	10.9	12.7	19.2	20.7	-20.6	-20.2	-10.5	-6.4	-4.2	-0.4	1.5	3.9	5.9	6.7	7.1	8.1	10	11.8	7.3	-10	-19.4	-15	-9.5	-6.3	-4.2	-3	-2'''

# Measure module
class avg_q_test_case6(avg_q_test_case):
 def getResult(self):
  import avg_q.Measure
  epochsource=avg_q.Epochsource("o.cnt",'100ms','1s')
  epochsource.set_trigpoints(['2s','5s'])
  M=avg_q.Measure.Measure_Script(self.avg_q_instance)
  M.add_Epochsource(epochsource)
  result=M.measure([['A17',[[90,120],[150,180]]],['A10',[[370,550]]]])
  return [str(x) for x in result]
 expected_output='''[1, 'A17', '90 120', -13.5, 111.667]
[1, 'A17', '150 180', -5.5, 161.923]
[1, 'A10', '370 550', 9.05263, 445.968]
[2, 'A17', '90 120', 3.25, 100.286]
[2, 'A17', '150 180', -25.75, 165.049]
[2, 'A10', '370 550', 12.1053, 468.944]'''

if __name__ == '__main__':
 import sys
 if len(sys.argv)>1:
  # Output the result of avg_q running the given test
  for testname in sys.argv[1:]:
   t=eval('avg_q_test_case%s()' % testname)
   t.setUp()
   print("\n".join(t.getResult()))
   t.tearDown()
 else:
  unittest.main()
