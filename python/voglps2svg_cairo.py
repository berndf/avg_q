#!/usr/bin/env python
# Copyright (C) 2008-2012,2017 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

import sys
import voglps
import cairo

# Derived class with cairo plotting operators
class cairovogl(voglps.voglps):
 def __init__(self,psfile,surface,width,height):
  voglps.voglps.__init__(self,psfile)
  self.ctx = cairo.Context(surface)
  self.ctx.set_font_size(20)
  self.width=width
  self.height=height
 def moveto(self,x,y):
  self.ctx.move_to(x,self.height-y)
 def lineto(self,x,y):
  self.ctx.line_to(x,self.height-y)
 def stroke(self):
  self.ctx.stroke()
 def text(self,text):
  self.ctx.show_text(text)
 def color(self,r,g,b):
  self._stroke()
  self.ctx.set_source_rgb(r,g,b)

# These are the postscript coordinates
WIDTH, HEIGHT  = 2700/voglps.scale_factor, 2000/voglps.scale_factor

if __name__ == '__main__':
 for psfile in sys.argv[1:]:
  svgfile=psfile.replace(".ps",".svg")
  print(svgfile)
  surface = cairo.SVGSurface(svgfile, WIDTH, HEIGHT)
  #v=voglps.voglps(psfile)
  v=cairovogl(psfile,surface,WIDTH,HEIGHT)
  #ctx.scale (WIDTH/1.0, HEIGHT/1.0)
  v.parse()
  #ctx.rel_line_to(1000,1000)
  #ctx.line_to(0,1000)
  #ctx.set_source_rgb(0,1,0)
  #ctx.stroke()
  surface.finish()
