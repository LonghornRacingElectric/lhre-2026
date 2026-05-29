#include "Lookup1D.h"

#include <math.h>
#include <string.h>

#define LOOKUP1D_SEGMENTS (LOOKUP1D_POINTS - 1)

void Lookup1D_init(Lookup1D *lookup1d, float x0, float x1,
                   const float y_data[LOOKUP1D_POINTS]) {
  if (!lookup1d) {
    return;
  }

  lookup1d->x0 = x0;
  lookup1d->x1 = x1;

  if (lookup1d->x1 <= lookup1d->x0) {
    lookup1d->x1 = lookup1d->x0 + 1.0f;
  }

  if (y_data) {
    memcpy(lookup1d->y, y_data, sizeof(lookup1d->y));
  } else {
    memset(lookup1d->y, 0, sizeof(lookup1d->y));
  }
}

float Lookup1D_evaluate(const Lookup1D *lookup1d, float x) {
  if (!lookup1d) {
    return 0.0f;
  }

  float pct_x = (x - lookup1d->x0) / (lookup1d->x1 - lookup1d->x0) *
                (float)LOOKUP1D_SEGMENTS;

  if (pct_x < 0.0f) {
    pct_x = 0.0f;
  } else if (pct_x > (float)LOOKUP1D_SEGMENTS) {
    pct_x = (float)LOOKUP1D_SEGMENTS;
  }

  int ix0 = (int)floorf(pct_x);
  int ix1 = (int)ceilf(pct_x);

  if (ix0 < 0) ix0 = 0;
  if (ix1 < 0) ix1 = 0;
  if (ix0 > LOOKUP1D_SEGMENTS) ix0 = LOOKUP1D_SEGMENTS;
  if (ix1 > LOOKUP1D_SEGMENTS) ix1 = LOOKUP1D_SEGMENTS;

  float ax = pct_x - (float)ix0;

  return (1.0f - ax) * lookup1d->y[ix0] + ax * lookup1d->y[ix1];
}
