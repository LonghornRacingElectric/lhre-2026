#include "wheelspeeds.h"
#include "LonghornLib/angel_can_ids.h"
#include "LonghornLib/angel_can.h"
#include "LonghornLib/faults.h"
#include "timeouts.h"

static CanInbox whs_fr_inbox;
static CanInbox whs_fl_inbox;
static CanInbox whs_br_inbox;
static CanInbox whs_bl_inbox;

void wheelspeeds_init() {
    can_addInbox(UNSFR_VCU_MAGNET, &whs_fr_inbox, UNS_TIMEOUT);
    can_addInbox(UNSFL_VCU_MAGNET, &whs_fl_inbox, UNS_TIMEOUT);
    can_addInbox(UNSBR_VCU_MAGNET, &whs_br_inbox, UNS_TIMEOUT);
    can_addInbox(UNSBL_VCU_MAGNET, &whs_bl_inbox, UNS_TIMEOUT);
}

void wheelspeeds_periodic(WheelMagnetValues* wheelMagnetValues) {
    if(whs_fr_inbox.isRecent) {
      wheelMagnetValues->fr = can_readFloat(int16_t, &whs_fr_inbox, 0, 0.0025f);
        whs_fr_inbox.isRecent = false;
    }
    if(whs_fr_inbox.isTimeout) {
      FAULT_SET(&faultVector, FAULT_VCU_UNS_FR);
    }
    else {
      FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_FR);
    }

    if(whs_fl_inbox.isRecent) {
      wheelMagnetValues->fl = can_readFloat(int16_t, &whs_fl_inbox, 0, 0.0025f);
        whs_fl_inbox.isRecent = false;
    }
    if(whs_fl_inbox.isTimeout) {
      FAULT_SET(&faultVector, FAULT_VCU_UNS_FL);
    }
    else {
      FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_FL);
    }

    if(whs_br_inbox.isRecent) {
      wheelMagnetValues->br = can_readFloat(int16_t, &whs_br_inbox, 0, 0.0025f);
        whs_br_inbox.isRecent = false;
    }
    if(whs_br_inbox.isTimeout) {
      FAULT_SET(&faultVector, FAULT_VCU_UNS_BR);
    }
    else {
      FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_BR);
    }

    if(whs_bl_inbox.isRecent) {
      wheelMagnetValues->bl = can_readFloat(int16_t, &whs_bl_inbox, 0, 0.0025f);
        whs_bl_inbox.isRecent = false;
    }
    if(whs_bl_inbox.isTimeout) {
      FAULT_SET(&faultVector, FAULT_VCU_UNS_BL);
    }
    else {
      FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_BL);
    }
}
