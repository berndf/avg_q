#ifndef _TUCKER_H_INCLUDED
#define _TUCKER_H_INCLUDED

#include <stdint.h>

struct tucker_header {
 uint32_t Version;
 uint16_t Year;
 uint16_t Month;
 uint16_t Day;
 uint16_t Hour;
 uint16_t Minute;
 uint16_t Sec;
 uint32_t MSec;
 uint16_t SampRate;
 uint16_t NChan;
 uint16_t Gain;
 uint16_t Bits;
 uint16_t Range;
 uint32_t NPoints;
 uint16_t NEvents;
};

extern struct_member sm_tucker[];
extern struct_member_description smd_tucker[];
#endif
