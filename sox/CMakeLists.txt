PROJECT(sox)

SET(SOX_DEFINES -DENABLE_GSM -DFAST_ULAW_COMPRESSION -DFAST_ALAW_COMPRESSION)
SET(FSRC
8svx.c adpcm.c aiff.c alsa.c au.c auto.c avr.c cdr.c cvsd.c dat.c
g711.c g721.c g723_16.c g723_24.c g723_40.c g72x.c gsm.c hcom.c
ima_rw.c maud.c nulfile.c oss.c raw.c sf.c smp.c sndrtool.c
sphere.c sunaudio.c tx16w.c voc.c vorbis.c wav.c wve.c
)
SET(ESRC
avg.c band.c bandpass.c breject.c btrworth.c chorus.c compand.c
copy.c dcshift.c deemphas.c earwax.c echo.c echos.c fade.c
filter.c flanger.c highp.c highpass.c lowp.c lowpass.c map.c
mask.c pan.c phaser.c pitch.c polyphas.c rate.c
resample.c reverb.c reverse.c silence.c speed.c
stat.c stretch.c swap.c synth.c trim.c vibro.c vol.c
)
SET(sources ${FSRC} ${ESRC} handlers.c misc.c getopt.c)
SET(GSMSRC
 add.c  code.c  decode.c long_term.c lpc.c  
 preprocess.c rpe.c  gsm_destroy.c gsm_decode.c gsm_encode.c 
 gsm_create.c gsm_option.c short_term.c table.c
)

ADD_DEFINITIONS(${SOX_DEFINES})
ADD_LIBRARY(st ${sources})
SET_TARGET_PROPERTIES(st
 PROPERTIES VERSION 2.0 SOVERSION 2
)
set(targets st)

IF(WIN32)
 FOREACH(src ${GSMSRC})
  LIST(APPEND gsmsources "gsm/${src}")
 ENDFOREACH(src)
 ADD_LIBRARY(gsm ${gsmsources})
 LIST(APPEND targets gsm)
ENDIF(WIN32)

IF(BUILD_SHARED_LIBS)
INSTALL(
 TARGETS ${targets}
 DESTINATION ${INSTALLDIR}
 RUNTIME PERMISSIONS ${EXECUTABLE_PERMISSIONS}
 LIBRARY PERMISSIONS ${FILE_PERMISSIONS}
)
ENDIF(BUILD_SHARED_LIBS)
