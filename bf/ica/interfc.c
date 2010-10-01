/******************************************************************************/
/* For all support and information please contact:                            */
/*                                                                            */
/*   Sigurd Enghoff                                                           */
/*   The Salk Institute, CNL                                                  */
/*   enghoff@salk.edu                                                         */
/*                                                                            */
/* Additional ICA software:                                                   */
/*   http://www.cnl.salk.edu/~enghoff/                                        */
/*                                                                            */
/******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#ifdef PVM
#include "pvmica.h"
#endif

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "ica.h"

#ifdef _WIN32
#define isascii __isascii
#define isatty  _isatty
#define fileno  _fileno
#define strdup  _strdup
#endif

#define TOKENLEN 160      /* Number of characters allocated for tokens. */
#define BUFSIZE 0x10      /* Help file buffer size. */
#define HELPFILE "ica.sc" /* File containing help message. */
#define COMM "!#%\0"      /* Characters to preceed comments. */

/* Globally defined variables */
integer pcaflag, sphering, posactflag, biasflag;


/**************************** Initialize variables ****************************/
/* Initialize global and external variables to their default values as        */
/* defined in ica.h                                                           */

void initdefaults () {
	pcaflag    = DEFAULT_PCAFLAG;
	sphering   = DEFAULT_SPHEREFLAG;
	posactflag = DEFAULT_POSACT;
	verbose    = DEFAULT_VERBOSE;
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


/***************************** Print help message *****************************/
/* Print help message retrieved from HELPFILE to standard output.             */

void help() {
	FILE *file = fopen(HELPFILE,"r");
	char *buffer;
	int items;

	if (!file) error("help file is missing");
	
	buffer = (char*)malloc((BUFSIZE+1)*sizeof(char));
	
	do {
		items = (int)fread(buffer,sizeof(char),BUFSIZE,file);
		buffer[items] = '\0';
		printf("%s",buffer);
	} while (items == BUFSIZE);
	
	fclose(file);
}


/************************ Convert string to lower case ************************/
/* Convert a null-terminated string to lower case.                            */
/*                                                                            */
/* str: char array (input/output)                                             */

void lower(char *str) {
	int i;
	for (i=0 ; str[i]!=0 ; i++) str[i] = (char)tolower(str[i]);
}


/**************************** Free linked key list ****************************/
/* Recursively dismantel a linked list of key elements.                       */
/*                                                                            */
/* keys: key pointer (input)                                                  */

void rmkeys(key *keys) {
	if (keys->prev) {
		rmkeys((key*)keys->prev);
		free(keys->prev);
	}
	free(keys->token);
}


/******************************* Parse a string *******************************/
/* Parse str for the tokens: on (1), off (0), and none (-2) and return the    */
/* corresponding values. Else, str is assumed to contain a boolean value      */
/* which state is returned.                                                   */
/*                                                                            */
/* str: char array (input)                                                    */

integer swtch(char *str) {
	lower(str);

	if (!strcmp(str,"on")) return 1;
	if (!strcmp(str,"off")) return 0;
	if (!strcmp(str,"none")) return -2;
	return atoi(str) > 0;
}


/**************************** Check file contents *****************************/
/* Checks file for binary (none-ascii) contents, i.e. bytes with values >127. */
/* Returns 1 if file contains binary data else 0.                             */
/*                                                                            */
/* file: FILE pointer (input)                                                 */

int isbin(FILE *file) {
	int ch;
	do ch = fgetc(file);
	while (isascii(ch) && ch!=EOF);

	fseek(file,0,SEEK_SET);
	return ch != EOF;
}


/***************************** Check file access ******************************/
/* Checks the access permission for the file named fname. The file is created */
/* with no contence.                                                          */
/*                                                                            */
/* fname: char array (input)                                                  */

int faccess(char *fname) {
	FILE *file = fopen(fname,"wb");

	if (!file) return 0;

	fclose(file);
	return 1;
}


/*************************** Print message and exit ***************************/
/* Prints the error message contain in str and exit with exit code -1.        */
/*                                                                            */
/* str: char array (input)                                                    */

void error(char *str) {
	fprintf(stderr,"\n\aica: %s\n\n",str);

#ifdef PVM
	fflush(stdout);
	pvm_exit();
#endif

	exit(-1);
}


/*************************** Read binary float file ***************************/
/* Read a total of size floting point values from the file specified by       */
/* fname. Subsequently convert and store the values as doublereal in mat.     */
/*                                                                            */
/* fname: char array (input)                                                  */
/* size:  int (input)                                                         */
/* mat:   doublereal array [size] (output)                                    */

void fb_matread(char *fname, int size, doublereal *mat) {
	FILE *file = fopen(fname,"rb");
	float *buffer;
	int i, items;

	if (!file) error("open failed");

#ifdef MMAP
	buffer = (float*)mapmalloc(size*sizeof(float));
#else
	buffer = (float*)malloc(size*sizeof(float));
#endif

	items = (int)fread(buffer,sizeof(float),size,file);
	if (items != size) error("invalid number of elements");
	
	for (i=0 ; i<size ; i++) mat[i] = (doublereal)buffer[i];

#ifdef MMAP
	mapfree(buffer,size*sizeof(float));
#else
	free(buffer);
#endif

	fclose(file);
}


/*************************** Write binary float file **************************/
/* Convert size doublereal elements of mat to floating point values and store */
/* the values in the binary file specified by fname.                          */
/*                                                                            */
/* fname: char array (input)                                                  */
/* size:  int (input)                                                         */
/* mat:   doublereal array [size] (input)                                     */

void fb_matwrite(char *fname, int size, doublereal *mat) {
	FILE *file = fopen(fname,"wb");
	float *buffer;
	int i, items;

	if (!file) error("open failed");

#ifdef MMAP
	buffer = (float*)mapmalloc(size*sizeof(float));
#else
	buffer = (float*)malloc(size*sizeof(float));
#endif

	for (i=0 ; i<size ; i++) buffer[i] = (float)mat[i];
	
	items = (int)fwrite(buffer,sizeof(float),size,file);
	if (items != size) error("invalid number of elements");
	
#ifdef MMAP
	mapfree(buffer,size*sizeof(float));
#else
	free(buffer);
#endif

	fclose(file);
}


/************************** Write ascii integer file **************************/
/* Write size integer values of mat as ascii numbers to the file specified by */
/* fname.                                                                     */
/*                                                                            */
/* fname: char array (input)                                                  */
/* size:  int (input)                                                         */
/* mat:   integer array [size] (input)                                        */

void ia_matwrite(char *fname, int size, integer *mat) {
	FILE *file = fopen(fname,"w");
	int i;

	if (!file) error("open failed");
	for (i=0 ; i<size ; i++) fprintf(file,"%d\n",(int)(mat[i]));
	fclose(file);
}


/*************************** Process linked key list **************************/
/* Process the linked key list pointed to by keys. The processing includes    */
/* interpreting tokens, pre-processing, ICA-decomposition, and post-          */
/* processing.                                                                */
/*                                                                            */
/* keys: key pointer (input)                                                  */

void doit(key *keys) {
	char *keyword, *value;
	
	char *data_f = NULL;
	char *act_f = NULL;
	char *weights_in_f = NULL;
	char *weights_out_f = NULL;
	char *sphere_f = NULL;
	char *bias_f = NULL;
	char *sign_f = NULL;
	
	int i, datasize, chans = 0, frames = 0, epochs = 0, datalength = 0, ncomps = 0;
	doublereal *dataA, *dataB, *weights, *sphere, *bias, *eigv;
	integer *signs;

#ifdef PVM
	int   window[NWINDOW];
	char *fnames[3];
	
	for (i=0 ; i<NWINDOW ; i++) window[i] = -1;

	fprintf(stderr,"\nICA/PVM %s\n",VER_INFO);
#else
	fprintf(stderr,"\nICA %s\n",VER_INFO);
#endif

	initdefaults();

/********************* Argument parsing and error checking ********************/
	while (keys) {
/* Extract next value from linked keys list */
		value = keys->token;
		keys = (key*)(keys->prev);
		
		if (!keys) error("even number of input arguments");
		
/* Extract next keyword from linked keys list */
		keyword = keys->token;
		keys = (key*)(keys->prev);
		lower(keyword);
		
/* Keyword: datafile */
		if (!strcmp(keyword,"datafile"))
			data_f = value;
				
/* Keyword: activationsfile */
		else if (!strcmp(keyword,"activationsfile"))
			act_f = value;
				
/* Keyword: weightsinfile */
		else if (!strcmp(keyword,"weightsinfile"))
			weights_in_f = value;
		
/* Keyword: weightsoutfile */
		else if (!strcmp(keyword,"weightsoutfile"))
			weights_out_f = value;
		
/* Keyword: spherefile */
		else if (!strcmp(keyword,"spherefile"))
			sphere_f = value;
		
/* Keyword: biasfile */
		else if (!strcmp(keyword,"biasfile"))
			bias_f = value;

/* Keyword: signfile */
		else if (!strcmp(keyword,"signfile"))
			sign_f = value;

/* Keywords: chans, chan */
		else if (!strcmp(keyword,"chans") || !strcmp(keyword,"chan")) {
			chans = atoi(value);
			if (chans < 2) error("chans value should be the number of input channels");
		}

#ifdef PVM
/* Keywords: frame, frames */
		else if (!strcmp(keyword,"frame") || !strcmp(keyword,"frames")) {
			frames = atoi(value);
			if (frames < 2) error("frames value should be the number of data points per epoch");
		}

/* Keywords: epoch, epochs */
		else if (!strcmp(keyword,"epoch") || !strcmp(keyword,"epochs")) {
			epochs = atoi(value);
			if (epochs < 2) error("epochs value should be the total number of epochs");
		}
#else
/* Keywords: frames, datalength */
		else if (!strcmp(keyword,"frames") || !strcmp(keyword,"datalength")) {
			datalength = atoi(value);
			if (datalength < 2) error("frames value should be the number of data points");
		}
#endif

/* Keyword: pca */
		else if (!strcmp(keyword,"pca")) {
			ncomps = atoi(value);
			if (ncomps != 0) {
				pcaflag = 1;
				if (ncomps < 1)
					error("pca value should be the number of principal components to retain");
			}
			else
				pcaflag = 0;
		}

/* Keyword: lrate */
		else if (!strcmp(keyword,"lrate")) {
			lrate = atof(value);
			if (lrate>MAX_LRATE || lrate<MIN_LRATE)
				error("lrate value is out of bounds");
		}

/* Keywords: block, blocksize */
		else if (!strcmp(keyword,"block") || !strcmp(keyword,"blocksize")) {
			block = atoi(value);
			if (block < 0)
				error("block size value must be positive");
		}
		
/* Keywords: stop, nochange */
		else if (!strcmp(keyword,"stop") || !strcmp(keyword,"nochange") || !strcmp(keyword,"stopping")) {
			nochange = atof(value);
			if (nochange < 0.0)
				error("stop wchange value must be positive");
		}

/* Keywords: maxsteps, steps */
		else if (!strcmp(keyword,"maxsteps") || !strcmp(keyword,"steps")) {
				maxsteps = atoi(value);
				if (maxsteps < 0)
					error("maxsteps value must be a positive integer");
		}

/* Keywords: anneal, annealstep */
		else if (!strcmp(keyword,"anneal") || !strcmp(keyword,"annealstep")) {
			annealstep = atof(value);
			if (annealstep<=0 || annealstep>1)
				error("anneal step value must be (0,1]");
		}

/* Keywords: annealdeg, degrees */
		else if (!strcmp(keyword,"annealdeg") || !strcmp(keyword,"degrees")) {
			annealdeg = atof(value);
			if (annealdeg>180 || annealdeg<0)
				error("annealdeg value is out of bounds [0,180]");
		}

/* Keyword: momentum */
		else if (!strcmp(keyword,"momentum")) {
			momentum = atof(value);
			if (momentum>1.0 || momentum<0.0)
				error("momentum value is out of bounds [0,1]");
		}

		else if (!strcmp(keyword,"sphering") || !strcmp(keyword,"sphereing") || !strcmp(keyword,"sphere")) {
			sphering = swtch(value);
			if (sphering == -1) error("sphering value must be on, off, or none");
		}

/* Keyword: bias */
		else if (!strcmp(keyword,"bias")) {
			biasflag = swtch(value);
			if (biasflag < 0) error("bias value must be on or off");
		}

/* Keywords: extended, extend */
		else if (!strcmp(keyword,"extended") || !strcmp(keyword,"extend")) {
			extblocks = atoi(value);
			extendedflag = 1;
					
			if (extblocks == 0) extendedflag = 0;
			else
				if (extblocks < 0) nsub = -extblocks;
		}

/* Keyword: posact */
		else if (!strcmp(keyword,"posact")) {
			posactflag = swtch(value);
			if (posactflag < 0) error("posact value must be on or off");
		}
			
/* Keyword: verbose */
		else if (!strcmp(keyword,"verbose")) {
			verbose = swtch(value);
			if (verbose < 0) error("verbose flag value must be on or off");
		}
		
#ifdef PVM
/* Keyword: framewindow */
		else if (!strcmp(keyword,"framewindow")) {
			window[FRAMEWINDOW] = atoi(value);
			if (window[FRAMEWINDOW] <= 0) error("framewindow value must be positive");
		}
/* Keyword: framestep */
		else if (!strcmp(keyword,"framestep")) {
			window[FRAMESTEP] = atoi(value);
			if (window[FRAMESTEP] <= 0) error("framestep value must be positive");
		}
/* Keyword: epochwindow */
		else if (!strcmp(keyword,"epochwindow")) {
			window[EPOCHWINDOW] = atoi(value);
			if (window[EPOCHWINDOW] <= 0) error("epochwindow value must be positive");
		}
/* Keyword: epochstep */
		else if (!strcmp(keyword,"epochstep")) {
			window[EPOCHSTEP] = atoi(value);
			if (window[EPOCHSTEP] <= 0) error("epochstep value must be positive");
		}
/* Keyword: baseline */
		else if (!strcmp(keyword,"baseline")) {
			window[BASELINE] = atoi(value);
			if (window[BASELINE] <= 0) error("Length of baseline must be positive");
		}
#endif
		else
			error("unknown flag");
	}


#ifdef PVM
	datalength = frames*epochs;

	if (window[FRAMEWINDOW] < 0) window[FRAMEWINDOW] = frames;
	if (window[FRAMESTEP]   < 0) window[FRAMESTEP]   = 1;
	if (window[EPOCHWINDOW] < 0) window[EPOCHWINDOW] = epochs;
	if (window[EPOCHSTEP]   < 0) window[EPOCHSTEP]   = 1;
	if (window[BASELINE]    < 0) window[BASELINE]    = 25;

	if (window[FRAMEWINDOW] > frames) error("window frame length must be <= frames");
	if (window[FRAMESTEP]   > frames) error("frame step size must be <= frames");
	if (window[EPOCHWINDOW] > epochs) error("window epoch length must be <= epochs");
	if (window[EPOCHSTEP]   > epochs) error("epoch step size must be <= epochs");
	if (window[BASELINE]    > frames) error("length of baseline must be <= frames");

	datasize = window[FRAMEWINDOW] * window[EPOCHWINDOW];
#else
	datasize = datalength;
#endif

	if (chans < 2) error("invalid number of channels");
	if (datasize < 3) error("invalid data length");

	if (lrate == 0.0) lrate = DEFAULT_LRATE(chans);
	if (block == 0) block = DEFAULT_BLOCK(datasize);
	if (ncomps == 0) ncomps = chans;

	if (ncomps > chans || ncomps < 1) error("invalid number of components");
	if (datasize < chans) error("data length less than data channels");
	if (block < 2) error("block size too small!");
	if (block > datasize) error("block size exceeds data length!");
	if (nsub > ncomps) error("sub-Gaussian components exceeds total number of components!");

	if (annealstep == 0.0)
		annealstep = (extendedflag) ? DEFAULT_EXTANNEAL : DEFAULT_ANNEALSTEP;


	if (extendedflag && extblocks>0) {
		pdfsize = MIN(pdfsize,datalength);
		if (pdfsize < MIN_PDFSIZE)
			fprintf(stderr,"ica: warning, PDF values are inexact\n");
	}


	if (data_f==NULL)
		error("input data file required");

	if (weights_out_f==NULL)
		error("output weights file required");
		
	if (sphere_f==NULL)
		error("output sphering file required");


	if (!faccess(weights_out_f))
		error("weights output file not writable");

	if (!faccess(sphere_f))
		error("sphere file not writable");


	if (act_f!=NULL && !faccess(act_f))
		error("activations file not writable");

	if (bias_f!=NULL && !faccess(bias_f))
		error("bias file not writable");

	if (sign_f!=NULL && !faccess(sign_f))
		error("sign file not writable");

/****************************** Process the data ******************************/
	if (verbose) {
#ifdef PVM
		printf("\nInput data size [%d,%d] = %d channels, %d epoch of %d frames.\n",chans,datalength,chans,epochs,frames);
#else
		printf("\nInput data size [%d,%d] = %d channels, %d frames.\n",chans,datalength,chans,datalength);
#endif
		if (pcaflag) printf("After PCA dimension reduction,\n  finding ");
		else printf("Finding ");
	
		if (!extendedflag)
			printf("%d ICA components using logistic ICA.\n",ncomps);
		else {
			printf("%d ICA components using extended ICA.\n",ncomps);
			
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
	
/******************************* Allocate memory ******************************/
	if (verbose) printf("\nLoading data from %s\n",data_f);

#ifdef MMAP
	dataA = (doublereal*)mapmalloc(chans*datalength*sizeof(doublereal));
#else
	dataA = (doublereal*)malloc(chans*datalength*sizeof(doublereal));
#endif

	fb_matread(data_f,chans*datalength,dataA);

	weights = (doublereal*)malloc(ncomps*chans*sizeof(doublereal));
	if (weights_in_f!=NULL) {
		if (verbose) printf("Loading weights from %s\n",weights_in_f);
		fb_matread(weights_in_f,ncomps*ncomps,weights);
	}
	else
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
	rmmean(dataA,(integer)chans,(integer)datalength);


/**************************** Perform PCA reduction ***************************/
	if (pcaflag) {
		if (verbose) printf("Reducing the data to %d principal dimensions...\n",ncomps);

		eigv = (doublereal*)malloc(chans*chans*sizeof(doublereal));
		pca(dataA,(integer)chans,(integer)datalength,eigv);

#ifdef MMAP
		dataB = (doublereal*)mapmalloc(ncomps*datalength*sizeof(doublereal));
		pcaproj(dataA,&eigv[chans*(chans-ncomps)],(integer)ncomps,(integer)datalength,(integer)chans,dataB);
		mapfree(dataA,chans*datalength*sizeof(doublereal));
#else
		dataB = (doublereal*)malloc(ncomps*datalength*sizeof(doublereal));
		pcaproj(dataA,&eigv[chans*(chans-ncomps)],(integer)ncomps,(integer)datalength,(integer)chans,dataB);
		free(dataA);
#endif
		dataA = dataB;
	}
	else
		eigv = NULL;
	
/**************************** Apply sphering matrix ***************************/
	if (sphering == 1) {
		if (verbose) printf("Computing the sphering matrix...\n");
		do_sphere(dataA,(integer)ncomps,(integer)datalength,sphere);
		
		if (verbose) printf("Sphering the data ...\n");

#ifdef MMAP
		dataB = (doublereal*)mapmalloc(ncomps*datalength*sizeof(doublereal));
		syproj(dataA,sphere,(integer)ncomps,(integer)datalength,dataB);
		mapfree(dataA,ncomps*datalength*sizeof(doublereal));
#else
		dataB = (doublereal*)malloc(ncomps*datalength*sizeof(doublereal));
		syproj(dataA,sphere,(integer)ncomps,(integer)datalength,dataB);
		free(dataA);
#endif
		dataA = dataB;
	}
	else if (sphering == 0) {
		if (weights_in_f==NULL) {
			if (verbose) printf("Using the sphering matrix as the starting weight matrix ...\n");
			do_sphere(dataA,(integer)ncomps,(integer)datalength,weights);
		}

		if (verbose) printf("Returning the identity matrix in variable \"sphere\" ...\n");
		eye((integer)ncomps,sphere);
	}
	else if (sphering == -2) {
		if (verbose) printf("Returning the identity matrix in variable \"sphere\" ...\n");
		eye((integer)ncomps,sphere);
	}

#ifdef PVM
	fnames[0] = weights_out_f;
	fnames[1] = bias_f;
	fnames[2] = sign_f;
	pvmica(dataA,weights,sphere,eigv,(integer)chans,(integer)ncomps,(integer)frames,(integer)epochs,window,bias,signs,fnames);
#else
	runica(dataA,weights,(integer)ncomps,(integer)datalength,1,bias,signs);

/*************** Orient components toward positive activations ****************/
#ifdef MMAP
	dataB = (doublereal*)mapmalloc(ncomps*datalength*sizeof(doublereal));

	if (posactflag) posact(dataA,weights,(integer)ncomps,(integer)datalength,dataB);
	else geproj(dataA,weights,(integer)ncomps,(integer)datalength,dataB);

	mapfree(dataA,ncomps*datalength*sizeof(doublereal));
#else
	dataB = (doublereal*)malloc(ncomps*datalength*sizeof(doublereal));
		
	if (posactflag) posact(dataA,weights,(integer)ncomps,(integer)datalength,dataB);
	else geproj(dataA,weights,(integer)ncomps,(integer)datalength,dataB);

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
		varsort(dataA,weights,sphere,&eigv[chans*(chans-ncomps)],bias,signs,(integer)ncomps,(integer)datalength,(integer)chans);
		eye((integer)chans,sphere);
	}
	else
		varsort(dataA,weights,sphere,NULL,bias,signs,(integer)ncomps,(integer)datalength,(integer)chans);


/**************************** Write results to disk ***************************/
	if (verbose) printf("Storing weights in %s\n",weights_out_f);
	fb_matwrite(weights_out_f,ncomps*chans,weights);
	
	if (act_f!=NULL) {
		if (verbose) printf("Storing activations in %s\n",act_f);
		fb_matwrite(act_f,ncomps*datalength,dataA);
	}
	
	if (bias_f!=NULL && bias) {
		if (verbose) printf("Storing bias vector in %s\n",bias_f);
		fb_matwrite(bias_f,ncomps,bias);
	}
	
	if (sign_f!=NULL && signs) {
		if (verbose) printf("Storing sign vector in %s\n",sign_f);
		for (i=0 ; i<ncomps ; i++) signs[i] = (signs[i]) ? (-1) : 1;
		ia_matwrite(sign_f,ncomps,signs);
	}
#endif

	if (verbose) printf("Storing sphering matrix in %s\n",sphere_f);
	fb_matwrite(sphere_f,chans*chans,sphere);

#ifdef MMAP
	if (dataA) mapfree(dataA,ncomps*datalength*sizeof(doublereal));
#else
	if (dataA) free(dataA);
#endif

	if (weights) free(weights);
	if (sphere) free(sphere);
	if (bias) free(bias);
	if (signs) free(signs);
	if (eigv) free(eigv);
}


/******************************** Script parser *******************************/
/* Creates a linked list of keywords and values based on the redirected       */
/* script file.                                                               */
/*                                                                            */
/* argc: int (input)                                                          */
/* argv: char array pointer (input)                                           */

int master(int argc, char **argv) {
	char token[TOKENLEN];
	key  *pkey, *keys = NULL;
	int  gchar;
	FILE *scriptfile;

/* Print help message and exit if arguments were supplied. */
	if (argc != 2) {
		help();
		return 0;
	}

	if (strcmp(argv[1], "stdin")==0) {
         scriptfile=stdin;
	} else {
	 if (NULL==(scriptfile=fopen(argv[1], "r"))) {
	  error("ica: Can't open script file");
	 }
	}

/* Print help and error message if no file was redirected. */
	if (isatty(fileno(scriptfile))) {
		help();
 		error("takes an ascii script file as redirected input");
	}

/* Print help and error message if a redirected file contains binary data. */
	if (isbin(scriptfile)) {
		help();
 		error("takes an ascii script file as input, not a binary file");
	}

/* Decompose script file into a linked list of tokens. */
	while (1) {
		if (fscanf(scriptfile, "%s",token) < 0) break;

		if (strchr(COMM,(int)token[0]))
			do gchar = fgetc(scriptfile);
			while (gchar!=EOL && gchar!=EOF);
		else {
			pkey = keys;
			keys = (key*)malloc(sizeof(key));
			keys->prev = (void*)pkey;
			keys->token = strdup(token);
		}
	}

	if (scriptfile!=stdin) fclose(scriptfile);

/* Process token list - do everything */
	doit(keys);

/* Dismantle token list */
	rmkeys(keys);
	
	return 0;
}


#ifdef PVM

/************************ OS entry point (PVM version) ************************/
/* Separate master and slave processes and calls their respective subroutines.*/
/*                                                                            */
/* argc: int (input)                                                          */
/* argv: char array pointer (input)                                           */

int main(int argc, char **argv) {
	int rtnval, parent;

	parent = pvm_parent();

	if (parent < 0 && parent != PvmNoParent) error("Invalid parent process");
	rtnval = (parent == PvmNoParent) ? master(argc,argv) : slave();

	pvm_exit();
	return rtnval;
}

#else

/********************** OS entry point (non-PVM version) **********************/
/* Dummy routine. Allows PVM and non-PVM versions to both access master (old  */
/* main)                                                                      */
/*                                                                            */
/* argc: int (input)                                                          */
/* argv: char array pointer (input)                                           */

int main(int argc, char **argv) {
	return master(argc,argv);
}

#endif
