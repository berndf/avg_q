/*
 * Copyright (C) 1994 Bernd Feige
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
 *==============================================================================
 *==============================================================================
 * this section is the old channel format     mfx_header_format == 1
 *==============================================================================
 *==============================================================================
*/
struct mfx_channel_data001a
{
	unsigned short data_type;
	char channel_name[16];
	char yaxis_label[16];
	short pad_to_4byte;
	long pad_to_8byte;
	double ymin;
	double ymax;
	double position[3];
	double vector[3];
	double transform[4][4];
};
