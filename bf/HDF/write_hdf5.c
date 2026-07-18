/*
 * Copyright (C) 2026 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*{{{}}}*/
/*{{{  Description*/
/*
 * write_hdf5.c method to write data to an HDF5 file
 *
 * Mirrors write_hdf.c. Dimension identification uses HDF5 Dimension Scales:
 *  - A "Channels" scale (1-D, length nr_of_channels) is attached to the
 *    channels dimension. Per-channel metadata (name + 3 probepos values) is
 *    stored as attributes on the scale, ordered by creation order so channel
 *    index == attribute creation order.
 *  - An "Item" scale is attached to the item dimension when rank==3.
 *  - The points dimension has no scale (identified positionally by readers).
 *
 * HDF5 dataset link names must be unique and cannot contain '/', so the
 * comment is sanitized for use as the link name and also stored verbatim as
 * a "Comment" string attribute.
 */
/*}}}  */

/*{{{  #includes*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
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

/*{{{  Arguments*/
LOCAL const char *const compression_choice[]={
 "-deflate",
 NULL
};
/* HDF5 offers several built-in (standard) compression filters: deflate (gzip,
 * levels 1-9, requires zlib), nbit (parameterless) and scaleoffset (takes a
 * scale factor, lossy for floating-point data). SZIP is deprecated and needs an
 * external library with licensing restrictions. Of these, only deflate has
 * configurable compression levels, so we expose just -deflate at the maximum
 * level (9). nbit and scaleoffset have no "level" to set and are a poor fit for
 * the generic float/double EEG data written here, so they are not offered. */
enum ARGS_ENUM {
 ARGS_APPEND=0,
 ARGS_CONTINUOUS,
 ARGS_COMPRESS,
 ARGS_OFILE,
 NR_OF_ARGUMENTS
};
LOCAL transform_argument_descriptor argument_descriptors[NR_OF_ARGUMENTS]={
 {T_ARGS_TAKES_NOTHING, "Append data if file exists", "a", FALSE, NULL},
 {T_ARGS_TAKES_NOTHING, "Continuous output (unlimited first dimension)", "c", FALSE, NULL},
 {T_ARGS_TAKES_SELECTION, "Choose output compression", " ", 0, compression_choice},
 {T_ARGS_TAKES_FILENAME, "Output file", "", ARGDESC_UNUSED, (const char *const *)"*.hdf5"}
};
/*}}}  */

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

/*{{{  Definition of write_hdf5_storage*/
/* This structure has to be defined to carry the sampling freq over epochs */
struct write_hdf5_storage {
 hid_t fileid;
 hid_t sdsid;
 hid_t file_datatype;
 long pointno;
 growing_buf triggers;
};
/*}}}  */

/*{{{  write_hdf5_make_link_name(comment, outbuf, bufsize)*/
/* Sanitize the comment into a valid, non-empty HDF5 link name: replace '/' and
 * leading whitespace; fall back to "epoch" if the result is empty. */
LOCAL void
write_hdf5_make_link_name(const char *comment, char *outbuf, size_t bufsize) {
 const char *src=comment;
 size_t i=0;
 if (src==NULL) src="";
 while (*src && i<bufsize-1) {
  if (*src=='/') {
   outbuf[i++]='_';
  } else {
   outbuf[i++]=*src;
  }
  src++;
 }
 outbuf[i]='\0';
 if (i==0) {
  snprintf(outbuf, bufsize, "epoch");
 }
}
/*}}}  */

/*{{{  write_hdf5_unique_link_name(fileid, comment, outbuf, bufsize)*/
/* Ensure the link name is unique within the file by appending _2, _3, ... */
LOCAL void
write_hdf5_unique_link_name(hid_t fileid, const char *comment, char *outbuf, size_t bufsize) {
 char base[DESCBUF_LEN-16];
 char tryname[DESCBUF_LEN];
 int suffix=2;
 write_hdf5_make_link_name(comment, base, sizeof(base));
 snprintf(tryname, sizeof(tryname), "%s", base);
 while (H5Lexists(fileid, tryname, H5P_DEFAULT)>0) {
  snprintf(tryname, sizeof(tryname), "%s_%d", base, suffix);
  suffix++;
  if (suffix>9999) break; /* safety valve */
 }
 snprintf(outbuf, bufsize, "%s", tryname);
}
/*}}}  */

/*{{{  write_hdf5_write_string_attr(objid, name, value)*/
LOCAL void
write_hdf5_write_string_attr(hid_t objid, const char *name, const char *value) {
 hid_t type=H5Tcopy(H5T_C_S1);
 hid_t space;
 hid_t attr;
 size_t len=strlen(value);
 H5Tset_size(type, len);
 space=H5Screate(H5S_SCALAR);
 attr=H5Acreate2(objid, name, type, space, H5P_DEFAULT, H5P_DEFAULT);
 H5Awrite(attr, type, value);
 H5Aclose(attr);
 H5Sclose(space);
 H5Tclose(type);
}
/*}}}  */

/*{{{  write_hdf5_attach_channels_scale(tinfo, sdsid, channelsdim)*/
/* Create the Channels Dimension Scale on the channels dimension and write
 * per-channel metadata (name=probepos[3]) as attributes on the scale. The
 * scale's dcpl enables attribute creation-order tracking so that channel
 * index == attribute creation order. */
LOCAL void
write_hdf5_attach_channels_scale(transform_info_ptr tinfo, hid_t sdsid, int channelsdim) {
 int nr_of_channels=tinfo->nr_of_channels;
 hsize_t scale_dims[1];
 hid_t scale_space, scale_dcpl, scaleid;
 char scale_name[32];
 int channel;
 int suffix=0;

 scale_dims[0]=nr_of_channels;
 scale_space=H5Screate_simple(1, scale_dims, scale_dims);
 scale_dcpl=H5Pcreate(H5P_DATASET_CREATE);
 H5Pset_attr_creation_order(scale_dcpl, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED);
 /* The scale dataset link name must be unique within the file root group.
  * First scale is "Channels", subsequent ones get a numeric suffix. */
 {
  hid_t fileid=((struct write_hdf5_storage *)tinfo->methods->local_storage)->fileid;
  suffix=0;
  do {
   if (suffix==0) {
    strcpy(scale_name, "Channels");
   } else {
    snprintf(scale_name, sizeof(scale_name), "Channels_%d", suffix);
   }
   suffix++;
  } while (H5Lexists(fileid, scale_name, H5P_DEFAULT)>0);
  scaleid=H5Dcreate2(fileid, scale_name, H5T_STD_I32LE, scale_space,
    H5P_DEFAULT, scale_dcpl, H5P_DEFAULT);
 }
 if (scaleid<0) {
  H5Pclose(scale_dcpl);
  H5Sclose(scale_space);
  ERREXIT(tinfo->emethods, "write_hdf5: H5Dcreate2 failed for Channels scale\n");
 }
 H5DSset_scale(scaleid, "Channels");
 /* Write per-channel attributes: name=3 float64 (probepos). */
 for (channel=0; channel<nr_of_channels; channel++) {
  hid_t attr_space=H5Screate_simple(1, (hsize_t[1]){3}, NULL);
  hid_t attr=H5Acreate2(scaleid, tinfo->channelnames[channel], H5T_IEEE_F64LE,
    attr_space, H5P_DEFAULT, H5P_DEFAULT);
  H5Awrite(attr, H5T_NATIVE_DOUBLE, &tinfo->probepos[3*channel]);
  H5Aclose(attr);
  H5Sclose(attr_space);
 }
 /* Attach the scale to the channels dimension of the dataset. */
 H5DSattach_scale(sdsid, scaleid, channelsdim);
 H5Dclose(scaleid); /* DIMENSION_LIST holds a reference; safe to close. */
 H5Pclose(scale_dcpl);
 H5Sclose(scale_space);
}
/*}}}  */

/*{{{  write_hdf5_attach_item_scale(tinfo, sdsid, itemsize)*/
LOCAL void
write_hdf5_attach_item_scale(transform_info_ptr tinfo, hid_t sdsid, int itemsize) {
 hsize_t scale_dims[1];
 hid_t scale_space, scale_dcpl, scaleid;
 char scale_name[32];
 int suffix=0;

 scale_dims[0]=itemsize;
 scale_space=H5Screate_simple(1, scale_dims, scale_dims);
 scale_dcpl=H5Pcreate(H5P_DATASET_CREATE);
 H5Pset_attr_creation_order(scale_dcpl, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED);
 /* The scale dataset link name must be unique within the file root group.
  * First scale is "Item", subsequent ones get a numeric suffix. */
 do {
  if (suffix==0) {
   strcpy(scale_name, "Item");
  } else {
   snprintf(scale_name, sizeof(scale_name), "Item_%d", suffix);
  }
  suffix++;
 } while (H5Lexists(((struct write_hdf5_storage *)tinfo->methods->local_storage)->fileid,
    scale_name, H5P_DEFAULT)>0);
 scaleid=H5Dcreate2(((struct write_hdf5_storage *)tinfo->methods->local_storage)->fileid,
    scale_name, H5T_STD_I32LE, scale_space, H5P_DEFAULT, scale_dcpl, H5P_DEFAULT);
 if (scaleid<0) {
  H5Pclose(scale_dcpl);
  H5Sclose(scale_space);
  ERREXIT(tinfo->emethods, "write_hdf5: H5Dcreate2 failed for Item scale\n");
 }
 H5DSset_scale(scaleid, "Item");
 H5DSattach_scale(sdsid, scaleid, 2); /* item dim is always axis 2 */
 H5Dclose(scaleid);
 H5Pclose(scale_dcpl);
 H5Sclose(scale_space);
}
/*}}}  */

/*{{{  write_hdf5_load_existing_triggers(transform_info_ptr tinfo, hid_t sdsid) {*/
/* Continuous-append mode: read the "Triggers" attribute already stored on the
 * existing dataset into local_arg->triggers so that, after record_triggers()
 * appends the new run's triggers, write_triggers() can rewrite the attribute
 * with the full combined list. The stored layout is [pos0, code0, pos1,
 * code1, ...] of the actual triggers (the file-start marker is not stored,
 * see write_triggers()). We re-create that marker here as the first entry. */
LOCAL void
write_hdf5_load_existing_triggers(transform_info_ptr tinfo, hid_t sdsid) {
 struct write_hdf5_storage *local_arg=(struct write_hdf5_storage *)tinfo->methods->local_storage;
 hid_t attrid;
 /* H5Aopen prints HDF5-DIAG spam on failure even when we check the return
  * value, so guard with H5Aexists first. Returns >0 if it exists, 0 if not. */
 if (H5Aexists(sdsid, "Triggers")<=0) return;
 attrid=H5Aopen(sdsid, "Triggers", H5P_DEFAULT);
 if (attrid>=0) {
  hid_t spacetype=H5Aget_type(attrid);
  hid_t space=H5Aget_space(attrid);
  int attrndims=H5Sget_simple_extent_ndims(space);
  hsize_t attrdims[1];
  hsize_t attrcount;
  int n_entries, i;
  int32_t *trigentries;
  if (attrndims==1 && H5Tget_class(spacetype)==H5T_INTEGER && H5Tget_size(spacetype)==4) {
   H5Sget_simple_extent_dims(space, attrdims, NULL);
   attrcount=attrdims[0];
   n_entries=attrcount/2;
   if (attrcount%2==0 && n_entries>=0
    && (trigentries=(int32_t *)malloc(attrcount*sizeof(int32_t)))!=NULL) {
    H5Aread(attrid, H5T_NATIVE_INT32, trigentries);
    /* Re-create the file-start marker that record_triggers() expects as the
     * first entry (position 0, code -1). */
    push_trigger(&local_arg->triggers, 0L, -1, NULL);
    for (i=0; i<n_entries; i++) {
     push_trigger(&local_arg->triggers, (long)trigentries[2*i], (int)trigentries[2*i+1], NULL);
    }
    free(trigentries);
   }
  }
  H5Tclose(spacetype);
  H5Sclose(space);
  H5Aclose(attrid);
 }
}
/*}}}  */

/*{{{  record_triggers(transform_info_ptr tinfo) {*/
LOCAL void
record_triggers(transform_info_ptr tinfo) {
 /* In order to write the full trigger list using write_triggers() at the end, we
  * record triggers from all epochs encountered in continuous mode */
 if (tinfo->triggers.buffer_start!=NULL) {
  struct write_hdf5_storage *local_arg=(struct write_hdf5_storage *)tinfo->methods->local_storage;
  struct trigger *intrig=(struct trigger *)tinfo->triggers.buffer_start+1;
  if (local_arg->triggers.current_length==0) {
   push_trigger(&local_arg->triggers,0L,-1,NULL);
  }
  while (intrig->code!=0) {
   push_trigger(&local_arg->triggers,local_arg->pointno+intrig->position,intrig->code,intrig->description);
   intrig++;
  }
 }
}
/*}}}  */

/*{{{  write_triggers(transform_info_ptr tinfo, growing_buf *triggersp, hid_t sdsid) {*/
LOCAL void
write_triggers(transform_info_ptr tinfo, growing_buf *triggersp, hid_t sdsid) {
 if (triggersp->buffer_start!=NULL) {
  /* Write triggers array as a data set attribute */
  struct trigger * const intrig=(struct trigger *)triggersp->buffer_start;
  int32_t * trigentries;
  int n_entries=1;
  hid_t space, attr;

  while (intrig[n_entries].code!=0) n_entries++;
  if (n_entries<=1) return;
  if ((trigentries=(int32_t *)malloc(2*n_entries*sizeof(int32_t)))==NULL) {
   ERREXIT(tinfo->emethods, "write_hdf5: Error allocating trigger array\n");
  }
  n_entries=0;
  do {
   trigentries[2*n_entries]  =intrig[n_entries].position;
   trigentries[2*n_entries+1]=intrig[n_entries].code;
   n_entries++;
  } while (intrig[n_entries].code!=0);
  {
   hsize_t attr_dims[1];
   /* When appending in continuous mode, the "Triggers" attribute already
    * exists from the previous run; remove it so H5Acreate2 can recreate it
    * with the combined trigger list. */
   if (H5Aexists(sdsid, "Triggers")>0) {
    H5Adelete(sdsid, "Triggers");
   }
   if (intrig[0].position>0) {
    attr_dims[0]=2*n_entries;
    space=H5Screate_simple(1, attr_dims, NULL);
    attr=H5Acreate2(sdsid, "Triggers", H5T_STD_I32LE, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, H5T_NATIVE_INT32, trigentries);
   } else {
    attr_dims[0]=2*(n_entries-1);
    space=H5Screate_simple(1, attr_dims, NULL);
    attr=H5Acreate2(sdsid, "Triggers", H5T_STD_I32LE, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, H5T_NATIVE_INT32, trigentries+2);
   }
   H5Aclose(attr);
   H5Sclose(space);
  }
  free(trigentries);
 }
}
/*}}}  */

/*{{{  write_hdf5_find_append_dataset(tinfo)*/
/* Continuous-append mode: iterate links in the file to find a dataset
 * compatible with the current epoch (float type, matching rank, channels and
 * itemsize, with an unlimited points dimension). On success, sets
 * local_arg->sdsid and local_arg->pointno to the current extent of the points
 * dim. Returns TRUE if a compatible dataset was found. */
typedef struct {
 struct write_hdf5_storage *local_arg;
 transform_info_ptr tinfo;
 int itemsize;
 int nr_of_channels;
 int rank_wanted;
 int found;
} append_visitor_ctx;

static herr_t
write_hdf5_append_visitor(hid_t group, const char *name, const H5L_info_t *info, void *op_data) {
 append_visitor_ctx *ctx=(append_visitor_ctx *)op_data;
 hid_t objid;
 hid_t space, type;
 int rank;
 hsize_t dims[MAXRANK_FOR_MYDATA], maxdims[MAXRANK_FOR_MYDATA];
 int dim;
 Bool has_unlimited=FALSE;
 hsize_t unlimited_dim=0;

 (void)group;
 (void)info;

 objid=H5Oopen(group, name, H5P_DEFAULT);
 if (objid<0) return 0;
 if (H5Iget_type(objid)!=H5I_DATASET) { H5Oclose(objid); return 0; }
 if (H5DSis_scale(objid)>0) { H5Oclose(objid); return 0; }
 space=H5Dget_space(objid);
 type=H5Dget_type(objid);
 rank=H5Sget_simple_extent_ndims(space);
 if (rank!=ctx->rank_wanted || H5Tget_class(type)!=H5T_FLOAT) {
  H5Tclose(type);
  H5Sclose(space);
  H5Oclose(objid);
  return 0;
 }
 H5Sget_simple_extent_dims(space, dims, maxdims);
 for (dim=0; dim<rank; dim++) {
  if (maxdims[dim]==H5S_UNLIMITED) { has_unlimited=TRUE; unlimited_dim=dim; break; }
 }
 if (!has_unlimited) {
  H5Tclose(type);
  H5Sclose(space);
  H5Oclose(objid);
  return 0;
 }
 /* Channels and itemsize must match. The non-unlimited, non-item dims must
  * match nr_of_channels (rank 2) or dims[non-points-non-item]==nr_of_channels
  * and dims[item]==itemsize (rank 3). For simplicity we check the two
  * non-points dims. */
 {
  int channels_ok=0, item_ok=0;
  int channelsdim;
  /* For rank 2: the other dim is channels. For rank 3: the remaining dim is
   * channels, and the third dim is item. */
  if (rank==2) {
   channelsdim = (unlimited_dim==0 ? 1 : 0);
   channels_ok = (dims[channelsdim]==ctx->nr_of_channels);
   item_ok = (ctx->itemsize==1);
  } else {
   /* rank 3: find channels and item dims among the non-points dims. */
   int other[2], k=0;
   for (dim=0; dim<3; dim++) if (dim!=(int)unlimited_dim) other[k++]=dim;
   /* Try both assignments: item is the dim matching itemsize, channels the
    * other matching nr_of_channels. */
   if (dims[other[0]]==ctx->itemsize && dims[other[1]]==ctx->nr_of_channels) {
    item_ok=1; channels_ok=1;
   } else if (dims[other[1]]==ctx->itemsize && dims[other[0]]==ctx->nr_of_channels) {
    item_ok=1; channels_ok=1;
   }
  }
  if (!channels_ok || !item_ok) {
   H5Tclose(type);
   H5Sclose(space);
   H5Oclose(objid);
   return 0;
  }
 }
 ctx->local_arg->sdsid=objid;
 ctx->local_arg->file_datatype=type;
 ctx->local_arg->pointno=dims[unlimited_dim];
 ctx->found=1;
 H5Sclose(space);
 return 1;
}

LOCAL Bool
write_hdf5_find_append_dataset(transform_info_ptr tinfo) {
 struct write_hdf5_storage *local_arg=(struct write_hdf5_storage *)tinfo->methods->local_storage;
 append_visitor_ctx ctx;
 hsize_t idx=0;
 ctx.local_arg=local_arg;
 ctx.tinfo=tinfo;
 ctx.itemsize=tinfo->itemsize;
 ctx.nr_of_channels=tinfo->nr_of_channels;
 ctx.rank_wanted=(tinfo->itemsize==1 ? 2 : 3);
 ctx.found=0;
 H5Literate(local_arg->fileid, H5_INDEX_NAME, H5_ITER_INC, &idx, write_hdf5_append_visitor, &ctx);
 return ctx.found ? TRUE : FALSE;
}
/*}}}  */

/*{{{  write_hdf5_init(transform_info_ptr tinfo) {*/
METHODDEF void
write_hdf5_init(transform_info_ptr tinfo) {
 struct write_hdf5_storage *local_arg=(struct write_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 Bool append=args[ARGS_APPEND].is_set;

 local_arg->pointno=0;
 local_arg->sdsid=H5I_INVALID_HID;
 local_arg->file_datatype=H5I_INVALID_HID;
 if (append) {
  struct stat statbuff;
  if (stat(args[ARGS_OFILE].arg.s, &statbuff)!=0) {
   append=FALSE;
  }
 }
 if (append) {
  local_arg->fileid=H5Fopen(args[ARGS_OFILE].arg.s, H5F_ACC_RDWR, H5P_DEFAULT);
  if (local_arg->fileid<0) {
   ERREXIT1(tinfo->emethods, "write_hdf5_init: Error opening file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
 } else {
  hid_t fcpl=H5Pcreate(H5P_FILE_CREATE);
  /* Enable link creation-order tracking+indexing on the root group so that
   * readers can iterate datasets in creation (write) order via
   * H5_INDEX_CRT_ORDER. Without this, only H5_INDEX_NAME (alphabetical) is
   * available, which scrambles epochs whose sanitized comment suffixes sort
   * lexically (eg. output_10 before output_2). */
  H5Pset_link_creation_order(fcpl, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED);
  local_arg->fileid=H5Fcreate(args[ARGS_OFILE].arg.s, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT);
  H5Pclose(fcpl);
  if (local_arg->fileid<0) {
   ERREXIT1(tinfo->emethods, "write_hdf5_init: Error opening file >%s<\n", MSGPARM(args[ARGS_OFILE].arg.s));
  }
 }
 if (args[ARGS_CONTINUOUS].is_set) {
  /*{{{  Look for a compatible dataset to append to (otherwise, a new one is created)*/
  if (append) {
   if (write_hdf5_find_append_dataset(tinfo)) {
    /* local_arg->sdsid and pointno are set. */
   } else {
    local_arg->sdsid=H5I_INVALID_HID;
   }
  } else {
   local_arg->sdsid=H5I_INVALID_HID;
  }
  /* Initialize the buffer to record triggers in */
  growing_buf_init(&local_arg->triggers);
  /* If appending to an existing dataset, pre-load its triggers so the final
   * write_triggers() rewrites the attribute with the combined list. */
  if (local_arg->sdsid!=H5I_INVALID_HID) {
   write_hdf5_load_existing_triggers(tinfo, local_arg->sdsid);
  }
  /*}}}  */
 }

 tinfo->methods->init_done=TRUE;
}
/*}}}  */

/*{{{  write_hdf5(transform_info_ptr tinfo) {*/
METHODDEF DATATYPE *
write_hdf5(transform_info_ptr tinfo) {
 struct write_hdf5_storage *local_arg=(struct write_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;
 int rank;
 int pointsdim, channelsdim;
 hsize_t starts[MAXRANK_FOR_MYDATA], ends[MAXRANK_FOR_MYDATA], dims[MAXRANK_FOR_MYDATA], maxdims[MAXRANK_FOR_MYDATA];
 hid_t sdsid=local_arg->sdsid;
 hid_t space, memspace, filespace, dcpl;
 char linkname[DESCBUF_LEN];

 if (args[ARGS_CONTINUOUS].is_set) multiplexed(tinfo);

 /*{{{  Prepare the variables for H5Screate_simple*/
 if (tinfo->itemsize==1) {
  rank=2;
 } else {
  rank=3;
  dims[2]=tinfo->itemsize;
  maxdims[2]=tinfo->itemsize;
 }
 if (tinfo->multiplexed) {
  pointsdim=0;
  channelsdim=1;
 } else {
  channelsdim=0;
  pointsdim=1;
 }
 starts[0]=starts[1]=starts[2]=0;
 if (args[ARGS_CONTINUOUS].is_set) {
  dims[pointsdim]=0;
  maxdims[pointsdim]=H5S_UNLIMITED;
 } else {
  dims[pointsdim]=tinfo->nr_of_points;
  maxdims[pointsdim]=tinfo->nr_of_points;
 }
 ends[pointsdim]=tinfo->nr_of_points;
 ends[channelsdim]=dims[channelsdim]=tinfo->nr_of_channels;
 maxdims[channelsdim]=tinfo->nr_of_channels;
 ends[2]=dims[2];
 /*}}}  */

 if (!args[ARGS_CONTINUOUS].is_set || sdsid==H5I_INVALID_HID) {
  /*{{{  Create a new dataset*/
  write_hdf5_unique_link_name(local_arg->fileid, tinfo->comment, linkname, sizeof(linkname));
  space=H5Screate_simple(rank, dims, maxdims);
  dcpl=H5Pcreate(H5P_DATASET_CREATE);
  if (args[ARGS_CONTINUOUS].is_set || args[ARGS_COMPRESS].is_set) {
   /* Chunked layout is required for unlimited dimensions and for filters
    * (eg. deflate). */
   hsize_t chunk_dims[MAXRANK_FOR_MYDATA];
   chunk_dims[pointsdim]=tinfo->nr_of_points;
   chunk_dims[channelsdim]=tinfo->nr_of_channels;
   chunk_dims[2]=tinfo->itemsize;
   H5Pset_chunk(dcpl, rank, chunk_dims);
  }
  if (args[ARGS_COMPRESS].is_set) {
   H5Pset_deflate(dcpl, 9);
  }
  sdsid=H5Dcreate2(local_arg->fileid, linkname, H5_FILE_DATATYPE, space,
    H5P_DEFAULT, dcpl, H5P_DEFAULT);
  H5Pclose(dcpl);
  H5Sclose(space);
  if (sdsid < 0) {
   ERREXIT(tinfo->emethods, "write_hdf5: H5Dcreate2 failed\n");
  }
  /* Attach Dimension Scales. */
  write_hdf5_attach_channels_scale(tinfo, sdsid, channelsdim);
  if (tinfo->itemsize>1) {
   write_hdf5_attach_item_scale(tinfo, sdsid, tinfo->itemsize);
  }
  /* Write dataset attributes. */
  {
   hid_t attr_space_scalar=H5Screate(H5S_SCALAR);
   hid_t attr;
   attr=H5Acreate2(sdsid, "Sfreq", H5_FILE_DATATYPE, attr_space_scalar, H5P_DEFAULT, H5P_DEFAULT);
   H5Awrite(attr, H5_MEM_DATATYPE, &tinfo->sfreq);
   H5Aclose(attr);
   attr=H5Acreate2(sdsid, "Beforetrig", H5T_STD_I32LE, attr_space_scalar, H5P_DEFAULT, H5P_DEFAULT);
   H5Awrite(attr, H5T_NATIVE_INT32, &tinfo->beforetrig);
   H5Aclose(attr);
   attr=H5Acreate2(sdsid, "Leaveright", H5T_STD_I32LE, attr_space_scalar, H5P_DEFAULT, H5P_DEFAULT);
   H5Awrite(attr, H5T_NATIVE_INT32, &tinfo->leaveright);
   H5Aclose(attr);
   attr=H5Acreate2(sdsid, "Condition", H5T_STD_I32LE, attr_space_scalar, H5P_DEFAULT, H5P_DEFAULT);
   H5Awrite(attr, H5T_NATIVE_INT32, &tinfo->condition);
   H5Aclose(attr);
   attr=H5Acreate2(sdsid, "Nr_of_averages", H5T_STD_I32LE, attr_space_scalar, H5P_DEFAULT, H5P_DEFAULT);
   H5Awrite(attr, H5T_NATIVE_INT32, &tinfo->nrofaverages);
   H5Aclose(attr);
   H5Sclose(attr_space_scalar);
  }
  write_hdf5_write_string_attr(sdsid, "Comment", tinfo->comment ? tinfo->comment : "");
  if (args[ARGS_CONTINUOUS].is_set) {
   record_triggers(tinfo);
  } else {
   write_triggers(tinfo,&tinfo->triggers,sdsid);
  }
  local_arg->file_datatype=H5_FILE_DATATYPE;
  /*}}}  */
 } else {
  /*{{{  Append to an existing dataset*/
  if (dims[channelsdim]!=tinfo->nr_of_channels) {
   ERREXIT(tinfo->emethods, "write_hdf5: Cannot write epochs with varying nr_of_channels!\n");
  }
  starts[pointsdim]=local_arg->pointno;
  record_triggers(tinfo);
  /*}}}  */
 }

 /*{{{  Write the data*/
 if (args[ARGS_CONTINUOUS].is_set) {
  /* Extend the dataset to hold the new data. */
  hsize_t new_extent[MAXRANK_FOR_MYDATA];
  new_extent[pointsdim]=local_arg->pointno+tinfo->nr_of_points;
  new_extent[channelsdim]=tinfo->nr_of_channels;
  new_extent[2]=tinfo->itemsize;
  H5Dset_extent(sdsid, new_extent);
 }
 filespace=H5Dget_space(sdsid);
 H5Sselect_hyperslab(filespace, H5S_SELECT_SET, starts, NULL, ends, NULL);
 memspace=H5Screate_simple(rank, ends, NULL);
 if (H5Dwrite(sdsid, H5_MEM_DATATYPE, memspace, filespace, H5P_DEFAULT, tinfo->tsdata)<0) {
  ERREXIT(tinfo->emethods, "write_hdf5: H5Dwrite failed\n");
 }
 H5Sclose(memspace);
 H5Sclose(filespace);
 /*}}}  */

 local_arg->pointno+=tinfo->nr_of_points;

 if (args[ARGS_CONTINUOUS].is_set) {
  local_arg->sdsid=sdsid;
 } else {
  H5Dclose(sdsid);
  local_arg->file_datatype=H5I_INVALID_HID;
 }

 return tinfo->tsdata;	/* Simply to return something `useful' */
}
/*}}}  */

/*{{{  write_hdf5_exit(transform_info_ptr tinfo) {*/
METHODDEF void
write_hdf5_exit(transform_info_ptr tinfo) {
 struct write_hdf5_storage *local_arg=(struct write_hdf5_storage *)tinfo->methods->local_storage;
 transform_argument *args=tinfo->methods->arguments;

 if (args[ARGS_CONTINUOUS].is_set) {
  if (local_arg->triggers.current_length>0) {
   push_trigger(&local_arg->triggers,0L,0,NULL); /* End of list */
   write_triggers(tinfo,&local_arg->triggers,local_arg->sdsid);
  }
  if (local_arg->triggers.buffer_start!=NULL) {
   clear_triggers(&local_arg->triggers);
   growing_buf_free(&local_arg->triggers);
  }
  if (local_arg->sdsid!=H5I_INVALID_HID) H5Dclose(local_arg->sdsid);
 }
 H5Fclose(local_arg->fileid);

 tinfo->methods->init_done=FALSE;
}
/*}}}  */

/*{{{  select_write_hdf5(transform_info_ptr tinfo) {*/
GLOBAL void
select_write_hdf5(transform_info_ptr tinfo) {
 tinfo->methods->transform_init= &write_hdf5_init;
 tinfo->methods->transform= &write_hdf5;
 tinfo->methods->transform_exit= &write_hdf5_exit;
 tinfo->methods->method_type=PUT_EPOCH_METHOD;
 tinfo->methods->method_name="write_hdf5";
 tinfo->methods->method_description=
  "Put-epoch method to write epochs to an HDF5 file.\n";
 tinfo->methods->local_storage_size=sizeof(struct write_hdf5_storage);
 tinfo->methods->nr_of_arguments=NR_OF_ARGUMENTS;
 tinfo->methods->argument_descriptors=argument_descriptors;
}
/*}}}  */
