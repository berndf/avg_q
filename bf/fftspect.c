/*
 * Copyright (C) 1996-1999,2001-2003,2010,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */

/*
 * fftspect.c module to calculate successive (spectral power) spectra
 * in a sliding data window. Spectral power calculation based on programs
 * from the Numerical Recipes.	-- Bernd Feige 9.12.1992
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <memory.h>
#include "transform.h"
#include "bf.h"

enum norm_types {
 NORM_COHERENCE,
 NORM_PHASEALIGNED,
 NORM_CROSS_SPECTRUM
};
LOCAL const char *const refchannel_analyses[]={
 "-cc", "-c", "-cs", NULL
};
enum ARGS_ENUM {
 ARGS_PADTO, 
 ARGS_CROSS_MODE, 
 ARGS_REFCHANNEL, 
 ARGS_WINDOWSIZE,
 ARGS_NROFSHIFTS,
 ARGS_OVERLAPS,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "padto: Zero-pad the window to padto points before fft", "p", ARGDESC_UNUSED, (char const *const *)"1s"},
 {T_ARGS_TAKES_SELECTION, "Complex spectra: coherence, phase aligned, cross-spectrum", " 1", 0, refchannel_analyses},
 {T_ARGS_TAKES_STRING_WORD, "refchannel", " ", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_STRING_WORD, "windowsize", "", ARGDESC_UNUSED, (char const *const *)"1s"},
 {T_ARGS_TAKES_LONG, "nrofshifts", "", 1, NULL},
 {T_ARGS_TAKES_LONG, "overlaps", "", 1, NULL}
};

struct fftspect_storage {
 int padto;
 int refchannel;
 int overlaps;

 /* Local copies of tinfo parameters */
 int windowsize;	/* The size of the fft window shifted */
 int nrofshifts;
};

/*{{{}}}*/
/*{{{  Local spectral analysis functions*/
LOCAL DATATYPE
square(DATATYPE arg) {
 return arg*arg;
}

/*{{{  Window function*/
LOCAL long oldwlength;
LOCAL DATATYPE *window=(DATATYPE *)NULL;
LOCAL DATATYPE sumw;	/* The window sum */
LOCAL DATATYPE sumw2;	/* The (squared window) sum */

/*
 * Welch window
 */

LOCAL void
mkwindow(long wlength) {
 long i;
 DATATYPE Nplus1d2, w;

 if (window) {
  /* old wlength is the same as new: nothing to do */
  if (oldwlength==wlength) return;
  free(window);
 }
 if ((window=(DATATYPE *)malloc(wlength*sizeof(DATATYPE)))==NULL) return;

 Nplus1d2=(wlength+1)/2.0;
 sumw=sumw2=0.0;
 for (i=0; i<wlength; i++) {
  window[i]=w=1.0-square((i+1-Nplus1d2)/Nplus1d2);
  sumw+=w;
  sumw2+=square(w);
 }
 oldwlength=wlength;
}
/*}}}  */

/*{{{  spect1: Spectral power estimation*/
/*
 * The spectral power calculation; Results are m values in p[]
 * indata must contain mm=2*m real data points for each segment
 * 2*k is the number of segments to average
 * ovrlap=1 lets the segments (fft windows) overlap by one half
 * dwin is the number of data points in each segment <= mm=2**n
 * indata contains dwin points (not necessarily power 2) for each segment
 * If dwin<mm, the data will be zero-padded to the fourier window size mm
 */

LOCAL void 
spect1(transform_info_ptr tinfo, DATATYPE *indata, DATATYPE *p, int dwin, int m, int k, int ovrlap) {
 int mm,m41,m4,kk,joff,j2,j;
 int n, dwin2=dwin*2, dwind2=dwin/2;
 DATATYPE w,*w1,den=0.0;
 DATATYPE *data, linreg_const, linreg_fact;

 data = indata;   /* assign temporary data pointer */

 mm=m+m;
 if (dwin<=0 || dwin>mm)
 	ERREXIT2(tinfo->emethods, "spect1: Wrong dwin value %d, mm=%d\n", MSGPARM(dwin), MSGPARM(mm));
 m41=(m4=mm+mm)+1;
 w1=(DATATYPE *)malloc(m4*sizeof(DATATYPE));
 mkwindow(dwin);	/* Prepare window function in window[dwin] and sumw2 */
 if (w1==NULL || window==NULL)
	ERREXIT(tinfo->emethods, "spect1: Error allocating temporary memory\n");

 for (j=0;j< m;j++) p[j]=0.0;
 for (kk=0;kk<k;kk++) {
  /*{{{  Get effectively two spectra and average their power*/
  for (joff=0;joff<=1;joff++) {
   /* Fit a line to the data to eliminate longwave effects (de-trending) */
   linreg(data, dwin, 1, &linreg_const, &linreg_fact);
   n=0;
   for (j=joff;j<dwin2;j+=2) {
    w1[j] = *data-linreg_const-n*linreg_fact;
    data++; n++;
   }
   if (ovrlap) data-=dwind2; /* Shift back by half the datawin size */
  }
  /*{{{  Multiply with window and zero-pad to m4 data points*/
  for (j=0;j<dwin;j++) {
   j2=j+j;
   w=window[j];
   w1[j2] *= w;
   w1[j2+1] *= w;
  }
  for (j=dwin2; j<m4; j++) {
   w1[j]=0.0;
  }
  /*}}}  */
  fourier((complex *)w1,mm,1);
  p[0] += (square(w1[0])+square(w1[1]));
  for (j=1;j<m;j++) {
   j2=j+j;
   p[j] += (square(w1[j2])+square(w1[j2+1])
        +square(w1[m4-j2])+square(w1[m41-j2]));
  }
  den += sumw2;
  /*}}}  */
 }
 den *= m4;
 for (j=0;j<m;j++) {
  p[j] /= den;
 }
 free(w1);
}
/*}}}  */

/*{{{  cspect: Complex phase aligned spectra*/
/*
 * Complex spectrum determination: In order to get complex spectra that
 * can be averaged, a reference phase needs to be defined. The reference
 * phase here is extracted from a reference data set.
 * The process is divided into two steps: cspect function takes a pointer
 * to the data to be analyzed and stores the results of all 2*k overlapping
 * Fourier transforms in the cspec object. Successive calls to cspect for
 * different channels should be done with advanced cspec->current_spectrum
 * pointer. After all spectral data is derived, the calc_refspectrum
 * function calculates the phase-aligned spectrum given two input spectra
 * (the normal channel spectrum pointed to by cspec->current_spectrum and the
 * reference channel spectrum being the refchannel'th spectrum in cspec),
 * using the normspec object as storage for the normalized reference spectrum.
 *
 * cspec and normspec data areas will be allocated automatically when called
 * with first_spectrum==NULL. normspec is only recalculated by calc_refspectrum
 * if normspec->is_normalized is FALSE. Thus, if a different reference channel
 * is to be used, changing the refchannel argument is not sufficient.
 * If coherence spectra are desired, cspec is also normalized only once
 * and cspec->is_normalized set to TRUE.
 *
 * Cave: There are m+1 complex frequency coefficients.
 */

typedef struct {
 complex *first_spectrum;
 complex *current_spectrum;
 int sizeof_spectrum;
 int nr_of_spectra;
 int nr_of_freq;
 int items_per_freq;
 int is_normalized;
} complex_spectrum;

/*{{{  cspect(transform_info_ptr tinfo, DATATYPE *indata, complex_spectrum *cspec, int dwin, int m, int k, int ovrlap) {*/
LOCAL void
cspect(transform_info_ptr tinfo, DATATYPE *indata, complex_spectrum *cspec, int dwin, int m, int k, int ovrlap) {
 int m1,mm,mm2,m4,kk,joff,j;
 int dwind2=dwin/2;
 DATATYPE *w1,*wp;
 DATATYPE *data, linreg_const, linreg_fact;
 complex *spectdest;

 data = indata;   /* assign temporary data pointer */

 m1=m+1;	 /* m+1 complex frequency coefficients per spectrum */
 mm2=(mm=m+m)+2; /* 2(m+1) complex frequency coefficients per real2fft */
 m4=mm+mm;       /* The DATATYPE size of the input range for real2fft */
 if (cspec->first_spectrum==NULL) {
  /*{{{  Allocate cspec memory*/
  cspec->sizeof_spectrum=k*mm2;
  if ((spectdest=(complex *)malloc(cspec->nr_of_spectra*cspec->sizeof_spectrum*sizeof(complex)))==NULL)
   ERREXIT(tinfo->emethods, "cspect: Error allocating cspec memory\n");
  cspec->current_spectrum=cspec->first_spectrum=spectdest;
  cspec->nr_of_freq=m1;
  cspec->items_per_freq=2*k;
  /*}}}  */
 } else {
  spectdest=cspec->current_spectrum;
 }
 w1=(DATATYPE *)malloc(m4*sizeof(DATATYPE));
 mkwindow(dwin);	/* Prepare window function in window[dwin] and sumw */
 if (w1==NULL || spectdest==NULL || window==NULL)
	ERREXIT(tinfo->emethods, "cspect: Error allocating temporary memory\n");

 for (kk=0;kk<k;kk++) {
  /*{{{  Get two spectra and store them in cspec*/
  wp=w1;
  for (joff=0;joff<=1;joff++) {
   /* Fit a line to the data to eliminate longwave effects (de-trending) */
   linreg(data, dwin, 1, &linreg_const, &linreg_fact);
   for (j=0;j<dwin;j++) {
    *wp++ = (*data++ -linreg_const-j*linreg_fact)*window[j];
   }
   for (;j<mm;j++) *wp++=0.0;	/* Zero-pad to mm values */
   if (ovrlap) data-=dwind2; /* Shift back by half the datawin size */
  }
  real2fft(w1, w1+mm, spectdest, mm);
  /* Just store each complex spectrum */
  spectdest+=mm2;
  /*}}}  */
 }
 free(w1);
}
/*}}}  */

/*{{{  calc_refspectrum(transform_info_ptr tinfo, complex_spectrum *cspec, complex_spectrum *normspec, int refchannel, complex *p, enum norm_types norm_type) {*/
LOCAL void
calc_refspectrum(transform_info_ptr tinfo, complex_spectrum *cspec, complex_spectrum *normspec, int refchannel, complex *p, enum norm_types norm_type) {
 int m1,kk,joff,k,i,j;
 DATATYPE den;
 complex *phaseref, *cwp, *pr;

 if (normspec->first_spectrum==NULL) {
  /*{{{  Allocate normspec memory*/
  normspec->nr_of_spectra=1; normspec->sizeof_spectrum=cspec->sizeof_spectrum;
  normspec->nr_of_freq=cspec->nr_of_freq; normspec->items_per_freq=cspec->items_per_freq;
  if ((normspec->first_spectrum=normspec->current_spectrum=(complex *)malloc(normspec->sizeof_spectrum*sizeof(complex)))==NULL)
   ERREXIT(tinfo->emethods, "calc_refspectrum: Error allocating normspec memory\n");
  /*}}}  */
 }
 phaseref=normspec->current_spectrum;
 if (!normspec->is_normalized) {
  /*{{{  Prepare the phase factors in normspec ready for multiplication*/
  pr=cspec->first_spectrum+refchannel*cspec->sizeof_spectrum;
  for (j=0;j<normspec->sizeof_spectrum;j++) {
   if (norm_type==NORM_CROSS_SPECTRUM) {
    /* Just conjugate the `reference spectrum' */
    phaseref[j]=c_konj(pr[j]);
   } else {
    /* This is the |x| \over x phase factor, ready to multiply with the spects */
    phaseref[j]=c_smult(c_konj(pr[j]), 1.0/c_abs(pr[j]));
   }
  }
  normspec->is_normalized=TRUE;
  /*}}}  */
 }
 if (norm_type==NORM_COHERENCE && !cspec->is_normalized) {
  /*{{{  Amplitude normalize cspec*/
  for (i=0; i<cspec->nr_of_spectra; i++) {
   complex *in_cspec=cspec->first_spectrum+i*cspec->sizeof_spectrum;

   for (j=0; j<cspec->sizeof_spectrum; j++) {
    in_cspec[j]=c_smult(in_cspec[j], 1.0/c_abs(in_cspec[j]));
   }
  }
  cspec->is_normalized=TRUE;
  /*}}}  */
 }

 m1=cspec->nr_of_freq;
 k=cspec->items_per_freq/2;
 cwp=cspec->current_spectrum;
 pr=normspec->current_spectrum;
 memset(p, 0, m1*sizeof(complex));	/* Clear destination range for the averager */
 
 for (kk=0;kk<k;kk++) {
  /*{{{  Get two spectra and average*/
  /* Add the two new spectra to the output spectrum after multiplication
   * with the inverse phase factor of the phaseref */
  for (joff=0;joff<=1;joff++) {
   for (j=0; j<m1; j++,cwp++,pr++) {
    p[j]=c_add(p[j], c_mult(*cwp, *pr));
   }
  }
  /*}}}  */
 }
 /*{{{  Divide the resulting spectrum by window sum, spectrum count and winlength*/
 switch (norm_type) {
  case NORM_CROSS_SPECTRUM:
   den = 0.5/sumw/sumw/k;
   break;
  case NORM_PHASEALIGNED:
   den = 0.5/sumw/k;
   break;
  case NORM_COHERENCE:
   /* In the case of coherence spectra, division by window weight is not necessary */
   den=0.5/k;
   break;
 }
 for (j=0; j<m1; j++) {
  p[j]=c_smult(p[j], den);
 }
 /*}}}  */
}
/*}}}  */
/*}}}  */

/*{{{  LOCAL void spect_exit(void)*/
LOCAL void
spect_exit(void) {
 free(window);
 window=NULL;
}
/*}}}  */
/*}}}  */

/*{{{  LOCAL int roundbase2(register long value)*/
/*
 * roundbase2 rounds the argument down to a power of 2
 */
LOCAL int 
roundbase2(register long value) {
 int i;
 for (i=0; value>>=1; i++);
 return 1<<i;
}
/*}}}  */

/*{{{  fftspect_init(transform_info_ptr tinfo)*/
METHODDEF void
fftspect_init(transform_info_ptr tinfo) {
 struct fftspect_storage *local_arg=(struct fftspect_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int wsize;
/* ARGS_OVERLAPS is the k-parameter of spect1;
 * ptsperfreq is the resulting ratio of window points to number of frequencies
 */
 int ptsperfreq, freqs, channel;

 if (tinfo->data_type==FREQ_DATA) {
  ERREXIT(tinfo->emethods, "fftspect_init: Scusi, what do you expect me to do with frequency domain data?\n");
 }
 if (tinfo->itemsize>1) {
  ERREXIT(tinfo->emethods, "fftspect_init: Sorry, this method does not handle tuple data.\n");
 }

 /*{{{  Parse arguments that can be in seconds*/
 tinfo->windowsize=gettimeslice(tinfo, args[ARGS_WINDOWSIZE].arg.s);
 local_arg->padto=(args[ARGS_PADTO].is_set ? gettimeslice(tinfo, args[ARGS_PADTO].arg.s) : 0);
 /*}}}  */
 /* As with some other methods, the invalid value `0' stands for `max size': */
 if (tinfo->windowsize<0) {
  ERREXIT1(tinfo->emethods, "fftspect_init: Invalid window size %d\n", MSGPARM(tinfo->windowsize));
 } else if (tinfo->windowsize==0) {
  tinfo->windowsize=tinfo->nr_of_points;
 } else if (tinfo->windowsize>tinfo->nr_of_points) {
  TRACEMS2(tinfo->emethods, 0, "fftspect_init: windowsize=%d, but only %d points are available - setting to maximal\n", MSGPARM(tinfo->windowsize), MSGPARM(tinfo->nr_of_points));
  tinfo->windowsize=tinfo->nr_of_points;
 }
 tinfo->nrofshifts=args[ARGS_NROFSHIFTS].arg.i;
 if (tinfo->nrofshifts<=0) {
  TRACEMS1(tinfo->emethods, 0, "fftspect_init: Invalid nrofshifts=%d, set to 1\n", MSGPARM(tinfo->nrofshifts));
  tinfo->nrofshifts=1;
 }
 local_arg->overlaps=args[ARGS_OVERLAPS].arg.i;
 if (local_arg->overlaps<=0) {
  TRACEMS1(tinfo->emethods, 0, "fftspect_init: Invalid overlap parameter=%d, set to 1\n", MSGPARM(local_arg->overlaps));
  local_arg->overlaps=1;
 }

 /*{{{  Find the reference channel if any*/
 if (args[ARGS_CROSS_MODE].is_set && args[ARGS_REFCHANNEL].is_set) {
  if (strcmp("ALL", args[ARGS_REFCHANNEL].arg.s)==0) {
   local_arg->refchannel= -1;
   if (tinfo->nrofshifts>1) {
    ERREXIT(tinfo->emethods, "fftspect_init: nrofshifts>1 not allowed with channel=ALL\n");
   }
  } else {
   channel=find_channel_number(tinfo, args[ARGS_REFCHANNEL].arg.s);
   if (channel== -1) {
    ERREXIT1(tinfo->emethods, "fftspect_init: Can't find reference channel >%s<\n", MSGPARM(args[ARGS_REFCHANNEL].arg.s));
   }
   local_arg->refchannel=channel+1;
  }
 } else {
  local_arg->refchannel=0;
 }
 /*}}}  */

 if (local_arg->padto<tinfo->windowsize) local_arg->padto=tinfo->windowsize;
 ptsperfreq=2*local_arg->overlaps+1;
 freqs=roundbase2(local_arg->padto/ptsperfreq);

 if (tinfo->nrofshifts<=1) {
  int newoverlaps;
  /*
   * In this case we can optimize the overlap parameter: Try to utilize
   * a maximum input window for the given number of freqs
   */
  newoverlaps=(local_arg->padto/freqs-1)/2;
  if (newoverlaps!=local_arg->overlaps) {
   local_arg->overlaps=newoverlaps;
   TRACEMS1(tinfo->emethods, 0, "fftspect_init: Overlap count changed to %d\n", MSGPARM(newoverlaps));
   ptsperfreq=2*newoverlaps+1;
  }
 }

 /* Calculate a correct window size and adjust if necessary */
 wsize=freqs*ptsperfreq;
 if (wsize!=local_arg->padto) {
  local_arg->padto=wsize;
  if (local_arg->padto<tinfo->windowsize) {
   tinfo->windowsize=local_arg->padto;
   TRACEMS1(tinfo->emethods, 0, "fftspect_init: Window size changed to %d\n", MSGPARM(wsize));
  } else {
   TRACEMS1(tinfo->emethods, 0, "fftspect_init: Pad size changed to %d\n", MSGPARM(wsize));
  }
 }
 if (tinfo->windowsize%ptsperfreq!=0) {
  tinfo->windowsize-=tinfo->windowsize%ptsperfreq;
  TRACEMS1(tinfo->emethods, 0, "fftspect_init: Data window size changed to %d\n", MSGPARM(tinfo->windowsize));
 }
 /* Save our final idea of what the parameters are */
 local_arg->windowsize=tinfo->windowsize;
 local_arg->nrofshifts=tinfo->nrofshifts;
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  fftspect(transform_info_ptr tinfo)*/
/*
 * The data representation for fftspect is completely `non-multiplexed':
 * The topmost building blocks in memory refer to the time shift; Input data
 * has only one 'time shift'. Each shift consists of nr_of_channels channels, 
 * which consist of nr_of_points or nroffreq points for input or output data.
 */

METHODDEF DATATYPE *
fftspect(transform_info_ptr tinfo) {
 struct fftspect_storage *local_arg=(struct fftspect_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 DATATYPE *spects=NULL, *data;
 const int ptsperfreq=2*local_arg->overlaps+1;
 int i, channel, channels, nspect, wshift, nfreq;

 tinfo->windowsize=local_arg->windowsize;
 tinfo->nrofshifts=local_arg->nrofshifts;

 tinfo->nroffreq=local_arg->padto/ptsperfreq;
 tinfo->shiftwidth=(tinfo->nrofshifts<=1 ? 0: 
	(tinfo->nr_of_points-tinfo->windowsize)/(tinfo->nrofshifts-1));
 tinfo->basefreq=tinfo->sfreq/tinfo->nroffreq/2;
 tinfo->basetime=tinfo->shiftwidth/tinfo->sfreq;
 data=tinfo->tsdata;
 nspect=tinfo->nrofshifts;
 wshift=tinfo->shiftwidth;
 nfreq=tinfo->nroffreq;
 channels=tinfo->nr_of_channels;
 nonmultiplexed(tinfo);	/* Reorganize data if necessary */

 if (local_arg->refchannel!=0) {
  /*{{{  Complex spectral averaging*/
  complex *cspects;
  complex_spectrum cspec, normspec;
  int refchan, fromchan, tochan;	/* Range of phase reference channels */

  nfreq= ++tinfo->nroffreq;
  tinfo->itemsize=sizeof(complex)/sizeof(DATATYPE);
  if (local_arg->refchannel== -1) {
   tinfo->length_of_output_region=channels*nfreq*channels*tinfo->itemsize;
   tinfo->nrofshifts=channels;
   nspect=1;
   fromchan=0; tochan=channels-1;
  } else {
   tinfo->length_of_output_region=channels*nfreq*nspect*tinfo->itemsize;
   fromchan=tochan=local_arg->refchannel-1;
  }

  if ((cspects=(complex *)malloc(tinfo->length_of_output_region*sizeof(DATATYPE)))!=NULL) {
   cspec.first_spectrum=normspec.first_spectrum=NULL;
   cspec.nr_of_spectra=channels;
   for (i=0; i<nspect; i++) {
    cspec.current_spectrum=cspec.first_spectrum;
    cspec.is_normalized=FALSE;
    /*{{{  Get the 2*k (overlaps) complex spectra for all channels*/
    for (channel=0; channel<channels; channel++) {
     cspect(tinfo, data+channel*tinfo->nr_of_points+i*wshift,
      &cspec, 2*(tinfo->windowsize/ptsperfreq), nfreq-1, local_arg->overlaps, 1);
     cspec.current_spectrum+=cspec.sizeof_spectrum;
    }
    /*}}}  */

    /*{{{  For all ref channels, calculate the phase-aligned spectrum for all channels*/
    for (refchan=fromchan; refchan<=tochan; refchan++) {
     cspec.current_spectrum=cspec.first_spectrum;
     normspec.is_normalized=FALSE;
     for (channel=0; channel<channels; channel++) {
      calc_refspectrum(tinfo, &cspec, &normspec, refchan,
       cspects+(i+refchan-fromchan)*channels*nfreq+channel*nfreq, (enum norm_types)args[ARGS_CROSS_MODE].arg.i);
      cspec.current_spectrum+=cspec.sizeof_spectrum;
     }
    }
    /*}}}  */
   }
   /* These arrays were allocated as needed by cspect and calc_refspectrum */
   free(cspec.first_spectrum); free(normspec.first_spectrum);
   spects=(DATATYPE *)cspects;	/* The pointer to return */
  }
  /*}}}  */
 } else {
  /*{{{  Spectral power estimation*/
  tinfo->itemsize=1;
  tinfo->length_of_output_region=channels*nfreq*nspect;
  if ((spects=(DATATYPE *)malloc(tinfo->length_of_output_region*sizeof(DATATYPE)))!=NULL) {
   for (i=0; i<nspect; i++) {
    for (channel=0; channel<channels; channel++) {
     spect1(tinfo, data+channel*tinfo->nr_of_points+i*wshift, 
      spects+i*channels*nfreq+channel*nfreq, 2*(tinfo->windowsize/ptsperfreq), nfreq, local_arg->overlaps, 1);
    }
   }
  }
  /*}}}  */
 }
 tinfo->data_type=FREQ_DATA;
 free_pointer((void **)&tinfo->xdata); tinfo->xchannelname=NULL;
 return spects;
}
/*}}}  */

/*{{{  fftspect_exit(transform_info_ptr tinfo)*/
METHODDEF void
fftspect_exit(transform_info_ptr tinfo) {
 spect_exit();
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_fftspect(transform_info_ptr tinfo)*/
GLOBAL void
select_fftspect(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &fftspect_init;
 tinfo->methods->transform= &fftspect;
 tinfo->methods->transform_exit= &fftspect_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="fftspect";
 tinfo->methods->method_description=
  "Transform method to analyze the data in the frequency domain.\n";
 tinfo->methods->local_storage_size=sizeof(struct fftspect_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
