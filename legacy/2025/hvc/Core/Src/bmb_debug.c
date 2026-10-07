#include "bmb_debug.h"

#include <stdbool.h>
#include <stdio.h>

#include "adbms.h"
#include "main.h"
#include "usb_vcp.h"

#if HVC_BMB_DEBUG_INDEX >= 0 && HVC_BMB_DEBUG_INDEX < NUM_BMS_ICS

/* C-ADC single shot finishes in well under this; matches cells_periodic(). */
#define BMB_DEBUG_ADCV_WAIT_MS 10U
/* The S-ADC measures the cells sequentially, so give it longer. */
#define BMB_DEBUG_ADSV_WAIT_MS 20U
#define BMB_DEBUG_LINE_SIZE 256U
/* Value of a cell voltage register that has not been written by a
   conversion since reset or clear. */
#define ADBMS_REGISTER_CLEARED 0x8000U

static adbms6830_raw_reply_t replies[NUM_BMS_ICS];

/* Same scaling as cells.c: 150 uV/LSB, 1.5 V offset, signed 16-bit. */
static float registerToVolts(uint16_t raw) {
    return (float)(int16_t)raw * 0.00015f + 1.5f;
}

static uint16_t registerWord(const adbms6830_raw_reply_t *reply, int word) {
    return (uint16_t)(reply->data[word * 2] | (reply->data[word * 2 + 1] << 8));
}

static bool replyPecOk(const adbms6830_raw_reply_t *reply) {
    return reply->calcPec == reply->rxPec;
}

static void printRawReply(const char *label, uint32_t matched,
                          bool decodeVoltages) {
    const adbms6830_raw_reply_t *reply = &replies[HVC_BMB_DEBUG_INDEX];
    char line[BMB_DEBUG_LINE_SIZE];
    int length = snprintf(
        line, sizeof(line),
        "BMBDBG BMB%02d %-14s data=[%02X %02X %02X %02X %02X %02X] "
        "pec_rx=0x%03X pec_calc=0x%03X cc=%u %s (chain %lu/%u PEC ok)",
        HVC_BMB_DEBUG_INDEX + 1, label,
        reply->data[0], reply->data[1], reply->data[2],
        reply->data[3], reply->data[4], reply->data[5],
        (unsigned int)reply->rxPec, (unsigned int)reply->calcPec,
        (unsigned int)reply->rxCounter,
        replyPecOk(reply) ? "PEC_OK" : "PEC_FAIL",
        (unsigned long)matched, (unsigned int)NUM_BMS_ICS);

    if (decodeVoltages && length > 0 && (size_t)length < sizeof(line)) {
        length += snprintf(&line[length], sizeof(line) - (size_t)length, " V=[");
        for (int word = 0; word < 3 && (size_t)length < sizeof(line); word++) {
            const uint16_t raw = registerWord(reply, word);
            if (raw == ADBMS_REGISTER_CLEARED) {
                length += snprintf(&line[length], sizeof(line) - (size_t)length,
                                   "%sCLEARED", word == 0 ? "" : " ");
            } else {
                length += snprintf(&line[length], sizeof(line) - (size_t)length,
                                   "%s%.4f", word == 0 ? "" : " ",
                                   (double)registerToVolts(raw));
            }
        }
        if ((size_t)length < sizeof(line)) {
            (void)snprintf(&line[length], sizeof(line) - (size_t)length, "]");
        }
    }
    println(line);
}

/* One line with every IC's PEC result and command counter, so a break in
   the daisy chain shows up as a run of failures from one position on. */
static void printChainSummary(const char *label) {
    char line[BMB_DEBUG_LINE_SIZE];
    int length = snprintf(line, sizeof(line), "BMBDBG chain %-6s", label);
    for (int ic = 0; ic < NUM_BMS_ICS && length > 0 &&
                     (size_t)length < sizeof(line); ic++) {
        length += snprintf(&line[length], sizeof(line) - (size_t)length,
                           " %02d:%s/cc%u", ic + 1,
                           replyPecOk(&replies[ic]) ? "ok" : "FAIL",
                           (unsigned int)replies[ic].rxCounter);
    }
    println(line);
}

static uint32_t readAndPrint(ADBMS6830_Command_t command, const char *label,
                             bool decodeVoltages, void (*settle)(void)) {
    /* Printed even when nothing matched: an all-PEC-fail reply is exactly
       what we want to see. */
    const uint32_t matched = adbms6830_cmd_read_raw(command, replies);
    printRawReply(label, matched, decodeVoltages);
    settle();
    return matched;
}

void bmb_debug_dump(void (*settle)(void)) {
    usb_printf("BMBDBG ---- BMB%02d raw register dump (cc should advance by 1 "
               "after each ADCV/ADSV; CLEARED = 0x8000, no conversion) ----",
               HVC_BMB_DEBUG_INDEX + 1);
    settle();

    adbms6830_wakeup();

    readAndPrint(CMD_RDCFGA, "RDCFGA", false, settle);
    printChainSummary("RDCFGA");
    settle();

    readAndPrint(CMD_RDSID, "RDSID", false, settle);

    readAndPrint(CMD_RDCVA, "RDCVA(before)", true, settle);
    const uint8_t ccBefore = replies[HVC_BMB_DEBUG_INDEX].rxCounter;

    adbms6830_wakeup();
    adbms6830_adcv();
    HAL_Delay(BMB_DEBUG_ADCV_WAIT_MS);
    adbms6830_wakeup();

    readAndPrint(CMD_RDCVA, "RDCVA(after)", true, settle);
    const uint8_t ccAfterAdcv = replies[HVC_BMB_DEBUG_INDEX].rxCounter;
    printChainSummary("RDCVA");
    settle();

    readAndPrint(CMD_RDFCA, "RDFCA", true, settle);

    adbms6830_wakeup();
    adbms6830_cmd_poll(CMD_ADSV);
    HAL_Delay(BMB_DEBUG_ADSV_WAIT_MS);
    adbms6830_wakeup();

    readAndPrint(CMD_RDSVA, "RDSVA", true, settle);
    const uint8_t ccAfterAdsv = replies[HVC_BMB_DEBUG_INDEX].rxCounter;

    usb_printf("BMBDBG BMB%02d cc: before=%u after_ADCV=%u (+%u) after_ADSV=%u (+%u)",
               HVC_BMB_DEBUG_INDEX + 1, (unsigned int)ccBefore,
               (unsigned int)ccAfterAdcv,
               (unsigned int)((ccAfterAdcv - ccBefore) & 0x3FU),
               (unsigned int)ccAfterAdsv,
               (unsigned int)((ccAfterAdsv - ccAfterAdcv) & 0x3FU));
    settle();
}

#else

void bmb_debug_dump(void (*settle)(void)) {
    (void)settle;
}

#endif
