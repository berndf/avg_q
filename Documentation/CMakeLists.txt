# Copyright (C) 2009,2024-2026 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

PROJECT(avg_q_Documentation)

FIND_PROGRAM(_LYX_EXECUTABLE lyx)
#message(STATUS "LyX: ${_LYX_EXECUTABLE}")

IF(NOT ${_LYX_EXECUTABLE} STREQUAL "_LYX_EXECUTABLE-NOTFOUND")
 SET(Programs_handbook_pdf ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.pdf)
 SET(Programs_handbook_lyx ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.lyx)
 # OUTPUT-based custom command: make checks at *build* time whether the
 # PDF is missing or older than the .lyx source, and only then reruns LyX.
 ADD_CUSTOM_COMMAND(
  OUTPUT ${Programs_handbook_pdf}
  COMMAND ${_LYX_EXECUTABLE} -e pdf4 ${Programs_handbook_lyx}
  DEPENDS ${Programs_handbook_lyx}
  COMMENT "Running LyX"
 )
 ADD_CUSTOM_TARGET(Programs_handbook ALL DEPENDS ${Programs_handbook_pdf})

 IF(CMAKE_BUILD_TYPE STREQUAL Release)
  INSTALL(
   FILES ${CMAKE_CURRENT_SOURCE_DIR}/Programs_handbook.pdf
   DESTINATION ${INSTALLDIR}
   PERMISSIONS ${FILE_PERMISSIONS}
  )
 ENDIF(CMAKE_BUILD_TYPE STREQUAL Release)
ENDIF(NOT ${_LYX_EXECUTABLE} STREQUAL "_LYX_EXECUTABLE-NOTFOUND")
