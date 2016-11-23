# Copyright (C) 2008-2012 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

# Base class for parsing the very simple VOGL .ps output

scale_factor=1.9 # Divide all coordinates by this to avoid overflowing oodraw's coordinates

class voglps(object):
 def __init__(self,psfile):
  self.infile=open(psfile,"r")
  #self.color=(1,0,0)
  self.linestarted=False
 def _stroke(self):
  if self.linestarted:
   self.stroke()
   self.linestarted=False
 def _moveto(self,x,y):
  self._stroke()
  self.moveto(float(x),float(y))
 def _lineto(self,x,y):
  self.lineto(x,y)
  self.linestarted=True
 def _text(self,text):
  if text.strip()!='':
   self.text(text)
 def _color(self,r,g,b):
  # Translate white to black:
  if r==1 and g==1 and b==1:
   r,g,b=0,0,0
  self.color(r,g,b)
 # vvv Derived class interface vvv
 def stroke(self):
  print("Stroke")
 def moveto(self,x,y):
  print("%g %g" % (x,y))
 def lineto(self,x,y):
  print("-> %g %g" % (x,y))
 def text(self,text):
  print(text)
 def color(self,r,g,b):
  print("Color %g %g %g" % (r,g,b))
 # ^^^ Derived class interface ^^^
 def parse(self):
  for line in self.infile:
   line=line.rstrip('\r\n')
   if line.endswith(" m"):
    x,y,o=line.split()
    self._moveto(float(x)/scale_factor,float(y)/scale_factor)
   elif line.endswith(" d"):
    x,y,o=line.split()
    self._lineto(float(x)/scale_factor,float(y)/scale_factor)
   elif line.endswith(" s"):
    text=line[1:-3]
    self._text(text)
   elif line.endswith(" c"):
    r,g,b,o=line.split()
    self._color(int(float(r)),int(float(g)),int(float(b)))
 def close(self):
  self._stroke()
  self.infile.close()
