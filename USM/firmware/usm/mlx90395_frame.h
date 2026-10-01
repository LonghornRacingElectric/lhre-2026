#ifndef ORION_MLX90395_FRAME_H
#define ORION_MLX90395_FRAME_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
  int16_t axis[3];
  uint8_t counter;
  bool fresh;
  bool sensor_reset;
} mlx90395_frame_t;
/* RM 0x40 complete response: status, CRC, X/Y/Z/T/V (big endian).
 * Accepts burst mode with no communication/overflow errors and valid CRC.
 * A decoded response may still be old; consumers must inspect fresh. */
bool mlx90395_decode_frame(const uint8_t response[12], mlx90395_frame_t *out);
#endif
