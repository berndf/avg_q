/*
 * Copyright (C) 1997-1999,2001,2003,2009,2011,2013 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * recode is a transform method to linearly map input to output value ranges.
 * 						-- Bernd Feige 16.09.1997
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_BYNAME=0,
 ARGS_ITEMPART, 
 ARGS_BLOCKS, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_SENTENCE, "blocks", "", ARGDESC_UNUSED, (char const *const *)"-Inf 0 0 0"}
};

/*{{{  struct blockdef {*/
/* Structure defining a frequency range suppression block. */
struct blockdef {
 DATATYPE fromstart;
 DATATYPE fromend;
 DATATYPE tostart;
 DATATYPE toend;
 int last_block;
};
/*}}}  */

struct recode_storage {
 struct blockdef *blockdefs;
 int *channel_list;
 Bool have_channel_list;
 int fromitem;
 int toitem;
};

/*{{{  recode_init(transform_info_ptr tinfo) {*/
METHODDEF void
recode_init(transform_info_ptr tinfo) {
 struct recode_storage *local_arg=(struct recode_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 growing_buf buf, tokenbuf;
 struct blockdef *inblocks;
 int nr_of_blocks=0, block;
 Bool havearg;

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
   ERREXIT1(tinfo->emethods, "recode_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 growing_buf_init(&buf);
 growing_buf_takethis(&buf, args[ARGS_BLOCKS].arg.s);
 growing_buf_init(&tokenbuf);
 growing_buf_allocate(&tokenbuf,0);

 havearg=growing_buf_get_firsttoken(&buf,&tokenbuf);
 while (havearg) {
  nr_of_blocks++;
  havearg=growing_buf_get_nexttoken(&buf,&tokenbuf);
 }

 havearg=growing_buf_get_firsttoken(&buf,&tokenbuf);
 if (!havearg || nr_of_blocks%4!=0) {
  ERREXIT(tinfo->emethods, "recode_init: Number of args must be divisible by 4.\n");
 }
 nr_of_blocks/=4;
 if ((local_arg->blockdefs=(struct blockdef *)malloc(nr_of_blocks*sizeof(struct blockdef)))==NULL) {
  ERREXIT(tinfo->emethods, "recode_init: Error allocating blockdefs memory\n");
 }
 for (inblocks=local_arg->blockdefs, block=0; havearg; block++, inblocks++) {
  inblocks->fromstart=get_value(tokenbuf.buffer_start, NULL); growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->fromend=get_value(tokenbuf.buffer_start, NULL); growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->tostart=get_value(tokenbuf.buffer_start, NULL); growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->toend=get_value(tokenbuf.buffer_start, NULL); havearg=growing_buf_get_nexttoken(&buf,&tokenbuf);
  inblocks->last_block=(block==nr_of_blocks-1);
  if (isnan(inblocks->fromstart) || isnan(inblocks->fromend)) {
   if (!isnan(inblocks->fromstart) || !isnan(inblocks->fromend)) {
    ERREXIT(tinfo->emethods, "recode_init: NaN cannot be mixed in from block!\n");
   }
   if (inblocks->tostart!=inblocks->toend) {
    ERREXIT(tinfo->emethods, "recode_init: NaN can only be recoded to one value!\n");
   }
  }
  if (isnan(inblocks->tostart) || isnan(inblocks->toend)) {
   if (!isnan(inblocks->tostart) || !isnan(inblocks->toend)) {
    ERREXIT(tinfo->emethods, "recode_init: NaN cannot be mixed in to block!\n");
   }
  }
 }
 growing_buf_free(&tokenbuf);
 growing_buf_free(&buf);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  recode(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
recode(transform_info_ptr tinfo) {
 struct recode_storage *local_arg=(struct recode_storage *)tinfo->methods->local_storage;
 transform_info_ptr const tinfoptr=tinfo;
 array myarray;

  int itempart;
  int shift, nrofshifts=1;
  DATATYPE *save_tsdata;

  tinfo_array(tinfoptr, &myarray);	/* The channels are the vectors */
  save_tsdata=myarray.start;

  if (tinfoptr->data_type==FREQ_DATA) {
   nrofshifts=tinfoptr->nrofshifts;
  }
  for (shift=0; shift<nrofshifts; shift++) {
   myarray.start=save_tsdata+shift*tinfo->nroffreq*tinfo->nr_of_channels*tinfo->itemsize;
   array_setreadwrite(&myarray);
   for (itempart=local_arg->fromitem; itempart<=local_arg->toitem; itempart++) {
    array_use_item(&myarray, itempart);
    do {
     if (local_arg->have_channel_list && !is_in_channellist(myarray.current_vector+1, local_arg->channel_list)) {
      array_nextvector(&myarray);
      continue;
     }
     do {
      DATATYPE hold=READ_ELEMENT(&myarray);
      struct blockdef *inblocks=local_arg->blockdefs;
      while (inblocks!=NULL) {
       if (isnan(inblocks->fromstart) && isnan(hold)) {
	/* Recode NaN values */
	hold=inblocks->tostart;
       } else
       if (hold>=inblocks->fromstart && hold<=inblocks->fromend) {
	if (inblocks->fromend==inblocks->fromstart || inblocks->toend==inblocks->tostart) {
	 hold=inblocks->tostart;
	} else {
	 hold=((hold-inblocks->fromstart)/(inblocks->fromend-inblocks->fromstart))*(inblocks->toend-inblocks->tostart)+inblocks->tostart;
	}
	break;
       }
       if (inblocks->last_block) break;
       inblocks++;
      }
      array_write(&myarray, hold);
     } while (myarray.message==ARRAY_CONTINUE);
    } while (myarray.message==ARRAY_ENDOFVECTOR);
   }
  }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  recode_exit(transform_info_ptr tinfo) {*/
METHODDEF void
recode_exit(transform_info_ptr tinfo) {
 struct recode_storage *local_arg=(struct recode_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->blockdefs);
 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_recode(transform_info_ptr tinfo) {*/
GLOBAL void
select_recode(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &recode_init;
 tinfo->methods->transform= &recode;
 tinfo->methods->transform_exit= &recode_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="recode";
 tinfo->methods->method_description=
  "Transform method to linearly map input to output value ranges\n"
  " `blocks' is: fromstart1 fromend1 tostart1 toend1  [block2...]\n"
  " where any value x in the interval fromstartN<=x<=fromendN is linearly mapped to the\n"
  " interval tostartN..toendN. Blocks are checked from left to right, and the first\n"
  " match is used. `Inf' denotes infinity. `NaN' denotes the undefined value\n"
  " (necessarily uninterpolated, i.e. start and end must be equal).\n";
 tinfo->methods->local_storage_size=sizeof(struct recode_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
