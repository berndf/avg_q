/*
 * Copyright (C) 1993,1994 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
 * 
 */
/*
 * source.h defines source description structures
 */

#include <array.h>

/*
 * At present, only dipolar sources are supported. The main source_desc
 * structure holds nrofsources and a pointer to a dipole description array 
 * of this length. In the future, it would hold a source type and different
 * pointers for different sources.
 */

struct source_desc {
 int nrofsources;
 struct dipole_desc *dipoles;
};

struct dipole_desc {
 /* To be set by the user interface: */
 array position;
 array dip_moment;
 /* Set by simulate_init */
 array leadfield;	/* nr_of_vectors==nr_of_channels, nr_of_elements==3 */
 /* Workspace for the time_function */
 array initial_moment;
 double time[8];	/* Different counters for the dipole behavior */
 void (*time_function_init)(struct dipole_desc *);
 void (*time_function)(struct dipole_desc *);
 void (*time_function_exit)(struct dipole_desc *);
};
