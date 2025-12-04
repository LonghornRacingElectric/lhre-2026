#ifndef LONGHORN_LIB_CAN_HAL_H
#define LONGHORN_LIB_CAN_HAL_H

#include <stdint.h>

/**
 * COPIED FROM STM32G4xx_HAL_Driver/Inc/stm32g4xx_hal_fdcan.h
 * FDCAN DEFINITION STRUCTS. THESE ARE USED TO MATCH THE HAL STRUCTS
 */

/**
 * @brief  FDCAN filter structure definition
 */
typedef struct {
    uint32_t IdType; /*!< Specifies the identifier type.
                          This parameter can be a value of @ref FDCAN_id_type */

    uint32_t FilterIndex; /*!< Specifies the filter which will be initialized.
                               This parameter must be a number between:
                                - 0 and (SRAMCAN_FLS_NBR-1), if IdType is
                             FDCAN_STANDARD_ID
                                - 0 and (SRAMCAN_FLE_NBR-1), if IdType is
                             FDCAN_EXTENDED_ID */

    uint32_t
        FilterType; /*!< Specifies the filter type.
                         This parameter can be a value of @ref
                       FDCAN_filter_type. The value FDCAN_FILTER_RANGE_NO_EIDM
                       is permitted only when IdType is FDCAN_EXTENDED_ID. */

    uint32_t FilterConfig; /*!< Specifies the filter configuration.
                                This parameter can be a value of @ref
                              FDCAN_filter_config */

    uint32_t
        FilterID1; /*!< Specifies the filter identification 1.
                        This parameter must be a number between:
                         - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                         - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID */

    uint32_t
        FilterID2; /*!< Specifies the filter identification 2.
                        This parameter must be a number between:
                         - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                         - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID */

} cFDCAN_FilterTypeDef;

/**
 * @brief  FDCAN Tx header structure definition
 */
typedef struct {
    uint32_t
        Identifier; /*!< Specifies the identifier.
                         This parameter must be a number between:
                          - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                          - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID */

    uint32_t
        IdType; /*!< Specifies the identifier type for the message that will be
                     transmitted.
                     This parameter can be a value of @ref FDCAN_id_type */

    uint32_t TxFrameType; /*!< Specifies the frame type of the message that will
                             be transmitted. This parameter can be a value of
                             @ref FDCAN_frame_type            */

    uint32_t DataLength; /*!< Specifies the length of the frame that will be
                            transmitted. This parameter can be a value of @ref
                            FDCAN_data_length_code     */

    uint32_t ErrorStateIndicator; /*!< Specifies the error state indicator.
                                       This parameter can be a value of @ref
                                     FDCAN_error_state_indicator */

    uint32_t BitRateSwitch; /*!< Specifies whether the Tx frame will be
                               transmitted with or without bit rate switching.
                                 This parameter can be a value of @ref
                               FDCAN_bit_rate_switching    */

    uint32_t
        FDFormat; /*!< Specifies whether the Tx frame will be transmitted in
                     classic or FD format. This parameter can be a value of
                     @ref FDCAN_format                */

    uint32_t TxEventFifoControl; /*!< Specifies the event FIFO control.
                                      This parameter can be a value of @ref
                                    FDCAN_EFC                   */

    uint32_t MessageMarker; /*!< Specifies the message marker to be copied into
                               Tx Event FIFO element for identification of Tx
                               message status. This parameter must be a number
                               between 0 and 0xFF                */

} cFDCAN_TxHeaderTypeDef;

/**
 * @brief  FDCAN Rx header structure definition
 */
typedef struct {
    uint32_t
        Identifier; /*!< Specifies the identifier.
                         This parameter must be a number between:
                          - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                          - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID */

    uint32_t IdType; /*!< Specifies the identifier type of the received message.
                          This parameter can be a value of @ref FDCAN_id_type */

    uint32_t RxFrameType; /*!< Specifies the the received message frame type.
                               This parameter can be a value of @ref
                             FDCAN_frame_type            */

    uint32_t DataLength; /*!< Specifies the received frame length.
                               This parameter can be a value of @ref
                            FDCAN_data_length_code     */

    uint32_t ErrorStateIndicator; /*!< Specifies the error state indicator.
                                       This parameter can be a value of @ref
                                     FDCAN_error_state_indicator */

    uint32_t BitRateSwitch; /*!< Specifies whether the Rx frame is received with
                               or without bit rate switching. This parameter can
                               be a value of @ref FDCAN_bit_rate_switching    */

    uint32_t FDFormat; /*!< Specifies whether the Rx frame is received in
                          classic or FD format. This parameter can be a value of
                          @ref FDCAN_format */

    uint32_t RxTimestamp; /*!< Specifies the timestamp counter value captured on
                             start of frame reception. This parameter must be a
                             number between 0 and 0xFFFF              */

    uint32_t
        FilterIndex; /*!< Specifies the index of matching Rx acceptance filter
                        element. This parameter must be a number between:
                           - 0 and (SRAMCAN_FLS_NBR-1), if IdType is
                        FDCAN_STANDARD_ID
                           - 0 and (SRAMCAN_FLE_NBR-1), if IdType is
                        FDCAN_EXTENDED_ID When the frame is a Non-Filter
                        matching frame, this parameter is unused. */

    uint32_t IsFilterMatchingFrame; /*!< Specifies whether the accepted frame
                                       did not match any Rx filter. Acceptance
                                       of non-matching frames may be enabled via
                                         HAL_FDCAN_ConfigGlobalFilter().
                                         This parameter takes 0 if the frame
                                       matched an Rx filter or 1 if it did not
                                       match any Rx filter */

} cFDCAN_RxHeaderTypeDef;

/**
 * @brief  FDCAN Tx event FIFO structure definition
 */
typedef struct {
    uint32_t
        Identifier; /*!< Specifies the identifier.
                         This parameter must be a number between:
                          - 0 and 0x7FF, if IdType is FDCAN_STANDARD_ID
                          - 0 and 0x1FFFFFFF, if IdType is FDCAN_EXTENDED_ID */

    uint32_t
        IdType; /*!< Specifies the identifier type for the transmitted message.
                     This parameter can be a value of @ref FDCAN_id_type */

    uint32_t TxFrameType; /*!< Specifies the frame type of the transmitted
                             message. This parameter can be a value of @ref
                             FDCAN_frame_type            */

    uint32_t DataLength; /*!< Specifies the length of the transmitted frame.
                              This parameter can be a value of @ref
                            FDCAN_data_length_code      */

    uint32_t ErrorStateIndicator; /*!< Specifies the error state indicator.
                                       This parameter can be a value of @ref
                                     FDCAN_error_state_indicator */

    uint32_t
        BitRateSwitch; /*!< Specifies whether the Tx frame is transmitted
                          with or without bit rate switching. This parameter
                          can be a value of @ref FDCAN_bit_rate_switching */

    uint32_t FDFormat; /*!< Specifies whether the Tx frame is transmitted in
                          classic or FD format. This parameter can be a value of
                          @ref FDCAN_format                */

    uint32_t TxTimestamp; /*!< Specifies the timestamp counter value captured on
                             start of frame transmission. This parameter must be
                             a number between 0 and 0xFFFF              */

    uint32_t MessageMarker; /*!< Specifies the message marker copied into Tx
                               Event FIFO element for identification of Tx
                               message status. This parameter must be a number
                               between 0 and 0xFF                */

    uint32_t EventType; /*!< Specifies the event type.
                             This parameter can be a value of @ref
                           FDCAN_event_type */

} cFDCAN_TxEventFifoTypeDef;

/**
 * @brief  HAL Status structures definition
 */
typedef enum {
    cHAL_OK = 0x00U,
    cHAL_ERROR = 0x01U,
    cHAL_BUSY = 0x02U,
    cHAL_TIMEOUT = 0x03U
} cHAL_StatusTypeDef;

/** Used for activating notifications */
#define NEW_MESSAGE_FIFO0 0x1
#define NEW_MESSAGE_FIFO1 0x8

#define FDCAN_STANDARD_ID ((uint32_t)0x00000000U) /*!< Standard ID element */
#define FDCAN_EXTENDED_ID ((uint32_t)0x40000000U) /*!< Extended ID element */

#define FDCAN_DATA_FRAME ((uint32_t)0x00000000U)   /*!< Data frame   */
#define FDCAN_REMOTE_FRAME ((uint32_t)0x20000000U) /*!< Remote frame */

#define FDCAN_ESI_ACTIVE \
    ((uint32_t)0x00000000U) /*!< Transmitting node is error active  */
#define FDCAN_ESI_PASSIVE \
    ((uint32_t)0x80000000U) /*!< Transmitting node is error passive */

#define FDCAN_BRS_OFF                                                          \
    ((uint32_t)0x00000000U) /*!< FDCAN frames transmitted/received without bit \
                               rate switching */
#define FDCAN_BRS_ON                                                        \
    ((uint32_t)0x00100000U) /*!< FDCAN frames transmitted/received with bit \
                               rate switching    */

#define FDCAN_CLASSIC_CAN                                                  \
    ((uint32_t)0x00000000U) /*!< Frame transmitted/received in Classic CAN \
                               format */
#define FDCAN_FD_CAN \
    ((uint32_t)0x00200000U) /*!< Frame transmitted/received in FDCAN format */

#define FDCAN_NO_TX_EVENTS \
    ((uint32_t)0x00000000U) /*!< Do not store Tx events */
#define FDCAN_STORE_TX_EVENTS \
    ((uint32_t)0x00800000U) /*!< Store Tx events        */

#define FDCAN_DLC_BYTES_0 ((uint32_t)0x00000000U)  /*!< 0 bytes data field  */
#define FDCAN_DLC_BYTES_1 ((uint32_t)0x00000001U)  /*!< 1 bytes data field  */
#define FDCAN_DLC_BYTES_2 ((uint32_t)0x00000002U)  /*!< 2 bytes data field  */
#define FDCAN_DLC_BYTES_3 ((uint32_t)0x00000003U)  /*!< 3 bytes data field  */
#define FDCAN_DLC_BYTES_4 ((uint32_t)0x00000004U)  /*!< 4 bytes data field  */
#define FDCAN_DLC_BYTES_5 ((uint32_t)0x00000005U)  /*!< 5 bytes data field  */
#define FDCAN_DLC_BYTES_6 ((uint32_t)0x00000006U)  /*!< 6 bytes data field  */
#define FDCAN_DLC_BYTES_7 ((uint32_t)0x00000007U)  /*!< 7 bytes data field  */
#define FDCAN_DLC_BYTES_8 ((uint32_t)0x00000008U)  /*!< 8 bytes data field  */
#define FDCAN_DLC_BYTES_12 ((uint32_t)0x00000009U) /*!< 12 bytes data field */
#define FDCAN_DLC_BYTES_16 ((uint32_t)0x0000000AU) /*!< 16 bytes data field */
#define FDCAN_DLC_BYTES_20 ((uint32_t)0x0000000BU) /*!< 20 bytes data field */
#define FDCAN_DLC_BYTES_24 ((uint32_t)0x0000000CU) /*!< 24 bytes data field */
#define FDCAN_DLC_BYTES_32 ((uint32_t)0x0000000DU) /*!< 32 bytes data field */
#define FDCAN_DLC_BYTES_48 ((uint32_t)0x0000000EU) /*!< 48 bytes data field */
#define FDCAN_DLC_BYTES_64 ((uint32_t)0x0000000FU) /*!< 64 bytes data field */

#endif