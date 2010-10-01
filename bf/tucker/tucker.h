#ifndef _TUCKER_H_INCLUDED
#define _TUCKER_H_INCLUDED

struct tucker_header {
 long  Version;
 short Year;
 short Month;
 short Day;
 short Hour;
 short Minute;
 short Sec;
 long MSec;
 short SampRate;
 short NChan;
 short Gain;
 short Bits;
 short Range;
 long  NPoints;
 short NEvents;
};

extern struct_member sm_tucker[];
#endif
