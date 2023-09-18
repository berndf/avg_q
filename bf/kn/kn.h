/* 
 * Includes used by both read_kn.c and write_kn.c
 */
#include "transform.h"
#include "bf.h"
#include "rawlimit.h"
#include "rawdatan.h"

/* Function defined in read_kn.c and also used by write_kn.c */
void parse_trigform(transform_info_ptr tinfo, struct Rtrial *trialb, short **trigger_form, char const *trigform);
