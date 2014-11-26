# Copyright (C) 2006-2013 Bernd Feige
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
 SET(ICA_LIBS lapack blas f2c)
 SET(AVG_Q_DEFINES -DLITTLE_ENDIAN ${HDF_DEFINES})
 IF(CMAKE_SYSTEM_VARIANT STREQUAL win64)
  SET(HDF_LIBPATH
   /home/charly/tmp/Infos/Computer/wingtk3/usr/x86_64-w64-mingw32/sys-root/mingw/lib)
  SET(CMAKE_C_FLAGS_RELEASE "-march=athlon64 -O4 -ffast-math -mms-bitfields -mthreads -Wall -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline")
  SET(ICA_INCLUDES ${CMAKE_SOURCE_DIR}/winlibs64/libf2c)
  SET(ICA_LIBPATH "${CMAKE_SOURCE_DIR}/winlibs64")
  SET(SOX_INCLUDES ${CMAKE_SOURCE_DIR}/winlibs64/sox/src)
 ELSE(CMAKE_SYSTEM_VARIANT STREQUAL win64)
  SET(HDF_LIBPATH
   /home/charly/tmp/Infos/Computer/wingtk3/usr/i686-w64-mingw32/sys-root/mingw/lib)
  SET(CMAKE_C_FLAGS_RELEASE "-march=i686 -O4 -ffast-math -mms-bitfields -mthreads -Wall -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline")
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
  SET(CMAKE_C_FLAGS_RELEASE "-march=athlon64 -O4 -fomit-frame-pointer -ffast-math -Wall -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline")
 ELSE(UNAME MATCHES "x86_64")
  SET(INSTALLDIR distrib/avg_q32)
  # -D_FILE_OFFSET_BITS=64 is necessary to have working fstat() on CIFS mounts
  SET(CMAKE_C_FLAGS_RELEASE "-march=i686 -O4 -D_FILE_OFFSET_BITS=64 -fomit-frame-pointer -ffast-math -Wall -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline")
 ENDIF(UNAME MATCHES "x86_64")
ENDIF(CMAKE_SYSTEM_NAME MATCHES Windows)

INCLUDE(FindOpenMP)
IF(OPENMP_FOUND)
 SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OpenMP_C_FLAGS}")
ENDIF(OPENMP_FOUND)

SET(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

# Permissons to be set on installed files
SET(EXECUTABLE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
SET(FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# vvvvvvvvvvvvvvv Library definitions vvvvvvvvvvvvvvv

SET(MFX_LIBS mfx)

SET(DUMMY_VGUI dummy_vogl_vgui)
SET(VGUI_LIB vogl_vgui)

INCLUDE(FindPkgConfig)
pkg_check_modules(SOX sox)
if (NOT SOX_FOUND)
 MESSAGE("SOX library not found - compiling without read_sound/write_sound")
endif (NOT SOX_FOUND)

SET(CMAKE_C_FLAGS_DEBUG "-g -Wall")
#SET(CMAKE_C_FLAGS_DEBUG -g -DMALLOC_TRACE)

SET(AVG_Q_DEFINES ${AVG_Q_DEFINES} -DTRACE -DFP_EXCEPTION -DFOR_AVG_Q)
SET(AVG_Q_INCLUDES ${CMAKE_SOURCE_DIR}/bf ${CMAKE_SOURCE_DIR}/bf/array
 ${CMAKE_SOURCE_DIR}/bf/biomag ${CMAKE_SOURCE_DIR}/mfx
 ${CMAKE_SOURCE_DIR}/bf/stat
 ${CMAKE_SOURCE_DIR}/bf/math ${CMAKE_SOURCE_DIR}/bf/optimize
 ${CMAKE_SOURCE_DIR}/bf/array ${CMAKE_SOURCE_DIR}/bf/biomag
 ${CMAKE_SOURCE_DIR}/vogl/src
 ${HDF_INCLUDES}
 ${ICA_INCLUDES}
 ${SOX_INCLUDES}
)

# Arranges that shared libs are found in the same dir as the binary
SET(CMAKE_INSTALL_RPATH "$ORIGIN")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG -g)
SET(BF_LIBPATH_DYNAMIC ${HDF_LIBPATH} ${ICA_LIBPATH})
SET(BF_LIBS bf ${MFX_LIBS} ${SOX_LIBRARIES} ${VOGL_LIBS} ${DUMMY_VGUI} ${HDF_LIBS} ${ICA_LIBS} m)
SET(BF_LIBS_VGUI bf ${MFX_LIBS} ${SOX_LIBRARIES} ${VOGL_LIBS} ${VGUI_LIB} ${HDF_LIBS} ${ICA_LIBS} m)

ADD_DEFINITIONS(${AVG_Q_DEFINES})
INCLUDE_DIRECTORIES(${AVG_Q_INCLUDES})
LINK_DIRECTORIES(${BF_LIBPATH_DYNAMIC})

# Imported libs:
ADD_SUBDIRECTORY(vogl)

ADD_SUBDIRECTORY(mfx)
ADD_SUBDIRECTORY(bf)
ADD_SUBDIRECTORY(readymade)
ADD_SUBDIRECTORY(avg_q_ui)

ADD_SUBDIRECTORY(Documentation)

ADD_EXECUTABLE(avg_q_vogl avg_q.c)
TARGET_LINK_LIBRARIES(avg_q_vogl ${BF_LIBS})

ADD_EXECUTABLE(do_posplot_vogl do_posplot.c)
TARGET_LINK_LIBRARIES(do_posplot_vogl ${BF_LIBS})

ADD_EXECUTABLE(extspec extspec.c)
TARGET_LINK_LIBRARIES(extspec ${BF_LIBS})

INSTALL(
 TARGETS avg_q_vogl do_posplot_vogl extspec
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
