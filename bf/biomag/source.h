/*
 * Copyright (C) 1993,1994 Bernd Feige
 * 
 * This file is part of avg_q.
 * 
 * avg_q is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * avg_q is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
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
