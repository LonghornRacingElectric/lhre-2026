#ifndef HVC_FAULTS_H
#define HVC_FAULTS_H

#include <stdint.h>

#define FAULT_BMS_COMMS        (1UL << 0)
#define FAULT_BMS_OVERVOLTAGE  (1UL << 1)
#define FAULT_BMS_UNDERVOLTAGE (1UL << 2)
#define FAULT_BMS_OVERTEMP     (1UL << 3)

void faults_init(void);
uint32_t get_faults(void);
uint32_t get_last_faults(void);
void latch_faults(uint32_t faults);
uint32_t get_latched_faults(void);

#endif // HVC_FAULTS_H
