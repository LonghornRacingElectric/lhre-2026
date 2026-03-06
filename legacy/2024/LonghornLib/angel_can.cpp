#include "angel_can.h"
#include "faults.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>

using namespace std;

static unordered_map<uint32_t, CanInbox *> allInboxes;
static unordered_map<uint32_t, CanOutbox *> allOutboxes;
static CAN_HANDLE *canHandleTypeDef;

/**
 * Returns the struct associated with this ID
 * If no struct is found, returns nullptr
 * @param id 11bit number
 * @return CanInbox* struct
 */
static CanInbox *can_getInbox(uint32_t id) {
  if (allInboxes.find(id) == allInboxes.end()) {
    return nullptr;
  } else {
    return allInboxes.at(id);
  }
}

#ifdef H7_SERIES

static uint8_t dlc_to_num(uint32_t dlc) {
  switch (dlc) {
    case FDCAN_DLC_BYTES_0:
      return 0;
    case FDCAN_DLC_BYTES_1:
      return 1;
    case FDCAN_DLC_BYTES_2:
      return 2;
    case FDCAN_DLC_BYTES_3:
      return 3;
    case FDCAN_DLC_BYTES_4:
      return 4;
    case FDCAN_DLC_BYTES_5:
      return 5;
    case FDCAN_DLC_BYTES_6:
      return 6;
    case FDCAN_DLC_BYTES_7:
      return 7;
    case FDCAN_DLC_BYTES_8:
      return 8;
    default:
      return 0;
  }
}

static uint32_t num_to_dlc(uint8_t num) {
  switch (num) {
    case 0:
      return FDCAN_DLC_BYTES_0;
    case 1:
      return FDCAN_DLC_BYTES_1;
    case 2:
      return FDCAN_DLC_BYTES_2;
    case 3:
      return FDCAN_DLC_BYTES_3;
    case 4:
      return FDCAN_DLC_BYTES_4;
    case 5:
      return FDCAN_DLC_BYTES_5;
    case 6:
      return FDCAN_DLC_BYTES_6;
    case 7:
      return FDCAN_DLC_BYTES_7;
    case 8:
      return FDCAN_DLC_BYTES_8;
    default:
      return FDCAN_DLC_BYTES_0;
  }
}

#endif

/**
 * Put a CAN packet in the Tx FIFO, which is guaranteed to be pushed onto the CAN BUS
 * @param id ID of the CAN packet
 * @param dlc Length of the CAN packet
 * @param data Data of the CAN packet
 * @return 0 if successful, 1 if unsuccessful
 */
uint32_t can_send(uint32_t id, uint8_t dlc, uint8_t *data) {
#ifdef H7_SERIES
  static FDCAN_TxHeaderTypeDef TxHeader;
  TxHeader.Identifier = id;
  TxHeader.IdType = (id > 0x7FF) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = num_to_dlc(dlc);
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  uint32_t error = HAL_FDCAN_AddMessageToTxFifoQ(canHandleTypeDef, &TxHeader, data);
  if (error != HAL_OK) {
    if(error & HAL_FDCAN_ERROR_FIFO_FULL) {
      FAULT_SET(&faultVector, FAULT_VCU_CAN_BAD_TX);
    } else if(error & 0xFF) {
      return canHandleTypeDef->ErrorCode;
    }
  }
#endif
#ifdef STM32L431xx
  static CAN_TxHeaderTypeDef TxHeader;
  if(id > 0x7FF) {
      TxHeader.IDE = CAN_ID_EXT;
      TxHeader.ExtId = id;
  } else {
      TxHeader.IDE = CAN_ID_STD;
      TxHeader.StdId = id;
  }
  TxHeader.DLC = dlc;
  TxHeader.RTR = CAN_RTR_DATA;

  uint32_t TxMailbox;
  volatile uint32_t error = HAL_CAN_AddTxMessage(canHandleTypeDef, &TxHeader, data, &TxMailbox);
  if (error != HAL_OK) {
    if(canHandleTypeDef->ErrorCode & 0x200000) { // full probably
      HAL_CAN_AbortTxRequest(canHandleTypeDef, 0x7);
      FAULT_SET(&faultVector, FAULT_VCU_CAN_BAD_TX);
    } else {
      return canHandleTypeDef->ErrorCode;
    }
  }
#endif

  return 0;
}


void can_init(CAN_HANDLE *handle) {
  canHandleTypeDef = handle;

#ifdef H7_SERIES
  HAL_FDCAN_Start(canHandleTypeDef);
#endif
#ifdef STM32L431xx
  HAL_CAN_Start(canHandleTypeDef);
#endif
}

void can_addOutbox(uint32_t id, float period, CanOutbox *outbox) {
  outbox->period = period;
  allOutboxes.insert({id, outbox});
}

void can_addOutboxes(uint32_t idLow, uint32_t idHigh, float period, CanOutbox *outboxes) {
  float staggerInterval = period / ((float)(idHigh - idLow + 1));
  float stagger = 0;
  for (uint32_t i = idLow; i <= idHigh; i++, outboxes++) {
    outboxes->_timer = stagger;
    can_addOutbox(i, period, outboxes);
    stagger += staggerInterval;
  }
}

void can_addInbox(uint32_t id, CanInbox *mailbox, float timeoutLimit) {
  mailbox->timeLimit = timeoutLimit;
  allInboxes.insert({id, mailbox});
}

void can_addInboxes(uint32_t idLow, uint32_t idHigh, CanInbox *mailboxes, float timeoutLimit) {
  for (uint32_t i = idLow; i <= idHigh; i++) {
    can_addInbox(i, &mailboxes[i - idLow], timeoutLimit);
  }
}

static uint32_t can_processRxFifo() {
#ifdef H7_SERIES
  static FDCAN_RxHeaderTypeDef RxHeader;
  static uint8_t RxData[8];

  while (HAL_FDCAN_GetRxMessage(canHandleTypeDef, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
    uint32_t id = RxHeader.Identifier;
    uint8_t dlc = dlc_to_num(RxHeader.DataLength);
    CanInbox *this_mailbox = can_getInbox(id);
    if (this_mailbox != nullptr) {
      copy(RxData, RxData + dlc, this_mailbox->data);
      this_mailbox->isRecent = true;
      this_mailbox->dlc = dlc;
      this_mailbox->ageSinceRx = 0;
      this_mailbox->isTimeout = false;
    }
  }
  // If error code is something other than the fifo being empty or full, return error
  if ((canHandleTypeDef->ErrorCode & 0xFF) != HAL_FDCAN_ERROR_NONE) {
    FAULT_SET(&faultVector, FAULT_VCU_CAN_BAD_RX);
    return canHandleTypeDef->ErrorCode;
  }
#endif
#ifdef STM32L431xx
  static CAN_RxHeaderTypeDef RxHeader;
    static uint8_t RxData[8];
    while(HAL_CAN_GetRxFifoFillLevel(canHandleTypeDef, CAN_RX_FIFO0)) {
        if(HAL_CAN_GetRxMessage(canHandleTypeDef, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
            uint32_t id = (RxHeader.IDE == CAN_ID_EXT) ? RxHeader.ExtId : RxHeader.StdId;
            uint32_t dlc = RxHeader.DLC;
            CanInbox *this_mailbox = can_getInbox(id);
            if (this_mailbox != nullptr) {
                copy(RxData, RxData + dlc, this_mailbox->data);
                this_mailbox->isRecent = true;
                this_mailbox->dlc = dlc;
                this_mailbox->ageSinceRx = 0;
                this_mailbox->isTimeout = false;
            }
        } else {
            return canHandleTypeDef->ErrorCode;
        }
    }
    if ((canHandleTypeDef->ErrorCode & 0xFF) != HAL_CAN_ERROR_NONE) {
      FAULT_SET(&faultVector, FAULT_VCU_CAN_BAD_RX);
      return canHandleTypeDef->ErrorCode;
  }
#endif
  return HAL_OK;
}

static uint32_t can_sendAll(float deltaTime) {
  for(const auto & [ id, outbox ] : allOutboxes) {
    outbox->_timer += deltaTime;
    if(outbox->_timer >= outbox->period) {
      outbox->_timer = fmod(outbox->_timer, outbox->period);
      uint32_t error = can_send(id, outbox->dlc, outbox->data);
      if(error != HAL_OK) {
        return error;
      }
    }
  }
  for(const auto & [ id, inbox ] : allInboxes) {
    inbox->ageSinceRx += deltaTime;
    if(inbox->timeLimit != 0 &&
      inbox->timeLimit < inbox->ageSinceRx) { // Checks if the age of the inbox is greater than the timeout and that timeout exists
      inbox->isTimeout = true;
      inbox->isRecent = false;
    }
  }
  return HAL_OK;
}

uint32_t can_periodic(float deltaTime) {
  uint32_t error = can_processRxFifo();
  if (error != HAL_OK) {
    return error; // 0x300
  }

  error = can_sendAll(deltaTime);
  if (error != HAL_OK) {
    return error;
  }

  return HAL_OK;
}
