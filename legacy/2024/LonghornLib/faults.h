#ifndef LONGHORN_LIBRARY_2024_FAULTS_H
#define LONGHORN_LIBRARY_2024_FAULTS_H

#include <stdint.h>
#include <stdbool.h>
extern uint32_t faultVector;

/**
 * Set a fault bit in the fault vector.
 * @param fault_vector
 * @param fault
 */
// void fault_set(uint32_t* fault_vector, uint32_t fault);
#define FAULT_SET(fault_vector, fault) (*(fault_vector) |= (fault))

/**
 * Clear a fault bit in the fault vector.
 * @param fault_vector
 * @param fault
 */
// void fault_clear(uint32_t* fault_vector, uint32_t fault);
#define FAULT_CLEAR(fault_vector, fault) (*(fault_vector) &= ~(fault))

/**
 * Clear all fault bits in the fault vector.
 * @param fault_vector
 */
// void fault_clearAll(uint32_t* fault_vector);
#define FAULT_CLEARALL(fault_vector) (*(fault_vector) = 0)

/**
 * Check if a fault bit is set in the fault vector.
 * @param fault_vector
 * @param fault
 * @return true if fault is set, false otherwise
 */
// bool fault_check(const uint32_t* fault_vector, uint32_t fault);
#define FAULT_CHECK(fault_vector, fault) ((*(fault_vector) & (fault)) != 0)

// VCU FAULTS
/*
 * Next 8 bits relate to CAN communication with other devices
 * This uses the timeout feature found in CanInboxes which must be declared at creation
 */

#define FAULT_VCU_INV (1 << 0) /// We haven't received a packet from Inverter in a significant amount of time
#define FAULT_VCU_PDU (1 << 1) /// We haven't received a packet from PDU in a significant amount of time
#define FAULT_VCU_HVC_NOT_TELEM (1 << 2) /// We haven't received a non-telemetry packet from HVC in a significant amount of time
#define FAULT_VCU_HVC_TELEM (1 << 3) /// We haven't received a telemetry packet from HVC in a significant amount of time
#define FAULT_VCU_UNS_FR (1 << 4) /// We haven't received a packet from the front right wheel magnet in a significant amount of time
#define FAULT_VCU_UNS_FL (1 << 5) /// We haven't received a packet from the front left wheel magnet in a significant amount of time
#define FAULT_VCU_UNS_BR (1 << 6) /// We haven't received a packet from the back right wheel magnet in a significant amount of time
#define FAULT_VCU_UNS_BL (1 << 7) /// We haven't received a packet from the back left wheel magnet in a significant amount of time

/*
 * Next 16 bits relate to the VCU board peripherals in general. This includes:
 * CAN, NVM, GPS, CELL, ADC
 */

#define FAULT_VCU_CAN_BAD_TX (1 << 8) /// If can_send fails to add packet to TxFiFo
#define FAULT_VCU_CAN_BAD_RX (1 << 9) /// If can_processRxFifo fails to process RxFifo
#define FAULT_VCU_NVM_NO_MOUNT (1 << 10) /// If SD Card's Drive fails to mount
#define FAULT_VCU_NVM_NO_CREATE (1 << 11) /// If SD Card's Telemetry File fails to create
#define FAULT_VCU_NVM_NO_WRITE (1 << 12) /// If Sd Card's Telemetry File fails to write
#define FAULT_VCU_GPS_BAD_RX (1 << 13) /// If gps message received cannot be split into NMEA sentences
#define FAULT_VCU_GPS_NO_DMA_START (1 << 14) /// If gps DMA fails to start
#define FAULT_VCU_GPS_TIMEOUT (1 << 15) /// If gps fails to send data for more than 1 second, activates DMA reset

#define FAULT_VCU_CELL_NOCXN (1 << 16) /// If cell is not connected (startup state)
#define FAULT_VCU_CELL_SEARCHING (1 << 17) /// If cell is searching for a connection, sending out cxn requests
#define FAULT_VCU_CELL_CONNECTING (1 << 18) /// If cell is connected to T-Mobile service, and now is connecting to server
#define FAULT_VCU_CELL_NO_HANDSHAKE (1 << 19) /// If cell is connected to server, and is now attempting handshake
#define FAULT_VCU_CELL_NO_DMA_START (1 << 20) /// If cell DMA fails to start
#define FAULT_VCU_CELL_BAD_RX (1 << 21) /// If cell message received cannot be parsed as AT commands
#define FAULT_VCU_CELL_QUEUE_FULL (1 << 22) /// If cell is still sending out message to server, will be cleared when message is finished
#define FAULT_VCU_ADC_NO_START (1 << 23) /// If ADCs fails to start interpreting analog data from driver inputs

#define CELL_RESET_STATE 0x000F0000 /// Utility bit mask to nullify state

#endif //LONGHORN_LIBRARY_2024_FAULTS_H
