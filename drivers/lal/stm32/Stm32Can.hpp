#pragma once
#include "drivers/lal/ICan.hpp"
#include <algorithm>

#if defined(STM32G474xx) || defined(STM32G4xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32H7xx)
#include "stm32h7xx_hal.h"
#elif defined(STM32L4xx)
#include "stm32l4xx_hal.h"
#else
#include "stm32g4xx_hal.h"
#endif

namespace lal {

class Stm32Can : public ICan {
public:
    explicit Stm32Can(FDCAN_HandleTypeDef* hfdcan) : hfdcan_(hfdcan) {}

    bool init() override { return true; }
    
    bool start() override {
        if (HAL_FDCAN_ActivateNotification(hfdcan_, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_BUS_OFF, 0) != HAL_OK) {
            return false;
        }
        return HAL_FDCAN_Start(hfdcan_) == HAL_OK;
    }

    bool stop() override { return HAL_FDCAN_Stop(hfdcan_) == HAL_OK; }

    bool send(const CanMessage& msg) override {
        FDCAN_TxHeaderTypeDef tx_header;
        tx_header.Identifier = msg.id;
        tx_header.IdType = msg.is_extended ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
        tx_header.TxFrameType = FDCAN_DATA_FRAME;
        tx_header.DataLength = GetDlcBytes(msg.dlc);
        tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        tx_header.BitRateSwitch = FDCAN_BRS_OFF;
        tx_header.FDFormat = FDCAN_CLASSIC_CAN;
        tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        tx_header.MessageMarker = 0;

        return HAL_FDCAN_AddMessageToTxFifoQ(hfdcan_, &tx_header, const_cast<uint8_t*>(msg.data)) == HAL_OK;
    }

    void register_rx_callback(RxCallback callback, void* context) override {
        rx_callback_ = callback;
        rx_context_ = context;
        RegisterInstance(hfdcan_, this);
    }

    uint32_t get_tx_fifo_free_level() const override { return HAL_FDCAN_GetTxFifoFreeLevel(hfdcan_); }

    void handle_rx_interrupt() {
        while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan_, FDCAN_RX_FIFO0) > 0) {
            FDCAN_RxHeaderTypeDef rx_header;
            uint8_t rx_data[64] = {0};
            if (HAL_FDCAN_GetRxMessage(hfdcan_, FDCAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
                if (rx_callback_) {
                    CanMessage msg;
                    msg.id = rx_header.Identifier;
                    msg.is_extended = (rx_header.IdType == FDCAN_EXTENDED_ID);
                    msg.dlc = GetBytesFromDlc(rx_header.DataLength);
                    std::copy(rx_data, rx_data + msg.dlc, msg.data);
                    rx_callback_(msg, rx_context_);
                }
            }
        }
    }

private:
    static inline uint32_t GetDlcBytes(uint8_t dlc) {
        switch (dlc) {
            case 0: return FDCAN_DLC_BYTES_0;
            case 1: return FDCAN_DLC_BYTES_1;
            case 2: return FDCAN_DLC_BYTES_2;
            case 3: return FDCAN_DLC_BYTES_3;
            case 4: return FDCAN_DLC_BYTES_4;
            case 5: return FDCAN_DLC_BYTES_5;
            case 6: return FDCAN_DLC_BYTES_6;
            case 7: return FDCAN_DLC_BYTES_7;
            case 8: return FDCAN_DLC_BYTES_8;
            default: return FDCAN_DLC_BYTES_8;
        }
    }

    static inline uint8_t GetBytesFromDlc(uint32_t dlc_const) {
        switch (dlc_const) {
            case FDCAN_DLC_BYTES_0: return 0;
            case FDCAN_DLC_BYTES_1: return 1;
            case FDCAN_DLC_BYTES_2: return 2;
            case FDCAN_DLC_BYTES_3: return 3;
            case FDCAN_DLC_BYTES_4: return 4;
            case FDCAN_DLC_BYTES_5: return 5;
            case FDCAN_DLC_BYTES_6: return 6;
            case FDCAN_DLC_BYTES_7: return 7;
            case FDCAN_DLC_BYTES_8: return 8;
            default: return 8;
        }
    }

    static inline void RegisterInstance(FDCAN_HandleTypeDef* hfdcan, Stm32Can* instance);

    FDCAN_HandleTypeDef* hfdcan_;
    RxCallback rx_callback_ = nullptr;
    void* rx_context_ = nullptr;

    friend void ::HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);
};

inline Stm32Can* g_can_instances[2] = {nullptr, nullptr};

inline void Stm32Can::RegisterInstance(FDCAN_HandleTypeDef* hfdcan, Stm32Can* instance) {
    if (g_can_instances[0] == nullptr || g_can_instances[0]->hfdcan_ == hfdcan) {
        g_can_instances[0] = instance;
    } else if (g_can_instances[1] == nullptr || g_can_instances[1]->hfdcan_ == hfdcan) {
        g_can_instances[1] = instance;
    }
}

} // namespace lal

extern "C" inline void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0) {
        for (auto* inst : lal::g_can_instances) {
            if (inst && inst->hfdcan_ == hfdcan) {
                inst->handle_rx_interrupt();
                break;
            }
        }
    }
}
