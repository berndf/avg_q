/*
 * Copyright (C) 1996,1997,2001,2002,2008 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * method.h contains the basic definitions and constructs for method-
 * oriented programming, following Thomas G. Lane's JPEG software.
 * Bernd Feige 7.12.1992
 */

#ifndef METHOD_H
#define METHOD_H

#include "avg_q_config.h"
#ifdef HAS_STDINT_H
/* This defines the integer bit sizes like int32_t etc. in ISO C99 */
#include <stdint.h>
#else
#ifdef HAS_INTTYPES_H
#include <inttypes.h>
#else
#ifdef HAS_SYS_INTTYPES_H
#include <sys/inttypes.h>
#endif
#endif
#endif
/* This type should accomodate both pointers and (signed) integers on the
 * current system: */
#if POINTER_SIZE==64
typedef int64_t msgparm_t;
#else
typedef int32_t msgparm_t;
#endif
#define MSGPARM(x) ((msgparm_t)(x))

#define METHODDEF static
#define LOCAL static
#define GLOBAL
/* Well, the BORLAND C doesn't understand #if DATATYPE==float, and GNU C
 * doesn't understand #if sizeof(DATATYPE)==sizeof(float), so we define an
 * additional variable here to be able to test it... Choice: */
#undef FLOAT_DATATYPE
#define DOUBLE_DATATYPE

#ifdef FLOAT_DATATYPE
#define DATATYPE float
/* We define the number of significant mantissa digits that DATATYPE can hold.
 * Note that the C99 standard knows DECIMAL_DIG, which is the precision of 
 * the widest floating-point type; Don't know how to get at this value generally. */
#define DATATYPE_DIGITS 6
#endif
#ifdef DOUBLE_DATATYPE
#define DATATYPE double
#define DATATYPE_DIGITS 9
#endif

#ifndef Bool
#define Bool int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* We usually want to use prototypes... */
#define PROTO

/* If not using GNU C, override the GNU extensions properly */
#ifndef __GNUC__
#define __attribute__(a)
#else
#if __GNUC__ == 3
/* __attribute__ is used only by the definition of error_exit, and with GCC 3.1
 * apparently is somehow linked to a `volatile' declaration which is refused by
 * g++-3.1 (eg in laplacian.cc) */
#define __attribute__(a)
#endif
#endif

/* This macro is used to declare a "method", that is, a function pointer. */
/* We want to supply prototype parameters if the compiler can cope. */
/* Note that the arglist parameter must be parenthesized! */

#ifdef PROTO
#define METHOD(type,methodname,arglist)  type (*methodname) arglist
#else
#define METHOD(type,methodname,arglist)  type (*methodname) ()
#endif

/* Macros to simplify using the error and trace message stuff */
/* The first parameter is generally tinfo->emethods */

#define ERREXIT(emeth,msg)		((*(emeth)->error_exit) (emeth, msg))
#define ERREXIT1(emeth,msg,p1)		((emeth)->message_parm[0] = (p1), \
					 (*(emeth)->error_exit) (emeth, msg))
#define ERREXIT2(emeth,msg,p1,p2)	((emeth)->message_parm[0] = (p1), \
					 (emeth)->message_parm[1] = (p2), \
					 (*(emeth)->error_exit) (emeth, msg))
#define ERREXIT3(emeth,msg,p1,p2,p3)	((emeth)->message_parm[0] = (p1), \
					 (emeth)->message_parm[1] = (p2), \
					 (emeth)->message_parm[2] = (p3), \
					 (*(emeth)->error_exit) (emeth, msg))
#define ERREXIT4(emeth,msg,p1,p2,p3,p4) ((emeth)->message_parm[0] = (p1), \
					 (emeth)->message_parm[1] = (p2), \
					 (emeth)->message_parm[2] = (p3), \
					 (emeth)->message_parm[3] = (p4), \
					 (*(emeth)->error_exit) (emeth, msg))

#ifdef TRACE
#define MAKESTMT(stuff)		do { stuff } while (0)

#define TRACEMS(emeth,lvl,msg)    \
  MAKESTMT( (*(emeth)->trace_message) (emeth, lvl, msg); )
#define TRACEMS1(emeth,lvl,msg,p1)    \
  MAKESTMT( (emeth)->message_parm[0] = (p1); \
		(*(emeth)->trace_message) (emeth, lvl, msg); )
#define TRACEMS2(emeth,lvl,msg,p1,p2)    \
  MAKESTMT( (emeth)->message_parm[0] = (p1); \
		(emeth)->message_parm[1] = (p2); \
		(*(emeth)->trace_message) (emeth, lvl, msg); )
#define TRACEMS3(emeth,lvl,msg,p1,p2,p3)    \
  MAKESTMT( msgparm_t * _mp = (emeth)->message_parm; \
		*_mp++ = (p1); *_mp++ = (p2); *_mp = (p3); \
		(*(emeth)->trace_message) (emeth, lvl, msg); )
#define TRACEMS4(emeth,lvl,msg,p1,p2,p3,p4)    \
  MAKESTMT( msgparm_t * _mp = (emeth)->message_parm; \
		*_mp++ = (p1); *_mp++ = (p2); *_mp++ = (p3); *_mp = (p4); \
		(*(emeth)->trace_message) (emeth, lvl, msg); )
#define TRACEMS8(emeth,lvl,msg,p1,p2,p3,p4,p5,p6,p7,p8)    \
  MAKESTMT( msgparm_t * _mp = (emeth)->message_parm; \
		*_mp++ = (p1); *_mp++ = (p2); *_mp++ = (p3); *_mp++ = (p4); \
		*_mp++ = (p5); *_mp++ = (p6); *_mp++ = (p7); *_mp = (p8); \
		(*(emeth)->trace_message) (emeth, lvl, msg); )
#else
#define TRACEMS(emeth,lvl,msg)
#define TRACEMS1(emeth,lvl,msg,p1)
#define TRACEMS2(emeth,lvl,msg,p1,p2)
#define TRACEMS3(emeth,lvl,msg,p1,p2,p3)
#define TRACEMS4(emeth,lvl,msg,p1,p2,p3,p4)
#define TRACEMS8(emeth,lvl,msg,p1,p2,p3,p4,p5,p6,p7,p8)
#endif

#endif	/* ifndef METHOD_H */
