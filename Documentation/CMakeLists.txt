# Copyright (C) 2009 Bernd Feige
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
PROJECT(avg_q_Documentation)

FIND_PROGRAM(_LYX_EXECUTABLE lyx)
#message(STATUS "LyX: ${_LYX_EXECUTABLE}")

IF(NOT ${_LYX_EXECUTABLE} STREQUAL "_LYX_EXECUTABLE-NOTFOUND")
 SET(Programs_handbook_pdf ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.pdf)
 SET(Programs_handbook_lyx ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.lyx)
 # O well, this is only checked at configuration time so it's no use...
 #IF (NOT EXISTS ${Programs_handbook_pdf} OR ${Programs_handbook_lyx} IS_NEWER_THAN ${Programs_handbook_pdf})
  ADD_CUSTOM_TARGET(Programs_handbook ALL)
  ADD_CUSTOM_COMMAND(
   TARGET Programs_handbook
   #OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.pdf
   COMMAND ${_LYX_EXECUTABLE} -e pdf2 ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.lyx
   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.lyx
   COMMENT "Running LyX"
  )
 #ENDIF (NOT EXISTS ${Programs_handbook_pdf} OR ${Programs_handbook_lyx} IS_NEWER_THAN ${Programs_handbook_pdf})

 IF(CMAKE_BUILD_TYPE STREQUAL Release)
  INSTALL(
   FILES ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.pdf
   DESTINATION ${INSTALLDIR}
   PERMISSIONS ${FILE_PERMISSIONS}
  )
 ENDIF(CMAKE_BUILD_TYPE STREQUAL Release)
ENDIF(NOT ${_LYX_EXECUTABLE} STREQUAL "_LYX_EXECUTABLE-NOTFOUND")
