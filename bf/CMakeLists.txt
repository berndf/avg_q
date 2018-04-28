# Copyright (C) 2006-2008,2010,2013,2016 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

PROJECT(bflib)

SET(ALL_SOURCES
 complex.c fourier.c realfft.c real2fft.c trafo_std.c
 fftspect.c chg_multiplex.c 
 average.c reject_flor.c reject_bandwidth.c fftfilter.c
 histogram.c differentiate.c sliding_average.c demean_maps.c
 tinfo_array.c integrate.c swap_fc.c swap_xz.c subtract.c
 linreg.c setup_queue.c remove_channel.c
 detrend.c baseline_divide.c malloc_trace.c set_values.c
 baseline_subtract.c collapse_channels.c set_channelposition.c
 calc.c show_memuse.c null_sink.c extract_item.c trim.c expand_channel_list.c
 run_external.c export_point.c normalize_channelbox.c rereference.c
 orthogonalize.c
 project.c scale_by.c zero_phase.c import_point.c write_crossings.c
 calc_binomial_items.c change_axes.c minmax.c correlate.c
 growing_buf.c set_comment.c svdecomp.c add_channels.c query.c 
 raw_fft.c add_zerochannel.c
 laplacian.cc append.c add_value.c
 recode.c assert.c null_source.c invert.c echo.c push.c pop.c
 link_order.c swap_ix.c swap_ic.c convolve.c
 method_list.c validate.c
)

IF (TIMEOUT)
 ADD_CUSTOM_COMMAND(
  # The "tmstamp" output is deliberately fake to force execution each time!
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tmstamp ${CMAKE_CURRENT_BINARY_DIR}/.chksum 
  COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/date_now ${CMAKE_CURRENT_SOURCE_DIR}/date_now.c
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/date_now >${CMAKE_CURRENT_BINARY_DIR}/.tmstamp
  COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/calc_time ${CMAKE_CURRENT_SOURCE_DIR}/calc_time.c
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/calc_time ${TIMEOUT} >${CMAKE_CURRENT_BINARY_DIR}/.timeout
  COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/calc_chksum
  ${CMAKE_CURRENT_SOURCE_DIR}/calc_chksum.c
  ${CMAKE_CURRENT_SOURCE_DIR}/validate.c -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}/array -I${CMAKE_CURRENT_SOURCE_DIR}/biomag -I${CMAKE_CURRENT_SOURCE_DIR}/../mfx -DVERSION=\"\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\"\" -DDATE=\"\\\"`cat ${CMAKE_CURRENT_BINARY_DIR}/.tmstamp`\\\"\" -DSIGNATURE=\\\"${SIGNATURE}\\\"
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/calc_chksum >${CMAKE_CURRENT_BINARY_DIR}/.chksum
 )
 SET_SOURCE_FILES_PROPERTIES(validate.c PROPERTIES 
  COMPILE_FLAGS "-DVERSION=\"\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\"\" -DDATE=\"\\\"`cat ${CMAKE_CURRENT_BINARY_DIR}/.tmstamp`\\\"\" -DSIGNATURE=\"\\\"${SIGNATURE}\\\"\" -DTIMEOUT=`cat ${CMAKE_CURRENT_BINARY_DIR}/.timeout` -DCHKSUM=`cat ${CMAKE_CURRENT_BINARY_DIR}/.chksum`"
  # This target never exists, only needed to renew .tmstamp each time!
  OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tmstamp
 )
ELSE (TIMEOUT)
 ADD_CUSTOM_COMMAND(
  # The "tmstamp" output is deliberately fake to force execution each time!
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tmstamp ${CMAKE_CURRENT_BINARY_DIR}/.chksum 
  COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/date_now ${CMAKE_CURRENT_SOURCE_DIR}/date_now.c
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/date_now >${CMAKE_CURRENT_BINARY_DIR}/.tmstamp
  COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/calc_time ${CMAKE_CURRENT_SOURCE_DIR}/calc_time.c
  COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/calc_chksum
  ${CMAKE_CURRENT_SOURCE_DIR}/calc_chksum.c
  ${CMAKE_CURRENT_SOURCE_DIR}/validate.c -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}/array -I${CMAKE_CURRENT_SOURCE_DIR}/biomag -I${CMAKE_CURRENT_SOURCE_DIR}/../mfx -DVERSION=\"\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\"\" -DDATE=\"\\\"`cat ${CMAKE_CURRENT_BINARY_DIR}/.tmstamp`\\\"\" -DSIGNATURE=\\\"${SIGNATURE}\\\"
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/calc_chksum >${CMAKE_CURRENT_BINARY_DIR}/.chksum
 )
 SET_SOURCE_FILES_PROPERTIES(validate.c PROPERTIES 
  COMPILE_FLAGS "-DVERSION=\"\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\"\" -DDATE=\"\\\"`cat ${CMAKE_CURRENT_BINARY_DIR}/.tmstamp`\\\"\" -DSIGNATURE=\"\\\"${SIGNATURE}\\\"\" -DCHKSUM=`cat ${CMAKE_CURRENT_BINARY_DIR}/.chksum`"
  # This target never exists, only needed to renew .tmstamp each time!
  OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tmstamp
 )
ENDIF (TIMEOUT)

SET(subdirs
 stat
 optimize
 array
 math
 biomag
 asc
 generic
 mfx
 kn
 neuroscan
 freiburg
 rec
 tucker
 vitaport
 kohden
 BrainVision
 LabView
 Inomed
 TriAn
 ica
 cfs
)

set(METHOD_LIST_DEFINES "")
if (SOX_FOUND)
 LIST(APPEND subdirs sound)
 STRING(APPEND METHOD_LIST_DEFINES " -DAVG_Q_WITH_SOUND")
endif (SOX_FOUND)

if (HDF_FOUND)
 LIST(APPEND subdirs HDF)
 STRING(APPEND METHOD_LIST_DEFINES " -DAVG_Q_WITH_HDF")
endif (HDF_FOUND)

if (WITH_POSPLOT)
 LIST(APPEND ALL_SOURCES posplot.c)
 # This is passed globally because it is also used in TriAn display modules
 ADD_DEFINITIONS(-DWITH_POSPLOT)
endif (WITH_POSPLOT)

SET_SOURCE_FILES_PROPERTIES(method_list.c PROPERTIES 
 COMPILE_FLAGS "${METHOD_LIST_DEFINES}"
)

FOREACH(subdir ${subdirs})
 # "." is binary_dir; Tells cmake to generate libbf here, not in subdirs
 # Doesn't work, libbf gets overwritten if subdir specifies ADD_LIBRARY
 #ADD_SUBDIRECTORY(${subdir} .)
 # Make sure no leftover values occur if a CMakeLists.txt doesn't set programs
 SET(programs)
 INCLUDE(${subdir}/CMakeLists.txt)
 FOREACH(src ${bf_sources})
  LIST(APPEND ALL_SOURCES "${subdir}/${src}")
 ENDFOREACH(src)

 IF(programs)
  FOREACH(prog ${programs})
   ADD_EXECUTABLE(${prog} ${subdir}/${prog}.c)
   TARGET_LINK_LIBRARIES(${prog} ${BF_LIBS})
  ENDFOREACH(prog)

  INSTALL(
   TARGETS ${programs}
   DESTINATION ${INSTALLDIR}
   PERMISSIONS ${EXECUTABLE_PERMISSIONS}
  )
 ENDIF(programs)
ENDFOREACH(subdir)

ADD_LIBRARY(bf ${ALL_SOURCES})
SET_TARGET_PROPERTIES(bf
 PROPERTIES VERSION 2.0 SOVERSION 2
)

IF(BUILD_SHARED_LIBS)
INSTALL(
 TARGETS bf
 DESTINATION ${INSTALLDIR}
 PERMISSIONS ${FILE_PERMISSIONS}
)
ENDIF(BUILD_SHARED_LIBS)
