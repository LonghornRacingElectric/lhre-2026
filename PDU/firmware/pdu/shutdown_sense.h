#ifndef PDU_SHUTDOWN_SENSE_H
#define PDU_SHUTDOWN_SENSE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool leg_11;
    bool leg_12;
    bool leg_13;
    bool leg_14;
    bool leg_15;
} shutdown_sense_t;

shutdown_sense_t shutdown_sense_read(void);
bool shutdown_sense_closed(void);

#ifdef __cplusplus
}
#endif

#endif  // PDU_SHUTDOWN_SENSE_H
