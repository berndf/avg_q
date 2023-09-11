/*
 * Copyright (C) 1996,1998-2000,2003,2007,2011,2013,2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "growing_buf.h"

#define MIN_INITIAL_SIZE 128

void 
growing_buf_clear(growing_buf *buf) {
 /* This call sets the contents of the current buffer to the empty 
  * string without touching the allocation status */
 buf->current_length=0;
}

void 
growing_buf_init(growing_buf *buf) {
 buf->buffer_start=buf->buffer_end=buf->current_token=NULL;
 /* This is just to save the token method users the fuss to set the delimiters.
  * This version skips all whitespace, newlines and even left-over MSDOS carriage-
  * return characters in the input line, since you will seldom want to see them. */
 buf->delimiters=" \t\r\n";
 buf->delim_protector='\0'; /* User has to set this explicitly if it is wanted */
 buf->current_length=0;
 buf->can_be_freed=FALSE;
}

Bool 
growing_buf_allocate(growing_buf *buf, long length) {
 long const new_length=(length<MIN_INITIAL_SIZE ? MIN_INITIAL_SIZE : length);
 growing_buf_free(buf);
 if ((buf->buffer_start=(char *)malloc(new_length))==NULL) return FALSE;
 buf->buffer_end=buf->buffer_start+new_length;
 buf->current_length=0;
 buf->can_be_freed=TRUE;
 return TRUE;
}

Bool 
growing_buf_takewithlength(growing_buf *buf, char const *str, long length) {
 if (growing_buf_allocate(buf, length)==0) return FALSE;
 memcpy(buf->buffer_start, str, length);
 buf->current_length=length;
 return TRUE;
}

Bool 
growing_buf_takethis(growing_buf *buf, char const *str) {
 return growing_buf_takewithlength(buf, str, strlen(str)+1);
}

static Bool
growing_buf_ensurelength(growing_buf *buf, long future_length) {
 long const current_size=buf->buffer_end-buf->buffer_start;
 long future_size=current_size;

 if (!buf->can_be_freed) return FALSE;

 while (future_size<future_length) {
  future_size*=2;
 }
 if (future_size!=current_size) {
  char *oldstart=buf->buffer_start;
  if ((buf->buffer_start=(char *)realloc(oldstart, future_size))==NULL) {
   growing_buf_init(buf);
   return FALSE;
  }
  buf->buffer_end=buf->buffer_start+future_size;
  if (buf->current_token!=NULL) buf->current_token+=buf->buffer_start-oldstart;
 }
 return TRUE;
}

Bool
growing_buf_append(growing_buf *buf, char const *from, long length) {
 long const future_length=buf->current_length+length;
 if (!growing_buf_ensurelength(buf, future_length)) return FALSE;
 memcpy(buf->buffer_start+buf->current_length, from, length);
 buf->current_length=future_length;
 return TRUE;
}

Bool 
growing_buf_appendstring(growing_buf *buf, char const *str) {
 /* With strings, current_length includes the zero at the end; we want to
  * overwrite this zero and copy that of str */
 if (buf->current_length>0) buf->current_length--;
 return growing_buf_append(buf, str, strlen(str)+1);
}

Bool 
growing_buf_appendchar(growing_buf *buf, char const c) {
 return growing_buf_append(buf, &c, 1);
}
Bool 
growing_buf_appendf(growing_buf *buf, const char *format, ...) {
 va_list ap;
 va_start(ap, format);
 /* With strings, current_length includes the zero at the end; we want to
  * overwrite this zero and copy that of str */
 if (buf->current_length>0) buf->current_length--;
 long const length=vsnprintf(NULL, 0, format, ap);
 long const future_length=buf->current_length+length+1;
 va_end(ap);
 if (!growing_buf_ensurelength(buf, future_length)) return FALSE;
 va_start(ap, format);
 vsnprintf(buf->buffer_start+buf->current_length, length+1, format, ap);
 va_end(ap);
 buf->current_length=future_length;
 return TRUE;
}

void 
growing_buf_free(growing_buf *buf) {
 if (buf->buffer_start==NULL) buf->can_be_freed=FALSE;
 if (buf->can_be_freed) free(buf->buffer_start);
 growing_buf_init(buf);
}

void 
growing_buf_settothis(growing_buf *buf, char *str) {
 growing_buf_init(buf);
 buf->current_length=strlen(str)+1;
 buf->buffer_start=str;
 buf->buffer_end=buf->buffer_start+buf->current_length;
}

void
growing_buf_read_line(FILE *infile, growing_buf *buf) {
 char c;
 growing_buf_clear(buf);
 while (1) {
  c=fgetc(infile);
  if (c=='\r') continue;
  if (feof(infile) || c=='\n') break;
  growing_buf_appendchar(buf, c);
 }
 growing_buf_appendchar(buf, '\0');
}

/* Token parsing implementation */
static Bool escape=FALSE;
static Bool
is_delimiter(growing_buf *buf) {
 return (*buf->current_token=='\0' ||
  (!escape && strchr(buf->delimiters, *buf->current_token)!=NULL) );
}
static void
transfer_character(growing_buf *buf, growing_buf *writebuf) {
 if (buf->delim_protector!='\0') {
  if (*buf->current_token==buf->delim_protector) {
   if (escape) {
    /* Double escape: Reset and output this character verbatim */
    escape=FALSE;
   } else {
    /* First escape: No transfer */
    escape=TRUE;
    buf->current_token++;
    return;
   }
  } else {
   /* This behavior is somewhat unusual, but we really want
    * to protect *only* delimiters and to leave single backslashes
    * in place otherwise */
   if (escape && strchr(buf->delimiters, *buf->current_token)==NULL && writebuf!=NULL) {
    growing_buf_appendchar(writebuf,buf->delim_protector);
   }
   escape=FALSE;
  }
 }
 if (writebuf!=NULL) growing_buf_appendchar(writebuf,*buf->current_token);
 buf->current_token++;
}
static void
token_skip_delimiter(growing_buf *buf) {
 char * const current_end=buf->buffer_start+buf->current_length;
 while (buf->current_token<current_end && is_delimiter(buf)) buf->current_token++;
}
static void
token_transfer_nondelimiter(growing_buf *buf, growing_buf *writebuf) {
 char * const current_end=buf->buffer_start+buf->current_length;
 while (buf->current_token<current_end && !is_delimiter(buf)) transfer_character(buf, writebuf);
 if (writebuf!=NULL) growing_buf_appendchar(writebuf,'\0');
 if (buf->current_token<current_end) buf->current_token++; /* Consume the delimiter */
}
Bool
growing_buf_get_nexttoken(growing_buf *buf, growing_buf *writebuf) {
 char * const current_end=buf->buffer_start+buf->current_length;
 escape=FALSE;
 token_skip_delimiter(buf);
 if (writebuf!=NULL) growing_buf_clear(writebuf);
 if (buf->current_token>=current_end) return FALSE;
 token_transfer_nondelimiter(buf, writebuf);
 return TRUE;
}
Bool
growing_buf_get_firsttoken(growing_buf *buf, growing_buf *writebuf) {
 buf->current_token=buf->buffer_start;
 return growing_buf_get_nexttoken(buf, writebuf);
}
/* While the former variant is more efficient because no empty tokens
 * are returned, this variant does return an empty token if multiple
 * adjacent delimiters are found. This may be needed if empty lines
 * must be counted as in setup_queue.c	*/
Bool
growing_buf_get_nextsingletoken(growing_buf *buf, growing_buf *writebuf) {
 char * const current_end=buf->buffer_start+buf->current_length;
 escape=FALSE;
 if (writebuf!=NULL) growing_buf_clear(writebuf);
 if (buf->current_token>=current_end) return FALSE;
 token_transfer_nondelimiter(buf, writebuf);
 return TRUE;
}
Bool
growing_buf_get_firstsingletoken(growing_buf *buf, growing_buf *writebuf) {
 buf->current_token=buf->buffer_start;
 return growing_buf_get_nextsingletoken(buf, writebuf);
}

int
growing_buf_count_tokens(growing_buf *buf) {
 int nr_of_tokens=0;
 buf->current_token=buf->buffer_start;
 escape=FALSE;
 while (growing_buf_get_nexttoken(buf, NULL)) nr_of_tokens++;
 return nr_of_tokens;
}
