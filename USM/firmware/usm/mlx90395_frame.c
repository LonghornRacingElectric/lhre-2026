#include "mlx90395_frame.h"
#include <stddef.h>
bool mlx90395_decode_frame(const uint8_t response[12], mlx90395_frame_t *out) {
  if (response == NULL || out == NULL || (response[0] & 0x80u) == 0u ||
      (response[0] & 0x0cu) != 0u) return false;
  uint8_t crc = 0u;
  for (unsigned i = 2; i < 12; ++i) {
    crc ^= response[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (uint8_t)((crc << 1) ^ ((crc & 0x80u) ? 0x07u : 0u));
  }
  if (crc != response[1]) return false;
  mlx90395_frame_t frame = {0};
  for (unsigned i = 0; i < 3; ++i)
    frame.axis[i] = (int16_t)((uint16_t)response[2 + i * 2] << 8 |
                            response[3 + i * 2]);
  frame.counter = (response[0] >> 4) & 7u;
  frame.fresh = (response[0] & 1u) != 0u;
  frame.sensor_reset = (response[0] & 2u) != 0u;
  *out = frame;
  return true;
}
