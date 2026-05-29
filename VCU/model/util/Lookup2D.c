#include "Lookup2D.h"

#include <math.h>
#include <string.h>

#define LOOKUP2D_SEGMENTS_X (LOOKUP2D_POINTS_X - 1)
#define LOOKUP2D_SEGMENTS_Y (LOOKUP2D_POINTS_Y - 1)

void Lookup2D_init(Lookup2D *lookup2d, float x0, float x1, float y0, float y1,
                   const float z_data[LOOKUP2D_POINTS_Y][LOOKUP2D_POINTS_X]) {
  if (!lookup2d) {
    return;
  }

  lookup2d->x0 = x0;
  lookup2d->x1 = x1;
  lookup2d->y0 = y0;
  lookup2d->y1 = y1;

  if (lookup2d->x1 <= lookup2d->x0) {
    lookup2d->x1 = lookup2d->x0 + 1.0f;
  }

  if (lookup2d->y1 <= lookup2d->y0) {
    lookup2d->y1 = lookup2d->y0 + 1.0f;
  }

  if (z_data) {
    memcpy(lookup2d->z, z_data, sizeof(lookup2d->z));
  } else {
    memset(lookup2d->z, 0, sizeof(lookup2d->z));
  }
}

float Lookup2D_evaluate(const Lookup2D *lookup2d, float x, float y) {
  if (!lookup2d) {
    return 0.0f;
  }

  float pct_x = (x - lookup2d->x0) / (lookup2d->x1 - lookup2d->x0) *
                (float)LOOKUP2D_SEGMENTS_X;
  float pct_y = (y - lookup2d->y0) / (lookup2d->y1 - lookup2d->y0) *
                (float)LOOKUP2D_SEGMENTS_Y;

  if (pct_x < 0.0f) {
    pct_x = 0.0f;
  } else if (pct_x > (float)LOOKUP2D_SEGMENTS_X) {
    pct_x = (float)LOOKUP2D_SEGMENTS_X;
  }

  if (pct_y < 0.0f) {
    pct_y = 0.0f;
  } else if (pct_y > (float)LOOKUP2D_SEGMENTS_Y) {
    pct_y = (float)LOOKUP2D_SEGMENTS_Y;
  }

  int ix0 = (int)floorf(pct_x);
  int ix1 = (int)ceilf(pct_x);
  int iy0 = (int)floorf(pct_y);
  int iy1 = (int)ceilf(pct_y);

  if (ix0 < 0) ix0 = 0;
  if (ix1 < 0) ix1 = 0;
  if (iy0 < 0) iy0 = 0;
  if (iy1 < 0) iy1 = 0;

  if (ix0 > LOOKUP2D_SEGMENTS_X) ix0 = LOOKUP2D_SEGMENTS_X;
  if (ix1 > LOOKUP2D_SEGMENTS_X) ix1 = LOOKUP2D_SEGMENTS_X;
  if (iy0 > LOOKUP2D_SEGMENTS_Y) iy0 = LOOKUP2D_SEGMENTS_Y;
  if (iy1 > LOOKUP2D_SEGMENTS_Y) iy1 = LOOKUP2D_SEGMENTS_Y;

  float z00 = lookup2d->z[iy0][ix0];
  float z01 = lookup2d->z[iy0][ix1];
  float z10 = lookup2d->z[iy1][ix0];
  float z11 = lookup2d->z[iy1][ix1];

  float ax = pct_x - (float)ix0;
  float ay = pct_y - (float)iy0;

  float z_interp_y0 = (1.0f - ax) * z00 + ax * z01;
  float z_interp_y1 = (1.0f - ax) * z10 + ax * z11;

  return (1.0f - ay) * z_interp_y0 + ay * z_interp_y1;
}