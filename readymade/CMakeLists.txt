# Copyright (C) 2006,2007 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

SET(programs
 ascaverage asccalc swap_xz
 #spline_gridder distance_correct
)

FOREACH(prog ${programs})
 ADD_EXECUTABLE(${prog} ${prog}.c)
 TARGET_LINK_LIBRARIES(${prog} ${BF_LIBS})
ENDFOREACH(prog)

INSTALL(
 TARGETS ${programs}
 DESTINATION ${INSTALLDIR}
 PERMISSIONS ${EXECUTABLE_PERMISSIONS}
)
