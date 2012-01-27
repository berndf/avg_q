# Copyright (C) 2006-2008 Bernd Feige
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
PROJECT(mfxlib)

IF(CMAKE_SYSTEM_NAME STREQUAL Windows)
 SET(BUILD_SHARED_LIBS OFF)
ELSE(CMAKE_SYSTEM_NAME STREQUAL Windows)
 EXECUTE_PROCESS(COMMAND /bin/uname -m
  OUTPUT_VARIABLE UNAME
 )
 IF(UNAME MATCHES "x86_64")
  SET(OPTIMIZEFLAGS -march=athlon64 -O4 -Wall -s)
 ELSE(UNAME MATCHES "x86_64")
  SET(OPTIMIZEFLAGS -march=i686 -O4 -Wall -s)
 ENDIF(UNAME MATCHES "x86_64")
 SET(ADD_LIBS)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL Windows)

SET(sources
 mfx_file.c mfx_errors.c mfx_infoout.c Intel_compat.c fix_headers.c read_struct.c
)
SET(programs
 mfxinfo mfxtriggers mfxcleartrigs mfxwritetrigs
)
ADD_LIBRARY(mfx ${sources})
SET_TARGET_PROPERTIES(mfx
 PROPERTIES VERSION 2.0 SOVERSION 2
)

SET(DEBUGFLAGS -g -Wall)
SET(MFXLIBS mfx m ${ADD_LIBS})

ADD_DEFINITIONS(${OPTIMIZEFLAGS})
INCLUDE_DIRECTORIES(.)

FOREACH(prog ${programs})
 ADD_EXECUTABLE(${prog} ${prog}.c)
 TARGET_LINK_LIBRARIES(${prog} ${MFXLIBS})
ENDFOREACH(prog)

IF(BUILD_SHARED_LIBS)
 SET(INSTALL_TARGETS mfx ${programs})
ELSE(BUILD_SHARED_LIBS)
 SET(INSTALL_TARGETS ${programs})
ENDIF(BUILD_SHARED_LIBS)
INSTALL(
 TARGETS ${INSTALL_TARGETS}
 DESTINATION ${INSTALLDIR}
 RUNTIME PERMISSIONS ${EXECUTABLE_PERMISSIONS}
 LIBRARY PERMISSIONS ${FILE_PERMISSIONS}
)
