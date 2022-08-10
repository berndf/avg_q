/*
 * Copyright (C) 1996-1999,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * raw_fft is a simple Fourier-only method for playing with the raw spectra.
 * 						-- Bernd Feige 26.10.1996
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

/*{{{  raw_fft_init(transform_info_ptr tinfo) {*/
METHODDEF void
raw_fft_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  raw_fft(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
raw_fft(transform_info_ptr tinfo) {
 int i;
 array myarray, fftarray;
 long datasize, fftsize;

 if (tinfo->itemsize==2) {
  /*{{{  Do the inverse transform*/
  if (tinfo->data_type==TIME_DATA) {
   tinfo->nroffreq=tinfo->nr_of_points;
   tinfo->nrofshifts=1;
  }
  datasize=tinfo->nroffreq;
  fftsize=datasize-3;
  for (i=0; fftsize>>=1; i++);
  fftsize=1<<(i+2);
  if (datasize!=fftsize/2+1 || tinfo->nrofshifts!=1) {
   ERREXIT(tinfo->emethods, "raw_fft: Input spectra must have 2^n+1 frequencies, 2 items and 1 shift\n");
  }

  fftarray.nr_of_elements=fftsize;
  fftarray.element_skip=1;
  fftarray.nr_of_vectors=tinfo->nr_of_channels;
  if (array_allocate(&fftarray)==NULL) {
   ERREXIT(tinfo->emethods, "raw_fft: Error allocating memory\n");
  }

  /* We access tsdata as 1-item data already now (this makes copying easier) */
  tinfo->itemsize=1;
  tinfo->nr_of_points*=2;
  tinfo->data_type=TIME_DATA;
  tinfo_array(tinfo, &myarray);
  do {
   myarray.current_element=0;
   myarray.current_vector=fftarray.current_vector;
   realfft(ARRAY_ELEMENT(&myarray), fftsize, -1);
   do {
    array_write(&fftarray, array_scan(&myarray));
   } while (fftarray.message==ARRAY_CONTINUE);
  } while (fftarray.message==ARRAY_ENDOFVECTOR);
  tinfo->nr_of_points=fftarray.nr_of_elements;
  /*}}}  */
 } else {
  /*{{{  Fourier-transform real-valued data*/
  DATATYPE norm, firstval, lastval;
  datasize=tinfo->nr_of_points;
  if (tinfo->itemsize!=1) {
   ERREXIT(tinfo->emethods, "raw_fft: This method only works on time domain data with 1 item\n");
  }
  fftsize=datasize-1;	/* This way, the result will be datasize if datasize is a power of 2 */
  for (i=0; fftsize>>=1; i++);
  fftsize=1<<(i+1);

  fftarray.nr_of_elements=fftsize+2;	/* 2 more for the additional coefficient */
  fftarray.element_skip=1;
  fftarray.nr_of_vectors=tinfo->nr_of_channels;
  if (array_allocate(&fftarray)==NULL) {
   ERREXIT(tinfo->emethods, "raw_fft: Error allocating memory\n");
  }

  /* This is all of the factor that's required for the cycle to/from the frequency
   * domain using realfft. */
  norm=(DATATYPE)(fftsize>>1);

  tinfo_array(tinfo, &myarray);
  do {
   fftarray.current_element=0;
   fftarray.current_vector=myarray.current_vector;
   firstval=READ_ELEMENT(&myarray)/norm;
   do {
    array_write(&fftarray, lastval=array_scan(&myarray)/norm);
   } while (myarray.message==ARRAY_CONTINUE);
   i=0;
   while (fftarray.message==ARRAY_CONTINUE) {
    i++;
    /* Pad with linear interpolation between lastval and firstval */
    array_write(&fftarray, lastval+(firstval-lastval)*i/(fftsize-datasize+1));
   }
   array_previousvector(&fftarray);
   realfft(ARRAY_ELEMENT(&fftarray), fftsize, 1);
  } while (myarray.message==ARRAY_ENDOFVECTOR);
  tinfo->itemsize=2;
  tinfo->nr_of_points=tinfo->nroffreq=fftarray.nr_of_elements/2;
  tinfo->data_type=FREQ_DATA;
  tinfo->basefreq=tinfo->sfreq/tinfo->nroffreq/2;
  tinfo->nrofshifts=1;
  tinfo->shiftwidth=0;
  /*}}}  */
 }

 tinfo->multiplexed=FALSE;
 if (tinfo->xdata!=NULL) {
  free(tinfo->xdata); tinfo->xdata=NULL; tinfo->xchannelname=NULL;
 }
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 return fftarray.start;
}
/*}}}  */

/*{{{  raw_fft_exit(transform_info_ptr tinfo) {*/
METHODDEF void
raw_fft_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_raw_fft(transform_info_ptr tinfo) {*/
GLOBAL void
select_raw_fft(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &raw_fft_init;
 tinfo->methods->transform= &raw_fft;
 tinfo->methods->transform_exit= &raw_fft_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="raw_fft";
 tinfo->methods->method_description=
  "Transform method to perform a raw Fourier transform on the input data.\n"
  " It is advisable to do a detrend first. The result is complex-valued.\n"
  " If applied on compatible complex-valued spectra, the inverse operation is\n"
  " performed, yielding time-domain data.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
