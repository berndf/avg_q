# Copyright (C) 2006-2008,2011,2012,2025,2026 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

PROJECT(mfxlib LANGUAGES C)

SET(ADD_LIBS)

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
