/*
 * Copyright (C) 1996-2011,2013,2014,2016,2018,2020 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#ifndef BF_H
#define BF_H

#include "mfx_file.h"
#include "transform.h"
#include "array.h"
#include "growing_buf.h"

/*{{{}}}*/
/*{{{  TRUE, FALSE*/
#ifndef Bool
#define Bool int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/*}}}  */

/*{{{  BFPLOT*/
#ifdef BFPLOT
void bfplot_start(void);
void bfplot_end(void);
void bfplot_reset(void);
void bfplot_next(void);
void bfplot_record(DATATYPE newitem);
void bfplot(void);
#else
#define bfplot_start()
#define bfplot_end();
#define bfplot_reset();
#define bfplot_next();
#define bfplot_record(a);
#define bfplot();
#endif
/*}}}  */

/*{{{  Compatibility*/
#ifdef SUN
/* Defines eg strtod, while this is def'd in stdlib.h on AIX */
#include <floatingpoint.h>
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#ifdef _AIX
extern double copysign (double x, double y);
#endif
#ifdef __MINGW32__
/* Compare http://www.mathworks.com/access/helpdesk/help/techdoc/ref/atan.shtml */
#define atanh(z) (0.5*log((1.0+(z))/(1.0-(z))))
#if __GNUC__ < 3
#define rint(f) ((int)((f)+0.5))
#endif
/* Hmmm, this comes from mingw/os-hacks.h: */
#define ftruncate chsize
#endif
#ifdef WIN32
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif
#ifdef __DJGPP__
#define copysign(x,y) fabs(x)*(y/fabs(y));
#endif
#ifdef __BORLANDC__
#define rint(f) ((int)((f)+0.5))
#endif
#ifdef __MWERKS__
/* Metrowerks doesn't have strcasecmp (used in trafo_std.c) */
#define strcasecmp(a,b) strcmp(a,b)
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __unix__
#define PATHSEP '/'
#else
#define PATHSEP '\\'
#endif
/*}}}  */

typedef struct {
 DATATYPE Re;
 DATATYPE Im;
} complex;

/*{{{  The type of individual histogram entries*/
#define HIST_TYPE DATATYPE
struct hist_boundary {		/* Struct for histogram definition */
 DATATYPE hist_min;
 DATATYPE hist_max;
 int nr_of_bins;
	/* Additional information from hist_autorange: */
 DATATYPE hist_mean;
	/* set by histogram_init: */
 DATATYPE hist_resolution;	/* The y bin width to count */
 HIST_TYPE *histogram;
};
/*}}}  */

extern char *bf_lib_timestamp;

/*{{{  Prototypes*/
void rdefault(char *prompt, char *fmt, void *var, char *r_default);

double c_phase(complex cmpl);
double c_power(complex cmpl);
double c_abs(complex cmpl);
complex c_add(complex cmpl1, complex cmpl2);
complex c_mult(complex cmpl1, complex cmpl2);
complex c_smult(complex cmpl, DATATYPE factor);
complex c_konj(complex cmpl);
complex c_inv(complex cmpl);
void fourier(complex *data, int nn, int isign);
void realfft(DATATYPE *data, int nn, int isign);
void real2fft(DATATYPE *data1, DATATYPE *data2, complex *p1, int n);
void select_fftspect(transform_info_ptr tinfo);
void select_fftfilter(transform_info_ptr tinfo);
void select_writeasc(transform_info_ptr tinfo);
void select_readasc(transform_info_ptr tinfo);
void select_get_mfxepoch(transform_info_ptr tinfo);
void select_dip_simulate(transform_info_ptr tinfo);
void select_null_source(transform_info_ptr tinfo);
void select_average(transform_info_ptr tinfo);
void select_assert(transform_info_ptr tinfo);
void select_reject_flor(transform_info_ptr tinfo);
void select_reject_bandwidth(transform_info_ptr tinfo);
void select_histogram(transform_info_ptr tinfo);
void select_posplot(transform_info_ptr tinfo);
void select_sliding_average(transform_info_ptr tinfo);
void select_differentiate(transform_info_ptr tinfo);
void select_baseline_divide(transform_info_ptr tinfo);
void select_baseline_subtract(transform_info_ptr tinfo);
void select_calc(transform_info_ptr tinfo);
void select_integrate(transform_info_ptr tinfo);
void select_subtract(transform_info_ptr tinfo);
void select_project(transform_info_ptr tinfo);
void select_correlate(transform_info_ptr tinfo);
void select_convolve(transform_info_ptr tinfo);
void select_scale_by(transform_info_ptr tinfo);
void select_zero_phase(transform_info_ptr tinfo);
void select_detrend(transform_info_ptr tinfo);
void select_demean_maps(transform_info_ptr tinfo);
void select_rereference(transform_info_ptr tinfo);
void select_dip_fit(transform_info_ptr tinfo);
void select_show_memuse(transform_info_ptr tinfo);
void select_run(transform_info_ptr tinfo);
void select_export_point(transform_info_ptr tinfo);
void select_import_point(transform_info_ptr tinfo);
void select_normalize_channelbox(transform_info_ptr tinfo);
void select_null_sink(transform_info_ptr tinfo);
void select_collapse_channels(transform_info_ptr tinfo);
void select_remove_channel(transform_info_ptr tinfo);
void select_write_channelpositions(transform_info_ptr tinfo);
void select_set_channelposition(transform_info_ptr tinfo);
void select_set(transform_info_ptr tinfo);
void select_extract_item(transform_info_ptr tinfo);
void select_calc_binomial_items(transform_info_ptr tinfo);
void select_change_axes(transform_info_ptr tinfo);
void select_write_kn(transform_info_ptr tinfo);
void select_read_kn(transform_info_ptr tinfo);
void select_read_synamps(transform_info_ptr tinfo);
void select_write_synamps(transform_info_ptr tinfo);
void select_read_freiburg(transform_info_ptr tinfo);
void select_write_freiburg(transform_info_ptr tinfo);
void select_read_rec(transform_info_ptr tinfo);
void select_write_rec(transform_info_ptr tinfo);
void select_minmax(transform_info_ptr tinfo);
void select_swap_fc(transform_info_ptr tinfo);
void select_swap_ic(transform_info_ptr tinfo);
void select_swap_ix(transform_info_ptr tinfo);
void select_write_crossings(transform_info_ptr tinfo);
void select_trim(transform_info_ptr tinfo);
void select_set_comment(transform_info_ptr tinfo);
void select_svdecomp(transform_info_ptr tinfo);
void select_icadecomp(transform_info_ptr tinfo);
void select_invert(transform_info_ptr tinfo);
void select_add_channels(transform_info_ptr tinfo);
void select_add_zerochannel(transform_info_ptr tinfo);
void select_query(transform_info_ptr tinfo);
void select_echo(transform_info_ptr tinfo);
void select_write_sound(transform_info_ptr tinfo);
void select_read_sound(transform_info_ptr tinfo);
void select_write_hdf(transform_info_ptr tinfo);
void select_read_hdf(transform_info_ptr tinfo);
void select_raw_fft(transform_info_ptr tinfo);
void select_read_generic(transform_info_ptr tinfo);
void select_write_generic(transform_info_ptr tinfo);
void select_read_tucker(transform_info_ptr tinfo);
void select_read_vitaport(transform_info_ptr tinfo);
void select_write_vitaport(transform_info_ptr tinfo);
void select_read_brainvision(transform_info_ptr tinfo);
void select_write_brainvision(transform_info_ptr tinfo);
void select_read_labview(transform_info_ptr tinfo);
void select_laplacian(transform_info_ptr tinfo);
void select_write_mfx(transform_info_ptr tinfo);
void select_append(transform_info_ptr tinfo);
void select_add(transform_info_ptr tinfo);
void select_recode(transform_info_ptr tinfo);
void select_read_neurofile(transform_info_ptr tinfo);
void select_read_nke(transform_info_ptr tinfo);
void select_read_inomed(transform_info_ptr tinfo);
void select_push(transform_info_ptr tinfo);
void select_pop(transform_info_ptr tinfo);
void select_link_order(transform_info_ptr tinfo);
void select_orthogonalize(transform_info_ptr tinfo);
void select_read_cfs(transform_info_ptr tinfo);
void select_read_sigma(transform_info_ptr tinfo);
enum add_channels_types {
 ADD_CHANNELS=0, ADD_POINTS, ADD_ITEMS, ADD_LINKEPOCHS
};
DATATYPE *add_channels_or_points(transform_info_ptr const to_tinfo, transform_info_ptr const from_tinfo, int * const channellist, enum add_channels_types const type, Bool const zero_newdata);
int get_mfxinfo(MFX_FILE *mfxfile, transform_info_ptr tinfo);
void get_mfxinfo_free(transform_info_ptr tinfo);
void multiplexed(transform_info_ptr tinfo);
void nonmultiplexed(transform_info_ptr tinfo);
void hist_autorange(transform_info_ptr tinfo);
transform_info_ptr swap_xz(transform_info_ptr tinfo);
void swap_xz_free(transform_info_ptr newtinfo);
void linreg(DATATYPE *data, int points, int skip, DATATYPE *linreg_constp, DATATYPE *linreg_factp);
void btimontage(transform_info_ptr tinfo, int nRing, double span, double curvature, double *center, double *axis0);
Bool allocate_methodmem(transform_info_ptr tinfo);
void free_methodmem(transform_info_ptr tinfo);
char *check_method_args(transform_info_ptr tinfo);
Bool accept_argument(transform_info_ptr tinfo, growing_buf *args, growing_buf *tokenbuf, transform_argument_descriptor *argument_descriptor);
Bool setup_method(transform_info_ptr tinfo, growing_buf *args);
enum SETUP_QUEUE_RESULT {
 QUEUE_AVAILABLE=0, QUEUE_NOT_AVAILABLE, QUEUE_AVAILABLE_EOF, QUEUE_NOT_AVAILABLE_EOF
};
enum SETUP_QUEUE_RESULT setup_queue(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), queue_desc *iter_queue, queue_desc *post_queue, FILE *scriptfile, growing_buf *scriptp);
Bool setup_queue_from_buffer(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), queue_desc *iter_queue, queue_desc *post_queue, growing_buf *scriptp);
void dump_queue(queue_desc *queue, FILE *dumpfile, char *prefix);
void sprint_argument_value(growing_buf *outbuf, transform_methods_ptr methodp, int argno, Bool with_escapes);
void sprint_argument(growing_buf *outbuf, transform_methods_ptr methodp, int argno);
void sprint_method(growing_buf *outbuf, transform_methods_ptr methodp);
void print_script(queue_desc *iter_queue, queue_desc *post_queue, FILE *printfile);
void init_queue_from_dump(transform_info_ptr tinfo, queue_desc *queue);
int set_queuevariables(transform_info_ptr tinfo, queue_desc *queue, int argc, char **argv);
void printhelp(transform_info_ptr tinfo, void (* const *m_selects)(transform_info_ptr), char *method_name);
DATATYPE *do_queue(transform_info_ptr tinfo, queue_desc *queue, int *nr_executedp);
void exit_queue(transform_info_ptr tinfo, queue_desc *queue);
void free_queuemethodmem(transform_info_ptr tinfo, queue_desc *queue);
void free_queue_storage(queue_desc *queue);
DATATYPE *do_queues(transform_info_ptr tinfo, queue_desc *iter_queue, queue_desc *post_queue);
int find_channel_number(transform_info_ptr tinfo, char const *channel_name);
double get_value(char const *number, char **EndPointer);
long gettimeslice(transform_info_ptr tinfo, char const *number);
double gettimefloat(transform_info_ptr tinfo, char const *number);
double getfreqfloat(transform_info_ptr tinfo, char const *number);
long find_pointnearx(transform_info_ptr tinfo, DATATYPE x);
long decode_xpoint(transform_info_ptr tinfo, char *token);
void create_xaxis(transform_info_ptr tinfo, char const *unitname);
void copy_channelinfo(transform_info_ptr tinfo, char **channelnames, double *probepos);
void deepcopy_tinfo(transform_info_ptr to_tinfo, transform_info_ptr from_tinfo);
void deepfree_tinfo(transform_info_ptr tinfo);
void free_tinfo(transform_info_ptr tinfo);
void create_channelgrid(transform_info_ptr tinfo);
void create_channelgrid_ncols(transform_info_ptr tinfo, int const ncols);
void free_pointer(void **ptr);
int comment2time(char *comment, short *dd, short *mm, short *yy, short *yyyy, short *hh, short *mi, short *ss);
int *get_trigcode_list(char const *const csv);
int read_trigger_from_trigfile(FILE *triggerfile, DATATYPE sfreq, long *trigpoint, char **descriptionp);
void push_trigger(growing_buf *triggersp, long position, int code, char *description);
void clear_triggers(growing_buf *triggersp);
void fprint_cstring(FILE *outfile, char const *string);
void tinfo_array(transform_info_ptr tinfo, array *thisarray);
void tinfo_array_setshift(transform_info_ptr tinfo, array *thisarray, int shift);
void tinfo_array_setfreq(transform_info_ptr tinfo, array *thisarray, int freq);
int *expand_channel_list(transform_info_ptr tinfo, char const *channelnames);
Bool is_in_channellist(int val, int *list);
struct source_desc *eg_dip_srcmodule(transform_info_ptr tinfo, char **args);
struct source_desc *var_random_srcmodule(transform_info_ptr tinfo, char **args);
struct source_desc *ramping_srcmodule(transform_info_ptr tinfo, char **args);

char const * validate(char * const program);
char const * get_avg_q_signature(void);
void (* const *get_method_list(void))(transform_info_ptr);

#ifndef __CYGWIN32__
float atoff(char *NumberPointer);	/* No prototype in system include files */
#endif
/*}}}  */
#endif
