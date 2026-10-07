#ifndef BMB_DEBUG_H
#define BMB_DEBUG_H

#include <stdint.h>

/* Chain index (0-based, same numbering as the "BMBnn" log labels minus one)
   of the BMB to dump raw ADBMS register reads for. -1 disables the dump. */
#ifndef HVC_BMB_DEBUG_INDEX
#define HVC_BMB_DEBUG_INDEX 9
#endif

/* Sends RDCFGA, RDSID, RDCVA, ADCV, RDCVA, RDFCA, ADSV and RDSVA to the
   chain and prints the selected BMB's raw replies (data, received PEC,
   computed PEC, command counter), plus a PEC/counter summary for every IC.
   settle() runs between printed lines. */
void bmb_debug_dump(void (*settle)(void));

#endif // BMB_DEBUG_H
