/*
 * Copyright (C) 1996-2001,2010,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * zero_phase.c method to set the phase of one channel in each map to zero
 *	-- Bernd Feige 26.09.1994
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "transform.h"
#include "bf.h"
/*}}}  */

enum ARGS_ENUM {
 ARGS_CHANNELNAME,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channel_name", "", ARGDESC_UNUSED, NULL}
};

/*{{{  zero_phase_init(transform_info_ptr tinfo)*/
METHODDEF void
zero_phase_init(transform_info_ptr tinfo) {
 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  zero_phase(transform_info_ptr tinfo)*/
METHODDEF DATATYPE *
zero_phase(transform_info_ptr tinfo) {
 transform_argument *args=tinfo->methods->arguments;
 Bool const maxamp=(strcmp(args[ARGS_CHANNELNAME].arg.s, "MAXAMP")==0);
 Bool const nyquist=(strcmp(args[ARGS_CHANNELNAME].arg.s, "NYQUIST")==0);
 int zero_phase_channel;
 complex phase_factor;
 array indata;

 if (tinfo->itemsize!=2 || tinfo->leaveright!=0) {
  ERREXIT(tinfo->emethods, "zero_phase: Can only work on plain complex data\n");
 }
 if (!maxamp && !nyquist) {
  /*{{{  Find zero_phase_channel number*/
  zero_phase_channel=find_channel_number(tinfo, args[ARGS_CHANNELNAME].arg.s);
  if (zero_phase_channel== -1) {
   ERREXIT1(tinfo->emethods, "zero_phase: Can't find zero_phase_channel %s\n", MSGPARM(args[ARGS_CHANNELNAME].arg.s));
  }
  /*}}}  */
 }
 tinfo_array(tinfo, &indata);
 array_transpose(&indata);	/* Vectors are maps */

 do {
  if (nyquist) {
   DATATYPE Sy=0, Sxy=0, Sx=0, Sxx=0, linreg_fact;
   do {
    complex const c= *(complex *)ARRAY_ELEMENT(&indata);
    Sx+=c.Re;
    Sxx+=c.Re*c.Re;
    Sy+=c.Im;
    Sxy+=c.Re*c.Im;
    array_advance(&indata);
   } while (indata.message==ARRAY_CONTINUE);
   array_previousvector(&indata);
   linreg_fact=(indata.nr_of_elements*Sxy-Sx*Sy)/(indata.nr_of_elements*Sxx-Sx*Sx);
#if 0
   linreg_const=(Sy-linreg_fact*Sx)/indata.nr_of_elements;
#endif
   phase_factor.Re=1.0;
   phase_factor.Im=linreg_fact;
  } else {
  if (maxamp) {
   /*{{{  Find channel with maximum amplitude*/
   DATATYPE thispower, maxpower=0.0;
   do {
    thispower=c_power(*(complex *)ARRAY_ELEMENT(&indata));
    if (thispower>maxpower) {
     zero_phase_channel=indata.current_element;
     maxpower=thispower;
    }
    array_advance(&indata);
   } while (indata.message==ARRAY_CONTINUE);
   array_previousvector(&indata);
   /*}}}  */
  }
  indata.current_element=zero_phase_channel;
  phase_factor= *(complex *)ARRAY_ELEMENT(&indata);
  }
  phase_factor=c_smult(c_konj(phase_factor), 1.0/c_abs(phase_factor));
  indata.current_element=0;
  do {
   complex *c_element=(complex *)ARRAY_ELEMENT(&indata);
   *c_element=c_mult(*c_element, phase_factor);
   array_advance(&indata);
  } while (indata.message==ARRAY_CONTINUE);
 } while (indata.message!=ARRAY_ENDOFSCAN);

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  zero_phase_exit(transform_info_ptr tinfo)*/
METHODDEF void
zero_phase_exit(transform_info_ptr tinfo) {
 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_zero_phase(transform_info_ptr tinfo)*/
GLOBAL void
select_zero_phase(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &zero_phase_init;
 tinfo->methods->transform= &zero_phase;
 tinfo->methods->transform_exit= &zero_phase_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="zero_phase";
 tinfo->methods->method_description=
  "Method to set, for each incoming complex map, the phase of one channel to\n"
  " zero by applying the appropriate phase factor to all channels.\n"
  " If channel_name is MAXAMP, for each map the channel with maximum\n"
  " amplitude is found and used as the zero_phase channel.\n"
  " If channel_name is NYQUIST, fit a linear regression line to each map\n"
  " in the Nyquist plane and define the zero phase by its angle.\n";
 tinfo->methods->local_storage_size=0;
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
