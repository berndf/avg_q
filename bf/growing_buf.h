/*
 * Copyright (C) 1996-2000,2003,2007,2013,2022 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#ifndef _GROWING_BUF_H
#define _GROWING_BUF_H

#ifndef Bool
#define Bool int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef struct {
 char *buffer_start;
 char *buffer_end;	/* Points one character past the end of the buffer */
 long current_length;
 Bool can_be_freed;

 char *delimiters;
 char delim_protector;
 char *current_token;
} growing_buf;

extern void growing_buf_clear(growing_buf *buf);
extern void growing_buf_init(growing_buf *buf);
extern Bool growing_buf_allocate(growing_buf *buf, long length);
extern Bool growing_buf_takewithlength(growing_buf *buf, char const *str, long length);
extern Bool growing_buf_takethis(growing_buf *buf, char const *str);
extern Bool growing_buf_append(growing_buf *buf, char const *from, long length);
extern Bool growing_buf_appendstring(growing_buf *buf, char const *str);
extern Bool growing_buf_appendchar(growing_buf *buf, char const c);
extern Bool growing_buf_appendf(growing_buf *buf, const char *format, ...);
extern void growing_buf_free(growing_buf *buf);

extern void growing_buf_settothis(growing_buf *buf, char *str);

extern void growing_buf_read_line(FILE *infile, growing_buf *buf);

extern Bool growing_buf_get_firsttoken(growing_buf *buf, growing_buf *writebuf);
extern Bool growing_buf_get_nexttoken(growing_buf *buf, growing_buf *writebuf);
extern Bool growing_buf_get_firstsingletoken(growing_buf *buf, growing_buf *writebuf);
extern Bool growing_buf_get_nextsingletoken(growing_buf *buf, growing_buf *writebuf);
extern int growing_buf_count_tokens(growing_buf *buf);
#endif
