#include "mlx90395_frame.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  /* Manufacturer CRC example: X=0, Y=-32768, Z=0, T=0x0526, V=0. */
  uint8_t frame[12] = {0x91, 0xcf, 0, 0, 0x80, 0, 0, 0, 5, 0x26, 0, 0};
  mlx90395_frame_t decoded = {0};
  assert(mlx90395_decode_frame(frame, &decoded));
  assert(decoded.axis[0] == 0 && decoded.axis[1] == -32768);
  assert(decoded.axis[2] == 0 && decoded.counter == 1 && decoded.fresh);
  const mlx90395_frame_t saved = decoded;
  for (unsigned byte = 1; byte < 12; ++byte) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      frame[byte] ^= (uint8_t)(1u << bit);
      assert(!mlx90395_decode_frame(frame, &decoded));
      assert(memcmp(&decoded, &saved, sizeof(saved)) == 0);
      frame[byte] ^= (uint8_t)(1u << bit);
    }
  }
  frame[0] = 0x90;
  assert(mlx90395_decode_frame(frame, &decoded) && !decoded.fresh);
  frame[0] = 0x93;
  assert(mlx90395_decode_frame(frame, &decoded) && decoded.sensor_reset);
  frame[0] = 0x95;
  assert(!mlx90395_decode_frame(frame, &decoded));
  frame[0] = 0x99;
  assert(!mlx90395_decode_frame(frame, &decoded));
  frame[0] = 0x11;
  assert(!mlx90395_decode_frame(frame, &decoded));
  assert(!mlx90395_decode_frame(NULL, &decoded));
  assert(!mlx90395_decode_frame(frame, NULL));
  puts("MLX90395: CRC vector, corruption, status, freshness and reset checks passed.");
  return 0;
}
