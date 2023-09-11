/*
 * Copyright (C) 2001,2002,2008,2014 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#ifndef AVG_Q_CONFIG_H
#define AVG_Q_CONFIG_H

#if defined __x86_64__ && !defined __ILP32__
# define POINTER_SIZE	64
#else
# define POINTER_SIZE	32
#endif

#undef INTTYPES_OK

#ifdef __sun__
#define HAS_SYS_INTTYPES_H
#define INTTYPES_OK
#endif

#ifdef __sgi__
#define HAS_INTTYPES_H
#define INTTYPES_OK
#endif

#ifdef __DJGPP__
#undef HAS_STDINT_H
/* This comes from sox' ststdint.h: */
#ifndef __int8_t_defined
#define __int8_t_defined
typedef char  int8_t;
typedef short int16_t;
typedef int   int32_t;
#endif
#define INTTYPES_OK
#endif

#ifndef INTTYPES_OK
#define HAS_STDINT_H
#endif

#undef INTTYPES_OK

#endif
