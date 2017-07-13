/*
 * Copyright (C) 1996-2001,2003,2004,2008,2011-2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * fftfilter is a simple Fourier filter method: Each sample is Fourier-
 + transformed, the coefficients multiplied by a transfer function, and
 * the result transformed back into the time domain.
 * The transfer function is specified as a series of `cut-out' blocks,
 * each with start and end ramps.
 * 						-- Bernd Feige 20.12.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include "transform.h"
#include "bf.h"
#include "growing_buf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_VERBOSE=0,
 ARGS_QUIT,
 ARGS_BYNAME,
 ARGS_ITEMPART, 
 ARGS_BLOCKS, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Verbose output of filtering diagnostics", "V", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Quit after possible output of diagnostics, reject the epoch", "q", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_SENTENCE, "blocks", "", ARGDESC_UNUSED, (const char *const *)"0.6 0.7 1 1"}
};

/*{{{  struct blockdef {*/
/* Structure defining a frequency range suppression block. */
struct blockdef {
 float start;
 float nullstart;
 float nullend;
 float end;
 DATATYPE factor; /* Note: This is the full factor x, given by the -x option */
 int last_block;
};
/*}}}  */

struct fftfilter_storage {
 struct blockdef *blockdefs;
 int *channel_list;
 Bool have_channel_list;
 int fromitem;
 int toitem;
 DATATYPE sfreq; /* To see whether sfreq changed */
};

LOCAL void
init_fftfilter_storage(transform_info_ptr tinfo) {
 struct fftfilter_storage *local_arg=(struct fftfilter_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 growing_buf buf, tokenbuf;
 struct blockdef *inblocks;
 int nr_of_blocks=0, block;
 Bool havearg;

 local_arg->sfreq=tinfo->sfreq;
 free_pointer((void **)&local_arg->blockdefs);

 growing_buf_init(&buf);
 growing_buf_takethis(&buf, args[ARGS_BLOCKS].arg.s);
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);

 havearg=growing_buf_get_firsttoken(&buf,&tokenbuf);
 while (havearg) {
  if (*tokenbuf.buffer_start!='-') nr_of_blocks++;
  havearg=growing_buf_get_nexttoken(&buf,&tokenbuf);
 }

 havearg=growing_buf_get_firsttoken(&buf,&tokenbuf);
 if (!havearg || nr_of_blocks%4!=0) {
  ERREXIT(tinfo->emethods, "fftfilter_init: Number of args must be divisible by 4.\n");
 }
 nr_of_blocks/=4;
 if ((local_arg->blockdefs=(struct blockdef *)malloc(nr_of_blocks*sizeof(struct blockdef)))==NULL) {
  ERREXIT(tinfo->emethods, "fftfilter_init: Error allocating blockdefs memory\n");
 }
 for (inblocks=local_arg->blockdefs, block=0; havearg; block++, inblocks++) {
  if (*tokenbuf.buffer_start=='-') {
   char *endptr;
   inblocks->factor=strtod(tokenbuf.buffer_start+1, &endptr);
   if (*endptr!='\0' || inblocks->factor<0) {
    ERREXIT1(tinfo->emethods, "fftfilter_init: Invalid factor value: %s\n", MSGPARM(tokenbuf.buffer_start+1));
   }
   growing_buf_get_nexttoken(&buf,&tokenbuf);
  } else {
   inblocks->factor=0.0;	/* Complete suppression */
  }
  inblocks->start=gettimefloat(tinfo, tokenbuf.buffer_start); growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->nullstart=gettimefloat(tinfo, tokenbuf.buffer_start); growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->nullend=gettimefloat(tinfo, tokenbuf.buffer_start); growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->end=gettimefloat(tinfo, tokenbuf.buffer_start); havearg=growing_buf_get_nexttoken(&buf,&tokenbuf);
  if (inblocks->start>inblocks->nullstart || inblocks->nullstart>inblocks->nullend || inblocks->nullend>inblocks->end) {
   ERREXIT1(tinfo->emethods, "fftfilter_init: Block %d is not ascending.\n", MSGPARM(block+1));
  }
  if (inblocks->start<0.0 || inblocks->end>1.0) {
    ERREXIT1(tinfo->emethods, "fftfilter_init: Block %d does not consist of numbers between 0 and 1.\n", MSGPARM(block+1));
  }
  inblocks->last_block=(block==nr_of_blocks-1);
 }
 growing_buf_free(&tokenbuf);
 growing_buf_free(&buf);

 if (args[ARGS_BYNAME].is_set) {
  /* Note that this is NULL if no channel matched, which is why we need have_channel_list as well... */
  local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_BYNAME].arg.s);
  local_arg->have_channel_list=TRUE;
 } else {
  local_arg->channel_list=NULL;
  local_arg->have_channel_list=FALSE;
 }

 local_arg->fromitem=0;
 local_arg->toitem=tinfo->itemsize-tinfo->leaveright-1;
 if (args[ARGS_ITEMPART].is_set) {
  local_arg->fromitem=local_arg->toitem=args[ARGS_ITEMPART].arg.i;
  if (local_arg->fromitem<0 || local_arg->fromitem>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "fftfilter_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }
}

/*{{{  fftfilter_init(transform_info_ptr tinfo) {*/
METHODDEF void
fftfilter_init(transform_info_ptr tinfo) {
 struct fftfilter_storage *local_arg=(struct fftfilter_storage *)tinfo->methods->local_storage;
 local_arg->blockdefs=NULL;
 init_fftfilter_storage(tinfo);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  fftfilter(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
fftfilter(transform_info_ptr tinfo) {
 struct fftfilter_storage *local_arg=(struct fftfilter_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int itempart,i;
 transform_info_ptr const tinfoptr=tinfo;
 DATATYPE norm, firstval, lastval;
 struct blockdef *inblocks;
 array myarray, fftarray;
 long datasize=tinfo->nr_of_points, fftsize;

 /* Re-init if sfreq changed. This is important both for data entered in Hz and
  * in relative-sfreq units. */
 if (tinfo->sfreq!=local_arg->sfreq) init_fftfilter_storage(tinfo);

 fftsize=datasize-1;	/* This way, the result will be datasize if datasize is a power of 2 */
 for (i=0; fftsize>>=1; i++);
 fftsize=1<<(i+1);
 fftarray.nr_of_elements=fftsize+2;	/* 2 more for the additional coefficient */
 fftarray.element_skip=fftarray.nr_of_vectors=1;
 if (array_allocate(&fftarray)==NULL) {
  ERREXIT(tinfo->emethods, "fftfilter: Error allocating temp storage\n");
 }
 fftarray.nr_of_elements=fftsize;	/* This size is used for time domain data */
 norm=(DATATYPE)(fftsize>>1);

 if (args[ARGS_VERBOSE].is_set) {
#define STRINGBUFFER_SIZE 100
  char stringbuffer[STRINGBUFFER_SIZE];
  snprintf(stringbuffer, STRINGBUFFER_SIZE, "fftfilter: FFT size %ld points, Frequency resolution %gHz\n", fftsize, tinfo->sfreq/fftsize);
  TRACEMS(tinfo->emethods, -1, stringbuffer);
  for (inblocks=local_arg->blockdefs; inblocks!=NULL; inblocks++) {
   snprintf(stringbuffer, STRINGBUFFER_SIZE, "fftfilter: Block %ld %ld %ld %ld Factor %g\n", (long)rint(inblocks->start*fftsize), (long)rint(inblocks->nullstart*fftsize), (long)rint(inblocks->nullend*fftsize), (long)rint(inblocks->end*fftsize), inblocks->factor);
   TRACEMS(tinfo->emethods, -1, stringbuffer);
   if (inblocks->last_block) break;
  }
 }
 if (args[ARGS_QUIT].is_set) {
  /* Don't actually perform the filter, reject the epoch */
  return NULL;
 }
 
  tinfo_array(tinfoptr, &myarray);
  for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
   /* VERBOSE: Show factors only once */
   Bool show_factors=args[ARGS_VERBOSE].is_set;
   array_use_item(&myarray, itempart);
   do {
    if (local_arg->have_channel_list && !is_in_channellist(myarray.current_vector+1, local_arg->channel_list)) {
     array_nextvector(&myarray);
     continue;
    }
    array_reset(&fftarray);
    firstval=READ_ELEMENT(&myarray);
    do {
     array_write(&fftarray, lastval=array_scan(&myarray));
    } while (myarray.message==ARRAY_CONTINUE);
    array_previousvector(&myarray);
    i=0;
    while (fftarray.message==ARRAY_CONTINUE) {
     i++;
     /* Pad with linear interpolation between lastval and firstval */
     array_write(&fftarray, lastval+(firstval-lastval)*i/(fftsize-datasize+1));
    }
    realfft(fftarray.start, fftsize, 1);

    if (show_factors) {
     TRACEMS(tinfo->emethods, -1, "fftfilter: Factors");
    }
    for (inblocks=local_arg->blockdefs; inblocks!=NULL; inblocks++) {
     for (i=0; i<=fftsize; i+=2) {
      float ratio=((float)i)/fftsize, factor;
      if (ratio<inblocks->start || ratio>inblocks->end) continue;
      /* factor is 1.0 in the middle of the block and 0.0 at the ends */
      if      (ratio<inblocks->nullstart) factor=(ratio-inblocks->start)/(inblocks->nullstart-inblocks->start);
      else if (ratio>inblocks->nullend)   factor=1.0-(ratio-inblocks->nullend)/(inblocks->end-inblocks->nullend);
      else factor=1.0;
      /* We interpolate between 1.0 and inblocks->factor, which may also be >1! */
      factor=1.0+(inblocks->factor-1.0)*factor;
      fftarray.start[i  ]*=factor;
      fftarray.start[i+1]*=factor;
      if (show_factors) {
#define STRINGBUFFER_SIZE 100
       char stringbuffer[STRINGBUFFER_SIZE];
       snprintf(stringbuffer, STRINGBUFFER_SIZE, "\t%d:%g", i/2, factor);
       TRACEMS(tinfo->emethods, -1, stringbuffer);
      }
     }
     if (inblocks->last_block) break;
    }
    if (show_factors) {
     TRACEMS(tinfo->emethods, -1, "\n");
     show_factors=FALSE;
    }

    realfft(fftarray.start, fftsize, -1);
    do {
     array_write(&myarray,  array_scan(&fftarray)/norm);
    } while (myarray.message==ARRAY_CONTINUE);
   } while (myarray.message==ARRAY_ENDOFVECTOR);
  }
 array_free(&fftarray);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  fftfilter_exit(transform_info_ptr tinfo) {*/
METHODDEF void
fftfilter_exit(transform_info_ptr tinfo) {
 struct fftfilter_storage *local_arg=(struct fftfilter_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->blockdefs);
 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_fftfilter(transform_info_ptr tinfo) {*/
GLOBAL void
select_fftfilter(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &fftfilter_init;
 tinfo->methods->transform= &fftfilter;
 tinfo->methods->transform_exit= &fftfilter_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="fftfilter";
 tinfo->methods->method_description=
  "Transform method to cut out part of the Fourier coefficients of each\n"
  "trace. Each coefficient reduction block is given by four ascending\n"
  "numbers between 0.0 and 1.0 (frequencies relative to half the sampling\n"
  "frequency): start, zerostart, zeroend, end. Frequencies can also be entered\n"
  "in Hz by appending the string 'Hz', eg 48.2Hz. The Fourier coefficients\n"
  "between zerostart and zeroend are multiplied by x (0 by default, see below),\n"
  "those between start and zerostart or end and zeroend by a factor linearly\n"
  "interpolated between 1 and x.\n"
  "`blocks' is: start1 zerostart1 zeroend1 end1  [block2...];\n"
  "-x, where x is a floating-point number between 0 and 1, may occur at the start\n"
  "of any block. If specified, the frequency range between zero_start and zero_end\n"
  "is multiplied by this factor instead of by zero.\n";
 tinfo->methods->local_storage_size=sizeof(struct fftfilter_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
