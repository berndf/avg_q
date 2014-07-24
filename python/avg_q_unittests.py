#!/usr/bin/env python
# Copyright (C) 2011-2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

# avg_q unit testing. Note that many tests will create files in the
# current directory, so it's best to start this from some work directory.

from avg_q.unittests import avg_q_test_case

# CNT files, triggers: Generate CNT data
class avg_q_test_case1(avg_q_test_case):
 script='''
dip_simulate 100 5 1s 1s eg_source
set trigger 500ms:5
set trigger 1500ms:1
write_synamps -c o.cnt 1
query -N triggers stdout
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
query -N condition stdout
query -N accepted_epochs stdout
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
 expected_output='''[1, 'A17', '90 120', -13.5, 94.4118]
[1, 'A17', '150 180', -5.5, 174.839]
[1, 'A10', '370 550', 9.05263, 453.279]
[2, 'A17', '90 120', 3.25, 94.8649]
[2, 'A17', '150 180', -25.75, 164.615]
[2, 'A10', '370 550', 12.1053, 465.367]'''

# get_description test
class avg_q_test_case7(avg_q_test_case):
 def getResult(self):
  import avg_q.avg_q_file
  infile=avg_q.avg_q_file("o.cnt")
  result=self.avg_q_instance.get_description(infile,('sfreq','points_in_file','comment'))
  return [str(x) for x in result]
 expected_output='''100.0
1000
Version 3.0,,,dip_simulate output,Unspecified'''

class avg_q_test_case8(avg_q_test_case):
 def getResult(self):
  import avg_q.avg_q_file
  import avg_q.Artifacts
  # Generate a file o.hdf with artifacts
  script=avg_q.Script(self.avg_q_instance)
  epochsource=avg_q.Epochsource(avg_q.avg_q_file(fileformat='null_source'),aftertrig='20s',epochs=1)
  epochsource.add_branchtransform('add gaussnoise 100')
  script.add_Epochsource(epochsource)
  # 1s Blocking artifact on channel 10
  epochsource=avg_q.Epochsource(avg_q.avg_q_file(fileformat='null_source'),aftertrig='1s',epochs=1)
  epochsource.add_branchtransform('add gaussnoise 100')
  epochsource.add_branchtransform('scale_by -n 10 0')
  script.add_Epochsource(epochsource)
  epochsource=avg_q.Epochsource(avg_q.avg_q_file(fileformat='null_source'),aftertrig='20s',epochs=1)
  epochsource.add_branchtransform('add gaussnoise 100')
  script.add_Epochsource(epochsource)
  # And a phasic event on channel 5
  epochsource=avg_q.Epochsource(avg_q.avg_q_file(fileformat='null_source'),aftertrig='20ms',epochs=1)
  # The height is important here - 2000 often triggers the jump detector as well
  epochsource.add_branchtransform('add -n 5 1900')
  script.add_Epochsource(epochsource)
  epochsource=avg_q.Epochsource(avg_q.avg_q_file(fileformat='null_source'),aftertrig='20s',epochs=1)
  epochsource.add_branchtransform('add gaussnoise 100')
  script.add_Epochsource(epochsource)
  script.set_collect('append')
  #script.add_postprocess('posplot')
  script.add_postprocess('write_hdf -c o.hdf')
  script.run()
  # Now do the artifact analysis
  infile=avg_q.avg_q_file('o.hdf')
  points_in_file=self.avg_q_instance.get_description(infile,'points_in_file')
  segmentation=avg_q.Artifacts.Artifact_Segmentation(self.avg_q_instance,infile,0,points_in_file)
  segmentation.collect_artifacts()
  #segmentation.show_artifacts()
  return [str(x) for x in segmentation.collected.artifacts]
 expected_output='''4100.0
(2001.0, 2100.0)'''

if __name__ == '__main__':
 import sys
 import unittest
 if len(sys.argv)>1:
  # Output the result of avg_q running the given test
  for testname in sys.argv[1:]:
   t=eval('avg_q_test_case%s()' % testname)
   t.setUp()
   print("\n".join(t.getResult()))
   t.tearDown()
 else:
  unittest.main()
