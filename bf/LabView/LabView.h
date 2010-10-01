/*
 * Copyright (C) 2008 Bernd Feige
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
/* Get int32_t etc */
#include <stdint.h>
#include <read_struct.h>

typedef float float32;

#define LabView_First_Offset_Table 192L
#define LabView_Entries_per_Offset_Table 128
#define LabView_Offset_Datatype int32_t
#define LabView_Datatype float32

typedef struct {
 uint16_t S;
 uint16_t DR;
 uint16_t B;
 uint16_t P;
 uint16_t T;
 uint16_t R;
 uint16_t Time;
 uint16_t TriggerOut;
 uint16_t AUX;
 uint32_t SecTot;
 uint32_t SecDR;
} LabView_TD1;

typedef struct {
 uint32_t nr_of_points;
 uint32_t nr_of_channels;
} LabView_TD2;

extern struct_member sm_LabView_TD1[], sm_LabView_TD2[];
extern struct_member_description smd_LabView_TD1[], smd_LabView_TD2[];
