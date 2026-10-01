#!/usr/bin/env python3
"""Host syntax checks and real VCU adapter tests; not an ARM firmware build."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[3];stm=r/'drivers/stm32g4';lib=r/'drivers/longhorn-lib'
with tempfile.TemporaryDirectory(prefix='orion-firmware-syntax-') as d:
 p=Path(d);(p/'longhorn').mkdir();(p/'longhorn/can').mkdir();(p/'longhorn/rtos').mkdir()
 for h in lib.glob('*.h'):(p/'longhorn'/h.name).symlink_to(h)
 for h in (lib/'rtos').glob('*.h'):(p/'longhorn/rtos'/h.name).symlink_to(h)
 (p/'longhorn/can/can_ids.h').symlink_to(lib/'can_ids.h');(p/'hvc_states').symlink_to(r/'HVC/firmware');(p/'vcu_model').symlink_to(r/'VCU/model')
 common=[p,stm/'STM32G4xx_HAL_Driver/Inc',stm/'CMSIS/Include',stm/'CMSIS/Device/ST/STM32G4xx/Include',stm/'FreeRTOS/include',stm/'FreeRTOS/portable/GCC/ARM_CM4F',stm/'FreeRTOS/CMSIS_RTOS_V2',stm/'USB_Device/App',stm/'USB_Device/Target',stm/'STM32_USB_Device_Library/Core/Inc',stm/'STM32_USB_Device_Library/Class/CDC/Inc',lib/'config',r/'drivers/ota',r/'drivers']
 for board in ('USM','VCU'):
  fw=r/board/'firmware';incs=common+[fw/'Core/Inc',fw/board.lower(),r/'VCU/model/inc',r/'VCU/model/components',r/'VCU/model/util']
  srcs=[fw/board.lower()/'orion_time_us.c',fw/board.lower()/(board.lower()+'_can.c'),fw/'Core/Src/app_freertos.c',fw/'Core/Src/fdcan.c']
  if board=='USM':srcs += [fw/'usm/wheel_speed.c',fw/'Core/Src/spi.c',fw/'Core/Src/gpio.c']
  for src in srcs:
   result=subprocess.run(['cc','-std=gnu11','-fsyntax-only','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-DSTM32G474xx','-DUSE_HAL_DRIVER','-DBOARD_FL',*['-I'+str(x) for x in incs],str(src)],text=True,capture_output=True)
   if result.returncode:print(src, result.stderr);raise SystemExit(1)
   print('syntax OK:',src.relative_to(r))

 # Link only the actual adapter/packing paths exercised below; HAL functions
 # in other sections are deliberately not simulated by this host test.
 source = p / 'adapter_test.c'
 source.write_text(r'''#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "VCU/firmware/vcu/vcu_can.c"
static uint32_t fake_us;
static bool inject_receive;
uint32_t orion_time_us(void) { return fake_us; }
void vPortEnterCritical(void) {
  if (inject_receive) {
    fake_us += 10;
    tc_handles[0]->latest_rx_us = fake_us;
    tc_fl.estimate_time_us = fake_us;
    inject_receive = false;
  }
}
void vPortExitCritical(void) {}
int main(void) {
  uint8_t data[8];
  msg_inverter_torque_command_t torque = {0};
  inverter_torque_command_mailbox.torque_request = 100.0f;
  inverter_torque_command_mailbox.enable = 1;
  inverter_torque_command_mailbox.torque_limit = 230.0f;
  pack_leased_torque(NULL, data);
  unpack_inverter_torque_command(data, &torque);
  assert(torque.enable == 0 && torque.torque_request == 0);
  control_update_seen = true;
  control_update_us = UINT32_MAX - 1000u;
  fake_us = 1000u;
  pack_leased_torque(NULL, data);
  unpack_inverter_torque_command(data, &torque);
  assert(torque.enable == 1 && fabsf(torque.torque_request - 100.0f) < 0.01f);
  fake_us = control_update_us + 9001u;
  pack_leased_torque(NULL, data);
  unpack_inverter_torque_command(data, &torque);
  assert(torque.enable == 0 && torque.torque_request == 0 && torque.torque_limit == 0);
  control_update_us = fake_us;
  pack_leased_torque(NULL, data);
  unpack_inverter_torque_command(data, &torque);
  assert(torque.enable == 1 && fabsf(torque.torque_request - 100.0f) < 0.01f);
  can_receive_message_t handle = {0};
  handle.ever_received = true;
  handle.latest_rx_us = fake_us;
  tc_handles[0] = &handle;
  tc_fl.angular_speed = -12.34f;
  tc_fl.sequence = 255;
  tc_fl.status = TC_WIRE_VERSION_1 | TC_WIRE_VALID | TC_WIRE_DIRECTION | TC_WIRE_SYNC;
  tc_inputs_t in;
  inject_receive = true;
  vcu_can_get_traction_inputs(&in);
  assert(in.wheel[0].valid && in.wheel[0].synchronized);
  assert(in.now_us == fake_us && in.wheel[0].timestamp_us == fake_us);
  assert(!in.motion_valid && !in.geometry_valid);
  tc_fl.sequence = 0;
  vcu_can_get_traction_inputs(&in);
  assert(in.wheel[0].valid && in.wheel[0].sequence == 256);
  fake_us += 10001;
  vcu_can_get_traction_inputs(&in);
  assert(!in.wheel[0].valid);
  msg_traction_wheel_fl_t wheel = {-12.34f, UINT32_MAX, 255, tc_fl.status};
  msg_traction_wheel_fl_t roundtrip = {0};
  pack_traction_wheel_fl(&wheel, data);
  unpack_traction_wheel_fl(data, &roundtrip);
  assert(fabsf(roundtrip.angular_speed - wheel.angular_speed) < 0.011f);
  assert(roundtrip.estimate_time_us == UINT32_MAX && roundtrip.sequence == 255);
  assert(roundtrip.status == wheel.status);
  tc_diagnostics.candidate_torque = 100;
  tc_diagnostics.worst_slip = -1.23f;
  tc_diagnostics.state = 5;
  tc_diagnostics.faults = 255;
  msg_traction_diagnostics_t diagnostics = {0};
  pack_tc_diagnostics_snapshot(NULL, data);
  unpack_traction_diagnostics(data, &diagnostics);
  assert(diagnostics.state == 5 && diagnostics.faults == 255);
  assert(fabsf(diagnostics.worst_slip + 1.23f) < 0.011f);
  puts("VCU adapter: command lease, wrap, RX snapshot, sequence wrap, wire encoding passed.");
  return 0;
}
''')
 incs=common+[r,r/'VCU/firmware/Core/Inc',r/'VCU/firmware/vcu',r/'VCU/model/inc',r/'VCU/model/components',r/'VCU/model/util']
 subprocess.run(['cc','-std=gnu11','-ffunction-sections','-fdata-sections','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-DSTM32G474xx','-DUSE_HAL_DRIVER',*['-I'+str(x) for x in incs],str(source),str(lib/'can_ids.c'),'-Wl,--gc-sections','-lm','-o',str(p/'adapter_test')],check=True)
 subprocess.run([str(p/'adapter_test')],check=True)
