# The Python launcher tries to interpret #! lines...
##!/bin/env python

# Set the path to the avg_q python installation here if you
# didn't add it to the global python path
import sys
sys.path.append("R:/avg_q/python")

import avg_q

# The avg_q_ui.exe binary will be found automatically in the directory
# above python/
a=avg_q.avg_q(avg_q='avg_q_ui')

import glob
# Iterate over all files with extension .edf
for infile in glob.glob('N:/*.edf'):
 # avg_q.Epochsource normally takes an avg_q_file() descriptor as first
 # argument but for convenience also a file name with type inferred 
 # from the extension
 # Read a single 2-s epoch from the start of the file
 epochsource=avg_q.Epochsource(infile,'1s','1s',continuous=True,epochs=1)
 script=avg_q.Script(a)
 script.add_Epochsource(epochsource)
 script.add_transform('''
query -N sfreq stdout
query -N nr_of_points stdout
posplot''')
 # avg_q.Script() sets the collect method to null_sink automatically
 script.run()

a.close()
