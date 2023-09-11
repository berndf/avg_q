# Copyright (C) 2006,2022 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

SET(bf_sources
 read_neurofile.c neurofile_sm.c neurofile_smd.c
 read_nke.c nke_format_sm.c nke_format_smd.c
 write_nke.c
)

SET(programs
 dump_neurofile_header
 dump_nke_header
)
