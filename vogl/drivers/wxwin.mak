# Generated automatically from Makefile.in by configure.
#
# Makefile for the WX library
#
# Author: Xiaokun Zhu
# Date: 
# Modified: 30 Apr. 1996
#
# Generated automatically from Makefile.in by configure.

#
# set variables for make.tmpl
#
local_inc = -I. -I../src -I/usr/local/include
local_lib = -L/usr/local/local -L/usr/local/lib -L$(HOME)/work/mfx -L$(HOME)/work/bf -L$(HOME)/lib -L/usr/X11R6/lib -lbf -lmfx -lvogl -lX11 -lm
top_srcdir = /usr/local/wxwin
#
# use make.tmpl
#
include $(top_srcdir)/make.tmpl

#
# define source
#
CXXSRCS = 
CSRCS=
local_obj = wxwin.$(OBJSUFF)
doc_src =
doc_dest =
PROG =
DEMOS= 
DEMOSRCS = wxwin.$(SRCSUFF)
lib_name =
GUI_OBJ=obj${GUI_SUFFIX}
#
# use make.rule
#
include $(top_srcdir)/make.rule

#
# define document
#
PSNAME=
TXTNAME=
HTMLNAME=

#
# default rules
#
all:: demo

.PHONY: demo
demo:: GUI_DEMODIR $(DEMOOBJS) $(DEMOPROGS)

ps::	$(PSNAME)

txt::	$(TXTNAME)

html::	$(HTMLNAME)

install::

clean::
	rm -f $(OBJS) $(DEMOOBJS) $(DEMOPROGS) *~ core *.bak

distclean:: clean

depend:: ${SRCS}
	makedepend -p$(obj_dest)/ -- ${CXXFLAGS} -- ${SRCS} 

