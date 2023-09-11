/*
 * Copyright (C) 1996,1998,1999,2001 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * calc_binomial_items adds probability items to tuple data. The original
 * data is copied and three more items calculated from the +/- counts that
 * form the two `leaveright' values:
 * The binomial probability p and -log10(p)*sign_of_change and
 * the relative change count (#+ - #-)/(#+ + #-)
 * 						-- Bernd Feige 16.03.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "transform.h"
#include "bf.h"
#include "array.h"
#include "stat.h"
/*}}}  */

/*{{{  calc_binomial_items_init(transform_info_ptr tinfo) {*/
METHODDEF void
calc_binomial_items_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  calc_binomial_items(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
calc_binomial_items(transform_info_ptr tinfo) {
 DATATYPE *newdata;
 array inarray, newarray;
 int item;

 if (tinfo->leaveright!=2) {
  ERREXIT1(tinfo->emethods, "calc_binomial_items: Leaveright=%d!=2.\n", MSGPARM(tinfo->leaveright));
 }
 tinfo_array(tinfo, &inarray);
 newarray.nr_of_elements=inarray.nr_of_elements;
 newarray.nr_of_vectors=inarray.nr_of_vectors;
 newarray.element_skip=tinfo->itemsize+3;
 if ((newdata=array_allocate(&newarray))==NULL) {
  ERREXIT(tinfo->emethods, "calc_binomial_items: Can't allocate new channel memory !\n");
 }

 for (item=0; item<tinfo->itemsize; item++) {
  array_use_item(&inarray, item);
  array_use_item(&newarray, item);
  array_copy(&inarray, &newarray);
 }
 array_use_item(&newarray, tinfo->itemsize-2);	/* Start of the counts */

 do {
  /*{{{  Output the probability given that [1] and [2] are +,- counts*/
  DATATYPE *item_address=ARRAY_ELEMENT(&newarray);
  const DATATYPE k1=item_address[0], k2=item_address[1];
  double p, log10p;

  /* p is the probability for max(k1, k2) of the two counts k1, k2 to deviate
   * from mean(k1, k2) by this many or more within n events if both mutually
   * exclusive cases counted are equally probable. The betai term gives the
   * probability that k>=k1 (cumulative binomial probability, NR p.229) */
  if (k1>k2) {
   p=2.0*betai(k1, k2+1.0, 0.5);
   if (p<FLT_MIN) log10p= -log10(FLT_MIN);	/* Cutoff at p<FLT_MIN */
   else            log10p= -log10(p);		/* Positive */
  } else if (k1<k2) {
   p=2.0*betai(k2, k1+1.0, 0.5);
   if (p<FLT_MIN) log10p=  log10(FLT_MIN);	/* Cutoff at p<FLT_MIN */
   else	     log10p=  log10(p);		/* Negative */
  } else {
   p=1.0; log10p=0.0;
  }
  item_address[2]=p;
  item_address[3]=log10p;
  item_address[4]=(k1-k2)/(float)(k1+k2);
  array_advance(&newarray);
  /*}}}  */
 } while (newarray.message!=ARRAY_ENDOFSCAN);

 tinfo->itemsize+=3;
 tinfo->leaveright=5;
 tinfo->length_of_output_region=newarray.nr_of_elements*newarray.element_skip*newarray.nr_of_vectors;

 return newdata;
}
/*}}}  */

/*{{{  calc_binomial_items_exit(transform_info_ptr tinfo) {*/
METHODDEF void
calc_binomial_items_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_calc_binomial_items(transform_info_ptr tinfo) {*/
GLOBAL void
select_calc_binomial_items(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &calc_binomial_items_init;
 tinfo->methods->transform= &calc_binomial_items;
 tinfo->methods->transform_exit= &calc_binomial_items_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="calc_binomial_items";
 tinfo->methods->method_description=
  "Transform method adding probability items calculated from +/- counts.\n"
  " Leaveright must be 2. The items added are the binomial probability p,\n"
  " -log10(p)*sign_of_change and the relative change count (#+ - #-)/(#+ + #-).\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=0;
}
/*}}}  */
