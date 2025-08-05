/*
 * Copyright (C) 2025 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include "bf.h"
#include "sox.h"

/* These global variables are defined in write_sound.c together with
 * the other SOX bindings: */
extern Bool sox_init_done;
extern char const *myname;
extern external_methods_ptr sox_emethods;
extern void sox_list_formats(external_methods_ptr emethods);

extern void
avg_q_sox_output_message_handler(
    unsigned level,                       /* 1 = FAIL, 2 = WARN, 3 = INFO, 4 = DEBUG, 5 = DEBUG_MORE, 6 = DEBUG_MOST. */
    LSX_PARAM_IN_Z char const * filename, /* Source code __FILENAME__ from which message originates. */
    LSX_PARAM_IN_PRINTF char const * fmt, /* Message format string. */
    LSX_PARAM_IN va_list ap               /* Message format parameters. */
);
