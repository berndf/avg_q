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
struct read_hdf5_storage {
 long current_point;
 int fromepoch;
 int epochs;
 hid_t fileid;
 hid_t sdsid;            /* Currently open dataset, H5I_INVALID_HID if none */
 hsize_t current_dataset;/* Index of next dataset link to try with H5Literate */
 H5_index_t iter_index;  /* H5_INDEX_CRT_ORDER if available, else H5_INDEX_NAME */
 int rank;
 hsize_t dims[MAXRANK_FOR_MYDATA];
 hsize_t maxdims[MAXRANK_FOR_MYDATA];
 hid_t file_datatype;    /* Native datatype id returned by H5Dget_type        */
 int pointsdim;
 int channelsdim;
 Bool is_continuous;     /* TRUE if the points dim is unlimited                */
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

/*{{{  read_hdf5_dataset_visitor(group, name, info, op_data)*/
/* H5Literate visitor: opens each link, skips non-datasets, skips dimension
 * scales, requires float data with rank<=3, and records the first suitable
 * dataset. Detects continuous data via an unlimited dimension. */
typedef struct {
 struct read_hdf5_storage *local_arg;
 transform_info_ptr tinfo;
 int found;
} dataset_visitor_ctx;

static herr_t
read_hdf5_dataset_visitor(hid_t group, const char *name, const H5L_info_t *info, void *op_data) {
 dataset_visitor_ctx *ctx=(dataset_visitor_ctx *)op_data;
 struct read_hdf5_storage *local_arg=ctx->local_arg;
 hid_t objid;
 H5I_type_t otype;
 hid_t space;
 hid_t type;
 int rank;
 hsize_t dims[MAXRANK_FOR_MYDATA];
 hsize_t maxdims[MAXRANK_FOR_MYDATA];
 int dim;
 Bool is_continuous=FALSE;

 (void)group;
 (void)info;

 objid=H5Oopen(group, name, H5P_DEFAULT);
 if (objid<0) return 0; /* Skip unopenable links */
 otype=H5Iget_type(objid);
 if (otype!=H5I_DATASET) {
  H5Oclose(objid);
  return 0;
 }
 /* Skip Dimension Scale datasets (they describe axes, not data). */
 if (H5DSis_scale(objid)>0) {
  H5Oclose(objid);
  return 0;
 }
 space=H5Dget_space(objid);
 type=H5Dget_type(objid);
 rank=H5Sget_simple_extent_ndims(space);
 if (rank<1 || rank>MAXRANK_FOR_MYDATA || H5Tget_class(type)!=H5T_FLOAT) {
  H5Tclose(type);
  H5Sclose(space);
  H5Oclose(objid);
  return 0;
 }
 H5Sget_simple_extent_dims(space, dims, maxdims);
 /* Detect continuous data: any unlimited (H5S_UNLIMITED) dimension. */
 for (dim=0; dim<rank; dim++) {
  if (maxdims[dim]==H5S_UNLIMITED) {
   is_continuous=TRUE;
   break;
  }
 }
 /* Accept this dataset. */
 local_arg->sdsid=objid;
 local_arg->rank=rank;
 local_arg->is_continuous=is_continuous;
 local_arg->file_datatype=type; /* keep open; closed by caller later */
 for (dim=0; dim<rank; dim++) {
  local_arg->dims[dim]=dims[dim];
  local_arg->maxdims[dim]=maxdims[dim];
 }
 /* Comment: use the dataset's link name; overwritten by "Comment" attr below
  * if present. */
 strncpy(local_arg->descbuf, name, DESCBUF_LEN-1);
 local_arg->descbuf[DESCBUF_LEN-1]='\0';
 ctx->found=1;
 H5Sclose(space);
 /* Note: type and objid are kept open; closed by the caller after use. */
 return 1; /* Stop iteration */
}
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
 /* Prefer iterating datasets in creation (write) order so that appended
  * epochs are read back in the order they were written. This requires the
  * root group to have link creation-order tracking, which write_hdf5
  * enables on new files. Detect it via max_corder (>0 means tracking is on);
  * fall back to name order for older files without tracking. */
 {
  H5G_info_t ginfo;
  if (H5Gget_info(local_arg->fileid, &ginfo)>=0 && ginfo.max_corder>0) {
   local_arg->iter_index=H5_INDEX_CRT_ORDER;
  } else {
   local_arg->iter_index=H5_INDEX_NAME;
  }
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
   dataset_visitor_ctx vctx;
   herr_t iter_status;
   H5G_info_t ginfo;

   /* Check whether we've already visited all links in the root group.
    * Calling H5Literate with an out-of-range index raises an error stack,
    * so we guard against that here. */
   if (H5Gget_info(local_arg->fileid, &ginfo)<0
       || local_arg->current_dataset>=ginfo.nlinks) {
    if (local_arg->rank==0) {
     ERREXIT(tinfo->emethods, "read_hdf5: Cannot find a suitable first epoch\n");
    } else {
     return NULL;
    }
   }

   vctx.local_arg=local_arg;
   vctx.tinfo=tinfo;
   vctx.found=0;
   iter_status=H5Literate(local_arg->fileid, local_arg->iter_index, H5_ITER_INC,
     &local_arg->current_dataset, read_hdf5_dataset_visitor, &vctx);
   if (iter_status<=0 || !vctx.found) {
    /* No more suitable datasets (iter_status==0) or iteration error (<0). */
    if (local_arg->rank==0) {
     ERREXIT(tinfo->emethods, "read_hdf5: Cannot find a suitable first epoch\n");
    } else {
     return NULL;
    }
   }

   /* Epoched datasets are accepted directly; continuous datasets are also
    * accepted and read in chunks below. Filter epoched datasets by condition
    * when a triglist was requested, and honour fromepoch. */
   if (!local_arg->is_continuous) {
    if (args[ARGS_CONTINUOUS].is_set) {
     /* Can't read epoch data in continuous mode */
     H5Tclose(local_arg->file_datatype);
     H5Oclose(local_arg->sdsid);
     local_arg->file_datatype=H5I_INVALID_HID;
     local_arg->sdsid=H5I_INVALID_HID;
     continue;
    }
    if (local_arg->trigcodes!=NULL) {
     hid_t condattr=H5Aopen(local_arg->sdsid, "Condition", H5P_DEFAULT);
     if (condattr>=0) {
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
       H5Tclose(local_arg->file_datatype);
       H5Oclose(local_arg->sdsid);
       local_arg->file_datatype=H5I_INVALID_HID;
       local_arg->sdsid=H5I_INVALID_HID;
       continue;
      }
     } else {
      H5Tclose(local_arg->file_datatype);
      H5Oclose(local_arg->sdsid);
      local_arg->file_datatype=H5I_INVALID_HID;
      local_arg->sdsid=H5I_INVALID_HID;
      continue;
     }
    }
    if (--local_arg->fromepoch>0) {
     H5Tclose(local_arg->file_datatype);
     H5Oclose(local_arg->sdsid);
     local_arg->file_datatype=H5I_INVALID_HID;
     local_arg->sdsid=H5I_INVALID_HID;
     continue;
    }
   }
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

 /* Identify dimensions via Dimension Scales; fall back to size heuristic. */
 if (!read_hdf5_find_scales(tinfo, local_arg->sdsid)) {
  /* No Channels scale: size-based guess (mirrors read_hdf.c). */
  local_arg->channelsdim=-1;
  local_arg->pointsdim=-1;
 }

 {
  hid_t sfreqattr=H5Aopen(local_arg->sdsid, "Sfreq", H5P_DEFAULT);
  if (sfreqattr>=0) {
   /* HDF5 auto-converts between float32/float64 to the memory type. */
   H5Aread(sfreqattr, H5_MEM_DATATYPE, &tinfo->sfreq);
   H5Aclose(sfreqattr);
  }
 }
 {
  hid_t attr=H5Aopen(local_arg->sdsid, "Beforetrig", H5P_DEFAULT);
  if (attr>=0) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->beforetrig); H5Aclose(attr); }
 }
 {
  hid_t attr=H5Aopen(local_arg->sdsid, "Leaveright", H5P_DEFAULT);
  if (attr>=0) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->leaveright); H5Aclose(attr); }
 }
 {
  hid_t attr=H5Aopen(local_arg->sdsid, "Condition", H5P_DEFAULT);
  if (attr>=0) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->condition); H5Aclose(attr); }
 }
 {
  hid_t attr=H5Aopen(local_arg->sdsid, "Nr_of_averages", H5P_DEFAULT);
  if (attr>=0) { H5Aread(attr, H5T_NATIVE_INT32, &tinfo->nrofaverages); H5Aclose(attr); }
 }
 /*}}}  */

 /*{{{  Check attributes and the array geometry*/
 if (tinfo->sfreq== 0.0) {
  tinfo->sfreq=100.0;
  TRACEMS(tinfo->emethods, 0, "read_hdf5: sfreq attribute not specified, setting to 100.\n");
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

 /*{{{  Read channel metadata from the Channels scale*/
 if (local_arg->channelsdim>=0) {
  read_hdf5_setup_channelinfo(tinfo);
 }
 /*}}}  */

 /*{{{  Allocate the comment*/
 {
  hid_t commentattr=H5Aopen(local_arg->sdsid, "Comment", H5P_DEFAULT);
  if (commentattr>=0) {
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
  /* Epoch mode: next epoch is next dataset. Close the current one. */
  H5Tclose(local_arg->file_datatype);
  H5Oclose(local_arg->sdsid);
  local_arg->file_datatype=H5I_INVALID_HID;
  local_arg->sdsid=H5I_INVALID_HID;
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
  if (local_arg->file_datatype!=H5I_INVALID_HID) H5Tclose(local_arg->file_datatype);
  H5Oclose(local_arg->sdsid);
  local_arg->sdsid=H5I_INVALID_HID;
  local_arg->file_datatype=H5I_INVALID_HID;
 }
 H5Fclose(local_arg->fileid);
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
