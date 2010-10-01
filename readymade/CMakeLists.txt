# Copyright (C) 2006,2007 Bernd Feige
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
