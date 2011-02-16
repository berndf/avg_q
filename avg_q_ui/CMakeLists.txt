# Copyright (C) 2006-2010 Bernd Feige
# 
# This file is part of avg_q.
# 
# avg_q is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# avg_q is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
PROJECT(avg_q_ui)

IF(WIN32)
 SET(GTKPATH ${CMAKE_SOURCE_DIR}/wingtk2)
 SET(GTK2_CFLAGS -I${GTKPATH}/include/gtk-2.0 -I${GTKPATH}/include/gdk-pixbuf-2.0 -I${GTKPATH}/include/atk-1.0 -I${GTKPATH}/include/cairo -I${GTKPATH}/include/pango-1.0 -I${GTKPATH}/include/glib-2.0)
 SET(GTK2_LIBRARIES -lgtk-win32-2.0.dll -lgdk-win32-2.0.dll -latk-1.0.dll
  -lgdk_pixbuf-2.0.dll -lm -lpango-1.0.dll -lcairo.dll -lgobject-2.0.dll -lgmodule-2.0.dll -lglib-2.0.dll)
 IF(${WIN32_THREADED})
  SET(GTHREAD_CFLAGS -I${GTKPATH}/include/glib-2.0)
  SET(USE_THREADING -DUSE_THREADING)
  SET(GTHREAD_LIBRARIES -lgthread-2.0.dll -lglib-2.0.dll)
 ELSE(${WIN32_THREADED})
  SET(GTHREAD_CFLAGS)
  SET(GTHREAD_LIBRARIES)
  SET(USE_THREADING)
 ENDIF(${WIN32_THREADED})
 LINK_DIRECTORIES(${GTKPATH}/lib)
ELSE(WIN32)
 IF(${UNIX_THREADED})
  SET(USE_THREADING -DUSE_THREADING)
 ELSE(${UNIX_THREADED})
  SET(USE_THREADING)
 ENDIF(${UNIX_THREADED})
INCLUDE(FindPkgConfig)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
pkg_check_modules(GTHREAD REQUIRED gthread-2.0)

#message(STATUS "inc: -${GTK2_INCLUDE_DIRS}")
#message(STATUS "link libraries: -${GTK2_LIBRARIES}")
#message(STATUS "cflags: -${GTK2_CFLAGS}")
#message(STATUS "link dir: -${GTK2_LINK_DIR}")
ENDIF(WIN32)

ADD_DEFINITIONS(${GTK2_CFLAGS} -DGTK_DISABLE_SINGLE_INCLUDES -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE)
ADD_DEFINITIONS(${GTHREAD_CFLAGS} ${USE_THREADING})

ADD_LIBRARY(vogl_vgui STATIC vogl_vgui_driver.c)
ADD_LIBRARY(dummy_vogl_vgui STATIC dummy_vogl_vgui_driver.c)

ADD_EXECUTABLE(avg_q_ui avg_q_ui.c)
TARGET_LINK_LIBRARIES(avg_q_ui ${BF_LIBS_VGUI} ${GTK2_LIBRARIES}
 ${GTHREAD_LIBRARIES})

IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
 ADD_EXECUTABLE(avg_q_ui1 avg_q_ui.c avg_q_dump.h)
 SET_TARGET_PROPERTIES(avg_q_ui1 PROPERTIES 
  COMPILE_FLAGS "-DSTANDALONE"
 )
 TARGET_LINK_LIBRARIES(avg_q_ui1 ${BF_LIBS_VGUI} ${GTK2_LIBRARIES}
  ${GTHREAD_LIBRARIES})
ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
IF(WIN32)
 ADD_CUSTOM_COMMAND(
  TARGET avg_q_ui
  TARGET avg_q_ui1
  PRE_LINK
  COMMAND ${WINDRES} ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_ui.rc ${CMAKE_CURRENT_BINARY_DIR}/avg_q_rc.o
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
 )
 SET_TARGET_PROPERTIES(avg_q_ui
  PROPERTIES LINK_FLAGS "-mwindows ${CMAKE_CURRENT_BINARY_DIR}/avg_q_rc.o"
 )
 IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
  SET_TARGET_PROPERTIES(avg_q_ui1
   PROPERTIES LINK_FLAGS "-mwindows ${CMAKE_CURRENT_BINARY_DIR}/avg_q_rc.o"
  )
 ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
ENDIF(WIN32)

INSTALL(
 TARGETS avg_q_ui
 DESTINATION ${INSTALLDIR}
 PERMISSIONS ${EXECUTABLE_PERMISSIONS}
)
