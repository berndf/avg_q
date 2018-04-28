/*
 * Copyright (C) 1999-2003,2005,2007,2008,2010,2013,2014,2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <bf.h>

LOCAL void (* const method_selects[])(transform_info_ptr)={
 select_readasc,
 select_dip_simulate,
 select_get_mfxepoch,
 select_null_source,
 select_read_brainvision,
 select_read_cfs,
 select_read_freiburg,
 select_read_generic,
#ifdef AVG_Q_WITH_HDF
 select_read_hdf,
#endif
 select_read_inomed,
 select_read_labview,
 select_read_kn,
 select_read_neurofile,
 select_read_rec,
#ifdef AVG_Q_WITH_SOUND
 select_read_sound,
#endif
 select_read_synamps,
 select_read_tucker,
 select_read_vitaport,

 select_assert,
 select_reject_bandwidth,
 select_reject_flor,

 select_add,
 select_add_channels,
 select_add_zerochannel,
 select_baseline_divide,
 select_baseline_subtract,
 select_calc,
 select_calc_binomial_items,
 select_change_axes,
 select_collapse_channels,
 select_convolve,
 select_correlate,
 select_demean_maps,
 select_detrend,
 select_differentiate,
 select_dip_fit,
 select_echo,
 select_export_point,
 select_extract_item,
 select_fftfilter,
 select_fftspect,
 select_icadecomp,
 select_import_point,
 select_integrate,
 select_invert,
 select_laplacian,
 select_link_order,
 select_normalize_channelbox,
 select_orthogonalize,
#ifdef WITH_POSPLOT
 select_posplot,
#endif
 select_project,
 select_push,
 select_pop,
 select_query,
 select_raw_fft,
 select_recode,
 select_remove_channel,
 select_rereference,
 select_run,
 select_scale_by,
 select_set,
 select_set_channelposition,
 select_set_comment,
#ifndef __BORLANDC__
 select_show_memuse,
#endif
 select_sliding_average,
 select_subtract,
 select_svdecomp,
 select_swap_fc,
 select_swap_ic,
 select_swap_ix,
 select_trim,
 select_write_crossings,
 select_zero_phase,

 select_append,
 select_average,
 select_histogram,
 select_minmax,
 select_null_sink,

 select_writeasc,
 select_write_brainvision,
 select_write_freiburg,
 select_write_generic,
#ifdef AVG_Q_WITH_HDF
 select_write_hdf,
#endif
 select_write_kn,
 select_write_mfx,
 select_write_rec,
#ifdef AVG_Q_WITH_SOUND
 select_write_sound,
#endif
 select_write_synamps,
 select_write_vitaport,

 NULL
};

GLOBAL void
(* const *get_method_list(void))(transform_info_ptr) {
 return method_selects;
}
