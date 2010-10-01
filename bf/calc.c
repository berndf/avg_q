/*
 * Copyright (C) 1996-2001,2003,2010 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * calc is a simple transform method doing elementwise operations on the whole
 * data set using one of the functions defined below
 * 						-- Bernd Feige 27.08.1993
 */
/*}}}  */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "transform.h"
#include "bf.h"
#include "stat.h"
/*}}}  */

/* Definition of the individual data modifier functions:
 * The convention is that the functions access the current array element,
 * but do not change the array pointer */
LOCAL void calc_log(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, log((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_log10(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, log10((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_exp(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, exp((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_exp10(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, pow(10.0, (double)READ_ELEMENT(myarray)));
}
LOCAL void calc_logdB(transform_info_ptr tinfo, array *myarray) {
 /* 1 Bel is log10(Power). Note that we assume a power ratio as input! */
 WRITE_ELEMENT(myarray, log10((double)READ_ELEMENT(myarray))*10.0);
}
LOCAL void calc_expdB(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, pow(10.0, (double)READ_ELEMENT(myarray)/10.0));
}
LOCAL void calc_atanh(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, atanh((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_sqrt(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, sqrt((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_square(transform_info_ptr tinfo, array *myarray) {
 DATATYPE const hold=READ_ELEMENT(myarray);
 WRITE_ELEMENT(myarray, hold*hold);
}
LOCAL void calc_neg(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, -READ_ELEMENT(myarray));
}
LOCAL void calc_fabs(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, fabs((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_inv(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, 1.0/READ_ELEMENT(myarray));
}
LOCAL void calc_ceil(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, ceil((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_floor(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, floor((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_rint(transform_info_ptr tinfo, array *myarray) {
 WRITE_ELEMENT(myarray, rint((double)READ_ELEMENT(myarray)));
}
LOCAL void calc_abs2(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 complex const c={element[0], element[1]};
 element[0]=c_abs(c);
 element[1]= 0.0;
}
LOCAL void calc_square2(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 complex const c={element[0], element[1]};
 element[0]=c_power(c);
 element[1]= 0.0;
}
LOCAL void calc_norm2(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 complex const c={element[0], element[1]};
 DATATYPE const absval=c_abs(c);
 element[0]/=absval;
 element[1]/=absval;
}
LOCAL void calc_phase(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 complex const c={element[0], element[1]};
 element[0]=c_phase(c);
 element[1]= 0.0;
}
LOCAL void calc_absandphase(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 complex const c={element[0], element[1]};
 element[0]=c_abs(c);
 element[1]=c_phase(c);
}
LOCAL void calc_coherence(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 complex const c={element[0], element[1]};
 long const averages=(tinfo->itemsize==3 ? (long int)rint(element[2]) : tinfo->nrofaverages);
 element[0]=c_abs(c);
 /* This yields a probability measure for he calculated coherence;
  * 2*averages is the number of independent sections from which the
  * coherence was calculated and is only correct here if the fftspect
  * overlap parameter was 1 in the calculation */
 element[1]= pow(1-element[0], 2*averages-1);
}
LOCAL void calc_ttest(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 long const averages=(tinfo->itemsize==4 ? (long int)rint(element[3]) : tinfo->nrofaverages);
 DATATYPE const datatemp=element[1];
 element[1]=datatemp/(averages*sqrt((element[2]-datatemp*datatemp/averages)/(averages*(averages-1))));
 element[2]=student_p(averages-1, fabs(element[1]));
}
LOCAL void calc_stddev(transform_info_ptr tinfo, array *myarray) {
 DATATYPE * const element=ARRAY_ELEMENT(myarray);
 long const averages=(tinfo->itemsize==4 ? (long int)rint(element[3]) : tinfo->nrofaverages);
 DATATYPE const datatemp=element[1];
 element[1]=sqrt((element[2]-datatemp*datatemp/averages)/averages); /* sigma */
 element[2]=element[1]/sqrt(averages-1);	/* standard error */
}

/* wantitems tells how many items a function needs to access; this is used for
 * checking in calc_init. 
 * Functions which access only one item will be executed on all non-leaveright
 * items automatically, the others won't. */
LOCAL struct {
 void (*function_pointer)(transform_info_ptr tinfo, array *myarray);
 int wantitems;
} function_descriptors[]={
 {&calc_log,	1},
 {&calc_log10,	1},
 {&calc_logdB,	1},
 {&calc_exp,	1},
 {&calc_exp10,	1},
 {&calc_expdB,	1},
 {&calc_atanh,	1},
 {&calc_sqrt,	1},
 {&calc_square,	1},
 {&calc_neg,	1},
 {&calc_fabs,	1},
 {&calc_inv,	1},
 {&calc_ceil,	1},
 {&calc_floor,	1},
 {&calc_rint,	1},
/* 2-parameter functions: */
 {&calc_abs2,	2},
 {&calc_square2,	2},
 {&calc_norm2,	2},
 {&calc_phase,	2},
 {&calc_absandphase,	2},
 {&calc_coherence,	2},
/* Special functions: */
 {&calc_ttest,	3},
 {&calc_stddev,	3}
};
/* Can't put this into function_descriptors[] because we pass this as selection array */
LOCAL const char *const function_names[]={
 "log", 
 "log10", 
 "logdB", 
 "exp", 
 "exp10", 
 "expdB", 
 "atanh", 
 "sqrt", 
 "square", 
 "neg", 
 "abs", 
 "inv", 
 "ceil", 
 "floor", 
 "rint", 
 "abs2",
 "square2",
 "norm2",
 "phase",
 "absandphase",
 "coherence",
 "ttest",
 "stddev",
 NULL
};

enum ARGS_ENUM {
 ARGS_BYNAME=0,
 ARGS_ITEMPART, 
 ARGS_FUNCTION, 
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_STRING_WORD, "channelnames: Restrict to these channel names", "n", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "nr_of_item: work only on this item # (>=0)", "i", 0, NULL},
 {T_ARGS_TAKES_SELECTION, "Function name", "", 0, function_names}
};

struct calc_storage {
 void (*function_pointer)(transform_info_ptr tinfo, array *myarray);
 int *channel_list;
 int fromitem;
 int toitem;
};

/*{{{  calc_init(transform_info_ptr tinfo) {*/
METHODDEF void
calc_init(transform_info_ptr tinfo) {
 struct calc_storage *local_arg=(struct calc_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int const wantitems=function_descriptors[args[ARGS_FUNCTION].arg.i].wantitems;
 local_arg->function_pointer=function_descriptors[args[ARGS_FUNCTION].arg.i].function_pointer;

 local_arg->channel_list=NULL;
 if (args[ARGS_BYNAME].is_set) {
  local_arg->channel_list=expand_channel_list(tinfo, args[ARGS_BYNAME].arg.s);
 }

 local_arg->fromitem=0;
 local_arg->toitem=tinfo->itemsize-tinfo->leaveright-1;
 if (args[ARGS_ITEMPART].is_set) {
  local_arg->fromitem=local_arg->toitem=args[ARGS_ITEMPART].arg.i;
  if (local_arg->fromitem<0 || local_arg->fromitem>=tinfo->itemsize) {
   ERREXIT1(tinfo->emethods, "calc_init: No item number %d in file\n", MSGPARM(local_arg->fromitem));
  }
 }

 if (wantitems>1) {
  /* Functions accessing more than one item are only executed once per tuple */
  local_arg->toitem=local_arg->fromitem;
  if (tinfo->itemsize-local_arg->fromitem<wantitems) {
   ERREXIT2(tinfo->emethods, "calc_init: Function `%s' needs at least %d items!\n", MSGPARM(function_names[args[ARGS_FUNCTION].arg.i]), MSGPARM(wantitems));
  }
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  calc(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
calc(transform_info_ptr tinfo) {
 struct calc_storage *local_arg=(struct calc_storage *)tinfo->methods->local_storage;
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
     if (local_arg->channel_list!=NULL && !is_in_channellist(myarray.current_vector+1, local_arg->channel_list)) {
      array_nextvector(&myarray);
      continue;
     }
     do {
      (*local_arg->function_pointer)(tinfo, &myarray);
      array_advance(&myarray);
     } while (myarray.message==ARRAY_CONTINUE);
    } while (myarray.message==ARRAY_ENDOFVECTOR);
   }
  }

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  calc_exit(transform_info_ptr tinfo) {*/
METHODDEF void
calc_exit(transform_info_ptr tinfo) {
 struct calc_storage *local_arg=(struct calc_storage *)tinfo->methods->local_storage;

 free_pointer((void **)&local_arg->channel_list);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_calc(transform_info_ptr tinfo) {*/
GLOBAL void
select_calc(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &calc_init;
 tinfo->methods->transform= &calc;
 tinfo->methods->transform_exit= &calc_exit;
 tinfo->methods->method_type=TRANSFORM_METHOD;
 tinfo->methods->method_name="calc";
 tinfo->methods->method_description=
  "Transform method to apply elementwise transformations to the input data\n";
 tinfo->methods->local_storage_size=sizeof(struct calc_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
