/*
 * Copyright (C) 1999-2001,2004,2008,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * icadecomp is the avg_q incorporation of the Independent Component Analysis
 * method developed at the Salk Institute, San Diego. See
 * http://www.cnl.salk.edu/~tewon/ica_cnl.html for an overview and
 * http://www.cnl.salk.edu/~enghoff/ for the actual `standalone' code
 * implemented by Sigurd Enghoff. We output the resulting component maps in
 * adjacent `points'. The activations can be calculated using the `project' 
 * method (with option -n).
 * 						-- Bernd Feige 14.03.1999
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "ica.h"
#include "transform.h"
#include "growing_buf.h"
/*}}}  */

GLOBAL transform_info_ptr tinfo_for_ica;
/* vvvvvvvv This segment is from interfc.c */
/* Globally defined variables */
integer pcaflag, sphering, posactflag, biasflag;


/**************************** Initialize variables ****************************/
/* Initialize global and external variables to their default values as        */
/* defined in ica.h                                                           */

LOCAL void 
initdefaults (void) {
	pcaflag    = DEFAULT_PCAFLAG;
	sphering   = DEFAULT_SPHEREFLAG;
	posactflag = DEFAULT_POSACT;
	biasflag   = DEFAULT_BIASFLAG;
	
	block      = 0;
	lrate      = 0.0;
	annealdeg  = DEFAULT_ANNEALDEG;
	annealstep = 0.0;
	nochange   = DEFAULT_STOP;
	momentum   = DEFAULT_MOMENTUM;
	maxsteps   = DEFAULT_MAXSTEPS;
	
	extendedflag   = DEFAULT_EXTENDED;
	extblocks  = DEFAULT_EXTBLOCKS;
 	pdfsize    = MAX_PDFSIZE;
	nsub       = DEFAULT_NSUB;
}
/* ^^^^^^^^ This segment is from interfc.c */

#include <stdarg.h>
GLOBAL void 
ica_printf(const char *format, ...) {
#define BUFFER_SIZE 128
 char buffer[BUFFER_SIZE], *inbuf;
 va_list ap;
 va_start(ap, format);
 vsnprintf(buffer, BUFFER_SIZE, format, ap);
 va_end(ap);
 inbuf=buffer;
 while (*inbuf!='\0') {
  if (*inbuf=='') *inbuf=' ';
  inbuf++;
 }
 TRACEMS(tinfo_for_ica->emethods, -2, buffer);
}
GLOBAL void 
ica_error(char *message) {
 char buffer[BUFFER_SIZE];
 snprintf(buffer, BUFFER_SIZE, "%s\n", message);
 ERREXIT(tinfo_for_ica->emethods, buffer);
#undef BUFFER_SIZE
}

enum ARGS_ENUM {
 ARGS_EXTENDED=0, 
 ARGS_LRATE, 
 ARGS_SPHEREFILE, 
 ARGS_NCOMPONENTS, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_LONG, "n: Perform an `extended ICA' with PDF estimation every n blocks", "e", 1, NULL},
 {T_ARGS_TAKES_DOUBLE, "lrate: Specify the initial learning rate (0<lrate<<1, ~1e-4)", "l", 0.0001, NULL},
 {T_ARGS_TAKES_FILENAME, "sphere_file: Sphere the data and output the sphering matrix to this file", "s", ARGDESC_UNUSED, (char const *const *)"*.asc"},
 {T_ARGS_TAKES_LONG, "n_components", "", 3, NULL}
};

/*{{{  icadecomp_init(transform_info_ptr tinfo) {*/
METHODDEF void
icadecomp_init(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 initdefaults();
 extblocks=extendedflag=0;
 if (args[ARGS_EXTENDED].is_set && args[ARGS_EXTENDED].arg.i!=0) {
  extendedflag=1;
  extblocks=args[ARGS_EXTENDED].arg.i;
 }
 if (args[ARGS_SPHEREFILE].is_set) {
  sphering=1;
 } else {
  sphering=0;
 }
 if (args[ARGS_LRATE].is_set) {
  lrate = args[ARGS_LRATE].arg.d;
  if (lrate<=0 || lrate>=1) {
   ERREXIT(tinfo->emethods, "icadecomp_init: lrate needs to be between 0 and 1.\n");
  }
 }
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  icadecomp(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
icadecomp(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 array myarray, maps;
 doublereal *in_doubledata;

 /* vvvvvvvv This segment is from interfc.c */
	char *weights_in_f = NULL;
	
	int chans = 0, frames = 0, ncomps = 0;
	doublereal *dataA, *dataB, *weights, *sphere, *bias, *eigv;
	integer *signs;
 /* ^^^^^^^^ This segment is from interfc.c */

 if (tinfo->itemsize>1) {
  ERREXIT(tinfo->emethods, "icadecomp: Currently, only itemsize==1 is supported.\n");
 }

 tinfo_for_ica=tinfo;

 ncomps=args[ARGS_NCOMPONENTS].arg.i;
 /* Allow negative entries to mean a dimensionality reduction by this many dims */
 if (ncomps<0) ncomps+=tinfo->nr_of_channels;
 chans=tinfo->nr_of_channels;
 frames=tinfo->nr_of_points;
 if (ncomps==0) {
  ncomps=chans;
  pcaflag=0;
 } else {
  pcaflag=1;
  /* With PCA dimensionality reduction, sphering is also the default: */
  sphering=1;
 }

 /* vvvvvvvv This segment is from interfc.c */
	if (chans < 2) error("invalid number of channels");
	if (frames < 3) error("invalid data length");

	if (lrate == 0.0) lrate = DEFAULT_LRATE(chans);
	if (block == 0) block = DEFAULT_BLOCK(frames);
	if (ncomps == 0) ncomps = chans;

	if (ncomps > chans || ncomps < 1) error("invalid number of components");
	if (frames < chans) error("data length less than data channels");
	if (block < 2) error("block size too small!");
	if (block > frames) error("block size exceeds data length!");
	if (nsub > ncomps) error("sub-Gaussian components exceeds total number of components!");

	if (annealstep == 0.0)
		annealstep = (extendedflag) ? DEFAULT_EXTANNEAL : DEFAULT_ANNEALSTEP;

	if (extendedflag && extblocks>0) {
 		pdfsize = MIN(pdfsize,frames);
 		if (pdfsize < MIN_PDFSIZE)
			printf("warning, PDF values are inexact\n");
	}

	if (verbose) {
		printf("\nICA %s\n",VER_INFO);
		printf("\nInput data size [%d,%d] = %d channels, %d frames.\n",chans,frames,chans,frames);

		if (pcaflag) printf("After PCA dimension reduction,\n  finding ");
		else printf("Finding ");
	
		if (!extendedflag)
			printf("%d ICA components using logistic ICA.\n",ncomps);
		else {
			printf("%d ICA components using extendedflag ICA.\n",ncomps);
			
			if (extblocks > 0)
 				printf("PDF will be calculated initially every %d blocks using %d data points.\n",extblocks,pdfsize);
			else
 				printf("PDF will not be calculated. Exactly %d sub-Gaussian components assumed.\n",nsub);
		}
		
		printf("Initial learning rate will be %g, block size %d.\n",lrate,block);
		
		if (momentum > 0.0)
			printf("Momentum will be %g.\n",momentum);
			
		printf("Learning rate will be multiplied by %g whenever angledelta >= %g deg.\n",annealstep,annealdeg);
		printf("Training will end when wchange < %g or after %d steps.\n",nochange,maxsteps);
		
		if (biasflag)
			printf("Online bias adjustment will be used.\n");
		else
			printf("Online bias adjustment will not be used.\n");
	}

	dataA = (doublereal*)malloc(chans*frames*sizeof(doublereal));
 /* ^^^^^^^^ This segment is from interfc.c */

 tinfo_array(tinfo, &myarray);
 array_transpose(&myarray);	/* We want to read the data channels-fastest */
 in_doubledata=dataA;
 do {
  *in_doubledata++ = array_scan(&myarray);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 /* vvvvvvvv This segment is from interfc.c */
	weights = (doublereal*)malloc(ncomps*chans*sizeof(doublereal));
#if 0
	if (weights_in_f!=NULL) {
		if (verbose) printf("Loading weights from %s\n",weights_in_f);
		fb_matread(weights_in_f,ncomps*ncomps,weights);
	}
	else
#endif
		zero(ncomps*chans,weights);

	sphere = (doublereal*)malloc(chans*chans*sizeof(doublereal));

	if (biasflag)
		bias = (doublereal*)malloc(ncomps*sizeof(doublereal));
	else
		bias = NULL;
	
	if (extendedflag)
		signs = (integer*)malloc(ncomps*sizeof(integer));
	else
		signs = NULL;


/************************** Remove overall row means **************************/
	if (verbose) printf("Removing mean of each channel ...\n");
	rmmean(dataA,(integer)chans,(integer)frames);


/**************************** Perform PCA reduction ***************************/
	if (pcaflag) {
		if (verbose) printf("Reducing the data to %d principal dimensions...\n",ncomps);

		eigv = (doublereal*)malloc(chans*chans*sizeof(doublereal));
		pca(dataA,(integer)chans,(integer)frames,eigv);

#ifdef MMAP
		dataB = (doublereal*)mapmalloc(ncomps*frames*sizeof(doublereal));
		pcaproj(dataA,&eigv[chans*(chans-ncomps)],(integer)ncomps,(integer)frames,(integer)chans,dataB);
		mapfree(dataA,chans*frames*sizeof(doublereal));
#else
		dataB = (doublereal*)malloc(ncomps*frames*sizeof(doublereal));
		pcaproj(dataA,&eigv[chans*(chans-ncomps)],(integer)ncomps,(integer)frames,(integer)chans,dataB);
		free(dataA);
#endif
		dataA = dataB;
	}
	else
		eigv = NULL;
	
/**************************** Apply sphering matrix ***************************/
	if (sphering == 1) {
		if (verbose) printf("Computing the sphering matrix...\n");
		do_sphere(dataA,(integer)ncomps,(integer)frames,sphere);
		
		if (verbose) printf("Sphering the data ...\n");

#ifdef MMAP
		dataB = (doublereal*)mapmalloc(ncomps*frames*sizeof(doublereal));
		syproj(dataA,sphere,(integer)ncomps,(integer)frames,dataB);
		mapfree(dataA,ncomps*frames*sizeof(doublereal));
#else
		dataB = (doublereal*)malloc(ncomps*frames*sizeof(doublereal));
		syproj(dataA,sphere,(integer)ncomps,(integer)frames,dataB);
		free(dataA);
#endif
		dataA = dataB;
	}
	else if (sphering == 0) {
		if (weights_in_f==NULL) {
			if (verbose) printf("Using the sphering matrix as the starting weight matrix ...\n");
			do_sphere(dataA,(integer)ncomps,(integer)frames,weights);
		}

		if (verbose) printf("Returning the identity matrix in variable \"sphere\" ...\n");
		eye((integer)ncomps,sphere);
	}
	else if (sphering == -2) {
		if (verbose) printf("Returning the identity matrix in variable \"sphere\" ...\n");
		eye((integer)ncomps,sphere);
	}

	runica(dataA,weights,(integer)ncomps,(integer)frames,1,bias,signs);


/*************** Orient components toward positive activations ****************/
#ifdef MMAP
	dataB = (doublereal*)mapmalloc(ncomps*frames*sizeof(doublereal));
		
	if (posactflag) posact(dataA,weights,(integer)ncomps,(integer)frames,dataB);
	else geproj(dataA,weights,(integer)ncomps,(integer)frames,dataB);

	mapfree(dataA,ncomps*frames*sizeof(doublereal));
#else
	dataB = (doublereal*)malloc(ncomps*frames*sizeof(doublereal));
		
	if (posactflag) posact(dataA,weights,(integer)ncomps,(integer)frames,dataB);
	else geproj(dataA,weights,(integer)ncomps,(integer)frames,dataB);

	free(dataA);
#endif
	dataA = dataB;


/******* Sort components in descending order of max projected variance ********/
	if (verbose) {
		if (pcaflag) {
			printf("Composing the eigenvector, weights, and sphere matrices\n");
			printf("  into a single rectangular weights matrix; sphere=eye(%d)\n",chans);
		}
		printf("Sorting components in descending order of mean projected variance ...\n");
	}
	
	if (eigv) {
		varsort(dataA,weights,sphere,&eigv[chans*(chans-ncomps)],bias,signs,(integer)ncomps,(integer)frames,(integer)chans);
		eye((integer)chans,sphere);
	}
	else
		varsort(dataA,weights,sphere,NULL,bias,signs,(integer)ncomps,(integer)frames,(integer)chans);
 /* ^^^^^^^^ This segment is from interfc.c */

 /* Store the sphering matrix */
 if (args[ARGS_SPHEREFILE].is_set) {
  struct transform_info_struct tmptinfo= *tinfo;
  growing_buf buf;
#define BUFFER_SIZE 80
  char buffer[BUFFER_SIZE];

  maps.element_skip=1;
  maps.nr_of_vectors=maps.nr_of_elements=chans;
  if (array_allocate(&maps) == NULL) {
   ERREXIT(tinfo->emethods, "icadecomp: Error allocating memory\n");
  }
  in_doubledata=sphere;
  do {
   array_write(&maps, *in_doubledata++);
  } while (maps.message!=ARRAY_ENDOFSCAN);

  tmptinfo.nr_of_points=tmptinfo.nr_of_channels=chans;
  tmptinfo.itemsize=1;
  tmptinfo.length_of_output_region=chans*chans;
  tmptinfo.tsdata=maps.start;
  /* This doesn't really matter because sphere is symmetric: */
  tmptinfo.multiplexed=FALSE;
  select_writeasc(&tmptinfo);

  growing_buf_init(&buf);
  growing_buf_allocate(&buf, 0);
  snprintf(buffer, BUFFER_SIZE, "-b %s ", args[ARGS_SPHEREFILE].arg.s);
  growing_buf_appendstring(&buf, buffer);
  if (!buf.can_be_freed || !setup_method(&tmptinfo, &buf)) {
   ERREXIT(tinfo->emethods, "icadecomp: Error setting writeasc arguments.\n");
  }
  (*tmptinfo.methods->transform_init)(&tmptinfo);
  (*tmptinfo.methods->transform)(&tmptinfo);
  (*tmptinfo.methods->transform_exit)(&tmptinfo);
  growing_buf_free(&buf);
  array_free(&maps);
 }

 /* Store the result */
 maps.element_skip=1;
 tinfo->nr_of_points=maps.nr_of_elements=ncomps;
 maps.nr_of_vectors=chans;
 if (array_allocate(&maps) == NULL) {
  ERREXIT(tinfo->emethods, "icadecomp: Error allocating memory\n");
 }
 in_doubledata=weights;
 do {
  array_write(&maps, *in_doubledata++);
 } while (maps.message!=ARRAY_ENDOFSCAN);

 /* vvvvvvvv This segment is from interfc.c */
#ifdef MMAP
	if (dataA) mapfree(dataA,ncomps*frames*sizeof(doublereal));
#else
	if (dataA) free(dataA);
#endif

	if (weights) free(weights);
	if (sphere) free(sphere);
	if (bias) free(bias);
	if (signs) free(signs);
	if (eigv) free(eigv);
 /* ^^^^^^^^ This segment is from interfc.c */

 /* ICA returns the maps in points(=components)-fastest mode: */
 tinfo->multiplexed=FALSE;
 tinfo->length_of_output_region=tinfo->nr_of_channels*tinfo->nr_of_points*tinfo->itemsize;
 tinfo->beforetrig=0;
 tinfo->sfreq=1;
 free_pointer((void **)&tinfo->xdata);
 tinfo->xchannelname=NULL;
 return maps.start;
}
/*}}}  */

/*{{{  icadecomp_exit(transform_info_ptr tinfo) {*/
METHODDEF void
icadecomp_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_icadecomp(transform_info_ptr tinfo) {*/
GLOBAL void
select_icadecomp(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &icadecomp_init;
 tinfo->methods->transform= &icadecomp;
 tinfo->methods->transform_exit= &icadecomp_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="icadecomp";
 tinfo->methods->method_description=
  "icadecomp is the avg_q incorporation of the Independent Component Analysis\n"
  " method developed at the Salk Institute, San Diego, adapted and extended for\n"
  " EEG/ERP analysis by Dr. Scott Makeig et al. See\n"
  " http://www.cnl.salk.edu/~scott/icafaq.html for an overview and\n"
  " http://www.cnl.salk.edu/~enghoff/ for the `standalone' code by Sigurd Enghoff.\n"
  " icadecomp returns the resulting component maps in adjacent `points'.\n"
  " The activations can be calculated using the `project' method. n_components is\n"
  " the number of PCA components to which to reduce the data before the analysis.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
