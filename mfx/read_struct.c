/*
 * Copyright (C) 1995,1996,2001,2003,2012 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/* read_struct module to read and write structures in an architecture- and 
 * compiler-independent way (byte alignment and big/little-endian order)
 * 		-- Bernd Feige 6.08.1995 */

/*{{{}}}*/
/*{{{  Application example*/
#if 0
/*
 * A structure type must be defined first.
 */
struct mystruct {
 int i;
 char c[5];
 short s;
};

/*
 * Then, a struct_member array can be initialized independent of the
 * compiler (alignment) used. The first column is the offset of each variable,
 * the second and third the offset and length of the variable in the file,
 * and the fourth column decides whether the value must be byte-swapped between
 * systems with different byte order.
 */
struct_member st[]={
 {sizeof(struct mystruct), 11, 0, 0},
 {(long)&((struct mystruct *)NULL)->i, 0, 4, 1},
 {(long)&((struct mystruct *)NULL)->c, 4, 5, 0},
 {(long)&((struct mystruct *)NULL)->s, 9, 2, 1},
 {0,0,0,0}
};
#endif
/*}}}  */
 
#include <stdio.h>
#include <string.h>
#include "read_struct.h"
#include "Intel_compat.h"

#undef DEBUG

/*{{{  change_byteorder(char *structure, struct_member *smp) {*/
void
change_byteorder(char *structure, struct_member *smp) {
 struct_member *in_smp;

 for (in_smp=smp+1; in_smp->length!=0; in_smp++) {
  if (in_smp->control_byteorder) {
   switch(in_smp->length) {
    case 2:
     Intel_int16((uint16_t *)(structure+in_smp->offset_this_compiler));
     break;
    case 4:
     Intel_int32((uint32_t *)(structure+in_smp->offset_this_compiler));
     break;
    case 8:
     Intel_int64((uint64_t *)(structure+in_smp->offset_this_compiler));
     break;
   }
  }
 }
}
/*}}}  */

/*{{{  write_struct(char *structure, struct_member *smp, FILE *stream) {*/
int
write_struct(char *structure, struct_member *smp, FILE *stream) {
 struct_member *in_smp=smp+1, *lastelement=in_smp;
 long lastdiff=0, nmemb;

 while (in_smp->length!=0) {
  long diff=in_smp->offset_this_compiler-in_smp->offset;
  if (diff!=lastdiff) {
   nmemb=in_smp->offset-lastelement->offset;
#ifdef DEBUG
   printf("%ld %ld\n", lastelement->offset_this_compiler, nmemb);
#else
   if (nmemb!=(int)fwrite((void *)(structure+lastelement->offset_this_compiler), 1, nmemb, stream)) return 0;
#endif
   lastelement=in_smp;
  }
  lastdiff=diff;
  in_smp++;
 }
 nmemb=(in_smp-1)->offset+(in_smp-1)->length-lastelement->offset;
#ifdef DEBUG
 printf("%ld %ld\n", lastelement->offset_this_compiler, nmemb);
#else
 if (nmemb!=(int)fwrite((void *)(structure+lastelement->offset_this_compiler), 1, nmemb, stream)) return 0;
#endif
 return 1;
}
/*}}}  */

/*{{{  read_struct(char *structure, struct_member *smp, FILE *stream) {*/
int
read_struct(char *structure, struct_member *smp, FILE *stream) {
 struct_member *in_smp=smp+1, *lastelement=in_smp;
 long lastdiff=0, nmemb;

 /* If items to read are smaller than the target type, then clearing
  * the target memory is mandatory */
 memset(structure, 0, smp->offset_this_compiler);
 while (in_smp->length!=0) {
  long diff=in_smp->offset_this_compiler-in_smp->offset;
  if (diff!=lastdiff) {
   nmemb=in_smp->offset-lastelement->offset;
#ifdef DEBUG
   printf("%ld %ld\n", lastelement->offset_this_compiler, nmemb);
#else
   if (nmemb!=(int)fread((void *)(structure+lastelement->offset_this_compiler), 1, in_smp->offset-lastelement->offset, stream)) return 0;
#endif
   lastelement=in_smp;
  }
  lastdiff=diff;
  in_smp++;
 }
 nmemb=(in_smp-1)->offset+(in_smp-1)->length-lastelement->offset;
#ifdef DEBUG
 printf("%ld %ld\n", lastelement->offset_this_compiler, nmemb);
#else
 if (nmemb!=(int)fread((void *)(structure+lastelement->offset_this_compiler), 1, nmemb, stream)) return 0;
#endif
 return 1;
}
/*}}}  */

/*{{{  print_structmembers(struct_member *smp, FILE *stream) {*/
void
print_structmembers(struct_member *smp, FILE *stream) {
 struct_member *in_smp=smp;

 fprintf(stream, "Structure size this compiler=%ld, original==%ld\n", in_smp->offset_this_compiler, in_smp->offset);
 in_smp++;
 while (in_smp->length!=0) {
  fprintf(stream, "offset_this_compiler=%ld, offset=%ld, length=%d, control_byteorder=%d\n", in_smp->offset_this_compiler, in_smp->offset, in_smp->length, in_smp->control_byteorder);
  in_smp++;
 }
}
/*}}}  */

/*{{{  print_structcontents(char *structure, struct_member *smp, struct_member_description *smdp, FILE *stream) {*/
void
print_structcontents(char *structure, struct_member *smp, struct_member_description *smdp, FILE *stream) {
 struct_member *in_smp=smp;
 struct_member_description *in_smdp=smdp;

 in_smp++;
 while (in_smp->length!=0) {
  char *in_structure=structure+in_smp->offset_this_compiler;
  fprintf(stream, "%s=", in_smdp->member_name);
  switch (in_smdp->member_type) {
   case STRUCT_MEMBER_SKIP:
    break;
   case STRUCT_MEMBER_INT: {
    long value=0;
    int have_value=TRUE;
    switch (in_smp->length) {
     case sizeof(int8_t):
      value=*((int8_t *)in_structure);
      break;
     case sizeof(int16_t):
      value=*((int16_t *)in_structure);
      break;
     case sizeof(int32_t):
      value=*((int32_t *)in_structure);
      break;
     case sizeof(int64_t):
      value=*((int64_t *)in_structure);
      break;
     default:
      fprintf(stream, "UNKNOWN INT SIZE\n");
      have_value=FALSE;
      break;
    }
    if (have_value) {
     fprintf(stream, "%ld\n", value);
    }
    }
    break;
   case STRUCT_MEMBER_UINT: {
    unsigned long value=0;
    int have_value=TRUE;
    switch (in_smp->length) {
     case sizeof(uint8_t):
      value=*((uint8_t *)in_structure);
      break;
     case sizeof(uint16_t):
      value=*((uint16_t *)in_structure);
      break;
     case sizeof(uint32_t):
      value=*((uint32_t *)in_structure);
      break;
     case sizeof(uint64_t):
      value=*((uint64_t *)in_structure);
      break;
     default:
      fprintf(stream, "UNKNOWN INT SIZE\n");
      have_value=FALSE;
      break;
    }
    if (have_value) {
     fprintf(stream, "%lu\n", value);
    }
    }
    break;
   case STRUCT_MEMBER_FLOAT: {
    double value=0;
    int have_value=TRUE;
    switch (in_smp->length) {
     case sizeof(float):
      value=*((float *)in_structure);
      break;
     case sizeof(double):
      value=*((double *)in_structure);
      break;
     default:
      fprintf(stream, "UNKNOWN FLOAT SIZE\n");
      have_value=FALSE;
      break;
    }
    if (have_value) {
     fprintf(stream, "%g\n", value);
    }
    }
    break;
   case STRUCT_MEMBER_CHAR: {
    int i=0;
    char c;
    fprintf(stream, "\'");
    while (i++<in_smp->length && (c= *in_structure++)!='\0') {
     fprintf(stream, "%c", c);
    }
    fprintf(stream, "\'\n");
    }
    break;
  }
  in_smp++; in_smdp++;
 }
}
/*}}}  */
