/*
 * Copyright (C) 1995,1996,2001,2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* This include file defines the data structures necessary to implement
 * an architecture- and compiler-independent reading of structures into
 * memory. 					-- Bernd Feige 6.08.1995 */

#ifndef READ_STRUCT_H
#define READ_STRUCT_H
#include <stdio.h>

/*{{{  TRUE, FALSE*/
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/*}}}  */

/* The last `struct_member' entry must have length=0.
 * The first `struct_member' entry contains the total size of the struct,
 * for this compiler in offset_this_compiler and for the original in offset. */
typedef struct {
 long offset_this_compiler;
 long offset;
 int length;
 int control_byteorder;
} struct_member;

/* The following structure adds information necessary to dump the struct
 * contents as name=value pairs. Only one item per member is needed: The size
 * info is provided by the struct_member array. 
 * This is kept separate because it adds a large amount of data that is not
 * needed by most programs using the read_struct package. */
enum struct_member_types {
 STRUCT_MEMBER_SKIP=0,
 STRUCT_MEMBER_INT,	/* Size of the int is given by the struct_member entry */
 STRUCT_MEMBER_UINT,	/* Size of the unsigned int is given by the struct_member entry */
 STRUCT_MEMBER_XINT,	/* As UINT but output in hexadecimal (%x) */
 STRUCT_MEMBER_FLOAT,	/* Size of the float is given by the struct_member entry */
 STRUCT_MEMBER_CHAR	/* Number of chars is given by the struct_member entry */
};
typedef struct {
 enum struct_member_types member_type;
 char *member_name;
} struct_member_description;

void change_byteorder(char *structure, struct_member *smp);
int write_struct(char *structure, struct_member *smp, FILE *stream);
int read_struct(char *structure, struct_member *smp, FILE *stream);
void print_structmembers(struct_member *smp, FILE *stream);
void print_structcontents(char *structure, struct_member *smp, struct_member_description *smdp, FILE *stream);
#endif
