#!/usr/bin/env python
"""\
SVG.py - Construct/display SVG scenes.

The following code is a lightweight wrapper around SVG files. The metaphor
is to construct a scene, add objects to it, and then write it to a file
to display it.

This program uses ImageMagick to display the SVG files. ImageMagick also
does a remarkable job of converting SVG files into other formats.

http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/325823
"""

import os
display_prog = 'display' # Command to execute to display images.

class Scene:
    def __init__(self,name="svg",height=400,width=400):
        self.name = name
        self.items = []
        self.height = height
        self.width = width
        return

    def add(self,item): self.items.append(item)

    def strarray(self):
        var = ["<?xml version=\"1.0\"?>\n",
               "<svg height=\"%d\" width=\"%d\" >\n" % (self.height,self.width),
               " <g style=\"fill-opacity:1.0; stroke:black;\n",
               "  stroke-width:1;\">\n"]
        for item in self.items: var += item.strarray()
        var += [" </g>\n</svg>\n"]
        return var

    def write_svg(self,filename=None):
        if filename:
            self.svgname = filename
        else:
            self.svgname = self.name + ".svg"
        svgfile = open(self.svgname,'w')
        svgfile.writelines(self.strarray())
        svgfile.close()
        return

    def display(self,prog=display_prog):
        os.system("%s %s" % (prog,self.svgname))
        return


class Line:
    def __init__(self,start,end):
        self.start = start #xy tuple
        self.end = end     #xy tuple
        return

    def strarray(self):
        return ["  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n" %\
                (self.start[0],self.start[1],self.end[0],self.end[1])]

class Path:
    def __init__(self,coords,color):
        self.coords=coords
        self.color=color
        return

    def strarray(self):
        toxy=lambda xy: "%d,%d" % (xy[0],xy[1])
        return [
'''<path style="fill:none;stroke:%s"
d="%s" />
''' % (colorstr(self.color),"M " + toxy(self.coords[0]) + " L " + "L ".join([toxy(x) for x in self.coords[1:]]))]

class Circle:
    def __init__(self,center,radius,color):
        self.center = center #xy tuple
        self.radius = radius #xy tuple
        self.color = color   #rgb tuple in range(0,256)
        return

    def strarray(self):
        return ["  <circle cx=\"%d\" cy=\"%d\" r=\"%d\"\n" %\
                (self.center[0],self.center[1],self.radius),
                "    style=\"fill:%s;\"  />\n" % colorstr(self.color)]

class Rectangle:
    def __init__(self,origin,height,width,color):
        self.origin = origin
        self.height = height
        self.width = width
        self.color = color
        return

    def strarray(self):
        return ["  <rect x=\"%d\" y=\"%d\" height=\"%d\"\n" %\
                (self.origin[0],self.origin[1],self.height),
                "    width=\"%d\" style=\"fill:%s;\" />\n" %\
                (self.width,colorstr(self.color))]

class Text:
    def __init__(self,origin,text,size=24,color=(0,0,0)):
        self.origin = origin
        self.text = text
        self.size = size
        self.color = color
        return

    def strarray(self):
        return ["  <text x=\"%d\" y=\"%d\" font-size=\"%d\"\n" %\
                (self.origin[0],self.origin[1],self.size),
		"   style=\"fill:%s;stroke:none\">\n" % colorstr(self.color),
                "   %s\n" % self.text,
                "  </text>\n"]


def colorstr(rgb): return "#%02x%02x%02x" % (rgb[0],rgb[1],rgb[2])

def test():
    scene = Scene('test')
    scene.add(Rectangle((100,100),200,200,(0,255,255)))
    scene.add(Line((200,200),(200,300)))
    scene.add(Line((200,200),(300,200)))
    scene.add(Line((200,200),(100,200)))
    scene.add(Line((200,200),(200,100)))
    scene.add(Circle((200,200),30,(0,0,255)))
    scene.add(Circle((200,300),30,(0,255,0)))
    scene.add(Circle((300,200),30,(255,0,0)))
    scene.add(Circle((100,200),30,(255,255,0)))
    scene.add(Circle((200,100),30,(255,0,255)))
    scene.add(Text((50,50),"Testing SVG"))
    scene.write_svg()
    scene.display()
    return

if __name__ == '__main__':
 test()
