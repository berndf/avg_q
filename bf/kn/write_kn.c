/*
 * Copyright (C) 1996-2001,2003,2006,2010 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_kn.c module to write data to a Konstanz (Patrick Berg) file.
 * part of this code was taken from Patrick's `3ascimp.c'.
 *	-- Bernd Feige 3.06.1995
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kn.h"
/*}}}  */

/* Size of trialb.parray: */
#define TRIAL_PARRAY_BYTES (10*local_arg->trialb.nkan)

enum ARGS_ENUM {
 ARGS_APPEND=0, 
 ARGS_PACK, 
 ARGS_CONDITION,
 ARGS_SUBJECT,
 ARGS_TRIGFORM, 
 ARGS_OFILE,
 ARGS_CONVFACTOR,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Pack data", "p", FALSE, NULL},
 {T_ARGS_TAKES_LONG, "condition: Set condition code", "c", 1, NULL},
 {T_ARGS_TAKES_LONG, "subject: Set subject code", "s", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "string: Specify how trigger codes are distributed into the 3 condition and 5 marker values, eg: m5c2. Default: c1", "T", ARGDESC_UNUSED, (const char *const *)"c1"},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.raw"},
 {T_ARGS_TAKES_DOUBLE, "conv_factor", "", 1, NULL}
};

/*{{{  struct write_kn_storage {*/
struct write_kn_storage {
 short kn_flags;

 struct Rtrial trialb;   /* trial base */
 short *trialarray;        /* trial array */
 short *trialdata;         /* trial data */
 struct Rheader headb;   /* header base */
 char headstring[MAXHDSTRING];/* header string */
 short *headarray;         /* header array */
 int condition;
 int subject;
 short *trigger_form[sizeof(int)];
};
/*}}}  */

extern struct external_methods_struct *kn_emethods;	/* Used by rawdatan.c */

/*{{{  decompose_trigger(short **trigger_form, int trigger) {*/
LOCAL void
decompose_trigger(short **trigger_form, int trigger) {
 int i;
 for (i=sizeof(int)-1; i>=0; i--) {
  if (trigger_form[i]!=NULL) {
   *trigger_form[i]=(trigger&0xff);
  }
  trigger>>=8;
 }
}
/*}}}  */

/*{{{  write_kn_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_kn_init(transform_info_ptr tinfo) {
 struct write_kn_storage *local_arg=(struct write_kn_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int i;
 FILE *TARG=fopen(args[ARGS_OFILE].arg.s, "r");
 if (TARG!=NULL) fclose(TARG);

 /*{{{  Process options*/
 local_arg->kn_flags=(args[ARGS_PACK].is_set ? 128 : 0);
 local_arg->condition=(args[ARGS_CONDITION].is_set ? args[ARGS_CONDITION].arg.i : 0);
 local_arg->subject=(args[ARGS_SUBJECT].is_set ? args[ARGS_SUBJECT].arg.i : 1);	/* If 0, this would be interpreted as an average file */
 /*}}}  */

 parse_trigform(tinfo, &local_arg->trialb, local_arg->trigger_form, (args[ARGS_TRIGFORM].is_set ? args[ARGS_TRIGFORM].arg.s : "c1"));

 kn_emethods=tinfo->emethods;
 if (args[ARGS_APPEND].is_set && TARG!=NULL) {   /* Append and target exists*/
  ROpen(args[ARGS_OFILE].arg.s,&local_arg->headb,&local_arg->trialb,"r+b");
 } else {
  /* Note that this is free()d by Rfreehead, ie by RClose... */
  char *pstring=(char *)malloc(strlen(tinfo->comment)+1);
  if (pstring==NULL) {
   ERREXIT(tinfo->emethods, "write_kn: Error allocating pstring memory\n");
  }
  strcpy(pstring, tinfo->comment);
  RCreateHead(&local_arg->headb,MAXTRIALS,tinfo->nr_of_channels,MAXCOND,-1,local_arg->kn_flags+TRIALBSIZE,pstring);
  RCreate(args[ARGS_OFILE].arg.s,&local_arg->headb,&local_arg->trialb);
  /* In non-averaged files, headb.parray contains the sensitivities */
  for(i=local_arg->headb.maxchannels; i<2*local_arg->headb.maxchannels; i++) local_arg->headb.parray[i] = nint(args[ARGS_CONVFACTOR].arg.d);
 }

 /*{{{  Init trial buffer*/
 /* This is overwritten in single trials: */
 local_arg->trialb.spare[0] = local_arg->subject;
 local_arg->trialb.spare[1] = 1;
 local_arg->trialb.nkan = tinfo->nr_of_channels;
 local_arg->trialb.nabt = (short int)rint(1e6/tinfo->sfreq);
 local_arg->trialb.ndat = (long)tinfo->nr_of_channels*tinfo->nr_of_points;
 /* This is overwritten in single trials: */
 local_arg->trialb.trialarray[0] = local_arg->headb.group;
 local_arg->trialb.trialhead = TRIALBSIZE + TRIAL_PARRAY_BYTES;
 local_arg->trialb.trialsize = local_arg->trialb.trialhead + 2*local_arg->trialb.ndat;
 for(i=0; i<5; i++) local_arg->trialb.marker[i] = (short)0;
 local_arg->trialb.parray = (short *)Rdataalloc(NULL,TRIAL_PARRAY_BYTES,"trial array");
 memset(local_arg->trialb.parray, 0, TRIAL_PARRAY_BYTES);
 local_arg->trialb.pdata = (short *)Rdataalloc(NULL,local_arg->trialb.ndat*sizeof(short),"trial data");
 if(local_arg->headb.subject == -1) {
  /* File has been created */
  for(i=local_arg->trialb.nkan; i<2*local_arg->trialb.nkan; i++) local_arg->trialb.parray[i] = nint(args[ARGS_CONVFACTOR].arg.d);
  local_arg->headb.subject=local_arg->subject;
 }
 /*}}}  */

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_kn(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_kn(transform_info_ptr tinfo) {
 struct write_kn_storage *local_arg=(struct write_kn_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 short *pdata;
 array myarray;
 int marker;

 if (tinfo->itemsize>1) {
  ERREXIT(tinfo->emethods, "write_kn: itemsize>1 not supported\n");
 }
 if (tinfo->nr_of_channels!=local_arg->trialb.nkan) {
  ERREXIT(tinfo->emethods, "write_kn: Cannot write epochs with varying nr_of_channels!\n");
 }
 tinfo_array(tinfo, &myarray);
 array_transpose(&myarray);	/* Kn byte order is interlaced, ie channels fastest */

 local_arg->trialb.trialno = local_arg->headb.trials+1;
 pdata=local_arg->trialb.pdata;
 do {
  *pdata++ = (short int)rint(array_scan(&myarray)*args[ARGS_CONVFACTOR].arg.d);
 } while (myarray.message!=ARRAY_ENDOFSCAN);

 marker=(local_arg->condition==0 ? tinfo->condition : local_arg->condition);
 /* This distributes the trigger across condition or marker entries
  * as requested by the -T option: */
 decompose_trigger(local_arg->trigger_form, marker);
 TRACEMS2(tinfo->emethods, 1, "write_kn: Writing trial %d, condition=%d\n", MSGPARM(local_arg->trialb.trialno), MSGPARM(marker));

 /*{{{  Handle list of triggers in epoch if one is given (for reaction times)*/
 if (tinfo->triggers.buffer_start!=NULL) {
  struct trigger * const triggers=(struct trigger *)tinfo->triggers.buffer_start;
  int trigno;
  /* trialb.trialarray[0] should contain the epoch trigger point in
   * tenths of a second.
   * The first `trigger' holds the epoch offset in the original file: */
  local_arg->trialb.trialarray[0]=(short int)rint(10*(triggers[0].position+tinfo->beforetrig)/tinfo->sfreq);
  TRACEMS1(tinfo->emethods, 2, "write_kn: local_arg->trialb.trialarray[0]=%d\n", MSGPARM(local_arg->trialb.trialarray[0]));

  /* Clear the trial array first (Patrick doesn't just orient at spare[0]) */
  memset(local_arg->trialb.parray, 0, TRIAL_PARRAY_BYTES);
  for (trigno=0; triggers[trigno+1].code!=0; trigno++) {
   local_arg->trialb.parray[2*trigno  ]=triggers[trigno+1].code;
   local_arg->trialb.parray[2*trigno+1]=(short int)rint(1000*triggers[trigno+1].position/tinfo->sfreq);
   TRACEMS2(tinfo->emethods, 2, "write_kn: Trial trigger %d at %dms\n", MSGPARM(local_arg->trialb.parray[2*trigno]), MSGPARM(local_arg->trialb.parray[2*trigno+1]));
  }
  local_arg->trialb.spare[0]=trigno;	/* Number of triggers in epoch */
  TRACEMS1(tinfo->emethods, 2, "write_kn: local_arg->trialb.spare[0]=%d\n", MSGPARM(local_arg->trialb.spare[0]));
 }
 /*}}}  */
 
 kn_emethods=tinfo->emethods;
 RAppendTrial(local_arg->trialb.trialno,&local_arg->headb,&local_arg->trialb);
 RUpdate(&local_arg->headb);

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_kn_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_kn_exit(transform_info_ptr tinfo) {
 struct write_kn_storage *local_arg=(struct write_kn_storage *)tinfo->methods->local_storage;

 kn_emethods=tinfo->emethods;
 RClose(&local_arg->headb,&local_arg->trialb);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_kn(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_kn(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_kn_init;
 tinfo->methods->transform= &write_kn;
 tinfo->methods->transform_exit= &write_kn_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_kn";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to a Konstanz (Patrick Berg) file.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_kn_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
