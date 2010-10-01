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
#include <time.h>
#include <math.h>
#include "ica.h"

#ifdef R250
#include "r250.h"
#endif

/* Global and externally accessible variables */
integer extendedflag, extblocks, pdfsize, nsub;
#ifndef FOR_AVG_Q
integer verbose;
#endif
integer block, maxsteps;

doublereal lrate, annealstep, annealdeg, nochange, momentum;


/***************************** Zero-fill a array ******************************/
/* Zero-fill array A of n elements.                                           */
/*                                                                            */
/* n: integer (input)                                                         */
/* A: double array [n] (output)                                                 */

void zero(integer n, doublereal *A) {
	doublereal dzero = 0.0;
	integer izero = 0, ione = 1;

	dcopy_(&n,&dzero,&izero,A,&ione);
}

/************************ Construct an identity matrix ************************/
/* Construct an identity matrix A of size m x m.                              */
/*                                                                            */
/* m: integer (input)                                                         */
/* A: double array [m,m] (output)                                             */

void eye(integer m, doublereal *A) {
	doublereal dzero = 0.0, done = 1.0;
	integer izero = 0, ione = 1;
	integer mxm = m*m, m1 = m+1;

	dcopy_(&mxm,&dzero,&izero,A,&ione);
	dcopy_(&m,&done,&izero,A,&m1);
}


/*********************** Initialize a permutation vector **********************/
/* Initialize a vector perm of n elements for constructing a random.          */
/*                                                                            */
/* permutation of the number in the range [0..n-1]                            */
/* perm: integer array [n] (output)                                           */
/* n:    integer (input)                                                      */

void initperm(integer *perm, integer n) {
	integer i;
	for (i=0 ; i<n ; i++) perm[i]=i;
}


/******************* Obtain and project a random permutation ******************/
/* n elements of vector perm are picked at random and used as indecies into   */
/* data. The designated elements of data are projected by trsf employing the  */
/* bias, if not NULL. k denotes the number of elements of perm left to be     */
/* extracted; perm is updated while calling routine must update k. The        */
/* resulting projections are returned in proj.                                */
/*                                                                            */
/* data: double array [m,?] (input)                                           */
/* trsf: double array [m,m] (input)                                           */
/* bias: double array [m] or NULL (input)                                     */
/* perm: integer array [k>] (input/output)                                    */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* k:    integer (input)                                                      */
/* proj: double array [m,n] (output)                                          */

void randperm(doublereal *data, doublereal *trsf, doublereal *bias, integer *perm, integer m, integer n, integer k, doublereal *proj) {
	integer i, im, swap, inc = 1;
	doublereal alpha = 1.0, beta;
	char trans='N';
	
	if (bias) {
		for (i=0,im=0 ; i<n ; i++,im+=m) dcopy_(&m,bias,&inc,&proj[im],&inc);
		beta = 1.0;
	}
	else
		beta = 0.0;

	for (i=0,im=0 ; i<n ; i++,im+=m) {
		swap = RAND()%k;
		dgemv_(&trans,&m,&m,&alpha,trsf,&m,&data[perm[swap]*m],&inc,&beta,&proj[im],&inc,1);
		perm[swap] = perm[--k];
	}
}


/********* Obtain and project data points from Laplacian distribution *********/
/* n samples are picked from a Laplacian distribution centered at the center  */
/* point of a length frames window. Data points are extracted from frames in  */
/* data according to the windowed distribution while chosen uniformly over    */
/* epochs. The designated elements of data are projected by trsf employing    */
/* the bias, if not NULL. The resulting projections are returned in proj.     */
/*                                                                            */
/* data:   double array [m,?] (input)                                         */
/* trsf:   double array [m,m] (input)                                         */
/* bias:   double array [m] or NULL (input)                                   */
/* m:      integer (input)                                                    */
/* n:      integer (input)                                                    */
/* frames: integer (input)                                                    */
/* epochs: integer (input)                                                    */
/* proj:   double array [m,n] (output)                                        */

void probperm(doublereal *data, doublereal *trsf, doublereal *bias, integer m, integer n, integer frames, integer epochs, doublereal *proj) {
	integer i, im, idx, norm, prob, inc = 1;
	doublereal alpha = 1.0, beta;
	char trans='N';

	if (bias) {
		for (i=0,im=0 ; i<n ; i++,im+=m) dcopy_(&m,bias,&inc,&proj[im],&inc);
		beta = 1.0;
	}
	else
		beta = 0.0;

	norm = (1<<((frames+1)/2+1))-2*((frames%2)-1);

	for (i=0,im=0 ; i<n ; i++,im+=m) {
		prob = RAND()%norm;

		if (prob & 1) {
			idx = 0;
			prob = (prob>>1) + 1;
			while (prob > 1) {
				prob >>= 1;
				idx++;
			}
		}
		else {
			idx = frames-1;
			prob = (prob>>1) + 1;
			while (prob > 1) {
				prob >>= 1;
				idx--;
			}
		}

		idx += frames*(RAND()%epochs);
		dgemv_(&trans,&m,&m,&alpha,trsf,&m,&data[idx*m],&inc,&beta,&proj[im],&inc,1);
	}
}


/**************************** Compute PDF estimate ****************************/
/* If perm is not NULL, n elements of vector perm are picked at random and    */
/* used as indecies into data. If perm is NULL, the first n elements of data  */
/* are referenced. The designated n elements of data used for the Kurosis     */
/* estimate. k denotes the number of elements of perm left to be extracted;   */
/* perm is updated while calling routine must update k. The resulting         */
/* PDF estimate is returned in kk.                                            */
/*                                                                            */
/* data: double array [m,? or n] (input)                                      */
/* trsf: double array [m,m] (input)                                           */
/* perm: integer array [k>] (input/output) or NULL                            */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* k:    integer (input)                                                      */
/* kk:   double array [m,n] (output)                                          */

void pdf(doublereal *data, doublereal *trsf, integer *perm, integer m, integer n, integer k, doublereal *kk) {
	integer i, j, im, swap, inc = 1;
	doublereal alpha = 1.0, beta = 0.0, tmp;
	char trans='N';

#ifdef KURTOSIS_ESTIMATE
    zero(2*m,&kk[m]);

    for (i=0,im=0 ; i<n ; i++,im+=m) {
        if (perm) {
            swap = RAND()%k;
            dgemv_(&trans,&m,&m,&alpha,trsf,&m,&data[perm[swap]*m],&inc,&beta,kk,&inc,1);
            perm[swap] = perm[--k];
        }
        else
            dgemv_(&trans,&m,&m,&alpha,trsf,&m,&data[im],&inc,&beta,kk,&inc,1);
        
        for (j=0 ; j<m ; j++) {
            tmp = kk[j]*kk[j];
            kk[j+m] += tmp;
            kk[j+2*m] += tmp*tmp;
        }
    }

    for (i=0 ; i<m ; i++)
        kk[i] = kk[i+2*m]*(doublereal)n / (kk[i+m]*kk[i+m]) - 3.0;

#else

	zero(3*m,&kk[m]);

	for (i=0,im=0 ; i<n ; i++,im+=m) {
		if (perm) {
			swap = RAND()%k;
			dgemv_(&trans,&m,&m,&alpha,trsf,&m,&data[perm[swap]*m],&inc,&beta,kk,&inc,1);
			perm[swap] = perm[--k];
		}
		else
			dgemv_(&trans,&m,&m,&alpha,trsf,&m,&data[im],&inc,&beta,kk,&inc,1);
		
		for (j=0 ; j<m ; j++) {
			tmp = 2.0 / (exp(kk[j])+exp(-kk[j]));
			kk[j+m] += tmp*tmp;
			kk[j+2*m] += kk[j]*kk[j];
			kk[j+3*m] += tanh(kk[j])*kk[j];
		}
	}

	for (i=0 ; i<m ; i++)
		kk[i] = (kk[i+m]*kk[i+2*m]/((doublereal)n)-kk[i+3*m])/((doublereal)n);
#endif
}


/******************* Project data using a general projection ******************/
/* Project data using trsf. Return projections in proj.                       */
/*                                                                            */
/* data: double array [m,n] (input)                                           */
/* trsf: double array [m,m] (input)                                           */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* proj: double array [m,n] (output)                                          */

void geproj(doublereal *data, doublereal *trsf, integer m, integer n, doublereal *proj) {
	doublereal alpha = 1.0, beta = 0.0;
	char trans='N';

	dgemm_(&trans,&trans,&m,&n,&m,&alpha,trsf,&m,data,&m,&beta,proj,&m,1,1);
}


/***************** Project data using a symmetrical projection ****************/
/* Project data using the upper triangular part of trsf, assuming trsf is     */
/* symmetrical. Return projections in proj.                                   */
/*                                                                            */
/* data: double array [m,n] (input)                                           */
/* trsf: double array [m,m] (input)                                           */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* proj: double array [m,n] (output)                                          */

void syproj(doublereal *data, doublereal *trsf, integer m, integer n, doublereal *proj) {
	doublereal alpha = 1.0, beta = 0.0;
	char uplo='U', side = 'L';

	dsymm_(&side,&uplo,&m,&n,&alpha,trsf,&m,data,&m,&beta,proj,&m,1,1);
}


/******************* Project data using a reduced projection ******************/
/* Project data using the first k rows of trsf only. Return projections       */
/* in proj.                                                                   */
/*                                                                            */
/* data: double array [k,n] (input)                                           */
/* trsf: double array [m,k] (input)                                           */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* k:    integer (input)                                                      */
/* proj: double array [m,n] (output)                                          */

void pcaproj(doublereal *data, doublereal *eigv, integer m, integer n, integer k, doublereal *proj) {
	doublereal alpha = 1.0, beta = 0.0;
	char transn='N', transt='T';

	dgemm_(&transt,&transn,&m,&n,&k,&alpha,eigv,&k,data,&k,&beta,proj,&m,1,1);
}


/******************************* Used by varsort ******************************/

int compar(const void *x, const void *y) {
	if (((idxelm*)x)->val < ((idxelm*)y)->val) return 1;
	if (((idxelm*)x)->val > ((idxelm*)y)->val) return -1;
	return 0;
}

/****************** Sort data according to projected variance *****************/
/* Compute back-projected variances for each component based on inverse of    */
/* weights and sphere or pseudoinverse of weights, sphere, and eigv. Reorder  */
/* data and weights accordingly. Also if not NULL, reorder bias and signs.    */
/*                                                                            */
/* data:    double array [m,n] (input/output)                                 */
/* weights: double array [m,m] (input/output)                                 */
/* sphere:  double array [m,m] (input)                                        */
/* eigv:    double array [m,k] (input) or NULL                                */
/* bias:    double array [m] (input/output) or NULL                           */
/* signs:   integer array [m] (input/output) or NULL                          */
/* m:       integer (input)                                                   */
/* n:       integer (input)                                                   */
/* k:       integer (input)                                                   */

void varsort(doublereal *data, doublereal *weights, doublereal *sphere, doublereal *eigv, doublereal *bias, integer *signs, integer m, integer n, integer k) {
	char name[] = "DGETRI\0", opts[] = "\0";
	doublereal alpha = 1.0, beta = 0.0;
	integer i, j, l, jm, ik, info = 0, ispec = 1, na = -1;
	char transn='N', transt='T', uplo='U', side = 'R';
	
	integer nb = ilaenv_(&ispec,name,opts,&m,&na,&na,&na,6,0);
	integer itmp, lwork = m*nb, inc = 1;
	doublereal act, dtmp, *wcpy;

	integer    *ipiv = (integer*)malloc(m*sizeof(integer));
	doublereal *work = (doublereal*)malloc(lwork*sizeof(doublereal));
	doublereal *winv = (doublereal*)malloc(m*k*sizeof(doublereal));
	doublereal *sum  = (doublereal*)malloc(n*sizeof(doublereal));
	idxelm  *meanvar = (idxelm*)malloc(m*sizeof(idxelm));
	
	if (eigv) {
/* Compute pseudoinverse of weights*sphere*eigv */
		wcpy = (doublereal*)malloc(m*m*sizeof(doublereal));
		/* wcpy=weights*sphere */
		dsymm_(&side,&uplo,&m,&m,&alpha,sphere,&m,weights,&m,&beta,wcpy,&m,1,1);
		/* weights=wcpy*eigv' */
		dgemm_(&transn,&transt,&m,&k,&m,&alpha,wcpy,&m,eigv,&k,&beta,weights,&m,1,1);

		/* LU decomp */
		dgetrf_(&m,&m,wcpy,&m,ipiv,&info);
		/* Inverse using LU */
		dgetri_(&m,wcpy,&m,ipiv,work,&lwork,&info);
		dgemm_(&transn,&transn,&k,&m,&m,&alpha,eigv,&k,wcpy,&m,&beta,winv,&k,1,1);
		free(wcpy);
	}
	else {
/* Compute inverse of weights*sphere */
		dsymm_(&side,&uplo,&m,&m,&alpha,sphere,&m,weights,&m,&beta,winv,&m,1,1);

		dgetrf_(&m,&m,winv,&m,ipiv,&info);
		dgetri_(&m,winv,&m,ipiv,work,&lwork,&info);
	}


/* Compute mean variances for back-projected components */
	for (i=0 ; i<m*k ; i++) winv[i] = winv[i]*winv[i];

	for (i=0,ik=0 ; i<m ; i++,ik+=k) {
		for (j=0,jm=0 ; j<n ; j++,jm+=m) {
			sum[j] = 0;
			act = data[i+jm]*data[i+jm];
			for(l=0 ; l<k ; l++) sum[j] += act*winv[l+ik];
		}
		
		meanvar[i].idx = i;
		meanvar[i].val = dsum_(&n,sum,&inc);
		if (verbose) printf("%d ",(int)(i+1));
	}
	if (verbose) printf("\n");
	
/* Sort meanvar */
	qsort(meanvar,m,sizeof(idxelm),compar);

	if (verbose) printf("Permuting the activation wave forms ...\n");

/* Perform in-place reordering of weights, data, bias, and signs */
	for (i=0 ; i<m-1 ; i++) {
		j = meanvar[i].idx;
		if (i != j) {
			dswap_(&k,&weights[i],&m,&weights[j],&m);
			dswap_(&n,&data[i],&m,&data[j],&m);

			if (bias) {
				dtmp = bias[i];
				bias[i] = bias[j];
				bias[j] = dtmp;
			}

			if (signs) {
				itmp = signs[i];
				signs[i] = signs[j];
				signs[j] = itmp;
			}
			
			for (l=i+1 ; i!=meanvar[l].idx ; l++);
			meanvar[l].idx = j;
		}
	}
	
	free(ipiv);
	free(work);
	free(winv);
	free(sum);
	free(meanvar);
}


/******************************* Remove row mean ******************************/
/* Remove row means from data.                                                */
/*                                                                            */
/* data: double array [m,n] (input/output)                                    */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */

void rmmean(doublereal *data, integer m, integer n) {
	doublereal *mean, alpha;
	integer i, j, one = 1;
	
	mean = (doublereal*)malloc(m*sizeof(doublereal));
	zero(m,mean);
	
	alpha = 1.0;
	for (i=0,j=0 ; i<n ; i++,j+=m) {
		daxpy_(&m,&alpha,&data[j],&one,mean,&one);
	}

	alpha = -1.0/(doublereal)n;
	for (i=0,j=0 ; i<n ; i++,j+=m) {
		daxpy_(&m,&alpha,mean,&one,&data[j],&one);
	}	
	
	free(mean);
}


/*************************** Compute sphering matrix **************************/
/* Compute sphering matrix based on data.                                     */
/*                                                                            */
/* data: double array [m,n] (input)                                           */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* sphe: double array [m,m] (output)                                          */

void do_sphere(doublereal *data, integer m, integer n, doublereal *sphe) {
	char name[] = "SSYTRD\0", opts[] = "U\0";
	doublereal alpha = 1.0/(doublereal)(n-1);
	doublereal beta = 0.0;
	integer info = 0, ispec = 1, na = -1;
	
	integer nb = ilaenv_(&ispec,name,opts,&m,&na,&na,&na,6,0);
	integer i, im, lwork = (nb+2)*m, inc = 1, mxm = m*m;

	char uplo='U', transn='N', jobz='V';
	doublereal *eigv = (doublereal*)malloc(m*m*sizeof(doublereal));
	doublereal *eigd = (doublereal*)malloc(m*sizeof(doublereal));
	integer    *ipiv = (integer*)malloc(m*sizeof(integer));
	doublereal *work = (doublereal*)malloc(lwork*sizeof(doublereal));

/*
   sphere = 2*inv(sqrtm(cov(data'))) is computes as:

   [v d] = eig(data*data')
   sphere = inv(diag(0.5 * sqrt(d))*v')*v'
*/

	dsyrk_(&uplo,&transn,&m,&n,&alpha,data,&m,&beta,sphe,&m,1,1);
	dsyev_(&jobz,&uplo,&m,sphe,&m,eigd,work,&lwork,&info,1,1);

/* Store transpose of sphe in eigv */
	for (i=0,im=0 ; i<m ; i++,im+=m)
		dcopy_(&m,&sphe[im],&inc,&eigv[i],&m);
	
/* Copy eigv to sphe */
	dcopy_(&mxm,eigv,&inc,sphe,&inc);
	
/* Scale rows of eigv by corresponding eigd values */
	for (i=0 ; i<m ; i++) {
		eigd[i] = 0.5 * sqrt(eigd[i]);
		dscal_(&m,&eigd[i],&eigv[i],&m);
	}

/* Solve eigv * sphere = sphe  ~  diag(0.5*sqrt(d))*v' * sphere = v' */
	dgesv_(&m,&m,eigv,&m,ipiv,sphe,&m,&info);

	free(eigv);
	free(eigd);
	free(ipiv);
	free(work);
}


/***************************** Compute PCA matrix *****************************/
/* Compute PCA decomposition matrix based on data.                            */
/*                                                                            */
/* data: double array [m,n] (input)                                           */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* trsf: double array [m,m] (output)                                          */

void pca(doublereal *data, integer m, integer n, doublereal *trsf) {
	char name[] = "SSYTRD\0", opts[] = "U\0";
	doublereal alpha = 1.0/(doublereal)(n-1);
	doublereal beta = 0.0;
	integer info = 0, ispec = 1, na = -1;
	
	integer nb = ilaenv_(&ispec,name,opts,&m,&na,&na,&na,6,0);
	integer lwork = (nb+2)*m;

	char uplo='U', transn='N', jobz='V';
	doublereal *eigd = (doublereal*)malloc(m*sizeof(doublereal));
	doublereal *work = (doublereal*)malloc(lwork*sizeof(doublereal));

	dsyrk_(&uplo,&transn,&m,&n,&alpha,data,&m,&beta,trsf,&m,1,1);
	dsyev_(&jobz,&uplo,&m,trsf,&m,eigd,work,&lwork,&info,1,1);
	
	free(eigd);
	free(work);
}


/*************** Orient components toward positive activations ****************/
/* Project data using trsf. Negate components and their corresponding weights */
/* to assure positive RMS. Returns projections in proj.                       */
/*                                                                            */
/* data: double array [m,n] (input)                                           */
/* trsf: double array [m,m] (input/output)                                    */
/* m:    integer (input)                                                      */
/* n:    integer (input)                                                      */
/* proj: double array [m,n] (output)                                          */

void posact(doublereal *data, doublereal *trsf, integer m, integer n, doublereal *proj) {
	char trans='N';
	doublereal alpha = 1.0;
	doublereal beta = 0.0;
	doublereal posrms, negrms;
	integer pos, neg, i, j;
	
	dgemm_(&trans,&trans,&m,&n,&m,&alpha,trsf,&m,data,&m,&beta,proj,&m,1,1);

	if (verbose) printf("Inverting negative activations: ");
	for (i=0 ; i<m ; i++) {
		posrms = 0.0; negrms = 0.0;
		pos = 0; neg = 0;
		for (j=i ; j<m*n ; j+=m)
			if (proj[j] >= 0) {
				posrms += proj[j]*proj[j];
				pos++;
			}
			else {
				negrms += proj[j]*proj[j];
				neg++;
			}

		if (negrms*(doublereal)pos > posrms*(doublereal)neg) {
			if (verbose) printf("-");
			for (j=i ; j<m*n ; j+=m) proj[j] = -proj[j];
			for (j=i ; j<m*m ; j+=m) trsf[j] = -trsf[j];
		}
		if (verbose) printf("%d ",(int)(i+1));
	}
	printf("\n");
}


/******************************* Perform infomax ******************************/
/* Perform infomax or extended-infomax on data. If any elements of weights    */
/* are none-zero use weights as starting weights else use identity matrix.    */
/* If bias not NULL employ biasing. The following externally accessible       */
/* variables are assumed initialized: extended, extblocks, pdfsize, nsub,     */
/* verbose, block, maxsteps, lrate, annealstep, annealdeg, nochange, and      */
/* momentum. If the boolean variable extended is set, signs must be defined   */
/* (i.e. not NULL)                                                            */
/*                                                                            */
/* data:    double array [chans,frames*epoch] (input)                         */
/* weights: double array [chans,chans] (input/output)                         */
/* chans:   integer (input)                                                   */
/* frames:  integer (input)                                                   */
/* epochs:  integer (input)                                                   */
/* bias:    double array [chans] (output) or NULL                             */
/* signs:   integer array [chans] (output) or NULL                            */

void runica(doublereal *data, doublereal *weights, integer chans, integer frames, integer epochs, doublereal *bias, integer *signs) {
	doublereal alpha = 1.0, beta = 0.0, gamma = -1.0, epsilon;
	doublereal change, oldchange, angledelta;
	char uplo='U', transn='N', transt='T';
	integer i, j, t = 0, inc = 1;
	
	integer datalength = frames*epochs;
	integer chxch = chans*chans;

	doublereal signsbias = DEFAULT_SIGNSBIAS;
	doublereal extmomentum = DEFAULT_EXTMOMENTUM;
	
	integer wts_blowup = 0;
	integer signcount = 0;
	integer pleft = 0;
	integer step = 0;
	integer blockno = 1;
	integer urextblocks;

/* Allocate arrays */
	doublereal *startweights = (doublereal*)malloc(chxch*sizeof(doublereal));
	doublereal *oldweights = (doublereal*)malloc(chxch*sizeof(doublereal));
	doublereal *tmpweights = (doublereal*)malloc(chxch*sizeof(doublereal));
	doublereal *delta = (doublereal*)malloc(chxch*sizeof(doublereal));
	doublereal *olddelta = (doublereal*)malloc(chxch*sizeof(doublereal));
	doublereal *u = (doublereal*)malloc(chans*block*sizeof(doublereal));
	doublereal *y = (doublereal*)malloc(chans*block*sizeof(doublereal));
	doublereal *yu = (doublereal*)malloc(chxch*sizeof(doublereal));
	doublereal *prevweights, *prevwtchange;
	doublereal *bsum, *kk, *old_kk;
	integer *oldsigns, *pdfperm;

#ifndef PROB_WINDOW
	integer *dataperm = (integer*)malloc(datalength*sizeof(integer));
#endif

/* Initialize weights if zero array */
	if (weights[idamax_(&chxch,weights,&inc)-1] == 0.0) eye(chans,weights);

/* Allocate and initialize arrays and variables needed for momentum */
	if (momentum > 0.0) {
		prevweights = (doublereal*)malloc(chxch*sizeof(doublereal));
		prevwtchange = (doublereal*)malloc(chxch*sizeof(doublereal));
		dcopy_(&chxch,weights,&inc,prevweights,&inc);
		zero(chxch,prevwtchange);
	}
	else
		prevweights = prevwtchange = NULL;

/* Allocate and initialize arrays and variables needed for biasing */
	if (bias) {
		bsum = (doublereal*)malloc(chans*sizeof(doublereal));
		zero(chans,bias);
	}
	else
		bsum = NULL;

/* Allocate and initialize arrays and variables needed for extended-ICA */
	if (extendedflag) {
		oldsigns = (integer*)malloc(chans*sizeof(integer));
		for (i=0 ; i<chans ; i++) oldsigns[i] = -1;
		for (i=0 ; i<nsub ; i++) signs[i] = 1;
		for (i=nsub ; i<chans ; i++) signs[i] = 0;
		
		if (pdfsize < datalength)
			pdfperm = (integer*)malloc(datalength*sizeof(integer));
		else {
			pdfsize = datalength;
			pdfperm = NULL;
		}
		
#ifdef KURTOSIS_ESTIMATE
		kk = (doublereal*)malloc(3*chans*sizeof(doublereal));
#else
		kk = (doublereal*)malloc(4*chans*sizeof(doublereal));
#endif
		old_kk = (doublereal*)malloc(chans*sizeof(doublereal));
		zero(chans,old_kk);

		if (extblocks<0 && verbose) {
			printf("Fixed extended-ICA sign assignments:  ");
			for (i=0 ; i<chans ; i++) printf("%d ",(int)(signs[i]));
			printf("\n");
		}
	}
	else {
		oldsigns = pdfperm = NULL;
		old_kk = kk = NULL;
	}
	urextblocks = extblocks;

/*************************** Initialize ICA training **************************/

	dcopy_(&chxch,weights,&inc,startweights,&inc);
	dcopy_(&chxch,weights,&inc,oldweights,&inc);
	
	if (verbose) {
		printf("Beginning ICA training ...");
		if (extendedflag) printf(" first training step may be slow ...\n");
		else printf("\n");
	}

#ifdef FIX_SEED	
	SRAND(1);
#else
	SRAND((int)time(NULL));
#endif

	while (step < maxsteps) {

#ifndef PROB_WINDOW
		initperm(dataperm,datalength);
#endif

/***************************** ICA training block *****************************/
		for (t=0 ; t<datalength-block && !wts_blowup ; t+=block) {

#ifdef PROB_WINDOW
			probperm(data,weights,bias,chans,block,frames,epochs,u);
#else
			randperm(data,weights,bias,dataperm,chans,block,datalength-t,u);
#endif

			if (!extendedflag) {
/************************* Logistic ICA weight update *************************/
				for (i=0 ; i<chans*block ; i++)
/*					y[i] = 1.0 - 2.0 / (1.0+exp(-u[i]));*/
					y[i] = -tanh(u[i]/2.0);

/*Bias sum for logistic ICA */
				if (bias)
					for (i=0 ; i<chans ; i++)
						bsum[i] = dsum_(&block,&y[i],&chans);

/* Compute: (1-2*y) * u' */
				dgemm_(&transn,&transt,&chans,&chans,&block,&alpha,y,&chans,u,&chans,&beta,yu,&chans,1,1);
			}
			else {
/************************* Extended-ICA weight update *************************/
				for (i=0 ; i<chans*block ; i++)
					y[i] = tanh(u[i]);

/* Bias sum for extended-ICA */
				if (bias)
					for (i=0 ; i<chans ; i++)
						bsum[i] = -2*dsum_(&block,&y[i],&chans);

/* Apply sign matrix */
				for (i=0 ; i<chans ; i++)
					if (signs[i])
						for (j=i ; j<block*chans ; j+=chans)
							y[j] = -y[j];

/* Compute: u * u' */
				dsyrk_(&uplo,&transn,&chans,&block,&alpha,u,&chans,&beta,yu,&chans,1,1);

				j = chxch - 2;
				for (i=1 ; i<chans ; i++) {
					dcopy_(&i,&yu[j],&chans,&yu[j-chans+1],&inc);
					j -= chans+1;
				}

/* Compute: -y * u' -u*u' */
				dgemm_(&transn,&transt,&chans,&chans,&block,&gamma,y,&chans,u,&chans,&gamma,yu,&chans,1,1);
			}
			
/* Add block identity matix */
			for (i=0 ; i<chxch ; i+=(chans+1))
				yu[i] += (doublereal)block;

/* Apply weight change */
			dcopy_(&chxch,weights,&inc,tmpweights,&inc);
			dgemm_(&transn,&transn,&chans,&chans,&chans,&lrate,yu,&chans,tmpweights,&chans,&alpha,weights,&chans,1,1);

/* Apply bias change */
			if (bias) daxpy_(&chans,&lrate,bsum,&inc,bias,&inc);
			
/******************************** Add momentum ********************************/
			if (momentum > 0.0) {
				daxpy_(&chxch,&momentum,prevwtchange,&inc,weights,&inc);
				for (i=0 ; i<chxch ; i++)
					prevwtchange[i] = weights[i] - prevweights[i];

				dcopy_(&chxch,weights,&inc,prevweights,&inc);
			}
			
			if (abs(weights[idamax_(&chxch,weights,&inc)-1]) > MAX_WEIGHT)
				wts_blowup = 1;

			if (extendedflag && !wts_blowup && extblocks>0 && blockno%extblocks==0) {
				if (pdfperm && pleft < pdfsize) {
					initperm(pdfperm,datalength);
					pleft = datalength;
				}
				
				pdf(data,weights,pdfperm,chans,pdfsize,pleft,kk);
				pleft -= pdfsize;
				
				if (extmomentum > 0.0) {
					epsilon = 1.0-extmomentum;
					dscal_(&chans,&epsilon,kk,&inc);
					daxpy_(&chans,&extmomentum,old_kk,&inc,kk,&inc);
					dcopy_(&chans,kk,&inc,old_kk,&inc);
				}
				
				for (i=0 ; i<chans ; i++)
					signs[i] = kk[i] < -signsbias;
				
				for (i=0 ; i<chans && signs[i]==oldsigns[i] ; i++);
				if (i==chans) signcount++;
				else signcount = 0;
				
				for (i=0 ; i<chans ; i++)
					oldsigns[i] = signs[i];
								
				if (signcount >= SIGNCOUNT_THRESHOLD) {
					extblocks = (integer)(extblocks * SIGNCOUNT_STEP);
					signcount = 0;
				}
			}
			blockno++;			
		}

		if (!wts_blowup) {
			step++;
			angledelta = 0.0;
			
			for (i=0 ; i<chxch ; i++)
				delta[i] = weights[i]-oldweights[i];
			
			change = ddot_(&chxch,delta,&inc,delta,&inc);
		}
		
/************************* Restart if weights blow up *************************/
		if (wts_blowup) {
			printf("\a");
			step = 0;
			change = nochange;
			wts_blowup = 0;
			blockno = 1;
			extblocks = urextblocks;
			lrate = lrate*DEFAULT_RESTART_FAC;
			dcopy_(&chxch,startweights,&inc,weights,&inc);
			zero(chxch,delta);
			zero(chxch,olddelta);

			if (bias) zero(chans,bias);
			
			if (momentum > 0.0) {
				dcopy_(&chxch,startweights,&inc,oldweights,&inc);
				dcopy_(&chxch,startweights,&inc,prevweights,&inc);
				zero(chxch,prevwtchange);
			}
						
			if (extendedflag) {
				for (i=0 ; i<chans ; i++) oldsigns[i] = -1;
				for (i=0 ; i<nsub ; i++) signs[i] = 1;
				for (i=nsub ; i<chans ; i++) signs[i] = 0;
			}
			
			if (lrate > MIN_LRATE)
				printf("Lowering learning rate to %g and starting again.\n",lrate);
			else
				error("QUITTING - weight matrix may not be invertible!\n");
		}
		else {
/*********************** Print weight update information **********************/
			if (step > 2) {
				epsilon = ddot_(&chxch,delta,&inc,olddelta,&inc);
				angledelta = acos(epsilon/sqrt(change*oldchange));

				if (verbose) {
					if (!extendedflag)
						printf("step %d - lrate %5f, wchange %7.6f, angledelta %4.1f deg\n",step,lrate,change,DEGCONST*angledelta);
					else {
						for (i=0,j=0 ; i<chans ; i++) j += signs[i];
						printf("step %d - lrate %5f, wchange %7.6f, angledelta %4.1f deg, %d subgauss\n",step,lrate,change,DEGCONST*angledelta,j);
					}
				}
			}
			else
				if (verbose) {
					if (!extendedflag)
						printf("step %d - lrate %5f, wchange %7.6f\n",step,lrate,change);
					else {
						for (i=0,j=0 ; i<chans ; i++) j += signs[i];
						printf("step %d - lrate %5f, wchange %7.6f, %d subgauss\n",step,lrate,change,j);
					}
				}
		}

/**************************** Save current values *****************************/
		dcopy_(&chxch,weights,&inc,oldweights,&inc);
		
		if (DEGCONST*angledelta > annealdeg) {
			dcopy_(&chxch,delta,&inc,olddelta,&inc);
			lrate = lrate*annealstep;
			oldchange = change;
		}
		else
			if (step == 1) {
				dcopy_(&chxch,delta,&inc,olddelta,&inc);
				oldchange = change;
			}

		if (step > 2 && change < nochange)
			step = maxsteps;
		else
			if (change > DEFAULT_BLOWUP)
				lrate = lrate*DEFAULT_BLOWUP_FAC;
	}

	if (bsum) free(bsum);
	if (oldsigns) free(oldsigns);
	if (prevweights) free(prevweights);
	if (prevwtchange) free(prevwtchange);
	if (startweights) free(startweights);
	if (oldweights) free(oldweights);
	if (tmpweights) free(tmpweights);
	if (delta) free(delta);
	if (olddelta) free(olddelta);
	if (kk) free(kk);
	if (old_kk) free(old_kk);
	if (yu) free(yu);
	if (y) free(y);
	if (u) free(u);
	if (pdfperm) free(pdfperm);

#ifndef PROB_WINDOW
	if (dataperm) free(dataperm);
#endif
}

