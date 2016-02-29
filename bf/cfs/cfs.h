/*
 * Copyright (C) 2016 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <read_struct.h>
#include <inttypes.h>

enum var_type_enum {
 FILEVAR=0,
 DSVAR
};

enum var_storage_enum {
 INT1=0,
 WRD1,
 INT2,
 WRD2,
 INT4,
 RL4,
 RL8,
 LSTR
};

enum chan_storage_type_enum {
 EQUALSPACED=0,
 MATRIX,
 SUBSIDIARY
};

#define noFlags 0                                 /* Declare a default flag */

/* Definitions of bits for DS flags */

#define FLAG7   1
#define FLAG6   2
#define FLAG5   4
#define FLAG4   8
#define FLAG3   16
#define FLAG2   32
#define FLAG1   64
#define FLAG0   128
#define FLAG15  256
#define FLAG14  512
#define FLAG13  1024
#define FLAG12  2048
#define FLAG11  4096
#define FLAG10  8192
#define FLAG9   16384
#define FLAG8   32768


/* define numbers of characters in various string types */

#define DESCCHARS    20
#define FNAMECHARS   12
#define COMMENTCHARS 72
#define UNITCHARS    8
#define MARKERCHARS    8                       /* characters in file marker */

/*character arrays used in data structure */

typedef char  TDataType;
typedef char  TCFSKind;
typedef char  TDesc[DESCCHARS+2];        /* Names in descriptions, 20 chars */
typedef char  TFileName[FNAMECHARS+2];              /* File names, 12 chars */
typedef char  TComment[COMMENTCHARS+2];            /* Comment, 72 chars max */
typedef char  TUnits[UNITCHARS+2];                    /* For units, 8 chars */
typedef char  TMarker[MARKERCHARS];                   /* for CED file marker */

/* other types for users benefit */

typedef uint16_t TSFlags;

                          /* 1. for file header (fixed) channel information */
typedef struct
{
    TDesc     chanName;                 /* users name for channel, 20 chars */
    TUnits    unitsY;                             /* name of Yunits 8 chars */
    TUnits    unitsX;                             /* name of xunits 8 chars */
    TDataType dType;                         /* storage type 1 of 8 allowed */
    TCFSKind  dKind;                   /* equalspaced, matrix or subsidiary */
    uint16_t     dSpacing;                           /* bytes between elements */
    uint16_t     otherChan;                            /* used for matrix data */
} TFilChInfo;

              /* 2. for data section (may vary with DS) channel information */

typedef struct
{
    uint32_t  dataOffset;
    uint32_t  dataPoints;
    float scaleY;
    float offsetY;
    float scaleX;
    float offsetX;
} TDSChInfo;

                                             /* 3. for data section headers */
typedef struct
{
    uint32_t      lastDS;            /* offset in file of header of previous DS */
    uint32_t      dataSt;                                /* data start position */
    uint32_t      dataSz;           /* data size in bytes (all channels) */
    TSFlags   flags;                              /* flags for this section */
    uint16_t     dSpace[8];                                     /* spare space */
} TDataHead;

                                                  /* 4. for the file header */
typedef struct
{
    TMarker    marker;
    TFileName  name;
    uint32_t       fileSz;
    char       timeStr[8];
    char       dateStr[8];
    uint16_t      dataChans;                     /* number of channels of data */
    uint16_t      filVars;                         /* number of file variables */
    uint16_t      datVars;                 /* number of data section variables */
    uint16_t      fileHeadSz;
    uint16_t      dataHeadSz;
    uint32_t       endPnt;               /* offset in file of header of last DS */
    uint16_t       dataSecs;                         /* number of data sections */
    uint16_t       diskBlkSize;                             /* usually 1 or 512 */
    TComment   commentStr;
    uint32_t       tablePos; 
                  /* offset in file for start of table of DS header offsets */
    uint16_t      fSpace[20];                                         /* spare */
} TFileHead;

/*  for data and data section variables */
typedef struct
{
   TDesc     varDesc;                      /* users description of variable */
   TDataType vType;                               /* one of 8 types allowed */
   TUnits    varUnits;                              /* users name for units */
   uint16_t  vSize;  /* for type lstr gives no. of chars +1 for length byte */
} TVarDesc;

typedef char           * TpStr;
typedef const char     * TpCStr;
typedef int16_t        * TpShort;
typedef float          * TpFloat;
typedef int32_t        * TpLong;
typedef void           * TpVoid;
typedef TSFlags        * TpFlags;
typedef TDataType      * TpDType;
typedef TCFSKind       * TpDKind;
typedef TVarDesc       * TpVDesc;
typedef const TVarDesc * TpCVDesc;
typedef signed char    * TpSStr;
typedef uint16_t           * TpUShort;

extern struct_member sm_TFilChInfo[];
extern struct_member sm_TDSChInfo[];
extern struct_member sm_TDataHead[];
extern struct_member sm_TFileHead[];
extern struct_member sm_TVarDesc[];

extern struct_member_description smd_TFilChInfo[];
extern struct_member_description smd_TDSChInfo[];
extern struct_member_description smd_TDataHead[];
extern struct_member_description smd_TFileHead[];
extern struct_member_description smd_TVarDesc[];
