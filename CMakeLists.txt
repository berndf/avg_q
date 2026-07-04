# Copyright (C) 2006-2014,2016,2018-2021,2023-2026 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
# Top-level build configuration for avg_q.
# Targets C17/C++17 and modern CMake (>= 3.21).
cmake_minimum_required(VERSION 3.21...4.3)

project(avg_q
 VERSION 6.2.0
 LANGUAGES C CXX
 DESCRIPTION "Multichannel (EEG/MEG) data processing tool"
)

# Include the local signature file before anything that might depend on it.
include("${CMAKE_CURRENT_SOURCE_DIR}/Signatures/Signature.GPL")

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(AVG_Q_BUILD_DEBUG "Build avg_q in debug mode" OFF)
option(AVG_Q_ENABLE_ASAN "Enable AddressSanitizer in debug builds" OFF)
option(AVG_Q_WITH_POSPLOT "Build the posplot method (requires vogl)" ON)
option(AVG_Q_WITH_AVG_Q_UI "Build the GTK GUI avg_q_ui" ON)
option(AVG_Q_WITH_HDF4 "Enable HDF4 readers/writers if HDF4 is found" ON)
option(AVG_Q_GTK4 "Build avg_q_ui against GTK4" ON)

# Legacy names used by subdirectory CMakeLists.txt files.
set(WITH_POSPLOT ${AVG_Q_WITH_POSPLOT})
set(WITH_AVG_Q_UI ${AVG_Q_WITH_AVG_Q_UI})

# ---------------------------------------------------------------------------
# C / C++ language standards
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build type and shared/static library choice
# ---------------------------------------------------------------------------
if(AVG_Q_BUILD_DEBUG)
 set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
elseif(NOT CMAKE_BUILD_TYPE)
 set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# The original build forced static libraries for Windows and for Debug builds.
# Preserve that behaviour unless the user explicitly requested otherwise.
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
 set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
elseif(CMAKE_BUILD_TYPE STREQUAL Debug)
 set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
endif()

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
set(CPACK_GENERATOR TBZ2)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
include(CPack)

# ---------------------------------------------------------------------------
# Platform-specific defaults
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
 unset(CMAKE_INSTALL_PREFIX)
 set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Installation prefix" FORCE)
 set(CMAKE_EXECUTABLE_SUFFIX ".exe")
 set(INSTALLDIR "bin")
 set(VOGL_LIBS vogl)
 set(HDF4_INCLUDES /usr/x86_64-w64-mingw32/usr/include/hdf4)
 set(HDF4_LIBS mfhdf)
else()
 set(X11_LIBS X11)
 set(VOGL_LIBS vogl ${X11_LIBS})
 execute_process(COMMAND uname -m
  OUTPUT_VARIABLE UNAME
  OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 if(UNAME STREQUAL "x86_64")
  set(INSTALLDIR distrib/avg_q64)
  set(C_FLAGS_ARCHITECTURE "-march=core2")
 else()
  set(INSTALLDIR distrib/avg_q32)
  # -D_FILE_OFFSET_BITS=64 is necessary to have working fstat() on CIFS mounts
  set(C_FLAGS_ARCHITECTURE "-march=i686")
 endif()
 set(HDF4_INCLUDES /usr/include/hdf)
 set(HDF4_LIBS mfhdf df)
endif()

# ---------------------------------------------------------------------------
# External dependencies
# ---------------------------------------------------------------------------
find_package(LAPACK)
if(LAPACK_FOUND)
 set(ICA_LIBS ${LAPACK_LIBRARIES})
endif()

find_package(PkgConfig)
if(PkgConfig_FOUND)
 pkg_check_modules(SOX sox)
endif()

if(AVG_Q_WITH_HDF4)
 # Try to detect HDF4 in a portable way; fall back to the legacy hard-coded
 # configuration if detection fails, so existing builds keep working.
 find_path(HDF4_INCLUDE_DIR hdf.h PATHS ${HDF4_INCLUDES})
 find_library(HDF4_MFHDF_LIBRARY NAMES mfhdf)
 find_library(HDF4_DF_LIBRARY NAMES df)
 if(HDF4_INCLUDE_DIR AND HDF4_MFHDF_LIBRARY)
  set(HDF4_FOUND TRUE)
  set(HDF4_LIBS ${HDF4_MFHDF_LIBRARY})
  if(HDF4_DF_LIBRARY)
   list(APPEND HDF4_LIBS ${HDF4_DF_LIBRARY})
  endif()
  set(HDF4_INCLUDES ${HDF4_INCLUDE_DIR})
 else()
  message(WARNING "HDF4 not found by CMake; using legacy hard-coded paths. Set AVG_Q_WITH_HDF4=OFF to disable.")
  set(HDF4_FOUND TRUE)
 endif()
else()
 set(HDF4_FOUND FALSE)
endif()

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
# NOTE: Do not use -ffast-math because it breaks NaN handling (e.g. in recode).
set(C_FLAGS_COMMON "${C_FLAGS_ARCHITECTURE} -O3 -fomit-frame-pointer")

set(C_FLAGS_WARNINGS
 -Wall
 -Wshadow
 -Wpointer-arith
 -Wstrict-prototypes
 -Wmissing-prototypes
 -Wmissing-declarations
 -Wnested-externs
 -Winline
)

set(CXX_FLAGS_WARNINGS
 -Wall
 -Wshadow
 -Wpointer-arith
)

find_package(OpenMP)
if(OpenMP_C_FOUND)
 set(C_FLAGS_COMMON "${C_FLAGS_COMMON} ${OpenMP_C_FLAGS}")
endif()

# Apply flags via CMake variables so that the legacy subdirectory builds pick
# them up automatically. New code should prefer target-specific options.
string(REPLACE ";" " " C_FLAGS_WARNINGS_STR "${C_FLAGS_WARNINGS}")
string(REPLACE ";" " " CXX_FLAGS_WARNINGS_STR "${CXX_FLAGS_WARNINGS}")

set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS_COMMON} ${C_FLAGS_WARNINGS_STR}")
set(CMAKE_CXX_FLAGS_RELEASE "${C_FLAGS_COMMON} ${CXX_FLAGS_WARNINGS_STR}")

set(CMAKE_C_FLAGS_DEBUG "-g ${C_FLAGS_WARNINGS_STR}")
set(CMAKE_CXX_FLAGS_DEBUG "-g ${CXX_FLAGS_WARNINGS_STR}")

if(AVG_Q_ENABLE_ASAN)
 set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
 set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
 set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

# ---------------------------------------------------------------------------
# Permissions for installed files
# ---------------------------------------------------------------------------
set(EXECUTABLE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# ---------------------------------------------------------------------------
# Library definitions and feature configuration
# ---------------------------------------------------------------------------
set(MFX_LIBS mfx)

set(AVG_Q_DEFINES -DLITTLE_ENDIAN)

# Expose POSIX.1-2008/GNU functions such as strdup in strict C17 mode.
if(NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
 list(APPEND AVG_Q_DEFINES -D_GNU_SOURCE)
endif()

if(NOT LAPACK_FOUND)
 message(STATUS "LAPACK libraries not found - compiling without icadecomp")
endif()
if(NOT SOX_FOUND)
 message(STATUS "SOX library not found - compiling without read_sound/write_sound")
endif()
if(NOT HDF4_FOUND)
 message(STATUS "HDF4 library not found - compiling without read_hdf/write_hdf")
endif()
if(NOT AVG_Q_WITH_POSPLOT)
 message(STATUS "Compiling without the posplot method.")
endif()
if(NOT AVG_Q_WITH_AVG_Q_UI)
 message(STATUS "Not creating the avg_q_ui GUI.")
endif()

set(AVG_Q_DEFINES ${AVG_Q_DEFINES} -DTRACE -DFP_EXCEPTION -DFOR_AVG_Q)

set(AVG_Q_INCLUDES
 ${CMAKE_SOURCE_DIR}/bf
 ${CMAKE_SOURCE_DIR}/bf/array
 ${CMAKE_SOURCE_DIR}/bf/biomag
 ${CMAKE_SOURCE_DIR}/mfx
 ${CMAKE_SOURCE_DIR}/bf/stat
 ${CMAKE_SOURCE_DIR}/bf/math
 ${CMAKE_SOURCE_DIR}/bf/optimize
 ${CMAKE_SOURCE_DIR}/bf/array
 ${CMAKE_SOURCE_DIR}/bf/biomag
)

# Arranges that shared libs are found in the same dir as the binary
set(CMAKE_INSTALL_RPATH "$ORIGIN")

set(BF_LIBPATH_DYNAMIC)
set(BF_LIBS bf ${MFX_LIBS} m)

if(LAPACK_FOUND)
 list(APPEND BF_LIBS ${LAPACK_LIBRARIES})
endif()

if(SOX_FOUND)
 if(NOT SOX_INCLUDES)
  set(SOX_INCLUDES ${SOX_INCLUDE_DIRS})
 endif()
 list(APPEND AVG_Q_INCLUDES ${SOX_INCLUDES})
 list(APPEND BF_LIBS ${SOX_LIBRARIES})
endif()

if(HDF4_FOUND)
 list(APPEND AVG_Q_INCLUDES ${HDF4_INCLUDES})
 list(APPEND BF_LIBS ${HDF4_LIBS})
endif()

if(AVG_Q_WITH_POSPLOT)
 list(APPEND AVG_Q_INCLUDES ${CMAKE_SOURCE_DIR}/vogl/src)
 set(BF_LIBS_VGUI ${BF_LIBS} ${VOGL_LIBS} vogl_vgui)
 add_library(dummy_vogl_vgui STATIC avg_q_ui/dummy_vogl_vgui_driver.c)
 list(APPEND BF_LIBS ${VOGL_LIBS} dummy_vogl_vgui)
else()
 set(BF_LIBS_VGUI ${BF_LIBS})
endif()

add_definitions(${AVG_Q_DEFINES})
include_directories(${AVG_Q_INCLUDES})
link_directories(${BF_LIBPATH_DYNAMIC})

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
if(AVG_Q_WITH_POSPLOT)
 add_subdirectory(vogl)
endif()

add_subdirectory(mfx)
add_subdirectory(bf)
add_subdirectory(readymade)

add_subdirectory(Documentation)

if(AVG_Q_WITH_AVG_Q_UI)
 add_subdirectory(avg_q_ui)
endif()

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------
set(PROGRAMS avg_q_vogl extspec)

if(AVG_Q_WITH_POSPLOT)
 list(APPEND PROGRAMS do_posplot_vogl)
endif()

foreach(program ${PROGRAMS})
 if(program STREQUAL avg_q_vogl)
  add_executable(${program} avg_q.c)
 elseif(program STREQUAL do_posplot_vogl)
  add_executable(${program} do_posplot.c)
 else()
  add_executable(${program} ${program}.c)
 endif()
 target_link_libraries(${program} ${BF_LIBS})
endforeach()

# ---------------------------------------------------------------------------
# Testing
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
 add_test(NAME test_avg_q_vogl COMMAND avg_q_vogl ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite/basic_run_test.script)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(
 TARGETS ${PROGRAMS}
 DESTINATION ${INSTALLDIR}
 PERMISSIONS ${EXECUTABLE_PERMISSIONS}
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/avg_q_dump.h)
 add_executable(avg_q_1 avg_q.c avg_q_dump.h)
 target_compile_definitions(avg_q_1 PRIVATE STANDALONE)
 target_link_libraries(avg_q_1 ${BF_LIBS})
endif()
