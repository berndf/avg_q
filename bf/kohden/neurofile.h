/*
 * Header for the Nihon Kohden NeuroFile II format
 */
#ifndef _NEUROFILE_H_INCLUDED
struct sequence {
 unsigned char	comment[40];
 unsigned char	second;
 unsigned char	minute;
 unsigned char	hour;
 unsigned char	day;
 unsigned char	month;
 unsigned char	year;
 unsigned short	length;
 unsigned char	els[9];
 unsigned char	ref;
 unsigned short	nchan,nfast,nwil,ncoh;
 unsigned char	elnam[32][4],coord[2][32],ewil[32],ecoh[32];
 float	gain;
 unsigned char	fastrate;
 unsigned char	slowrate;
 unsigned short	syncode;
 unsigned short	sync0;
};

extern struct_member sm_sequence[];
extern struct_member_description smd_sequence[];
extern float neurofile_map_sfreq(int fastrate);
#endif
