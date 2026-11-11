/*
 * Copyright (C) 2026 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * read_hdf5.c module to read data from an HDF5 file
 *
 * HDF5 is special in that it supports both epoched and continuous data.
 * We specify that beforetrig and aftertrig are optional.
 * Another interesting difference is that multiple data sets may be present, also
 * with mixed dimensionality. Epochs are stored as separate data sets by default.
 *
 * Continuous file: is read as a single epoch if beforetrig and aftertrig are
 * missing or zero. Otherwise, the specified section around triggers given in
 * the file or via a trigger file is read.
 * Epoched file: Epochs are read.
 *
 * Dimension identification uses HDF5 Dimension Scales (H5DS API, libhdf5_hl):
 *  - A "Channels" scale attached to a dimension identifies the channels axis.
 *  - An "Item" scale attached to a dimension identifies the item axis (rank 3).
 *  - The remaining dimension is the points axis (identified positionally).
 * If no Dimension Scales are present, a size-based heuristic is used (mirroring
 * read_hdf.c). Per-channel metadata (name + 3 probepos values) is stored as
 * attributes on the Channels scale, ordered by attribute creation order so
 * channel index == attribute creation order.
 */
/*}}}  */

/*{{{  Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform.h"
#include "bf.h"

/* Pin the HDF5 1.10 API so the code builds against both 1.10.x and 1.12+/
 * 1.14: on 1.12+ this maps H5Literate etc. to their 1.10 (H5L_info_t)
 * variants; on 1.10.x it is a no-op. Must precede the HDF5 headers. */
#define H5_USE_110_API
#include "hdf5.h"
#include "hdf5_hl.h"
#include "H5DSpublic.h"
/*}}}  */

/*{{{  #defines*/
enum ARGS_ENUM {
 ARGS_CONTINUOUS=0,
 ARGS_TRIGTRANSFER,
 ARGS_TRIGLIST,
 ARGS_FROMEPOCH,
 ARGS_EPOCHS,
 ARGS_OFFSET,
 ARGS_TRIGFILE,
 ARGS_DATASET,
 ARGS_IFILE,
 ARGS_BEFORETRIG,
 ARGS_AFTERTRIG,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Continuous mode. Read the file in chunks of the given size without triggers", "c", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Transfer a list of triggers within the read epoch", "T", FALSE, NULL},
 {T_ARGS_TAKES_STRING_WORD, "trigger_list: Restrict epochs to these `StimTypes'. Eg: 1,2", "t", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_LONG, "fromepoch: Specify start epoch beginning with 1", "f", 1, NULL},
 {T_ARGS_TAKES_LONG, "epochs: Specify maximum number of epochs to get", "e", 1, NULL},
 {T_ARGS_TAKES_STRING_WORD, "offset: The zero point 'beforetrig' is shifted by offset", "o", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "trigger_file: Read trigger points and codes from this file", "R", ARGDESC_UNUSED, (const char *const *)"*.trg"},
 {T_ARGS_TAKES_STRING_WORD, "dataset: Only read this dataset/series (path fragment or 1-based index)", "d", ARGDESC_UNUSED, NULL},
 {T_ARGS_TAKES_FILENAME, "Input file", "", ARGDESC_UNUSED, (const char *const *)"*.hdf5"},
 {T_ARGS_TAKES_STRING_WORD, "beforetrig", " ", ARGDESC_UNUSED, (const char *const *)"1s"},
 {T_ARGS_TAKES_STRING_WORD, "aftertrig", " ", ARGDESC_UNUSED, (const char *const *)"1s"}
};

/* HDF5 stores in the file datatype (always little-endian IEEE float here); the
 * library auto-converts to the in-memory DATATYPE on H5Dread, so we do not need
 * a separate C_OTHERDATATYPE buffer the way read_hdf.c does. */
#ifdef FLOAT_DATATYPE
#define H5_FILE_DATATYPE H5T_IEEE_F32LE
#define H5_MEM_DATATYPE  H5T_NATIVE_FLOAT
#endif
#ifdef DOUBLE_DATATYPE
#define H5_FILE_DATATYPE H5T_IEEE_F64LE
#define H5_MEM_DATATYPE  H5T_NATIVE_DOUBLE
#endif

/* MAXRANK_FOR_MYDATA defines what is the maximum dimensionality acceptable to us */
#define MAXRANK_FOR_MYDATA 3

#define DESCBUF_LEN 2048
/*}}}  */

/*{{{  Definition of read_hdf5_storage*/
/* Layouts of datasets that can be read as time series:
 * - LAYOUT_PLAIN: regular numeric dataset, as written by write_hdf5.
 * - LAYOUT_WIDE: compound table of foreign files where every scalar numeric
 *   member is a channel and the rows are the time points.
 * - LAYOUT_LONG: "long format" compound table (e.g. PyTables/InfluxDB
 *   exports) where each row holds a series name (the string member(s)), a
 *   time stamp and a value. Every distinct series is read as a separate
 *   single-channel epoch. */
#define LAYOUT_PLAIN 0
#define LAYOUT_WIDE  1
#define LAYOUT_LONG  2

#define MAX_COMPOUND_MEMBERS 256

struct long_series {
 char *key;          /* Full series key (all grouping string members) */
 char *name;         /* Display name (key or "field/device")          */
 unsigned long hash;
 long count;
 double t_first;     /* In raw time column units */
 double t_last;
 double sfreq;       /* Estimated from the time column; 0 if unknown */
};

struct key_member_info {
 size_t offset;      /* Offset in the native compound type */
 size_t size;        /* Element size (0 for variable length strings) */
};

struct read_hdf5_storage {
 long current_point;
 int fromepoch;
 int epochs;
 hid_t fileid;
 hid_t sdsid;            /* Currently open dataset, H5I_INVALID_HID if none */
 hsize_t current_dataset;/* Index into dataset_paths of the dataset being read */
 char **dataset_paths;   /* All datasets in the file, enumerated recursively */
 int nr_of_datasets;
 int rank;
 hsize_t dims[MAXRANK_FOR_MYDATA];
 hsize_t maxdims[MAXRANK_FOR_MYDATA];
 hid_t file_datatype;    /* Native datatype id returned by H5Dget_type        */
 int pointsdim;
 int channelsdim;
 Bool is_continuous;     /* TRUE if the points dim is unlimited                */
 int layout;             /* LAYOUT_PLAIN/WIDE/LONG                             */
 double estimated_sfreq; /* From a time column, if there is no Sfreq attribute */
 hid_t native_type;      /* Native memory type of compound tables              */
 size_t native_size;
 hsize_t compound_rows;
 int wide_members[MAX_COMPOUND_MEMBERS];     /* LAYOUT_WIDE: member index per channel  */
 size_t wide_offsets[MAX_COMPOUND_MEMBERS];  /* LAYOUT_WIDE: member offset per channel */
 hid_t wide_member_types[MAX_COMPOUND_MEMBERS];
 int nr_of_wide_members;
 struct long_series *series;                 /* LAYOUT_LONG: series found in the table */
 int nr_of_series;
 int current_series;
 char *series_filter;    /* -d selection applied to series names/keys */
 int selected_series;    /* -d <n> on a long table: pinned series index, -1 if none */
 struct key_member_info key_members[MAX_COMPOUND_MEMBERS];
 int nr_of_key_members;
 int display_members[2]; /* Indices into key_members used for the display name */
 int nr_of_display_members;
 struct key_member_info value_member;        /* LAYOUT_LONG: sample value column */
 hid_t value_member_type;
 struct key_member_info time_member;         /* Time column, if any              */
 hid_t time_member_type;
 int has_vlen;           /* Table contains variable length strings */
 char descbuf[DESCBUF_LEN];

 int *trigcodes;
 int current_trigger;
 growing_buf triggers;
};
/*}}}  */

/*{{{  Maintaining the triggers list*/
LOCAL void
read_hdf5_reset_triggerbuffer(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 local_arg->current_trigger=0;
}

/*{{{  read_hdf5_build_trigbuffer(transform_info_ptr tinfo) {*/
LOCAL void
read_hdf5_build_trigbuffer(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (local_arg->triggers.buffer_start==NULL) {
  growing_buf_allocate(&local_arg->triggers, 0);
 } else {
  growing_buf_clear(&local_arg->triggers);
 }
 if (args[ARGS_TRIGFILE].is_set) {
  FILE * const triggerfile=(strcmp(args[ARGS_TRIGFILE].arg.s,"stdin")==0 ? stdin : fopen(args[ARGS_TRIGFILE].arg.s, "r"));
  TRACEMS(tinfo->emethods, 1, "read_hdf5_build_trigbuffer: Reading event file\n");
  if (triggerfile==NULL) {
   ERREXIT1(tinfo->emethods, "read_hdf5_build_trigbuffer: Can't open trigger file >%s<\n", MSGPARM(args[ARGS_TRIGFILE].arg.s));
  }
  while (TRUE) {
   long trigpoint;
   char *description;
   int const code=read_trigger_from_trigfile(triggerfile, tinfo->sfreq, &trigpoint, &description);
   if (code==0) break;
   push_trigger(&local_arg->triggers, trigpoint, code, description);
   free_pointer((void **)&description);
  }
  if (triggerfile!=stdin) fclose(triggerfile);
 } else {
  /* Triggers are stored as an int32 attribute "Triggers" on the dataset,
   * laid out as [pos0, code0, pos1, code1, ...] exactly as in read_hdf.c.
   * The first entry holds the file start point (code -1).
   * It is not an error if the attribute is absent (epoch written without
   * triggers); check with H5Aexists first to avoid HDF5-DIAG noise that
   * H5Aopen would emit for a missing attribute. */
  hid_t attrid=H5I_INVALID_HID;
  if (H5Aexists(local_arg->sdsid, "Triggers")>0)
   attrid=H5Aopen(local_arg->sdsid, "Triggers", H5P_DEFAULT);
  if (attrid>=0) {
   hid_t spacetype=H5Aget_type(attrid);
   hid_t space=H5Aget_space(attrid);
   int attrndims=H5Sget_simple_extent_ndims(space);
   hsize_t attrdims[1];
   hsize_t attrcount;
   int ntrigs;
   int this_entry=0;
   int32_t *trigentries;
   if (attrndims!=1 || H5Tget_class(spacetype)!=H5T_INTEGER || H5Tget_size(spacetype)!=4) {
    TRACEMS(tinfo->emethods, 0, "read_hdf5 warning: Trigger information unusable\n");
    H5Tclose(spacetype);
    H5Sclose(space);
    H5Aclose(attrid);
    return;
   }
   H5Sget_simple_extent_dims(space, attrdims, NULL);
   attrcount=attrdims[0];
   ntrigs=attrcount/2-1;
   if (ntrigs<0 || attrcount%2!=0) {
    TRACEMS(tinfo->emethods, 0, "read_hdf5 warning: Trigger information unusable\n");
    H5Tclose(spacetype);
    H5Sclose(space);
    H5Aclose(attrid);
    return;
   }
   if ((trigentries=(int32_t *)malloc(attrcount*sizeof(int32_t)))==NULL) {
    ERREXIT(tinfo->emethods, "read_hdf5: Error allocating trigger memory.\n");
   }
   H5Aread(attrid, H5T_NATIVE_INT32, trigentries);
   while (this_entry<=ntrigs) {
    push_trigger(&local_arg->triggers, trigentries[2*this_entry], trigentries[2*this_entry+1], NULL);
    this_entry++;
   }
   free(trigentries);
   H5Tclose(spacetype);
   H5Sclose(space);
   H5Aclose(attrid);
  }
  if (local_arg->triggers.current_length==0) {
   /* Dataset without trigger information (common for foreign files): read
    * it as a single epoch starting at point 0. This is not an error. */
   push_trigger(&local_arg->triggers, 0, -1, NULL);
  }
 }
}
/*}}}  */
/*{{{  read_hdf5_read_trigger(transform_info_ptr tinfo) {*/
/* This is the highest-level access function: Read the next trigger and
 * advance the trigger counter. If this function is not called at least
 * once, event information is not even touched. */
LOCAL int
read_hdf5_read_trigger(transform_info_ptr tinfo, long *position, char **descriptionp) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 int code=0;
 if (local_arg->triggers.current_length==0) {
  /* Load the event information */
  read_hdf5_build_trigbuffer(tinfo);
 }
 {
 int const nevents=local_arg->triggers.current_length/sizeof(struct trigger);

 if (local_arg->current_trigger<nevents) {
  struct trigger * const intrig=((struct trigger *)local_arg->triggers.buffer_start)+local_arg->current_trigger;
  *position=intrig->position;
   code    =intrig->code;
  if (descriptionp!=NULL) *descriptionp=intrig->description;
 }
 }
 local_arg->current_trigger++;
 return code;
}
/*}}}  */
/*}}}  */

/*{{{  read_hdf5_find_channels_scale(tinfo, sdsid, channelsdim_out)*/
/* Locate the Channels dimension by looking for a Dimension Scale named
 * "Channels" attached to one of the dataset's dimensions. Also identifies
 * the Item dimension (rank 3 only) by a scale named "Item". The remaining
 * dimension is the points dimension.
 *
 * Returns TRUE if a Channels scale was found (and sets local_arg->pointsdim,
 * local_arg->channelsdim). Returns FALSE if no Dimension Scales are present,
 * in which case the caller falls back to the size-based heuristic. */
typedef struct {
 int found_channels_dim;
 int found_item_dim;
} find_scales_ctx;

static herr_t
read_hdf5_scale_visitor(hid_t dset, unsigned dim, hid_t scale, void *visitor_data) {
 find_scales_ctx *ctx=(find_scales_ctx *)visitor_data;
 char name[64];
 ssize_t n;

 (void)dset;
 name[0]='\0';
 n=H5DSget_scale_name(scale, name, sizeof(name));
 if (n<=0) return 0; /* No name or error; skip */
 if (strcmp(name, "Channels")==0) {
  ctx->found_channels_dim=dim;
 } else if (strcmp(name, "Item")==0) {
  ctx->found_item_dim=dim;
 }
 return 0; /* Keep iterating */
}

LOCAL Bool
read_hdf5_find_scales(transform_info_ptr tinfo, hid_t sdsid) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 find_scales_ctx ctx;
 int dim;
 int num_scales_anywhere=0;

 ctx.found_channels_dim=-1;
 ctx.found_item_dim=-1;

 for (dim=0; dim<local_arg->rank; dim++) {
  int n=H5DSget_num_scales(sdsid, dim);
  int idx=0;
  if (n>0) {
   num_scales_anywhere+=n;
   H5DSiterate_scales(sdsid, dim, &idx, read_hdf5_scale_visitor, &ctx);
  }
 }

 if (ctx.found_channels_dim<0 || num_scales_anywhere==0) {
  return FALSE;
 }

 local_arg->channelsdim=ctx.found_channels_dim;
 /* Points dim is the one that is neither Channels nor Item. For rank 2 the
  * other dim is points; for rank 3 the third dim is Item. */
 if (local_arg->rank==3 && ctx.found_item_dim>=0) {
  int i;
  local_arg->pointsdim=-1;
  for (i=0; i<local_arg->rank; i++) {
   if (i!=ctx.found_channels_dim && i!=ctx.found_item_dim) {
    local_arg->pointsdim=i;
    break;
   }
  }
 } else {
  int i;
  local_arg->pointsdim=-1;
  for (i=0; i<local_arg->rank; i++) {
   if (i!=ctx.found_channels_dim) {
    local_arg->pointsdim=i;
    break;
   }
  }
 }
 /* Multiplexed layout: points dim is axis 0. */
 tinfo->multiplexed=(local_arg->pointsdim==0);
 return TRUE;
}
/*}}}  */

/*{{{  read_hdf5_setup_channelinfo_scale(scaleid, tinfo)*/
/* Per-channel metadata is stored as attributes on the Channels scale: each
 * attribute's name is the channel name and its value is 3 float64 (probepos).
 * The scale was created with attribute creation-order tracking so channel
 * index == attribute creation order. We iterate attributes in creation order;
 * if that index type is unavailable on the scale we fall back to name order
 * (with a warning, since channel order may be scrambled). */
typedef struct {
 transform_info_ptr tinfo;
 int pass;            /* 0 = count + sum namelen, 1 = fill                 */
 int nr_of_channels;
 int channel;         /* Filled during pass 1                              */
 long namelen;
 char *innames;       /* Pass 1 write pointer into the names buffer        */
 int scrambled;
} channel_attr_ctx;

static herr_t
read_hdf5_channel_attr_visitor(hid_t loc_id, const char *attr_name, const H5A_info_t *ainfo, void *op_data) {
 channel_attr_ctx *ctx=(channel_attr_ctx *)op_data;
 hid_t attrid;
 hid_t attrtype;
 hid_t attrspace;
 int attrndims;
 hsize_t attrdims[1];
 hsize_t attrcount;

 (void)ainfo;
 /* Skip the attributes that H5DSset_scale creates automatically. */
 if (strcmp(attr_name, "CLASS")==0 || strcmp(attr_name, "NAME")==0
     || strcmp(attr_name, "REFERENCE_LIST")==0 || strcmp(attr_name, "DIMENSION_LIST")==0) {
  return 0;
 }

 attrid=H5Aopen(loc_id, attr_name, H5P_DEFAULT);
 if (attrid<0) return 0;
 attrtype=H5Aget_type(attrid);
 attrspace=H5Aget_space(attrid);
 attrndims=H5Sget_simple_extent_ndims(attrspace);
 if (attrndims==1) {
  H5Sget_simple_extent_dims(attrspace, attrdims, NULL);
  attrcount=attrdims[0];
 } else {
  attrcount=0;
 }
 /* We only accept 3-element float64 attributes as channel metadata. */
 if (H5Tget_class(attrtype)==H5T_FLOAT && H5Tget_size(attrtype)==8 && attrcount==3) {
  if (ctx->pass==0) {
   ctx->nr_of_channels++;
   ctx->namelen+=strlen(attr_name)+1;
  } else {
   if (ctx->channel < ctx->nr_of_channels) {
    ctx->tinfo->channelnames[ctx->channel]=ctx->innames;
    strcpy(ctx->innames, attr_name);
    ctx->innames+=strlen(ctx->innames)+1;
    H5Aread(attrid, H5T_NATIVE_DOUBLE, &ctx->tinfo->probepos[3*ctx->channel]);
    ctx->channel++;
   }
  }
 }
 H5Tclose(attrtype);
 H5Sclose(attrspace);
 H5Aclose(attrid);
 return 0;
}

LOCAL void
read_hdf5_setup_channelinfo_scale(transform_info_ptr tinfo, hid_t scaleid) {
 channel_attr_ctx ctx;
 hsize_t idx;
 herr_t status;

 ctx.tinfo=tinfo;
 ctx.pass=0;
 ctx.nr_of_channels=0;
 ctx.channel=0;
 ctx.namelen=0;
 ctx.innames=NULL;
 ctx.scrambled=0;

 idx=0;
 status=H5Aiterate2(scaleid, H5_INDEX_CRT_ORDER, H5_ITER_INC, &idx, read_hdf5_channel_attr_visitor, &ctx);
 if (status<0) {
  /* No creation-order index on this scale; fall back to name order. */
  idx=0;
  ctx.scrambled=1;
  H5Aiterate2(scaleid, H5_INDEX_NAME, H5_ITER_INC, &idx, read_hdf5_channel_attr_visitor, &ctx);
 }
 if (ctx.nr_of_channels==0) {
  return;
 }
 if (ctx.scrambled) {
  TRACEMS(tinfo->emethods, 0, "read_hdf5 warning: Channels scale lacks creation-order tracking; channel order may be scrambled.\n");
 }
 if ((tinfo->channelnames=(char **)malloc(ctx.nr_of_channels*sizeof(char *)))==NULL ||
     (ctx.innames=(char *)malloc(ctx.namelen))==NULL ||
     (tinfo->probepos=(double *)malloc(3*ctx.nr_of_channels*sizeof(double)))==NULL) {
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating names/probepos memory\n");
 }
 ctx.pass=1;
 idx=0;
 if (!ctx.scrambled) {
  H5Aiterate2(scaleid, H5_INDEX_CRT_ORDER, H5_ITER_INC, &idx, read_hdf5_channel_attr_visitor, &ctx);
 } else {
  H5Aiterate2(scaleid, H5_INDEX_NAME, H5_ITER_INC, &idx, read_hdf5_channel_attr_visitor, &ctx);
 }
}
/*}}}  */

/*{{{  read_hdf5_channel_scale_visitor(...)*/
/* H5DSiterate_scales visitor for the channels dimension: reads per-channel
 * metadata from the Channels scale. The scale id is only safely valid during
 * the callback, so all channel-attribute reads happen here. */
typedef struct {
 transform_info_ptr tinfo;
 int done;
} channel_scale_ctx;

static herr_t
read_hdf5_channel_scale_visitor(hid_t dset, unsigned dim, hid_t scale, void *visitor_data) {
 channel_scale_ctx *ctx=(channel_scale_ctx *)visitor_data;
 (void)dset;
 (void)dim;
 read_hdf5_setup_channelinfo_scale(ctx->tinfo, scale);
 ctx->done=1;
 return 1; /* Stop after the first (Channels) scale. */
}
/*}}}  */

/*{{{  read_hdf5_setup_channelinfo(transform_info_ptr tinfo) {*/
LOCAL void
read_hdf5_setup_channelinfo(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 channel_scale_ctx cctx;
 int idx=0;
 cctx.tinfo=tinfo;
 cctx.done=0;
 H5DSiterate_scales(local_arg->sdsid, local_arg->channelsdim, &idx,
   read_hdf5_channel_scale_visitor, &cctx);
}
/*}}}  */

/*{{{  Foreign-file time-series support*/
/* The following helpers allow read_hdf5 to open time-series data from
 * arbitrary HDF5 files, not only files written by write_hdf5:
 * - LAYOUT_PLAIN: plain numeric datasets (the write_hdf5 format), unchanged.
 * - LAYOUT_WIDE: rank-1 compound tables where every scalar numeric member is
 *   a channel and the rows are the time points (e.g. MEA system exports).
 * - LAYOUT_LONG: rank-1 compound tables in "long format" (e.g. PyTables or
 *   InfluxDB exports) with string key member(s), a time column and a value
 *   column; every distinct key is read as a separate single-channel epoch. */

#define SERIES_CHUNK_ROWS 16384
#define SERIES_MAP_INITIAL 256

/*{{{  read_hdf5_time_divisor(double t_first) {*/
/* Guess the unit of a raw time column from its magnitude and return the
 * divisor converting it to seconds. */
LOCAL double
read_hdf5_time_divisor(double t_first) {
 if (t_first>1e17) return 1e9;   /* Nanoseconds */
 if (t_first>1e14) return 1e6;   /* Microseconds */
 if (t_first>1e11) return 1e3;   /* Milliseconds */
 return 1.0;                     /* Seconds */
}
/*}}}  */

/*{{{  read_hdf5_open_attr(hid_t objid, const char *name, hid_t *attrid_out) {*/
/* Open an attribute if it exists, without letting H5Aopen emit HDF5-DIAG
 * error stacks for missing attributes (the normal case for foreign files).
 * Returns FALSE if the attribute is absent or cannot be opened. */
LOCAL Bool
read_hdf5_open_attr(hid_t objid, const char *name, hid_t *attrid_out) {
 if (H5Aexists(objid, name)<=0) return FALSE;
 *attrid_out=H5Aopen(objid, name, H5P_DEFAULT);
 return (*attrid_out>=0);
}
/*}}}  */

/*{{{  read_hdf5_member_lowername(hid_t ftype, unsigned member, char *buf, size_t bufsize) {*/
/* Copy a compound member name in lower case into buf. */
LOCAL Bool
read_hdf5_member_lowername(hid_t ftype, unsigned member, char *buf, size_t bufsize) {
 char *name=H5Tget_member_name(ftype, member);
 size_t i;
 if (name==NULL) return FALSE;
 for (i=0; i+1<bufsize && name[i]!='\0'; i++) {
  char c=name[i];
  buf[i]=(c>='A' && c<='Z') ? (char)(c-'A'+'a') : c;
 }
 buf[i]='\0';
 H5free_memory(name);
 return TRUE;
}
/*}}}  */

/*{{{  read_hdf5_row_join / read_hdf5_key_hash {*/
/* Join the string values of the given compound members with '/'; handles
 * both variable-length and fixed-length strings, truncating safely. */
LOCAL void
read_hdf5_row_join(const char *row, const struct key_member_info members[], int nr, char *buf, size_t bufsize) {
 size_t pos=0;
 int i;
 if (bufsize==0) return;
 buf[0]='\0';
 for (i=0; i<nr; i++) {
  const char *s;
  size_t len;
  if (members[i].size==0) {
   /* Variable-length string: the row holds a char pointer. */
   s=*(char **)(row+members[i].offset);
   len=(s!=NULL ? strlen(s) : 0);
  } else {
   s=row+members[i].offset;
   len=strnlen(s, members[i].size);
  }
  if (i>0) {
   if (pos+1>=bufsize) break;
   buf[pos++]='/';
  }
  if (pos+len>=bufsize) len=bufsize-1-pos;
  memcpy(buf+pos, s, len);
  pos+=len;
  buf[pos]='\0';
 }
}

/* djb2 string hash used to group table rows into series. */
LOCAL unsigned long
read_hdf5_key_hash(const char *key) {
 unsigned long hash=5381;
 while (*key!='\0') {
  hash=hash*33+(unsigned char)*key++;
 }
 return hash;
}
/*}}}  */

typedef struct {
 unsigned long hash;
 int series;     /* Index into storage->series, -1 for an empty slot */
} series_hash_entry;

/*{{{  read_hdf5_scan_series(transform_info_ptr tinfo, struct read_hdf5_storage *local_arg) {*/
/* Scan a whole compound table and group the rows into series (LAYOUT_LONG).
 * The key members (string columns) form the group key; for every series the
 * row count and the time span are tracked so that the sampling frequency can
 * be estimated. Rows may be in arbitrary order. */
LOCAL Bool
read_hdf5_scan_series(transform_info_ptr tinfo, struct read_hdf5_storage *local_arg) {
 hid_t filespace=H5I_INVALID_HID;
 hid_t memspace;
 hsize_t nrows=local_arg->compound_rows;
 hsize_t start=0;
 char *rowbuf=NULL;
 series_hash_entry *map=NULL;
 size_t mapsize=SERIES_MAP_INITIAL;
 size_t used=0;
 int i;

 if ((filespace=H5Dget_space(local_arg->sdsid))<0) return FALSE;
 if ((rowbuf=(char *)malloc((size_t)SERIES_CHUNK_ROWS*local_arg->native_size))==NULL) {
  H5Sclose(filespace);
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating scan buffer\n");
 }
 if ((map=(series_hash_entry *)malloc(mapsize*sizeof(series_hash_entry)))==NULL) {
  H5Sclose(filespace);
  free(rowbuf);
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating series map\n");
 }
 for (i=0; i<(int)mapsize; i++) map[i].series=-1;

 while (start<nrows) {
  hsize_t count=nrows-start;
  if (count>SERIES_CHUNK_ROWS) count=SERIES_CHUNK_ROWS;
  H5Sselect_hyperslab(filespace, H5S_SELECT_SET, &start, NULL, &count, NULL);
  if ((memspace=H5Screate_simple(1, &count, NULL))<0) {
   H5Sclose(filespace);
   free(rowbuf);
   free(map);
   return FALSE;
  }
  if (H5Dread(local_arg->sdsid, local_arg->native_type, memspace, filespace, H5P_DEFAULT, rowbuf)<0) {
   H5Sclose(memspace);
   H5Sclose(filespace);
   free(rowbuf);
   free(map);
   return FALSE;
  }
  {
   hsize_t idx;
   for (idx=0; idx<count; idx++) {
    char *row=rowbuf+idx*local_arg->native_size;
    char keybuf[4096];
    unsigned long hash;
    size_t slot;
    double tval=0.0;

    read_hdf5_row_join(row, local_arg->key_members, local_arg->nr_of_key_members, keybuf, sizeof keybuf);
    hash=read_hdf5_key_hash(keybuf);
    if (local_arg->time_member_type!=H5I_INVALID_HID) {
     char tmp[16];
     memcpy(tmp, row+local_arg->time_member.offset, local_arg->time_member.size);
     H5Tconvert(local_arg->time_member_type, H5T_NATIVE_DOUBLE, 1, tmp, NULL, H5P_DEFAULT);
     tval=*(double *)tmp;
    }
    slot=hash&(mapsize-1);
    while (map[slot].series!=-1) {
     if (map[slot].hash==hash
         && strcmp(local_arg->series[map[slot].series].key, keybuf)==0) break;
     slot=(slot+1)&(mapsize-1);
    }
    if (map[slot].series==-1) {
     struct long_series *s;

     if ((used+1)*10>=mapsize*7) {
      /* Grow the hash map and rehash the existing entries. */
      series_hash_entry *newmap;
      size_t newsize=mapsize*2;
      int j;
      if ((newmap=(series_hash_entry *)malloc(newsize*sizeof(series_hash_entry)))==NULL) {
       H5Sclose(memspace);
       H5Sclose(filespace);
       free(rowbuf);
       free(map);
       ERREXIT(tinfo->emethods, "read_hdf5: Error allocating series map\n");
      }
      for (j=0; j<(int)newsize; j++) newmap[j].series=-1;
      for (j=0; j<(int)mapsize; j++) {
       if (map[j].series!=-1) {
        size_t newslot=map[j].hash&(newsize-1);
        while (newmap[newslot].series!=-1) newslot=(newslot+1)&(newsize-1);
        newmap[newslot]=map[j];
       }
      }
      free(map);
      map=newmap;
      mapsize=newsize;
      slot=hash&(mapsize-1);
      while (map[slot].series!=-1) slot=(slot+1)&(mapsize-1);
     }
     if ((local_arg->series=(struct long_series *)realloc(local_arg->series,
         (size_t)(local_arg->nr_of_series+1)*sizeof(struct long_series)))==NULL) {
      H5Sclose(memspace);
      H5Sclose(filespace);
      free(rowbuf);
      free(map);
      ERREXIT(tinfo->emethods, "read_hdf5: Error allocating series list\n");
     }
     s=&local_arg->series[local_arg->nr_of_series];
     if ((s->key=(char *)malloc(strlen(keybuf)+1))==NULL
         || (s->name=(char *)malloc(strlen(keybuf)+1))==NULL) {
      H5Sclose(memspace);
      H5Sclose(filespace);
      free(rowbuf);
      free(map);
      ERREXIT(tinfo->emethods, "read_hdf5: Error allocating series key\n");
     }
     strcpy(s->key, keybuf);
     if (local_arg->nr_of_display_members>0) {
      char namebuf[512];
      struct key_member_info dm[2];
      int d;
      for (d=0; d<local_arg->nr_of_display_members && d<2; d++) {
       dm[d]=local_arg->key_members[local_arg->display_members[d]];
      }
      read_hdf5_row_join(row, dm, d, namebuf, sizeof namebuf);
      strcpy(s->name, namebuf);
     } else {
      strcpy(s->name, keybuf);
     }
     s->hash=hash;
     s->count=1;
     s->t_first=tval;
     s->t_last=tval;
     s->sfreq=0.0;
     map[slot].hash=hash;
     map[slot].series=local_arg->nr_of_series;
     local_arg->nr_of_series++;
     used++;
    } else {
     struct long_series *s=&local_arg->series[map[slot].series];
     s->count++;
     if (tval<s->t_first) s->t_first=tval;
     if (tval>s->t_last) s->t_last=tval;
    }
   }
  }
  if (local_arg->has_vlen) {
   H5Dvlen_reclaim(local_arg->native_type, memspace, H5P_DEFAULT, rowbuf);
  }
  H5Sclose(memspace);
  start+=count;
 }
 H5Sclose(filespace);
 free(rowbuf);
 free(map);
 for (i=0; i<local_arg->nr_of_series; i++) {
  struct long_series *s=&local_arg->series[i];
  if (s->count>1 && s->t_last>s->t_first) {
   double divisor=read_hdf5_time_divisor(s->t_first);
   double span=(s->t_last-s->t_first)/divisor;
   if (span>0.0) s->sfreq=(double)(s->count-1)/span;
  }
 }
 return TRUE;
}
/*}}}  */
/*{{{  read_hdf5_read_row_time(transform_info_ptr tinfo, hsize_t row, double *out) {*/
/* Read the time column value of a single row of the open compound table. */
LOCAL Bool
read_hdf5_read_row_time(transform_info_ptr tinfo, hsize_t row, double *out) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 hid_t filespace, memspace;
 hsize_t starts[1], counts[1];
 char *rowbuf;
 char tmp[16];

 if (local_arg->time_member_type==H5I_INVALID_HID
     || local_arg->time_member.size>sizeof tmp) return FALSE;
 if ((filespace=H5Dget_space(local_arg->sdsid))<0) return FALSE;
 starts[0]=row;
 counts[0]=1;
 H5Sselect_hyperslab(filespace, H5S_SELECT_SET, starts, NULL, counts, NULL);
 if ((memspace=H5Screate_simple(1, counts, NULL))<0) {
  H5Sclose(filespace);
  return FALSE;
 }
 if ((rowbuf=(char *)malloc(local_arg->native_size))==NULL) {
  H5Sclose(memspace);
  H5Sclose(filespace);
  return FALSE;
 }
 if (H5Dread(local_arg->sdsid, local_arg->native_type, memspace, filespace, H5P_DEFAULT, rowbuf)<0) {
  free(rowbuf);
  H5Sclose(memspace);
  H5Sclose(filespace);
  return FALSE;
 }
 H5Sclose(memspace);
 H5Sclose(filespace);
 memcpy(tmp, rowbuf+local_arg->time_member.offset, local_arg->time_member.size);
 free(rowbuf);
 if (H5Tconvert(local_arg->time_member_type, H5T_NATIVE_DOUBLE, 1, tmp, NULL, H5P_DEFAULT)<0) return FALSE;
 *out=*(double *)tmp;
 return TRUE;
}
/*}}}  */

/*{{{  read_hdf5_classify_compound(transform_info_ptr tinfo, hid_t space, hid_t ftype) {*/
/* Analyze a rank-1 compound dataset as a time-series table. Tries the long
 * format (string key members + time + value; one epoch per series) first and
 * falls back to the wide format (every scalar numeric member is a channel).
 * Returns FALSE if the table cannot be interpreted as a time series. */
LOCAL Bool
read_hdf5_classify_compound(transform_info_ptr tinfo, hid_t space, hid_t ftype) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 int rank=H5Sget_simple_extent_ndims(space);
 int nmembers;
 int m;
 int string_members[MAX_COMPOUND_MEMBERS], nr_string=0;
 int numeric_members[MAX_COMPOUND_MEMBERS], nr_numeric=0;
 int array_member=-1;
 hsize_t array_len=0;
 int time_member=-1;
 int time_score=0;

 if (rank!=1) return FALSE;
 if ((local_arg->compound_rows=H5Sget_simple_extent_npoints(space))<1) return FALSE;
 nmembers=H5Tget_nmembers(ftype);
 if (nmembers<1 || nmembers>MAX_COMPOUND_MEMBERS) return FALSE;
 /* Assigned immediately so that the caller's failure path (close_dataset)
  * can release it. */
 if ((local_arg->native_type=H5Tget_native_type(ftype, H5T_DIR_DEFAULT))==H5I_INVALID_HID) return FALSE;
 local_arg->native_size=H5Tget_size(local_arg->native_type);

 for (m=0; m<nmembers; m++) {
  char name[128];
  hid_t mtype=H5Tget_member_type(ftype, m);
  H5T_class_t mclass;
  if (mtype<0) continue;
  if (!read_hdf5_member_lowername(ftype, m, name, sizeof name)) {
   H5Tclose(mtype);
   continue;
  }
  mclass=H5Tget_class(mtype);
  if (mclass==H5T_STRING) {
   string_members[nr_string++]=m;
   if (H5Tis_variable_str(mtype)>0) local_arg->has_vlen=TRUE;
  } else if (mclass==H5T_ARRAY) {
   /* An array member may hold the value column (e.g. PyTables
    * values_block_*); only numeric 1-d arrays are candidates. */
   hid_t base=H5Tget_super(mtype);
   if (base>=0) {
    H5T_class_t baseclass=H5Tget_class(base);
    hsize_t adims[H5S_MAX_RANK];
    int ndims=H5Tget_array_dims(mtype, adims);
    if ((baseclass==H5T_FLOAT || baseclass==H5T_INTEGER)
        && ndims==1 && adims[0]>=1
        && (array_member<0 || adims[0]<array_len)) {
     array_member=m;
     array_len=adims[0];
    }
    H5Tclose(base);
   }
  } else if (mclass==H5T_FLOAT || mclass==H5T_INTEGER) {
   int score=0;
   numeric_members[nr_numeric++]=m;
   if (strcmp(name, "time")==0) score=3;
   else if (strcmp(name, "timestamp")==0 || strcmp(name, "_time")==0) score=2;
   else if (strstr(name, "time")!=NULL) score=1;
   if (score>time_score) {
    time_score=score;
    time_member=m;
   }
  }
  H5Tclose(mtype);
 }

 /*{{{  Try the long format (string key + time + value)*/
 if (nr_string>0) {
  int k;
  int device_key=-1;
  int value_member=-1;
  local_arg->nr_of_key_members=0;
  for (k=0; k<nr_string; k++) {
   char name[128];
   hid_t mtype;
   size_t msize;
   if ((mtype=H5Tget_member_type(ftype, string_members[k]))<0) continue;
   msize=(H5Tis_variable_str(mtype)>0 ? 0 : H5Tget_size(mtype));
   H5Tclose(mtype);
   if (!read_hdf5_member_lowername(ftype, string_members[k], name, sizeof name)) continue;
   if (strstr(name, "time")!=NULL) continue;
   local_arg->key_members[local_arg->nr_of_key_members].offset=H5Tget_member_offset(local_arg->native_type, string_members[k]);
   local_arg->key_members[local_arg->nr_of_key_members].size=msize;
   if (device_key<0 && strstr(name, "device")!=NULL) device_key=local_arg->nr_of_key_members;
   local_arg->nr_of_key_members++;
  }
  if (local_arg->nr_of_key_members>0) {
   local_arg->display_members[0]=0;
   if (device_key>0) {
    local_arg->display_members[1]=device_key;
    local_arg->nr_of_display_members=2;
   } else {
    local_arg->nr_of_display_members=1;
   }
   /* Value column: a numeric array member (first element), or the first
    * scalar numeric member that is neither the time column nor "index". */
   if (array_member>=0) {
    hid_t mtype=H5Tget_member_type(ftype, array_member);
    hid_t base=H5Tget_super(mtype);
    local_arg->value_member_type=H5Tget_native_type(base, H5T_DIR_DEFAULT);
    local_arg->value_member.offset=H5Tget_member_offset(local_arg->native_type, array_member);
    local_arg->value_member.size=H5Tget_size(local_arg->value_member_type);
    H5Tclose(base);
    H5Tclose(mtype);
    value_member=array_member;
   } else {
    for (k=0; k<nr_numeric; k++) {
     char name[128];
     if (numeric_members[k]==time_member) continue;
     if (!read_hdf5_member_lowername(ftype, numeric_members[k], name, sizeof name)) continue;
     if (strcmp(name, "index")==0) continue;
     {
      hid_t mtype=H5Tget_member_type(ftype, numeric_members[k]);
      local_arg->value_member_type=H5Tget_native_type(mtype, H5T_DIR_DEFAULT);
      H5Tclose(mtype);
     }
     local_arg->value_member.offset=H5Tget_member_offset(local_arg->native_type, numeric_members[k]);
     local_arg->value_member.size=H5Tget_size(local_arg->value_member_type);
     value_member=numeric_members[k];
     break;
    }
   }
   if (value_member>=0 && local_arg->value_member_type!=H5I_INVALID_HID) {
    if (time_member>=0) {
     hid_t mtype=H5Tget_member_type(ftype, time_member);
     local_arg->time_member_type=H5Tget_native_type(mtype, H5T_DIR_DEFAULT);
     local_arg->time_member.offset=H5Tget_member_offset(local_arg->native_type, time_member);
     local_arg->time_member.size=H5Tget_size(local_arg->time_member_type);
     H5Tclose(mtype);
    }
    if (read_hdf5_scan_series(tinfo, local_arg)) {
     int s;
     for (s=0; s<local_arg->nr_of_series; s++) {
      if (local_arg->series[s].count>1) {
       /* Long format confirmed. */
       local_arg->layout=LAYOUT_LONG;
       local_arg->rank=1;
       local_arg->dims[0]=local_arg->maxdims[0]=(hsize_t)local_arg->series[0].count;
       local_arg->dims[1]=local_arg->maxdims[1]=1;
       local_arg->dims[2]=local_arg->maxdims[2]=0;
       local_arg->pointsdim=0;
       local_arg->channelsdim=1;
       return TRUE;
      }
     }
    }
   }
  }
  /* The long format did not work out; undo its state before trying wide. */
  {
   int s;
   for (s=0; s<local_arg->nr_of_series; s++) {
    free(local_arg->series[s].key);
    free(local_arg->series[s].name);
   }
   free_pointer((void **)&local_arg->series);
   local_arg->nr_of_series=0;
   local_arg->current_series=0;
   if (local_arg->value_member_type!=H5I_INVALID_HID) {
    H5Tclose(local_arg->value_member_type);
    local_arg->value_member_type=H5I_INVALID_HID;
   }
   if (local_arg->time_member_type!=H5I_INVALID_HID) {
    H5Tclose(local_arg->time_member_type);
    local_arg->time_member_type=H5I_INVALID_HID;
   }
   local_arg->nr_of_key_members=0;
   local_arg->nr_of_display_members=0;
  }
 }
 /*}}}  */

 /*{{{  Wide format: every scalar numeric member is a channel*/
 if (nr_numeric>0) {
  int c;
  for (c=0; c<nr_numeric; c++) {
   hid_t mtype=H5Tget_member_type(ftype, numeric_members[c]);
   local_arg->wide_members[c]=numeric_members[c];
   local_arg->wide_offsets[c]=H5Tget_member_offset(local_arg->native_type, numeric_members[c]);
   local_arg->wide_member_types[c]=H5Tget_native_type(mtype, H5T_DIR_DEFAULT);
   H5Tclose(mtype);
  }
  local_arg->nr_of_wide_members=nr_numeric;
  local_arg->layout=LAYOUT_WIDE;
  local_arg->rank=2;
  local_arg->dims[0]=local_arg->maxdims[0]=local_arg->compound_rows;
  local_arg->dims[1]=local_arg->maxdims[1]=nr_numeric;
  local_arg->dims[2]=local_arg->maxdims[2]=0;
  local_arg->pointsdim=0;
  local_arg->channelsdim=1;
  local_arg->estimated_sfreq=0.0;
  if (time_member>=0) {
   hid_t mtype=H5Tget_member_type(ftype, time_member);
   local_arg->time_member_type=H5Tget_native_type(mtype, H5T_DIR_DEFAULT);
   local_arg->time_member.offset=H5Tget_member_offset(local_arg->native_type, time_member);
   local_arg->time_member.size=H5Tget_size(local_arg->time_member_type);
   H5Tclose(mtype);
   {
    double t_first, t_last;
    if (local_arg->compound_rows>1
        && read_hdf5_read_row_time(tinfo, 0, &t_first)
        && read_hdf5_read_row_time(tinfo, local_arg->compound_rows-1, &t_last)
        && t_last>t_first) {
     double divisor=read_hdf5_time_divisor(t_first);
     double span=(t_last-t_first)/divisor;
     if (span>0.0) local_arg->estimated_sfreq=(double)(local_arg->compound_rows-1)/span;
    }
   }
  }
  return TRUE;
 }
 return FALSE;
 /*}}}  */
}
/*}}}  */

/*{{{  read_hdf5_close_dataset(transform_info_ptr tinfo) {*/
/* Close the currently open dataset and release everything belonging to it. */
LOCAL void
read_hdf5_close_dataset(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 int i;

 if (local_arg->file_datatype!=H5I_INVALID_HID) {
  H5Tclose(local_arg->file_datatype);
  local_arg->file_datatype=H5I_INVALID_HID;
 }
 if (local_arg->sdsid!=H5I_INVALID_HID) {
  H5Oclose(local_arg->sdsid);
  local_arg->sdsid=H5I_INVALID_HID;
 }
 if (local_arg->native_type!=H5I_INVALID_HID) {
  H5Tclose(local_arg->native_type);
  local_arg->native_type=H5I_INVALID_HID;
 }
 local_arg->native_size=0;
 local_arg->compound_rows=0;
 for (i=0; i<local_arg->nr_of_wide_members; i++) {
  if (local_arg->wide_member_types[i]!=H5I_INVALID_HID) H5Tclose(local_arg->wide_member_types[i]);
 }
 local_arg->nr_of_wide_members=0;
 for (i=0; i<local_arg->nr_of_series; i++) {
  free(local_arg->series[i].key);
  free(local_arg->series[i].name);
 }
 free_pointer((void **)&local_arg->series);
 local_arg->nr_of_series=0;
 local_arg->current_series=0;
 free_pointer((void **)&local_arg->series_filter);
 local_arg->selected_series=-1;
 if (local_arg->value_member_type!=H5I_INVALID_HID) {
  H5Tclose(local_arg->value_member_type);
  local_arg->value_member_type=H5I_INVALID_HID;
 }
 if (local_arg->time_member_type!=H5I_INVALID_HID) {
  H5Tclose(local_arg->time_member_type);
  local_arg->time_member_type=H5I_INVALID_HID;
 }
 local_arg->layout=LAYOUT_PLAIN;
 local_arg->estimated_sfreq=0.0;
 local_arg->has_vlen=FALSE;
}
/*}}}  */

/*{{{  Enumeration of all datasets in the file {*/
typedef struct {
 struct read_hdf5_storage *local_arg;
 const char *prefix;
 int depth;
} enum_ctx;

static void read_hdf5_enumerate_group(hid_t group, enum_ctx *ctx);

/* Join a group prefix and a link name into a fresh malloc'd path. */
static char *
read_hdf5_make_path(const char *prefix, const char *name) {
 size_t plen=strlen(prefix);
 char *path;
 if ((path=(char *)malloc(plen+strlen(name)+2))==NULL) return NULL;
 if (plen>0) {
  strcpy(path, prefix);
  strcat(path, "/");
  strcat(path, name);
 } else {
  strcpy(path, name);
 }
 return path;
}

/* Append a path to the storage's dataset list; takes ownership of path. */
LOCAL Bool
read_hdf5_add_path(struct read_hdf5_storage *local_arg, char *path) {
 char **newpaths;
 if ((newpaths=(char **)realloc(local_arg->dataset_paths,
     (size_t)(local_arg->nr_of_datasets+1)*sizeof(char *)))==NULL) {
  return FALSE;
 }
 local_arg->dataset_paths=newpaths;
 local_arg->dataset_paths[local_arg->nr_of_datasets++]=path;
 return TRUE;
}

static herr_t
read_hdf5_enum_visitor(hid_t group, const char *name, const H5L_info_t *info, void *op_data) {
 enum_ctx *ctx=(enum_ctx *)op_data;
 hid_t objid;
 H5I_type_t otype;
 char *path;

 (void)info;

 if ((objid=H5Oopen(group, name, H5P_DEFAULT))<0) return 0;
 otype=H5Iget_type(objid);
 if ((path=read_hdf5_make_path(ctx->prefix, name))==NULL) {
  H5Oclose(objid);
  return 0;
 }
 if (otype==H5I_GROUP) {
  /* PyTables stores internal index structures under a group named
   * "_i_table"; they are metadata, not time series. */
  if (ctx->depth<32 && strcmp(name, "_i_table")!=0) {
   enum_ctx child;
   child.local_arg=ctx->local_arg;
   child.prefix=path;
   child.depth=ctx->depth+1;
   read_hdf5_enumerate_group(objid, &child);
  }
 } else if (otype==H5I_DATASET) {
  if (!read_hdf5_add_path(ctx->local_arg, path)) {
   free(path);
  } else {
   path=NULL; /* Ownership transferred */
  }
 }
 H5Oclose(objid);
 free(path);
 return 0;
}

static void
read_hdf5_enumerate_group(hid_t group, enum_ctx *ctx) {
 H5G_info_t ginfo;
 unsigned index_type=H5_INDEX_NAME;
 hsize_t idx=0;

 if (H5Gget_info(group, &ginfo)<0) return;
 /* Prefer creation (write) order so that appended epochs are read back in
  * the order they were written; fall back to name order. */
 if (ginfo.max_corder>0) index_type=H5_INDEX_CRT_ORDER;
 H5Literate(group, index_type, H5_ITER_INC, &idx, read_hdf5_enum_visitor, ctx);
}

/* Collect the paths of all datasets in the file, recursively. */
LOCAL void
read_hdf5_enumerate_datasets(struct read_hdf5_storage *local_arg) {
 enum_ctx ctx;
 ctx.local_arg=local_arg;
 ctx.prefix="";
 ctx.depth=0;
 read_hdf5_enumerate_group(local_arg->fileid, &ctx);
}
/*}}}  */

/*{{{  read_hdf5_open_dataset(transform_info_ptr tinfo, const char *path) {*/
/* Open and classify the dataset at path; apply the -d selection and the
 * fromepoch option. Returns FALSE (with everything closed) if the dataset is
 * not usable or not selected; the caller then moves on to the next path. */
LOCAL Bool
read_hdf5_open_dataset(transform_info_ptr tinfo, const char *path) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 hid_t space;
 H5T_class_t tclass;
 int rank;
 int dim;

 if ((local_arg->sdsid=H5Dopen(local_arg->fileid, path, H5P_DEFAULT))<0) {
  local_arg->sdsid=H5I_INVALID_HID;
  return FALSE;
 }
 /* Skip Dimension Scale datasets (they describe axes, not data). */
 if (H5DSis_scale(local_arg->sdsid)>0) {
  read_hdf5_close_dataset(tinfo);
  return FALSE;
 }
 if ((space=H5Dget_space(local_arg->sdsid))<0) {
  read_hdf5_close_dataset(tinfo);
  return FALSE;
 }
 if ((local_arg->file_datatype=H5Dget_type(local_arg->sdsid))<0) {
  H5Sclose(space);
  read_hdf5_close_dataset(tinfo);
  return FALSE;
 }
 rank=H5Sget_simple_extent_ndims(space);
 tclass=H5Tget_class(local_arg->file_datatype);
 if (rank>=1 && rank<=MAXRANK_FOR_MYDATA
     && (tclass==H5T_FLOAT || tclass==H5T_INTEGER)) {
  /* Plain numeric dataset, as written by write_hdf5. */
  H5Sget_simple_extent_dims(space, local_arg->dims, local_arg->maxdims);
  local_arg->rank=rank;
  local_arg->is_continuous=FALSE;
  for (dim=0; dim<rank; dim++) {
   if (local_arg->maxdims[dim]==H5S_UNLIMITED) {
    local_arg->is_continuous=TRUE;
    break;
   }
  }
  local_arg->layout=LAYOUT_PLAIN;
 } else if (rank==1 && tclass==H5T_COMPOUND) {
  if (!read_hdf5_classify_compound(tinfo, space, local_arg->file_datatype)) {
   H5Sclose(space);
   read_hdf5_close_dataset(tinfo);
   return FALSE;
  }
  /* Compound tables are read epoched (per series/table) or, under -c, as a
   * chunked stream of the selected series. */
  local_arg->is_continuous=args[ARGS_CONTINUOUS].is_set;
 } else {
  H5Sclose(space);
  read_hdf5_close_dataset(tinfo);
  return FALSE;
 }
 H5Sclose(space);
 /* Comment: use the dataset path; overwritten by a "Comment" attribute
  * (or the series name) below if available. */
 strncpy(local_arg->descbuf, path, DESCBUF_LEN-1);
 local_arg->descbuf[DESCBUF_LEN-1]='\0';

 /*{{{  Apply the -d dataset selection*/
 if (args[ARGS_DATASET].is_set) {
  const char *sel=args[ARGS_DATASET].arg.s;
  Bool matched=FALSE;
  if (sel[0]!='\0' && sel[strspn(sel, "0123456789")]=='\0') {
   long number=strtol(sel, NULL, 10);
   if (local_arg->layout==LAYOUT_LONG) {
    /* On long tables, a numeric selection addresses a series (1-based):
     * the table itself is usually the only dataset, so a dataset index
     * would be meaningless. */
    if (number>=1 && number<=local_arg->nr_of_series) {
     local_arg->current_series=(int)(number-1);
     local_arg->selected_series=local_arg->current_series;
     matched=TRUE;
    }
   } else {
    /* 1-based dataset index. */
    matched=((long)local_arg->current_dataset+1==number);
   }
  } else {
   matched=(strstr(path, sel)!=NULL);
  }
  if (!matched && local_arg->layout==LAYOUT_LONG) {
   /* Long tables carry many series; apply the selection to those. */
   if ((local_arg->series_filter=(char *)malloc(strlen(sel)+1))==NULL) {
    ERREXIT(tinfo->emethods, "read_hdf5: Error allocating series filter\n");
   }
   strcpy(local_arg->series_filter, sel);
   while (local_arg->current_series<local_arg->nr_of_series
          && strstr(local_arg->series[local_arg->current_series].key, sel)==NULL
          && strstr(local_arg->series[local_arg->current_series].name, sel)==NULL) {
    local_arg->current_series++;
   }
   matched=(local_arg->current_series<local_arg->nr_of_series);
  }
  if (!matched) {
   read_hdf5_close_dataset(tinfo);
   return FALSE;
  }
 }
 /*}}}  */

 if (local_arg->layout==LAYOUT_LONG) {
  if (!local_arg->is_continuous) {
   /* Each series is one epoch; skip fromepoch-1 series. */
   while (local_arg->fromepoch>1 && local_arg->current_series+1<local_arg->nr_of_series) {
    local_arg->current_series++;
    local_arg->fromepoch--;
   }
   if (local_arg->fromepoch>1) {
    read_hdf5_close_dataset(tinfo);
    return FALSE;
   }
  }
  local_arg->dims[0]=local_arg->maxdims[0]=(hsize_t)local_arg->series[local_arg->current_series].count;
  local_arg->estimated_sfreq=local_arg->series[local_arg->current_series].sfreq;
  /* The comment gets the full series key; the channel name stays the
   * shorter display name. */
  strncpy(local_arg->descbuf, local_arg->series[local_arg->current_series].key, DESCBUF_LEN-1);
  local_arg->descbuf[DESCBUF_LEN-1]='\0';
 } else if (!local_arg->is_continuous) {
  if (local_arg->layout==LAYOUT_PLAIN) {
   if (args[ARGS_CONTINUOUS].is_set) {
    /* Can't read epoch data in continuous mode */
    read_hdf5_close_dataset(tinfo);
    return FALSE;
   }
   if (local_arg->trigcodes!=NULL) {
    hid_t condattr;
    if (read_hdf5_open_attr(local_arg->sdsid, "Condition", &condattr)) {
     int trigno=0;
     Bool not_correct_trigger=TRUE;
     H5Aread(condattr, H5T_NATIVE_INT32, &tinfo->condition);
     H5Aclose(condattr);
     while (local_arg->trigcodes[trigno]!=0) {
      if (local_arg->trigcodes[trigno]==tinfo->condition) {
       not_correct_trigger=FALSE;
       break;
      }
      trigno++;
     }
     if (not_correct_trigger) {
      read_hdf5_close_dataset(tinfo);
      return FALSE;
     }
    } else {
     read_hdf5_close_dataset(tinfo);
     return FALSE;
    }
   }
  }
  if (--local_arg->fromepoch>0) {
   read_hdf5_close_dataset(tinfo);
   return FALSE;
  }
 }
 return TRUE;
}
/*}}}  */

/*{{{  read_hdf5_setup_channelinfo_wide(transform_info_ptr tinfo) {*/
/* Channel names for LAYOUT_WIDE: the compound member names. */
LOCAL void
read_hdf5_setup_channelinfo_wide(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 int nr_of_channels=tinfo->nr_of_channels;
 char **channelnames;
 char *buf;
 size_t total=1;
 int c;

 for (c=0; c<local_arg->nr_of_wide_members; c++) {
  char *name=H5Tget_member_name(local_arg->file_datatype, local_arg->wide_members[c]);
  total+=(name!=NULL ? strlen(name)+1 : 2);
  if (name!=NULL) H5free_memory(name);
 }
 if ((channelnames=(char **)malloc(nr_of_channels*sizeof(char *)))==NULL
     || (buf=(char *)malloc(total))==NULL) {
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating channel names\n");
 }
 channelnames[0]=buf;
 for (c=0; c<nr_of_channels; c++) {
  char *name=H5Tget_member_name(local_arg->file_datatype, local_arg->wide_members[c]);
  if (name!=NULL) {
   strcpy(buf, name);
   H5free_memory(name);
  } else {
   strcpy(buf, "?");
  }
  channelnames[c]=buf;
  buf+=strlen(buf)+1;
 }
 tinfo->channelnames=channelnames;
}
/*}}}  */

/*{{{  read_hdf5_setup_channelinfo_series(transform_info_ptr tinfo) {*/
/* Channel name for LAYOUT_LONG: the series name. */
LOCAL void
read_hdf5_setup_channelinfo_series(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 char **channelnames;

 if ((channelnames=(char **)malloc(sizeof(char *)))==NULL
     || (channelnames[0]=(char *)malloc(strlen(local_arg->series[local_arg->current_series].name)+1))==NULL) {
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating channel names\n");
 }
 strcpy(channelnames[0], local_arg->series[local_arg->current_series].name);
 tinfo->channelnames=channelnames;
}
/*}}}  */

/*{{{  read_hdf5_read_wide(transform_info_ptr tinfo, DATATYPE *out, long start_point, long nr_of_points) {*/
/* Read rows start_point..start_point+nr_of_points-1 of a LAYOUT_WIDE table
 * into out, one sample per channel (multiplexed layout). */
LOCAL void
read_hdf5_read_wide(transform_info_ptr tinfo, DATATYPE *out, long start_point, long nr_of_points) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 hid_t filespace, memspace;
 hsize_t starts[1], counts[1];
 char *rowbuf;
 long p;
 int c;

 starts[0]=(hsize_t)start_point;
 counts[0]=(hsize_t)nr_of_points;
 if ((filespace=H5Dget_space(local_arg->sdsid))<0) {
  ERREXIT(tinfo->emethods, "read_hdf5: Error getting dataspace\n");
 }
 H5Sselect_hyperslab(filespace, H5S_SELECT_SET, starts, NULL, counts, NULL);
 if ((memspace=H5Screate_simple(1, counts, NULL))<0) {
  H5Sclose(filespace);
  ERREXIT(tinfo->emethods, "read_hdf5: Error creating memory dataspace\n");
 }
 if ((rowbuf=(char *)malloc((size_t)nr_of_points*local_arg->native_size))==NULL) {
  H5Sclose(memspace);
  H5Sclose(filespace);
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating read buffer\n");
 }
 if (H5Dread(local_arg->sdsid, local_arg->native_type, memspace, filespace, H5P_DEFAULT, rowbuf)<0) {
  free(rowbuf);
  H5Sclose(memspace);
  H5Sclose(filespace);
  ERREXIT(tinfo->emethods, "read_hdf5: H5Dread failed\n");
 }
 H5Sclose(memspace);
 H5Sclose(filespace);
 for (p=0; p<nr_of_points; p++) {
  const char *row=rowbuf+p*local_arg->native_size;
  for (c=0; c<local_arg->nr_of_wide_members; c++) {
   char tmp[16];
   size_t n=H5Tget_size(local_arg->wide_member_types[c]);
   if (n>sizeof tmp) n=sizeof tmp;
   memcpy(tmp, row+local_arg->wide_offsets[c], n);
   H5Tconvert(local_arg->wide_member_types[c], H5_MEM_DATATYPE, 1, tmp, NULL, H5P_DEFAULT);
   out[p*local_arg->nr_of_wide_members+c]=*(DATATYPE *)tmp;
  }
 }
 free(rowbuf);
}
/*}}}  */

/*{{{  read_hdf5_read_series(transform_info_ptr tinfo, DATATYPE *out, long start_point, long nr_of_points) {*/
/* Read nr_of_points samples of the current series (LAYOUT_LONG) into out,
 * skipping start_point samples of that series first. The table rows may be
 * in arbitrary order, so the whole table is scanned. */
LOCAL void
read_hdf5_read_series(transform_info_ptr tinfo, DATATYPE *out, long start_point, long nr_of_points) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 hid_t filespace=H5I_INVALID_HID;
 hid_t memspace;
 hsize_t nrows=local_arg->compound_rows;
 hsize_t start=0;
 char *rowbuf=NULL;
 long matched=0;
 long const end=start_point+nr_of_points;
 unsigned long const thishash=local_arg->series[local_arg->current_series].hash;
 char const *thiskey=local_arg->series[local_arg->current_series].key;

 if ((filespace=H5Dget_space(local_arg->sdsid))<0) {
  ERREXIT(tinfo->emethods, "read_hdf5: Error getting dataspace\n");
 }
 if ((rowbuf=(char *)malloc((size_t)SERIES_CHUNK_ROWS*local_arg->native_size))==NULL) {
  H5Sclose(filespace);
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating read buffer\n");
 }
 while (start<nrows && matched<end) {
  hsize_t count=nrows-start;
  if (count>SERIES_CHUNK_ROWS) count=SERIES_CHUNK_ROWS;
  H5Sselect_hyperslab(filespace, H5S_SELECT_SET, &start, NULL, &count, NULL);
  if ((memspace=H5Screate_simple(1, &count, NULL))<0) {
   H5Sclose(filespace);
   free(rowbuf);
   ERREXIT(tinfo->emethods, "read_hdf5: Error creating memory dataspace\n");
  }
  if (H5Dread(local_arg->sdsid, local_arg->native_type, memspace, filespace, H5P_DEFAULT, rowbuf)<0) {
   H5Sclose(memspace);
   H5Sclose(filespace);
   free(rowbuf);
   ERREXIT(tinfo->emethods, "read_hdf5: H5Dread failed\n");
  }
  {
   hsize_t idx;
   for (idx=0; idx<count && matched<end; idx++) {
    const char *row=rowbuf+idx*local_arg->native_size;
    char keybuf[4096];
    read_hdf5_row_join(row, local_arg->key_members, local_arg->nr_of_key_members, keybuf, sizeof keybuf);
    if (read_hdf5_key_hash(keybuf)!=thishash || strcmp(keybuf, thiskey)!=0) continue;
    if (matched>=start_point) {
     char tmp[16];
     size_t n=H5Tget_size(local_arg->value_member_type);
     if (n>sizeof tmp) n=sizeof tmp;
     memcpy(tmp, row+local_arg->value_member.offset, n);
     H5Tconvert(local_arg->value_member_type, H5_MEM_DATATYPE, 1, tmp, NULL, H5P_DEFAULT);
     out[matched-start_point]=*(DATATYPE *)tmp;
    }
    matched++;
   }
  }
  if (local_arg->has_vlen) {
   H5Dvlen_reclaim(local_arg->native_type, memspace, H5P_DEFAULT, rowbuf);
  }
  H5Sclose(memspace);
  start+=count;
 }
 H5Sclose(filespace);
 free(rowbuf);
}
/*}}}  */
/*}}}  */

/*{{{  read_hdf5_init(transform_info_ptr tinfo) {*/
METHODDEF void
read_hdf5_init(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 /*{{{  Process options*/
 local_arg->fromepoch=(args[ARGS_FROMEPOCH].is_set ? args[ARGS_FROMEPOCH].arg.i : 1);
 local_arg->epochs=(args[ARGS_EPOCHS].is_set ? args[ARGS_EPOCHS].arg.i : -1);
 /*}}}  */

 local_arg->fileid=H5Fopen(args[ARGS_IFILE].arg.s, H5F_ACC_RDONLY, H5P_DEFAULT);
 if (local_arg->fileid<0) {
  ERREXIT1(tinfo->emethods, "read_hdf5_init: Error opening %s\n", MSGPARM(args[ARGS_IFILE].arg.s));
 }
 local_arg->trigcodes=NULL;
 if (!args[ARGS_CONTINUOUS].is_set) {
  /* The actual trigger file is read when the first event is accessed! */
  if (args[ARGS_TRIGLIST].is_set) {
   local_arg->trigcodes=get_trigcode_list(args[ARGS_TRIGLIST].arg.s);
   if (local_arg->trigcodes==NULL) {
    ERREXIT(tinfo->emethods, "read_hdf5_init: Error allocating triglist memory\n");
   }
  }
 }
 read_hdf5_reset_triggerbuffer(tinfo);
 local_arg->current_dataset=0;
 local_arg->current_point=0;
 local_arg->sdsid=H5I_INVALID_HID;
 local_arg->rank=0;
 local_arg->file_datatype=H5I_INVALID_HID;
 local_arg->pointsdim=-1;
 local_arg->channelsdim=-1;
 local_arg->is_continuous=FALSE;
 local_arg->dataset_paths=NULL;
 local_arg->nr_of_datasets=0;
 local_arg->layout=LAYOUT_PLAIN;
 local_arg->estimated_sfreq=0.0;
 local_arg->native_type=H5I_INVALID_HID;
 local_arg->native_size=0;
 local_arg->nr_of_wide_members=0;
 local_arg->series=NULL;
 local_arg->nr_of_series=0;
 local_arg->current_series=0;
 local_arg->series_filter=NULL;
 local_arg->selected_series=-1;
 local_arg->nr_of_key_members=0;
 local_arg->nr_of_display_members=0;
 local_arg->value_member_type=H5I_INVALID_HID;
 local_arg->time_member_type=H5I_INVALID_HID;
 local_arg->has_vlen=FALSE;
 /* Collect the paths of all datasets in the file; the suitable ones are
  * opened one by one in read_hdf5(). */
 read_hdf5_enumerate_datasets(local_arg);

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  read_hdf5(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
read_hdf5(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 array myarray;

 int ret;
 long offset;
 long trigger_point=0, file_start_point, file_end_point;
 hsize_t dimsize0;
 hsize_t starts[MAXRANK_FOR_MYDATA], ends[MAXRANK_FOR_MYDATA];
 hid_t memspace=H5I_INVALID_HID, filespace=H5I_INVALID_HID;

 if (local_arg->epochs-- ==0) return NULL;

 if (local_arg->sdsid==H5I_INVALID_HID) {
  /* A new dataset must be found; it carries its own triggers. */
  growing_buf_clear(&local_arg->triggers);
  read_hdf5_reset_triggerbuffer(tinfo);
  local_arg->current_point=0;

  /*{{{  Select the next suitable dataset*/
  while (local_arg->sdsid==H5I_INVALID_HID) {
   if (local_arg->current_dataset>=(hsize_t)local_arg->nr_of_datasets) {
    if (local_arg->rank==0) {
     ERREXIT(tinfo->emethods, "read_hdf5: Cannot find a suitable first epoch\n");
    } else {
     return NULL;
    }
   }
   if (read_hdf5_open_dataset(tinfo, local_arg->dataset_paths[local_arg->current_dataset])) break;
   local_arg->current_dataset++;
  }
  /*}}}  */
  TRACEMS1(tinfo->emethods, 1, "read_hdf5: Reading dataset >%s<\n", MSGPARM(local_arg->descbuf));
 }

 /*{{{  Examine the dataset and dimension attributes*/
 tinfo->sfreq=0.0;
 tinfo->multiplexed= -1;
 tinfo->beforetrig=0;
 tinfo->aftertrig=0;
 tinfo->leaveright=0;
 tinfo->condition=0;
 tinfo->xdata=NULL;
 tinfo->probepos=NULL;
 tinfo->channelnames=NULL;
 tinfo->nrofaverages=1;

 if (local_arg->layout==LAYOUT_PLAIN) {
  /* Identify dimensions via Dimension Scales; fall back to size heuristic. */
  if (!read_hdf5_find_scales(tinfo, local_arg->sdsid)) {
   /* No Channels scale: size-based guess (mirrors read_hdf.c). */
   local_arg->channelsdim=-1;
   local_arg->pointsdim=-1;
  }
 } else {
  /* Compound tables: dimensions were determined by the classifier. */
  tinfo->multiplexed=TRUE;
 }

 {
  hid_t sfreqattr;
  if (read_hdf5_open_attr(local_arg->sdsid, "Sfreq", &sfreqattr)) {
   /* HDF5 auto-converts between float32/float64 to the memory type. */
   H5Aread(sfreqattr, H5_MEM_DATATYPE, &tinfo->sfreq);
   H5Aclose(sfreqattr);
  }
 }
 {
  hid_t attr;
  if (read_hdf5_open_attr(local_arg->sdsid, "Beforetrig", &attr)) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->beforetrig); H5Aclose(attr); }
 }
 {
  hid_t attr;
  if (read_hdf5_open_attr(local_arg->sdsid, "Leaveright", &attr)) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->leaveright); H5Aclose(attr); }
 }
 {
  hid_t attr;
  if (read_hdf5_open_attr(local_arg->sdsid, "Condition", &attr)) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->condition); H5Aclose(attr); }
 }
 {
  hid_t attr;
  if (read_hdf5_open_attr(local_arg->sdsid, "Nr_of_averages", &attr)) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->nrofaverages); H5Aclose(attr); }
 }
 /*}}}  */

 /*{{{  Check attributes and the array geometry*/
 if (tinfo->sfreq== 0.0) {
  if (local_arg->layout!=LAYOUT_PLAIN && local_arg->estimated_sfreq>0.0) {
   /* Foreign files: use the sampling frequency estimated from the table's
    * time column. */
   tinfo->sfreq=local_arg->estimated_sfreq;
  } else {
   tinfo->sfreq=100.0;
   TRACEMS(tinfo->emethods, 0, "read_hdf5: sfreq attribute not specified, setting to 100.\n");
  }
 }
 /* An unlimited dimension must be the points dimension. */
 if (local_arg->is_continuous) {
  /* Find the unlimited dim and force pointsdim to it. */
  int dim;
  for (dim=0; dim<local_arg->rank; dim++) {
   if (local_arg->maxdims[dim]==H5S_UNLIMITED) {
    local_arg->pointsdim=dim;
    break;
   }
  }
  tinfo->multiplexed=(local_arg->pointsdim==0);
 }
 if (tinfo->multiplexed== -1) {
  if (local_arg->rank==1) {
   local_arg->dims[1]=1;
   tinfo->multiplexed=TRUE;
  } else {
   if (local_arg->dims[0]>local_arg->dims[1]) {
    tinfo->multiplexed=TRUE;
   } else {
    tinfo->multiplexed=FALSE;
   }
  }
  TRACEMS1(tinfo->emethods, 0, "read_hdf5: Assuming %s format.\n", MSGPARM(tinfo->multiplexed ? "MULTIPLEXED" : "NONMULTIPLEXED"));
 }
 if (local_arg->pointsdim<0 || local_arg->channelsdim<0) {
  if (tinfo->multiplexed) {
   local_arg->pointsdim=0;
   local_arg->channelsdim=1;
  } else {
   local_arg->channelsdim=0;
   local_arg->pointsdim=1;
  }
 }
 tinfo->nr_of_channels=local_arg->dims[local_arg->channelsdim];
 tinfo->points_in_file=local_arg->dims[local_arg->pointsdim];
 /*}}}  */

 /*{{{  Read channel metadata*/
 if (local_arg->layout==LAYOUT_WIDE) {
  read_hdf5_setup_channelinfo_wide(tinfo);
 } else if (local_arg->layout==LAYOUT_LONG) {
  read_hdf5_setup_channelinfo_series(tinfo);
 } else if (local_arg->channelsdim>=0) {
  read_hdf5_setup_channelinfo(tinfo);
 }
 /*}}}  */

 /*{{{  Allocate the comment*/
 {
  hid_t commentattr;
  if (read_hdf5_open_attr(local_arg->sdsid, "Comment", &commentattr)) {
   hid_t type=H5Aget_type(commentattr);
   size_t len=H5Tget_size(type);
   char *buf;
   /* Fixed-length or variable-length string handling. */
   if (H5Tis_variable_str(type)) {
    char *vstr=NULL;
    H5Aread(commentattr, type, &vstr);
    if (vstr!=NULL) {
     if ((tinfo->comment=(char *)malloc(strlen(vstr)+1))==NULL) {
      ERREXIT(tinfo->emethods, "read_hdf5: Error allocating comment\n");
     }
     strcpy(tinfo->comment, vstr);
     free(vstr);
    } else {
     if ((tinfo->comment=(char *)malloc(1))==NULL) {
      ERREXIT(tinfo->emethods, "read_hdf5: Error allocating comment\n");
     }
     tinfo->comment[0]='\0';
    }
   } else {
    if ((buf=(char *)malloc(len+1))==NULL) {
     ERREXIT(tinfo->emethods, "read_hdf5: Error allocating comment\n");
    }
    H5Aread(commentattr, type, buf);
    buf[len]='\0';
    tinfo->comment=buf;
   }
   H5Tclose(type);
   H5Aclose(commentattr);
  } else {
   /* Fall back to the dataset link name. */
   if ((tinfo->comment=(char *)malloc(strlen(local_arg->descbuf)+1))==NULL) {
    ERREXIT(tinfo->emethods, "read_hdf5: Error allocating comment\n");
   }
   strcpy(tinfo->comment, local_arg->descbuf);
  }
 }
 /*}}}  */

 offset=(args[ARGS_OFFSET].is_set ? gettimeslice(tinfo, args[ARGS_OFFSET].arg.s) : 0);
 if (args[ARGS_BEFORETRIG].is_set) tinfo->beforetrig=gettimeslice(tinfo, args[ARGS_BEFORETRIG].arg.s);
 if (args[ARGS_AFTERTRIG].is_set) tinfo->aftertrig=gettimeslice(tinfo, args[ARGS_AFTERTRIG].arg.s);
 if (tinfo->aftertrig==0) {
  /* Automatically set the epoch length*/
  tinfo->aftertrig=local_arg->dims[local_arg->pointsdim]-tinfo->beforetrig;
 }
 tinfo->nr_of_points=tinfo->beforetrig+tinfo->aftertrig;
 if (tinfo->nr_of_points<=0) {
  ERREXIT1(tinfo->emethods, "read_hdf5: Invalid nr_of_points %d\n", MSGPARM(tinfo->nr_of_points));
 }

 /* dimsize0: 0 means continuous (unlimited points dim), >0 means epoched.
  * Used exactly as in read_hdf.c. */
 dimsize0 = local_arg->is_continuous ? 0 : local_arg->dims[local_arg->pointsdim];

 /*{{{  Find a suitable epoch*/
 while (TRUE) {
  /* Skip local_arg->fromepoch-1 epochs */
  file_start_point=local_arg->current_point;
  if (dimsize0==0) {
   if (args[ARGS_CONTINUOUS].is_set) {
    /* Simulate a trigger at current_point+beforetrig */
    trigger_point=file_start_point+tinfo->beforetrig;
    file_end_point=trigger_point+tinfo->aftertrig-1;
    if (file_end_point>=local_arg->dims[local_arg->pointsdim]) return NULL;
    local_arg->current_trigger++;
    local_arg->current_point+=tinfo->nr_of_points;
    tinfo->condition=0;
   } else {
    Bool not_correct_trigger=FALSE;
    char *description=NULL;
    do {
     tinfo->condition=read_hdf5_read_trigger(tinfo, &trigger_point, &description);
     if (tinfo->condition==0) return NULL;	/* No more triggers in file */
     file_start_point=trigger_point-tinfo->beforetrig+offset;
     file_end_point=trigger_point+tinfo->aftertrig-1-offset;

     if (local_arg->trigcodes==NULL) {
      not_correct_trigger=FALSE;
     } else {
      int trigno=0;
      not_correct_trigger=TRUE;
      while (local_arg->trigcodes[trigno]!=0) {
       if (local_arg->trigcodes[trigno]==tinfo->condition) {
        not_correct_trigger=FALSE;
        break;
       }
       trigno++;
      }
     }
    } while (not_correct_trigger || file_start_point<0 || file_end_point>=local_arg->dims[local_arg->pointsdim]);
   }
  }
  if (local_arg->fromepoch<=1) break;
  local_arg->fromepoch--;
 }
 TRACEMS3(tinfo->emethods, 1, "read_hdf5: Reading around tag %d at %d, condition=%d\n", MSGPARM(local_arg->current_trigger), MSGPARM(trigger_point), MSGPARM(tinfo->condition));
 /*}}}  */
 file_end_point=file_start_point+tinfo->nr_of_points-1;

 /*{{{  Handle triggers within the epoch (option -T)*/
 if (args[ARGS_TRIGTRANSFER].is_set) {
  int code;
  long trigpoint;
  long const old_current_trigger=local_arg->current_trigger;
  char *thisdescription;

  read_hdf5_reset_triggerbuffer(tinfo);
  /* First trigger entry holds file_start_point */
  if (dimsize0==0) {
   push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
  } else {
   code=read_hdf5_read_trigger(tinfo, &trigpoint, &thisdescription);
   if (code== -1) {
    /* File start already coded in epoch read - transfer it */
    push_trigger(&tinfo->triggers, trigpoint, code, thisdescription);
   } else {
    read_hdf5_reset_triggerbuffer(tinfo); /* Back to the first trigger */
    push_trigger(&tinfo->triggers, file_start_point, -1, NULL);
   }
  }
  for (; (code=read_hdf5_read_trigger(tinfo, &trigpoint, &thisdescription))!=0;) {
   if (trigpoint>=file_start_point && trigpoint<=file_end_point) {
    push_trigger(&tinfo->triggers, trigpoint-file_start_point, code, thisdescription);
   }
  }
  push_trigger(&tinfo->triggers, 0, 0, NULL); /* End of list */
  local_arg->current_trigger=old_current_trigger;
 }
 /*}}}  */

 /*{{{  Setup and allocate myarray*/
 myarray.element_skip=tinfo->itemsize=(local_arg->rank==3 ? local_arg->dims[2] : 1);
 if (tinfo->multiplexed) {
  myarray.nr_of_vectors=tinfo->nr_of_points;
  myarray.nr_of_elements=tinfo->nr_of_channels;
 } else {
  myarray.nr_of_vectors=tinfo->nr_of_channels;
  myarray.nr_of_elements=tinfo->nr_of_points;
 }
 if (dimsize0==0) {
  if (file_start_point+tinfo->nr_of_points>local_arg->dims[0]) {
   free_pointer((void **)&tinfo->comment);
   if (tinfo->channelnames!=NULL) {
    free_pointer((void **)&tinfo->channelnames[0]);
    free_pointer((void **)&tinfo->channelnames);
   }
   free_pointer((void **)&tinfo->probepos);
   return NULL;
  }
  starts[local_arg->pointsdim]=file_start_point;
 } else {
  starts[local_arg->pointsdim]=0;
 }
 starts[local_arg->channelsdim]=0;
 starts[2]=0;
 ends[local_arg->pointsdim]=tinfo->nr_of_points;
 ends[local_arg->channelsdim]=tinfo->nr_of_channels;
 ends[2]=tinfo->itemsize;
 tinfo->length_of_output_region=tinfo->nr_of_points*tinfo->nr_of_channels*tinfo->itemsize;
 if (array_allocate(&myarray)==NULL) {
  ERREXIT(tinfo->emethods, "read_hdf5: Error allocating data\n");
 }
 /*}}}  */

 /*{{{  Read the data*/
 if (local_arg->layout==LAYOUT_WIDE) {
  read_hdf5_read_wide(tinfo, myarray.start, file_start_point, tinfo->nr_of_points);
 } else if (local_arg->layout==LAYOUT_LONG) {
  read_hdf5_read_series(tinfo, myarray.start, file_start_point, tinfo->nr_of_points);
 } else {
  filespace=H5Dget_space(local_arg->sdsid);
  H5Sselect_hyperslab(filespace, H5S_SELECT_SET, starts, NULL, ends, NULL);
  memspace=H5Screate_simple(local_arg->rank, ends, NULL);
  ret=H5Dread(local_arg->sdsid, H5_MEM_DATATYPE, memspace, filespace, H5P_DEFAULT, myarray.start);
  H5Sclose(memspace);
  H5Sclose(filespace);
  if (ret<0) {
   array_free(&myarray);
   ERREXIT(tinfo->emethods, "read_hdf5: H5Dread failed\n");
  }
 }
 /*}}}  */

 /* This subroutine prepares any channel names/positions that might not be set */
 create_channelgrid(tinfo);

 /*{{{  Setup tinfo values*/
 tinfo->file_start_point=file_start_point;
 tinfo->z_label=NULL;
 tinfo->tsdata=myarray.start;
 tinfo->aftertrig=tinfo->nr_of_points-tinfo->beforetrig;
 tinfo->data_type=TIME_DATA;
 /*}}}  */

 if (dimsize0>0) {
  /* Epoch mode: advance to the next epoch (series or dataset). */
  if (local_arg->layout==LAYOUT_LONG) {
   local_arg->current_series++;
   while (local_arg->series_filter!=NULL
          && local_arg->current_series<local_arg->nr_of_series
          && strstr(local_arg->series[local_arg->current_series].key, local_arg->series_filter)==NULL
          && strstr(local_arg->series[local_arg->current_series].name, local_arg->series_filter)==NULL) {
    local_arg->current_series++;
   }
   if (local_arg->selected_series>=0
       && local_arg->current_series!=local_arg->selected_series) {
    /* A numeric -d selection pins the stream to exactly that series. */
    local_arg->current_series=local_arg->nr_of_series;
   }
   if (local_arg->current_series<local_arg->nr_of_series) {
    /* Stay within the table; the next series is the next epoch. */
    growing_buf_clear(&local_arg->triggers);
    read_hdf5_reset_triggerbuffer(tinfo);
    local_arg->current_point=0;
    local_arg->dims[0]=local_arg->maxdims[0]=(hsize_t)local_arg->series[local_arg->current_series].count;
    local_arg->estimated_sfreq=local_arg->series[local_arg->current_series].sfreq;
    strncpy(local_arg->descbuf, local_arg->series[local_arg->current_series].key, DESCBUF_LEN-1);
    local_arg->descbuf[DESCBUF_LEN-1]='\0';
   } else {
    read_hdf5_close_dataset(tinfo);
    local_arg->current_dataset++;
   }
  } else {
   read_hdf5_close_dataset(tinfo);
   local_arg->current_dataset++;
  }
 }

 tinfo->filetriggersp=&local_arg->triggers;

 return tinfo->tsdata;
}
/*}}}  */

/*{{{  read_hdf5_exit(transform_info_ptr tinfo) {*/
METHODDEF void
read_hdf5_exit(transform_info_ptr tinfo) {
 struct read_hdf5_storage *local_arg=(struct read_hdf5_storage *)tinfo->methods->local_storage;

 if (local_arg->sdsid!=H5I_INVALID_HID) {
  read_hdf5_close_dataset(tinfo);
 }
 H5Fclose(local_arg->fileid);
 {
  int i;
  for (i=0; i<local_arg->nr_of_datasets; i++) free(local_arg->dataset_paths[i]);
  free_pointer((void **)&local_arg->dataset_paths);
  local_arg->nr_of_datasets=0;
 }
 free_pointer((void **)&local_arg->trigcodes);

 if (local_arg->triggers.buffer_start!=NULL) {
  clear_triggers(&local_arg->triggers);
  growing_buf_free(&local_arg->triggers);
 }

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_read_hdf5(transform_info_ptr tinfo) {*/
GLOBAL void
select_read_hdf5(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &read_hdf5_init;
 tinfo->methods->transform= &read_hdf5;
 tinfo->methods->transform_exit= &read_hdf5_exit;
 tinfo->methods->method_type=GET_EPOCH_METHOD;
 tinfo->methods->method_name="read_hdf5";
 tinfo->methods->method_description=
  "Get-epoch method to read epochs from an HDF5 file.\n";
 tinfo->methods->local_storage_size=sizeof(struct read_hdf5_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
