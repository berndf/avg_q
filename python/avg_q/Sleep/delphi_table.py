# Copyright (C) 2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

"""
Delphi table storage format - serialized into lines, row-fastest, with empty row separator lines.
"""

class delphi_table(list):
 def read_file(self,filename):
  with open(filename,"rb") as infile:
   next(infile)
   self.ncol=int(next(infile))+1
   next(infile)
   self.nrow=int(next(infile))

   for col in range(self.ncol):
    for row in range(self.nrow):
     l=next(infile).rstrip(b"\r\n").decode('latin1')
     if col==0:
      self.append([l])
     else:
      self[row].append(l)
    next(infile)

 def write_file(self,filename):
  with open(filename,"wb") as outfile:
   outfile.write(("0\r\n%d\r\n0\r\n%d\r\n" % (self.ncol-1,self.nrow)).encode('latin1'))

   for col in range(self.ncol):
    for row in range(self.nrow):
     outfile.write(("%s\r\n" % self[row][col]).encode('latin1'))
    outfile.write(b"\r\n")

if __name__ == '__main__':
 import sys
 for filename in sys.argv[1:]:
  a=delphi_table()
  a.read_file(filename)
  print(a)
  #a.write_file('out.mkk')
