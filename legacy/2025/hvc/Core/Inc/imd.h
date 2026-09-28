//
// Created by Gautham Ramanarayanan on 2025/01/09.
//

#ifndef IMD_H
#define IMD_H

#include <stdbool.h>

#define IMD_INFO_GENERAL 0x37
#define IMD_INFO_ISO_DETAIL 0x38
#define IMD_INFO_VOLTAGE 0x39
#define IMD_INFO_IT_SYS 0x3A
#define IMD_REQUEST 0x22
#define IMD_RESPONSE 0x23

bool isImdOk();
void testSetIMD(bool error);
void imd_can_init();
void imd_can_periodic();
void imd_send_cmd();

#endif //IMD_H
