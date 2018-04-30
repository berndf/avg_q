# Copyright (C) 2006-2014 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

PROJECT(avg_q)

OPTION(AVG_Q_BUILD_DEBUG "Build avg_q in debug mode" OFF)
# Note that setting this in ccmake doesn't work, pkg_check_modules runs too
# early and is cached... Use "cmake -DAVG_Q_GTK3=Off" instead.
OPTION(AVG_Q_GTK3 "Build avg_q_ui against GTK3" On)

IF(AVG_Q_BUILD_DEBUG)
 SET(CMAKE_BUILD_TYPE Debug)
ELSE(AVG_Q_BUILD_DEBUG)
 SET(CMAKE_BUILD_TYPE Release)
ENDIF(AVG_Q_BUILD_DEBUG)

cmake_minimum_required(VERSION 2.6)

INCLUDE("Signatures/Signature.GPL")

SET(CPACK_GENERATOR TBZ2)
SET(CPACK_PACKAGE_VERSION_MAJOR 6)
SET(CPACK_PACKAGE_VERSION_MINOR 0)
SET(CPACK_PACKAGE_VERSION_PATCH 0)
INCLUDE(CPack)
IF(CMAKE_SYSTEM_NAME MATCHES Windows)
 SET(BUILD_SHARED_LIBS OFF)
 SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
 SET(CMAKE_EXECUTABLE_SUFFIX ".exe")
 SET(INSTALLDIR "bin")
 SET(VOGL_LIBS vogl)
 SET(HDF_INCLUDES 
  ${CMAKE_SOURCE_DIR}/winlibs64/HDF/hdf-4.2.10/hdf/src
  ${CMAKE_SOURCE_DIR}/winlibs64/HDF/hdf-4.2.10/mfhdf/libsrc
  ${CMAKE_SOURCE_DIR}/winlibs64/HDF/hdf)
 SET(HDF_LIBS mfhdf hdf xdr jpeg z ws2_32) # Without ws2_32, "undefined reference to `_htonl@4'" results
 #SET(HDF_DEFINES -D_DLL) # Use the multithreaded DLLs
 SET(HDF_DEFINES -U_MT) # Disable multithreading for static linking
 SET(LAPACK_FOUND TRUE) # On Windows, set this to FALSE manually if lapack libraries are not available
 SET(ICA_LIBS lapack blas f2c)
 SET(AVG_Q_DEFINES -DLITTLE_ENDIAN ${HDF_DEFINES})
 IF(CMAKE_SYSTEM_VARIANT STREQUAL win64)
  SET(HDF_LIBPATH
   /home/charly/tmp/Infos/Computer/wingtk3/usr/x86_64-w64-mingw32/sys-root/mingw/lib)
  SET(C_FLAGS_ARCHITECTURE "-march=athlon64 -no-pie")
  SET(ICA_INCLUDES ${CMAKE_SOURCE_DIR}/winlibs64/libf2c)
  SET(ICA_LIBPATH "${CMAKE_SOURCE_DIR}/winlibs64")
  SET(SOX_INCLUDES ${CMAKE_SOURCE_DIR}/winlibs64/sox/src)
 ELSE(CMAKE_SYSTEM_VARIANT STREQUAL win64)
  SET(HDF_LIBPATH
   /home/charly/tmp/Infos/Computer/wingtk3/usr/i686-w64-mingw32/sys-root/mingw/lib)
  SET(C_FLAGS_ARCHITECTURE "-march=i686 -no-pie")
  SET(ICA_INCLUDES ${CMAKE_SOURCE_DIR}/winlibs32/libf2c)
  SET(ICA_LIBPATH "${CMAKE_SOURCE_DIR}/winlibs32")
  SET(SOX_INCLUDES ${CMAKE_SOURCE_DIR}/winlibs32/sox/src)
 ENDIF(CMAKE_SYSTEM_VARIANT STREQUAL win64)
ELSE(CMAKE_SYSTEM_NAME MATCHES Windows)
 IF(CMAKE_BUILD_TYPE STREQUAL Debug)
  SET(BUILD_SHARED_LIBS OFF)
 ELSE(CMAKE_BUILD_TYPE STREQUAL Debug)
  SET(BUILD_SHARED_LIBS ON)
 ENDIF(CMAKE_BUILD_TYPE STREQUAL Debug)
 SET(X11_LIBS X11)
 SET(VOGL_LIBS vogl ${X11_LIBS})
 SET(HDF_INCLUDES "/usr/include/hdf")
 SET(HDF_LIBS mfhdf df jpeg z)
 SET(HDF_LIBPATH)
 #SET(CMAKE_INSTALL_PREFIX /usr/local/) # Set in signature file
 EXECUTE_PROCESS(COMMAND ${CMAKE_UNAME} -m
  OUTPUT_VARIABLE UNAME
 )
 INCLUDE(FindLAPACK)
 IF(LAPACK_FOUND)
  SET(ICA_LIBS ${LAPACK_LIBRARIES})
  SET(ICA_LIBPATH )
 ENDIF(LAPACK_FOUND)
 SET(AVG_Q_DEFINES -DLITTLE_ENDIAN)
 IF(UNAME MATCHES "x86_64")
  SET(INSTALLDIR distrib/avg_q64)
  SET(C_FLAGS_ARCHITECTURE "-march=athlon64")
 ELSE(UNAME MATCHES "x86_64")
  SET(INSTALLDIR distrib/avg_q32)
  # -D_FILE_OFFSET_BITS=64 is necessary to have working fstat() on CIFS mounts
  SET(C_FLAGS_ARCHITECTURE "-march=i686")
 ENDIF(UNAME MATCHES "x86_64")
ENDIF(CMAKE_SYSTEM_NAME MATCHES Windows)

# NOTE Don't include -ffast-math because it screws up NaN handling (e.g. in
# recode, specifically anything that tests for NaN)
SET(C_FLAGS_COMMON "${C_FLAGS_ARCHITECTURE} -O4 -fomit-frame-pointer -Wall -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline")

INCLUDE(FindOpenMP)
IF(OPENMP_FOUND)
 SET(C_FLAGS_COMMON "${C_FLAGS_COMMON} ${OpenMP_C_FLAGS}")
ENDIF(OPENMP_FOUND)

# GCC 5 defaults to -std=gnu11, GCC 4 not. This is necessary for loop variable
# declaration within 'for' loops. The option only makes sense for C, not C++...
SET(CMAKE_C_FLAGS_RELEASE "-std=gnu11 ${C_FLAGS_COMMON}")
SET(CMAKE_CXX_FLAGS_RELEASE ${C_FLAGS_COMMON})

# Permissons to be set on installed files
SET(EXECUTABLE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
SET(FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)


# vvvvvvvvvvvvvvv Library definitions and feature configuration. Look here when porting! vvvvvvvvvvvvvvv

SET(MFX_LIBS mfx)

INCLUDE(FindPkgConfig)
pkg_check_modules(SOX sox)

# For now, set this to FALSE manually if you don't have HDF4 libraries
# installed...
SET(HDF_FOUND TRUE)

# Set these to FALSE manually if you don't want posplot or the GTK GUI avg_q_ui to be built.
# This can be commendable to get a version of avg_q running with minimal
# dependencies.
SET(WITH_POSPLOT TRUE)
SET(WITH_AVG_Q_UI TRUE)

if (NOT LAPACK_FOUND)
 MESSAGE("LAPACK libraries not found - compiling without icadecomp")
endif (NOT LAPACK_FOUND)
if (NOT SOX_FOUND)
 MESSAGE("SOX library not found - compiling without read_sound/write_sound")
endif (NOT SOX_FOUND)
if (NOT HDF_FOUND)
 MESSAGE("HDF library not found - compiling without read_hdf/write_hdf")
endif (NOT HDF_FOUND)
if (NOT WITH_POSPLOT)
 MESSAGE("Compiling without the posplot method.")
endif (NOT WITH_POSPLOT)
if (NOT WITH_AVG_Q_UI)
 MESSAGE("Not creating the avg_q_ui GUI.")
endif (NOT WITH_AVG_Q_UI)

SET(CMAKE_C_FLAGS_DEBUG "-g -Wall")
#SET(CMAKE_C_FLAGS_DEBUG -g -DMALLOC_TRACE)

SET(AVG_Q_DEFINES ${AVG_Q_DEFINES} -DTRACE -DFP_EXCEPTION -DFOR_AVG_Q)
SET(AVG_Q_INCLUDES ${CMAKE_SOURCE_DIR}/bf ${CMAKE_SOURCE_DIR}/bf/array
 ${CMAKE_SOURCE_DIR}/bf/biomag ${CMAKE_SOURCE_DIR}/mfx
 ${CMAKE_SOURCE_DIR}/bf/stat
 ${CMAKE_SOURCE_DIR}/bf/math ${CMAKE_SOURCE_DIR}/bf/optimize
 ${CMAKE_SOURCE_DIR}/bf/array ${CMAKE_SOURCE_DIR}/bf/biomag
)

# Arranges that shared libs are found in the same dir as the binary
SET(CMAKE_INSTALL_RPATH "$ORIGIN")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG -g)
SET(BF_LIBPATH_DYNAMIC)
SET(BF_LIBS bf ${MFX_LIBS} m)

if (LAPACK_FOUND)
 LIST(APPEND AVG_Q_INCLUDES ${ICA_INCLUDES})
 LIST(APPEND BF_LIBPATH_DYNAMIC ${ICA_LIBPATH})
 LIST(APPEND BF_LIBS ${ICA_LIBS})
endif (LAPACK_FOUND)

if (SOX_FOUND)
 LIST(APPEND AVG_Q_INCLUDES ${SOX_INCLUDES})
 LIST(APPEND BF_LIBS ${SOX_LIBRARIES})
endif (SOX_FOUND)

if (HDF_FOUND)
 LIST(APPEND AVG_Q_INCLUDES ${HDF_INCLUDES})
 LIST(APPEND BF_LIBPATH_DYNAMIC ${HDF_LIBPATH})
 LIST(APPEND BF_LIBS ${HDF_LIBS})
endif (HDF_FOUND)

if (WITH_POSPLOT)
 LIST(APPEND AVG_Q_INCLUDES ${CMAKE_SOURCE_DIR}/vogl/src)
 SET(BF_LIBS_VGUI ${BF_LIBS} ${VOGL_LIBS} vogl_vgui)
 ADD_LIBRARY(dummy_vogl_vgui STATIC avg_q_ui/dummy_vogl_vgui_driver.c)
 LIST(APPEND BF_LIBS ${VOGL_LIBS} dummy_vogl_vgui)
else (WITH_POSPLOT)
 SET(BF_LIBS_VGUI ${BF_LIBS})
endif (WITH_POSPLOT)

ADD_DEFINITIONS(${AVG_Q_DEFINES})
INCLUDE_DIRECTORIES(${AVG_Q_INCLUDES})
LINK_DIRECTORIES(${BF_LIBPATH_DYNAMIC})

# Imported libs:
ADD_SUBDIRECTORY(vogl)

ADD_SUBDIRECTORY(mfx)
ADD_SUBDIRECTORY(bf)
ADD_SUBDIRECTORY(readymade)

ADD_SUBDIRECTORY(Documentation)

ADD_EXECUTABLE(avg_q_vogl avg_q.c)
TARGET_LINK_LIBRARIES(avg_q_vogl ${BF_LIBS})

IF (WITH_POSPLOT)
 LIST(APPEND PROGRAMS do_posplot_vogl)
 ADD_EXECUTABLE(do_posplot_vogl do_posplot.c)
 TARGET_LINK_LIBRARIES(do_posplot_vogl ${BF_LIBS})
endif (WITH_POSPLOT)

IF (WITH_AVG_Q_UI)
ADD_SUBDIRECTORY(avg_q_ui)
endif (WITH_AVG_Q_UI)

SET(PROGRAMS extspec)

FOREACH(program ${PROGRAMS})
 ADD_EXECUTABLE(${program} ${program}.c)
 TARGET_LINK_LIBRARIES(${program} ${BF_LIBS})
ENDFOREACH(program)

INSTALL(
 TARGETS avg_q_vogl ${PROGRAMS}
 DESTINATION ${INSTALLDIR}
 PERMISSIONS ${EXECUTABLE_PERMISSIONS}
)

IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
 ADD_EXECUTABLE(avg_q_1 avg_q.c avg_q_dump.h)
 SET_TARGET_PROPERTIES(avg_q_1 PROPERTIES 
  COMPILE_FLAGS "-DSTANDALONE"
 )
 TARGET_LINK_LIBRARIES(avg_q_1 ${BF_LIBS})
ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
