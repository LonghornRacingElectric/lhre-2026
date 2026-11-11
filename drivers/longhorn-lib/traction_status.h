#ifndef ORION_TRACTION_STATUS_H
#define ORION_TRACTION_STATUS_H
/* Version 1 wheel-control status. Source time is VCU-mapped only with SYNC. */
#define TC_WIRE_VALID 0x01u
#define TC_WIRE_SYNC 0x02u
#define TC_WIRE_DIRECTION 0x04u
#define TC_WIRE_FAULT 0x08u
#define TC_WIRE_VERSION_1 0x10u
#define TC_WIRE_VERSION_MASK 0xf0u
#endif
