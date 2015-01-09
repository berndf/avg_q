/*
 * Copyright (C) 1996-2001,2003,2004,2006-2010,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * transform.h defines the method structures used for the data analysis
 * package. The method mechanism follows Thomas G. Lane's JPEG software.
 * Bernd Feige 7.12.1992
 */
/*}}}  */

#ifndef TRANSFORM_H
#define TRANSFORM_H
#include "method.h"
#include "source.h"
#include "growing_buf.h"

/* These two are defined in setup_queue.c: */
extern char const *methodtype_names[];
extern char const *transform_argument_type_names[];
enum transform_argument_types {
 T_ARGS_TAKES_NOTHING=0, 
 T_ARGS_TAKES_LONG, 
 T_ARGS_TAKES_DOUBLE, 
 T_ARGS_TAKES_STRING_WORD, 
 T_ARGS_TAKES_SENTENCE, 
 T_ARGS_TAKES_FILENAME,
 T_ARGS_TAKES_SELECTION,
 T_ARGS_NR_OF_TYPES
};
typedef struct {
 enum transform_argument_types type;
 const char *description;
 char option_letters[4];
 double default_value;
 /* NULL-terminated string pointer list: */
 const char *const *choices;
} transform_argument_descriptor;

/* This is to be used to fill, eg, the unused default_value in string types: */
#define ARGDESC_UNUSED 0

typedef struct {
 Bool const is_set;
 /* 0 if setting the argument from outside was not requested, the number of the
  * variable to set this to otherwise */
 int const variable;
 union {
  long const i;
  double const d;
  char const *s;
 } arg;
} transform_argument;
/* Have to use this version wherever arg needs to be written: */
typedef struct {
 Bool is_set;
 int variable;
 union {
  long i;
  double d;
  char *s;
 } arg;
} transform_argument_writeable;

/* This is to store the trigger positions and codes relative to the start
 * of the epoch. code==0 serves as an end marker of struct trigger [], and 
 * the first entry in this array defines the absolute position of the start
 * point of this epoch in the file. */
struct trigger {
 long position;
 int code;
 char *description;
};

typedef struct external_methods_struct * external_methods_ptr;
typedef struct transform_info_struct * transform_info_ptr;
typedef struct transform_methods_struct * transform_methods_ptr;

/*{{{  struct external_methods_struct {*/
typedef enum execution_callback_places {
 E_CALLBACK_BEFORE_INIT=0,
 E_CALLBACK_AFTER_INIT,
 E_CALLBACK_BEFORE_EXEC,
 E_CALLBACK_AFTER_EXEC,
 E_CALLBACK_BEFORE_EXIT,
 E_CALLBACK_AFTER_EXIT
} execution_callback_place;
struct external_methods_struct {
 Bool has_been_set;

 /* User interface: error exit and trace message routines */
 METHOD(void, error_exit, (const external_methods_ptr emeth, const char *msgtext) __attribute__ ((__noreturn__)));
 METHOD(void, trace_message, (const external_methods_ptr emeth, const int lvl, const char *msgtext));
 /* Working data for error/trace facility */
 /* See macros below for the usage of these variables */
 int trace_level;	/* level of detail of tracing messages */
 /* Use level 0 for unsuppressable messages (nonfatal errors) */
 /* Use levels 1, 2, 3 for successively more detailed trace options */

msgparm_t message_parm[8];	/* store numeric parms for messages here */

 METHOD(void, execution_callback, (const transform_info_ptr tinfo, const execution_callback_place where));
};
/*}}}  */

/*{{{  struct transform_methods_struct {*/
enum method_types {
 GET_EPOCH_METHOD, PUT_EPOCH_METHOD, COLLECT_METHOD, TRANSFORM_METHOD,
 REJECT_METHOD,
 NR_OF_METHOD_TYPES
};

struct transform_methods_struct {
 METHOD(void, transform_init, (transform_info_ptr tinfo));
 METHOD(DATATYPE *, transform, (transform_info_ptr tinfo));
 METHOD(void, transform_exit, (transform_info_ptr tinfo));

 /* The single point interface */
 METHOD(int,  get_singlepoint, (transform_info_ptr tinfo, array *toarray));
 METHOD(void, seek_point, (transform_info_ptr tinfo, long to_point));
 METHOD(int, seek_trigger, (transform_info_ptr tinfo, int trigcode));
 METHOD(void, get_filestrings, (transform_info_ptr tinfo));

 char const *method_name;
 char const *method_description;
 enum method_types method_type;
 int nr_of_arguments;	/* Filled by method_select */
 transform_argument_descriptor *argument_descriptors; /* Filled by method_select */
 transform_argument *arguments;	/* Allocated and filled by method configurer */
 int local_storage_size;	/* Filled by method_select */
 void *local_storage;	/* Allocated by method configurer */
 Bool init_done;

 Bool within_branch;	/* TRUE if this belongs to a get_epoch branch (set by setup_queue) */
 Bool get_epoch_override;	/* TRUE if this carries the `!' tag (set by setup_queue) */

 /* Info to be used by the user interface: */
 /* Set to the line # from which the method was configured by setup_queue: */
 int line_of_script;
 /* Tells which of a sequence of scripts this was: */
 int script_number;
};
/*}}}  */

/*{{{  struct transform_info_struct {*/
enum data_types { TIME_DATA, FREQ_DATA };

struct transform_info_struct {
 transform_methods_ptr methods;	/* Points to list of methods to use */
 external_methods_ptr emethods;	/* Points to list of methods to use */
	/* For handling of multiple data sets */
 transform_info_ptr next;
 transform_info_ptr previous;
 enum data_types data_type;	/* TIME_DATA, FREQ_DATA */
 char *z_label;
 DATATYPE z_value;
	/* To be set for get_epoch methods: */
 int beforetrig;	/* Number of points before trigger */
 int aftertrig;
 int nr_of_points;	/* The total number of points in memory */
 int nr_of_channels;
 int itemsize;	/* Number of DATATYPE items per data point (eg complex=2) */
 int leaveright;	/* Number of items to leave from processing */
 Bool multiplexed;	/* =0 if data for each channel comes separate */
 char *xchannelname;	/* Name of the x axis */
 char **channelnames;
 DATATYPE *xdata;	/* x axis data */
 DATATYPE *tsdata;	/* Convention: Each channel separate, not multiplexed */
 double *probepos;	/* 3 values * nr_of_channels */
	/* Set by all methods returning data regions (unit: sizeof(DATATYPE)): */
 long length_of_output_region;
	/* To be set for writeasc and set by readasc: */
 char *comment;
	/* To be set for spect: */
 DATATYPE sfreq;	/* Sampling frequency in Hz */
 int windowsize;	/* The size of the fft window shifted */
 int nrofshifts;
	/* Set by spect: */
 DATATYPE basefreq;	/* Frequency step in Hz */
 DATATYPE basetime;	/* Time step in s */
 int shiftwidth;
 int nroffreq;
	/* Set by get-epoch methods: */
 long file_start_point;	/* Starting point for epoch in file */
 long points_in_file;
	/* Set by average: */
 int nrofaverages;
	/* Set by assert: */
 int failed_assertions;
 	/* Causes the iterated queue to terminate */
 Bool stopsignal;
	/* Set by do_script: */
 int accepted_epochs;
 int rejected_epochs;
	/* Used for the trigger or condition code for this epoch */
 int condition;
 growing_buf triggers;
	/* Read-only pointer to file triggers, if available */
 growing_buf *filetriggersp;
};
/*}}}  */

/*{{{  struct queue_desc_struct {*/
typedef struct queue_desc_struct {
 transform_methods_ptr start;
 int nr_of_methods;	/* Total size of queue */
 int allocated_methods;	/* Capacity of the allocated memory */
 int nr_of_get_epoch_methods;
 int current_get_epoch_method;
 int current_input_line;
 int current_input_script;
} queue_desc;
/*}}}  */

extern void trafo_std_defaults(transform_info_ptr tinfo);
extern void clear_external_methods(const external_methods_ptr emeth);
extern void set_external_methods(const external_methods_ptr emeth, 
  void (*error_exitp)(const external_methods_ptr emeth, const char *msgtext),
  void (*trace_messagep)(const external_methods_ptr emeth, const int lvl, const char *msgtext),
  void (*execution_callbackp)(const transform_info_ptr tinfo, const execution_callback_place where));

#endif	/* ifndef(TRANSFORM_H) */
