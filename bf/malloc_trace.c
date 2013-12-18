/*
 * Copyright (C) 1994,1998 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*
 * malloc_trace module -- keeps track of allocated memory patches and
 * frees only those that were actually allocated before. Freeing memory
 * 'on suspicion' of it being allocated can cause subtle errors if the
 * address now is located outside the address space of the process, for
 * example, or if it coincides by chance with a memory patch still needed.
 * 					-- Bernd Feige, Roma, 18.02.1994
 */

#ifdef MALLOC_TRACE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

/* Prototypes are in array.h (only def'd if MALLOC_TRACE is set) */
#include <array.h>

/* These are defined for all other programs to our own functions */
#undef malloc
#undef calloc
#undef realloc
#undef free

#define MALLOC_LISTSTEP 256

/*{{{}}}*/
/*{{{  Local list management*/
static long malloc_processed=0, free_successful=0, free_failed=0;
static long malloc_failed_attempts=0, malloc_null_attempts=0, free_null_ptr_attempts=0;
static char *lowstack, *highstack;

static long malloc_entries=0;
static long malloc_listlen=0;
static void **malloc_list=NULL;

/*{{{  stat_at_exit(void) {*/
static void
stat_at_exit(void) {
 printf("malloc_trace: at_exit statistics\n");
 printf("malloc_processed=%ld, free_successful=%ld, free_failed=%ld\n",
        malloc_processed, free_successful, free_failed);
 printf("malloc_failed_attempts=%ld, malloc_null_attempts=%ld, free_null_ptr_attempts=%ld\n",
	malloc_failed_attempts, malloc_null_attempts, free_null_ptr_attempts);
 printf("Final malloc_listlen=%ld, malloc_entries=%ld\n", malloc_listlen, malloc_entries);
 printf("highstack-lowstack=%d bytes\n", highstack-lowstack);
}
/*}}}  */

/*{{{  find_mallocentry(void *ptr) {*/
static void **
find_mallocentry(void *ptr) {
 long i=malloc_entries;
 void **iptr=malloc_list;

 while (i-- >0) {
  while (*iptr==NULL) iptr++;
  if (*iptr==ptr) return iptr;
  iptr++;
 }

 return (void **)NULL;
}
/*}}}  */
/*{{{  new_mallocentry(void *ptr) {*/
static void
new_mallocentry(void *ptr) {
 long count;
 void **iptr;
 if (malloc_listlen==0) {
  /*{{{  Allocate first chunk of malloc list memory*/
  if ((malloc_list=(void **)calloc(MALLOC_LISTSTEP, sizeof(void *)))==NULL) {
   fprintf(stderr, "new_mallocentry: Can't malloc malloc_list.\n");
   exit(-1);
  }
  malloc_listlen=MALLOC_LISTSTEP;
  /*}}}  */
  /*{{{  Register stat_atexit for malloc stats at exit*/
  atexit(&stat_at_exit);
  /*}}}  */
  lowstack=highstack=(char *)&count;
 } else {
  if (lowstack>(char *)&count) lowstack=(char *)&count;
  if (highstack<(char *)&count) highstack=(char *)&count;
 }
 for (iptr=malloc_list, count=malloc_listlen; *iptr!=NULL && count>0;
      iptr++, count--);
 if (count==0) {
  /*{{{  Expand the malloc list*/
  iptr=realloc(malloc_list, (malloc_listlen+MALLOC_LISTSTEP)*sizeof(void *));
  if (iptr==NULL) {
   fprintf(stderr, "new_mallocentry: Can't realloc malloc_list.\n");
   exit(-1);
  }
  malloc_list=iptr;
  iptr=malloc_list+malloc_listlen;
  memset((char *)iptr, 0, MALLOC_LISTSTEP*sizeof(void *));
  malloc_listlen+=MALLOC_LISTSTEP;
  /*}}}  */
 }
 *iptr=ptr; malloc_entries++;
 malloc_processed++;
}
/*}}}  */
/*{{{  delete_mallocentry(void *ptr) {*/
/* delete_mallocentry returns the position of the cleared entry or NULL
 * if it was not in the list */
static void **
delete_mallocentry(void *ptr) {
 void **iptr=find_mallocentry(ptr);
 if (iptr!=NULL) {
  *iptr=NULL;
  malloc_entries--;
  free_successful++;
 } else {
  free_failed++;
 }
 return iptr;
}
/*}}}  */
/*}}}  */

/*{{{  traced_malloc(size_t size) {*/
void *
traced_malloc(size_t size) {
 void *ptr=malloc(size);
 if (size==0) {
  malloc_null_attempts++;
 }
 if (ptr!=NULL) {
  new_mallocentry(ptr);
 } else {
  malloc_failed_attempts++;
 }
 return ptr;
}
/*}}}  */
/*{{{  traced_calloc(size_t nelem, size_t elsize) {*/
void *
traced_calloc(size_t nelem, size_t elsize) {
 void *ptr=calloc(nelem, elsize);
 if (nelem==0 || elsize==0) {
  malloc_null_attempts++;
 }
 if (ptr!=NULL) {
  new_mallocentry(ptr);
 } else {
  malloc_failed_attempts++;
 }
 return ptr;
}
/*}}}  */
/*{{{  traced_realloc(void *ptr, size_t size) {*/
void *
traced_realloc(void *ptr, size_t size) {
 void *newptr=realloc(ptr, size);
 if (size==0) {
  malloc_null_attempts++;
 }
 if (newptr==NULL) {
  malloc_failed_attempts++;
 }
 if (find_mallocentry(ptr)!=NULL) {
  if (newptr!=ptr) {
   delete_mallocentry(ptr);
   new_mallocentry(newptr);
  }
 } else {
  fprintf(stderr,
   "traced_realloc: Attempt to reallocate unknown block. System said %s\n",
   (newptr==NULL ? "FAILED" : "OKAY"));
 }
 return newptr;
}
/*}}}  */
/*{{{  traced_free(void *ptr) {*/
void
traced_free(void *ptr) {
 if (ptr==NULL) {
  free_null_ptr_attempts++;
 }
 if (delete_mallocentry(ptr)!=NULL) {
  free(ptr);
 }
}
/*}}}  */

#undef MALLOC_LISTSTEP
#endif MALLOC_TRACE
